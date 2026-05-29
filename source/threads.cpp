/*
 * TRIUMVIRATUS - ABDADA Multi-threaded Search
 *
 * ABDADA = Alpha-Beta Distributed Avoiding Duplicate Analysis
 *
 * Key insight: In Lazy SMP, threads often search the same nodes simultaneously,
 * wasting work. ABDADA marks nodes as "busy" when a thread starts searching them.
 * Other threads skip busy nodes (for non-critical moves) and search them later
 * if still needed.
 *
 * Result: Real time speedup, not just NPS increase!
 */

#include "threads.h"
#include "tt.h"
#include "search.h"
#include "misc.h"
#include "movegen.h"
#include "evaluation.h"
#include "magic.h"
#include "attacks.h"
#include "see.h"
#include "sf_bridge.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include "io.h"
#include "defs.h"

#include "policy_bridge.h"
#include "syzygy.h"

 // ============================================================================
 // GLOBALS
 // ============================================================================

tt_entry* hash_table = nullptr;

// Stockfish piece codes indexed by Triumviratus piece (P,N,B,R,Q,K,p,n,b,r,q,k):
//   white W_PAWN..W_KING = 1..6, black B_PAWN..B_KING = 9..14. Defined here
//   (before td_make_move) so the make-move NNUE mirror hook can use it too.
static const int sf_piece_code[12] = { 1, 2, 3, 4, 5, 6, 9, 10, 11, 12, 13, 14 };

// Policy on/off toggle (UCI option "UsePolicy"). Lets a cutechess match A/B the
// hybrid (policy) build against pure NNUE using the SAME binary, to measure
// whether the policy actually adds Elo over the existing move ordering.
static bool g_use_policy = true;
void set_use_policy(bool v) { g_use_policy = v; }

// ---- Self-play data logging (Phase 1: policy-net training dataset) ----------
// When enabled, each completed root search appends one TSV record:
//   <FEN> \t <bestmove-uci> \t <score-cp (side-to-move relative)> \t <depth>
// Toggled via the DataLog / DataFile UCI options. The actual output file gets a
// ".<pid>" suffix per process, so multiple parallel self-play instances never
// collide; merge them afterwards (e.g. cat triumviratus_dataset.txt.* > all.txt).
static bool        g_data_log_enabled = false;
static std::string g_data_log_file    = "triumviratus_dataset.txt";

void set_data_log_enabled(bool enabled) { g_data_log_enabled = enabled; }
void set_data_log_file(const char* path) { if (path && *path) g_data_log_file = path; }

static void log_search_record(int best_move, int score, int depth) {
    // Per-process file (".<pid>") so parallel self-play instances never
    // interleave/corrupt one another. Merge later: cat <DataFile>.* > all.txt
    std::string path = g_data_log_file + "." + std::to_string((unsigned long)GetCurrentProcessId());
    std::ofstream out(path, std::ios::app | std::ios::binary);  // pure '\n'
    if (!out) return;
    out << board_to_fen() << '\t' << move_to_uci(best_move)
        << '\t' << score << '\t' << depth << '\n';
}
U64 hash_entries = 0;
int current_age = 0;

extern U64 nodes;

std::vector<std::thread> search_threads;
std::vector<ThreadData> thread_data;
std::atomic<bool> stop_threads(false);
std::mutex output_mutex;
int num_threads = 1;
int search_start_time = 0;
int soft_time_limit = 0;

// ============================================================================
// LMR TABLE
// ============================================================================

int lmr_table[64][64];

void init_lmr_table() {
    for (int depth = 0; depth < 64; depth++) {
        for (int moves = 0; moves < 64; moves++) {
            if (depth == 0 || moves == 0) {
                lmr_table[depth][moves] = 0;
            }
            else {
                lmr_table[depth][moves] = (int)(0.75 + log(depth) * log(moves) / 2.25);
            }
        }
    }
}

// LMP thresholds
const int lmp_table[9] = { 0, 3, 5, 8, 13, 20, 30, 42, 56 };

// ============================================================================
// UNDO STRUCTURE
// ============================================================================

struct UndoInfo {
    int captured_piece;
    int captured_square;
    int old_castle;
    int old_enpassant;
    int old_fifty;
    U64 old_hash;
};

// ============================================================================
// THREAD INITIALIZATION
// ============================================================================

void init_threads(int thread_count) {
    if (thread_count < 1) thread_count = 1;
    if (thread_count > MAX_THREADS) thread_count = MAX_THREADS;

    // Free any existing incremental-NNUE handles before resizing (resize may
    // move/destroy elements); they are recreated fresh in the loop below.
    for (auto& td : thread_data)
        if (td.sfpos) { sf_pos_destroy(td.sfpos); td.sfpos = nullptr; }

    num_threads = thread_count;
    thread_data.resize(num_threads);

    static bool lmr_initialized = false;
    if (!lmr_initialized) {
        init_lmr_table();
        lmr_initialized = true;
    }

    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].sfpos = sf_pos_create();
        thread_data[i].nodes = 0;
        thread_data[i].best_move = 0;
        thread_data[i].best_score = -infinity;
        thread_data[i].depth = 0;
        memset(thread_data[i].killer_moves, 0, sizeof(thread_data[i].killer_moves));
        memset(thread_data[i].history_moves, 0, sizeof(thread_data[i].history_moves));
        memset(thread_data[i].counter_moves, 0, sizeof(thread_data[i].counter_moves));
        memset(thread_data[i].capture_history, 0, sizeof(thread_data[i].capture_history));
        memset(thread_data[i].pv_table, 0, sizeof(thread_data[i].pv_table));
        memset(thread_data[i].pv_length, 0, sizeof(thread_data[i].pv_length));
    }
}

void copy_board_to_thread(ThreadData& td) {
    memcpy(td.bitboards, bitboards, sizeof(bitboards));
    memcpy(td.occupancies, occupancies, sizeof(occupancies));
    td.side = side;
    td.enpassant = enpassant;
    td.castle = castle;
    td.hash_key = hash_key;
    td.fifty = fifty;
    memcpy(td.repetition_table, repetition_table, sizeof(repetition_table));
    td.repetition_index = repetition_index;
    td.ply = 0;
    td.nodes = 0;
    td.best_move = 0;
    td.best_score = -infinity;
    td.depth = 0;
}

// ============================================================================
// ATTACK DETECTION
// ============================================================================

static inline int td_is_square_attacked(ThreadData& td, int square, int attacker_side) {
    if (attacker_side == white) {
        if (pawn_attacks[black][square] & td.bitboards[P]) return 1;
        if (knight_attacks[square] & td.bitboards[N]) return 1;
        if (king_attacks[square] & td.bitboards[K]) return 1;
        U64 occ = td.occupancies[both];
        if (get_bishop_attacks(square, occ) & (td.bitboards[B] | td.bitboards[Q])) return 1;
        if (get_rook_attacks(square, occ) & (td.bitboards[R] | td.bitboards[Q])) return 1;
    }
    else {
        if (pawn_attacks[white][square] & td.bitboards[p]) return 1;
        if (knight_attacks[square] & td.bitboards[n]) return 1;
        if (king_attacks[square] & td.bitboards[k]) return 1;
        U64 occ = td.occupancies[both];
        if (get_bishop_attacks(square, occ) & (td.bitboards[b] | td.bitboards[q])) return 1;
        if (get_rook_attacks(square, occ) & (td.bitboards[r] | td.bitboards[q])) return 1;
    }
    return 0;
}

// ============================================================================
// MAKE MOVE (returns 1 if legal)
// ============================================================================

static inline int td_make_move(ThreadData& td, int move, UndoInfo& undo) {
    undo.old_castle = td.castle;
    undo.old_enpassant = td.enpassant;
    undo.old_fifty = td.fifty;
    undo.old_hash = td.hash_key;
    undo.captured_piece = -1;
    undo.captured_square = -1;

    int source = get_move_source(move);
    int target = get_move_target(move);
    int piece = get_move_piece(move);
    int promoted = get_move_promoted(move);
    int capture = get_move_capture(move);
    int double_push = get_move_double(move);
    int enpass = get_move_enpassant(move);
    int castling = get_move_castling(move);

    pop_bit(td.bitboards[piece], source);
    set_bit(td.bitboards[piece], target);
    td.hash_key ^= piece_keys[piece][source];
    td.hash_key ^= piece_keys[piece][target];

    td.fifty++;
    if (piece == P || piece == p) td.fifty = 0;

    if (capture) {
        td.fifty = 0;

        if (enpass) {
            undo.captured_square = (td.side == white) ? target + 8 : target - 8;
            undo.captured_piece = (td.side == white) ? p : P;
        }
        else {
            undo.captured_square = target;
            int start = (td.side == white) ? p : P;
            int end = (td.side == white) ? k : K;
            for (int pc = start; pc <= end; pc++) {
                if (get_bit(td.bitboards[pc], target)) {
                    undo.captured_piece = pc;
                    break;
                }
            }
        }

        if (undo.captured_piece != -1) {
            pop_bit(td.bitboards[undo.captured_piece], undo.captured_square);
            td.hash_key ^= piece_keys[undo.captured_piece][undo.captured_square];
        }
    }

    if (promoted) {
        pop_bit(td.bitboards[piece], target);
        td.hash_key ^= piece_keys[piece][target];
        set_bit(td.bitboards[promoted], target);
        td.hash_key ^= piece_keys[promoted][target];
    }

    if (td.enpassant != no_sq) td.hash_key ^= enpassant_keys[td.enpassant];
    td.enpassant = no_sq;

    if (double_push) {
        td.enpassant = (td.side == white) ? target + 8 : target - 8;
        td.hash_key ^= enpassant_keys[td.enpassant];
    }

    if (castling) {
        switch (target) {
        case g1:
            pop_bit(td.bitboards[R], h1);
            set_bit(td.bitboards[R], f1);
            td.hash_key ^= piece_keys[R][h1];
            td.hash_key ^= piece_keys[R][f1];
            break;
        case c1:
            pop_bit(td.bitboards[R], a1);
            set_bit(td.bitboards[R], d1);
            td.hash_key ^= piece_keys[R][a1];
            td.hash_key ^= piece_keys[R][d1];
            break;
        case g8:
            pop_bit(td.bitboards[r], h8);
            set_bit(td.bitboards[r], f8);
            td.hash_key ^= piece_keys[r][h8];
            td.hash_key ^= piece_keys[r][f8];
            break;
        case c8:
            pop_bit(td.bitboards[r], a8);
            set_bit(td.bitboards[r], d8);
            td.hash_key ^= piece_keys[r][a8];
            td.hash_key ^= piece_keys[r][d8];
            break;
        }
    }

    td.hash_key ^= castle_keys[td.castle];
    td.castle &= castling_rights[source];
    td.castle &= castling_rights[target];
    td.hash_key ^= castle_keys[td.castle];

    td.occupancies[white] = td.bitboards[P] | td.bitboards[N] | td.bitboards[B] |
        td.bitboards[R] | td.bitboards[Q] | td.bitboards[K];
    td.occupancies[black] = td.bitboards[p] | td.bitboards[n] | td.bitboards[b] |
        td.bitboards[r] | td.bitboards[q] | td.bitboards[k];
    td.occupancies[both] = td.occupancies[white] | td.occupancies[black];

    td.side ^= 1;
    td.hash_key ^= side_key;

    int king_sq = get_ls1b_index((td.side == white) ? td.bitboards[k] : td.bitboards[K]);
    if (td_is_square_attacked(td, king_sq, td.side)) {
        td.side ^= 1;

        if (promoted) {
            pop_bit(td.bitboards[promoted], target);
            set_bit(td.bitboards[piece], source);
        }
        else {
            pop_bit(td.bitboards[piece], target);
            set_bit(td.bitboards[piece], source);
        }

        if (undo.captured_piece != -1) {
            set_bit(td.bitboards[undo.captured_piece], undo.captured_square);
        }

        if (castling) {
            switch (target) {
            case g1: pop_bit(td.bitboards[R], f1); set_bit(td.bitboards[R], h1); break;
            case c1: pop_bit(td.bitboards[R], d1); set_bit(td.bitboards[R], a1); break;
            case g8: pop_bit(td.bitboards[r], f8); set_bit(td.bitboards[r], h8); break;
            case c8: pop_bit(td.bitboards[r], d8); set_bit(td.bitboards[r], a8); break;
            }
        }

        td.castle = undo.old_castle;
        td.enpassant = undo.old_enpassant;
        td.fifty = undo.old_fifty;
        td.hash_key = undo.old_hash;

        td.occupancies[white] = td.bitboards[P] | td.bitboards[N] | td.bitboards[B] |
            td.bitboards[R] | td.bitboards[Q] | td.bitboards[K];
        td.occupancies[black] = td.bitboards[p] | td.bitboards[n] | td.bitboards[b] |
            td.bitboards[r] | td.bitboards[q] | td.bitboards[k];
        td.occupancies[both] = td.occupancies[white] | td.occupancies[black];

        return 0;
    }

    // Mirror the (now legal) move on the incremental NNUE position, in
    // Stockfish encoding. The moving piece is dirtyPiece[0] (king-refresh).
    {
        SfMove sm;
        sm.movedPiece    = sf_piece_code[piece];
        sm.from          = nnue_squares[source];
        sm.to            = nnue_squares[target];
        sm.promoPiece    = promoted ? sf_piece_code[promoted] : 0;
        sm.capturedPiece = (undo.captured_piece != -1) ? sf_piece_code[undo.captured_piece] : 0;
        sm.capturedSq    = (undo.captured_piece != -1) ? nnue_squares[undo.captured_square] : -1;
        sm.rookPiece = 0; sm.rookFrom = -1; sm.rookTo = -1;
        if (castling) {
            int rpc = R, rf = h1, rt = f1;
            switch (target) {
                case g1: rpc = R; rf = h1; rt = f1; break;
                case c1: rpc = R; rf = a1; rt = d1; break;
                case g8: rpc = r; rf = h8; rt = f8; break;
                case c8: rpc = r; rf = a8; rt = d8; break;
            }
            sm.rookPiece = sf_piece_code[rpc];
            sm.rookFrom  = nnue_squares[rf];
            sm.rookTo    = nnue_squares[rt];
        }
        sm.rule50 = td.fifty;
        sf_pos_do(td.sfpos, &sm);
    }

    return 1;
}

// ============================================================================
// UNMAKE MOVE
// ============================================================================

static inline void td_unmake_move(ThreadData& td, int move, UndoInfo& undo) {
    sf_pos_undo(td.sfpos);   // retract the move on the incremental NNUE mirror
    td.side ^= 1;

    int source = get_move_source(move);
    int target = get_move_target(move);
    int piece = get_move_piece(move);
    int promoted = get_move_promoted(move);
    int castling = get_move_castling(move);

    if (promoted) {
        pop_bit(td.bitboards[promoted], target);
        set_bit(td.bitboards[piece], source);
    }
    else {
        pop_bit(td.bitboards[piece], target);
        set_bit(td.bitboards[piece], source);
    }

    if (undo.captured_piece != -1) {
        set_bit(td.bitboards[undo.captured_piece], undo.captured_square);
    }

    if (castling) {
        switch (target) {
        case g1: pop_bit(td.bitboards[R], f1); set_bit(td.bitboards[R], h1); break;
        case c1: pop_bit(td.bitboards[R], d1); set_bit(td.bitboards[R], a1); break;
        case g8: pop_bit(td.bitboards[r], f8); set_bit(td.bitboards[r], h8); break;
        case c8: pop_bit(td.bitboards[r], d8); set_bit(td.bitboards[r], a8); break;
        }
    }

    td.castle = undo.old_castle;
    td.enpassant = undo.old_enpassant;
    td.fifty = undo.old_fifty;
    td.hash_key = undo.old_hash;

    td.occupancies[white] = td.bitboards[P] | td.bitboards[N] | td.bitboards[B] |
        td.bitboards[R] | td.bitboards[Q] | td.bitboards[K];
    td.occupancies[black] = td.bitboards[p] | td.bitboards[n] | td.bitboards[b] |
        td.bitboards[r] | td.bitboards[q] | td.bitboards[k];
    td.occupancies[both] = td.occupancies[white] | td.occupancies[black];
}

// ============================================================================
// MOVE GENERATION
// ============================================================================

static void td_generate_moves(ThreadData& td, moves* move_list, bool captures_only = false) {
    move_list->count = 0;
    int source_square, target_square;
    U64 bitboard, attacks;

    // --- MAGIA BITWISE: Precalcoliamo le maschere ---
    U64 enemies = (td.side == white) ? td.occupancies[black] : td.occupancies[white];
    U64 friends = (td.side == white) ? td.occupancies[white] : td.occupancies[black];
    // Se vogliamo solo catture, le uniche case valide sono quelle nemiche. 
    // Altrimenti, tutte le case tranne le nostre.
    U64 allowed_squares = captures_only ? enemies : ~friends;

    for (int piece = P; piece <= k; piece++) {
        bitboard = td.bitboards[piece];

        if (td.side == white) {
            if (piece == P) {
                while (bitboard) {
                    source_square = get_ls1b_index(bitboard);
                    target_square = source_square - 8;

                    // Mosse silenziose dei pedoni: bloccate se captures_only � true
                    if (!captures_only) {
                        if (!(target_square < a8) && !get_bit(td.occupancies[both], target_square)) {
                            if (source_square >= a7 && source_square <= h7) {
                                add_move(move_list, encode_move(source_square, target_square, piece, Q, 0, 0, 0, 0));
                                add_move(move_list, encode_move(source_square, target_square, piece, R, 0, 0, 0, 0));
                                add_move(move_list, encode_move(source_square, target_square, piece, B, 0, 0, 0, 0));
                                add_move(move_list, encode_move(source_square, target_square, piece, N, 0, 0, 0, 0));
                            }
                            else {
                                add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                                if ((source_square >= a2 && source_square <= h2) && !get_bit(td.occupancies[both], target_square - 8))
                                    add_move(move_list, encode_move(source_square, target_square - 8, piece, 0, 0, 1, 0, 0));
                            }
                        }
                    }

                    // Catture dei pedoni
                    attacks = pawn_attacks[td.side][source_square] & enemies;
                    while (attacks) {
                        target_square = get_ls1b_index(attacks);
                        if (source_square >= a7 && source_square <= h7) {
                            add_move(move_list, encode_move(source_square, target_square, piece, Q, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, R, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, B, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, N, 1, 0, 0, 0));
                        }
                        else {
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                        }
                        pop_bit(attacks, target_square);
                    }

                    // En passant (� sempre una cattura, lo generiamo sempre)
                    if (td.enpassant != no_sq) {
                        U64 enpassant_attacks = pawn_attacks[td.side][source_square] & (1ULL << td.enpassant);
                        if (enpassant_attacks) {
                            int target_enpassant = get_ls1b_index(enpassant_attacks);
                            add_move(move_list, encode_move(source_square, target_enpassant, piece, 0, 1, 0, 1, 0));
                        }
                    }
                    pop_bit(bitboard, source_square);
                }
            }
            if (piece == K) {
                // Arrocco: bloccato se captures_only � true
                if (!captures_only) {
                    if (td.castle & wk) {
                        if (!get_bit(td.occupancies[both], f1) && !get_bit(td.occupancies[both], g1)) {
                            if (!td_is_square_attacked(td, e1, black) && !td_is_square_attacked(td, f1, black))
                                add_move(move_list, encode_move(e1, g1, piece, 0, 0, 0, 0, 1));
                        }
                    }
                    if (td.castle & wq) {
                        if (!get_bit(td.occupancies[both], d1) && !get_bit(td.occupancies[both], c1) && !get_bit(td.occupancies[both], b1)) {
                            if (!td_is_square_attacked(td, e1, black) && !td_is_square_attacked(td, d1, black))
                                add_move(move_list, encode_move(e1, c1, piece, 0, 0, 0, 0, 1));
                        }
                    }
                }
            }
        }
        else {
            if (piece == p) {
                while (bitboard) {
                    source_square = get_ls1b_index(bitboard);
                    target_square = source_square + 8;

                    if (!captures_only) {
                        if (!(target_square > h1) && !get_bit(td.occupancies[both], target_square)) {
                            if (source_square >= a2 && source_square <= h2) {
                                add_move(move_list, encode_move(source_square, target_square, piece, q, 0, 0, 0, 0));
                                add_move(move_list, encode_move(source_square, target_square, piece, r, 0, 0, 0, 0));
                                add_move(move_list, encode_move(source_square, target_square, piece, b, 0, 0, 0, 0));
                                add_move(move_list, encode_move(source_square, target_square, piece, n, 0, 0, 0, 0));
                            }
                            else {
                                add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                                if ((source_square >= a7 && source_square <= h7) && !get_bit(td.occupancies[both], target_square + 8))
                                    add_move(move_list, encode_move(source_square, target_square + 8, piece, 0, 0, 1, 0, 0));
                            }
                        }
                    }

                    attacks = pawn_attacks[td.side][source_square] & enemies;
                    while (attacks) {
                        target_square = get_ls1b_index(attacks);
                        if (source_square >= a2 && source_square <= h2) {
                            add_move(move_list, encode_move(source_square, target_square, piece, q, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, r, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, b, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, n, 1, 0, 0, 0));
                        }
                        else {
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                        }
                        pop_bit(attacks, target_square);
                    }
                    if (td.enpassant != no_sq) {
                        U64 enpassant_attacks = pawn_attacks[td.side][source_square] & (1ULL << td.enpassant);
                        if (enpassant_attacks) {
                            int target_enpassant = get_ls1b_index(enpassant_attacks);
                            add_move(move_list, encode_move(source_square, target_enpassant, piece, 0, 1, 0, 1, 0));
                        }
                    }
                    pop_bit(bitboard, source_square);
                }
            }
            if (piece == k) {
                if (!captures_only) {
                    if (td.castle & bk) {
                        if (!get_bit(td.occupancies[both], f8) && !get_bit(td.occupancies[both], g8)) {
                            if (!td_is_square_attacked(td, e8, white) && !td_is_square_attacked(td, f8, white))
                                add_move(move_list, encode_move(e8, g8, piece, 0, 0, 0, 0, 1));
                        }
                    }
                    if (td.castle & bq) {
                        if (!get_bit(td.occupancies[both], d8) && !get_bit(td.occupancies[both], c8) && !get_bit(td.occupancies[both], b8)) {
                            if (!td_is_square_attacked(td, e8, white) && !td_is_square_attacked(td, d8, white))
                                add_move(move_list, encode_move(e8, c8, piece, 0, 0, 0, 0, 1));
                        }
                    }
                }
            }
        }

        // --- PEZZI (Qui applichiamo la maschera allowed_squares) ---
        if ((td.side == white) ? piece == N : piece == n) {
            while (bitboard) {
                source_square = get_ls1b_index(bitboard);
                attacks = knight_attacks[source_square] & allowed_squares;
                while (attacks) {
                    target_square = get_ls1b_index(attacks);
                    if (!get_bit(enemies, target_square)) add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    else add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }

        if ((td.side == white) ? piece == B : piece == b) {
            while (bitboard) {
                source_square = get_ls1b_index(bitboard);
                attacks = get_bishop_attacks(source_square, td.occupancies[both]) & allowed_squares;
                while (attacks) {
                    target_square = get_ls1b_index(attacks);
                    if (!get_bit(enemies, target_square)) add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    else add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }

        if ((td.side == white) ? piece == R : piece == r) {
            while (bitboard) {
                source_square = get_ls1b_index(bitboard);
                attacks = get_rook_attacks(source_square, td.occupancies[both]) & allowed_squares;
                while (attacks) {
                    target_square = get_ls1b_index(attacks);
                    if (!get_bit(enemies, target_square)) add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    else add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }

        if ((td.side == white) ? piece == Q : piece == q) {
            while (bitboard) {
                source_square = get_ls1b_index(bitboard);
                attacks = get_queen_attacks(source_square, td.occupancies[both]) & allowed_squares;
                while (attacks) {
                    target_square = get_ls1b_index(attacks);
                    if (!get_bit(enemies, target_square)) add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    else add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }

        if ((td.side == white) ? piece == K : piece == k) {
            while (bitboard) {
                source_square = get_ls1b_index(bitboard);
                attacks = king_attacks[source_square] & allowed_squares;
                while (attacks) {
                    target_square = get_ls1b_index(attacks);
                    if (!get_bit(enemies, target_square)) add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    else add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }
    }
}

// ============================================================================
// EVALUATION
// ============================================================================

// Build the Stockfish piece list (codes + squares) from a thread's board.
// Returns the number of pieces written into pieces[]/squares[].
static inline int sf_build_piece_list(ThreadData& td, int pieces[33], int squares[33]) {
    int count = 0;
    for (int bb_piece = P; bb_piece <= k; bb_piece++) {
        U64 bb = td.bitboards[bb_piece];
        while (bb) {
            int sq = get_ls1b_index(bb);
            pieces[count]  = sf_piece_code[bb_piece];
            squares[count] = nnue_squares[sq];   // engine square -> SF square (a1=0..h8=63)
            count++;
            pop_bit(bb, sq);
        }
    }
    return count;
}

// Re-sync the incremental NNUE mirror to the thread's current board (full
// refresh). Called at the root of each iterative-deepening iteration.
static inline void sf_root_sync(ThreadData& td) {
    int pieces[33], squares[33];
    int count = sf_build_piece_list(td, pieces, squares);
    sf_pos_set(td.sfpos, td.side == white, pieces, squares, count, td.fifty);
}

static inline int td_evaluate(ThreadData& td) {
#ifdef NNUE_VERIFY
    // Verification oracle: the incrementally maintained eval must EXACTLY equal
    // a full from-scratch refresh. Build with /DNNUE_VERIFY to enable.
    int pieces[33], squares[33];
    int count = sf_build_piece_list(td, pieces, squares);
    int full  = sf_eval(td.side == white, pieces, squares, count, td.fifty);
    int inc   = sf_pos_eval(td.sfpos);
    if (full != inc) {
        std::cerr << "NNUE MISMATCH ply=" << td.ply << " full=" << full
                  << " inc=" << inc << " hash=0x" << std::hex << td.hash_key << std::dec
                  << std::endl;
        std::exit(2);
    }
    return inc;
#else
    // Incremental: the accumulator is updated only for the pieces that changed
    // (HalfKAv2_hm king-bucket refresh handled by Stockfish's transform()).
    return sf_pos_eval(td.sfpos);
#endif
}

// ============================================================================
// MOVE SCORING
// ============================================================================

// Maximum magnitude of a history score. Kept below the killer-move scores
// (8000) so quiet history can never outrank killers or captures, and bounded
// via "gravity" so the table cannot overflow over a long search.
#define HISTORY_MAX 7000

// Update a history entry with gravity: the value saturates towards
// +/-HISTORY_MAX, giving recent evidence more weight (Stockfish-style).
static inline void td_update_history(int& h, int bonus) {
    if (bonus > HISTORY_MAX) bonus = HISTORY_MAX;
    else if (bonus < -HISTORY_MAX) bonus = -HISTORY_MAX;
    h += bonus - h * (bonus < 0 ? -bonus : bonus) / HISTORY_MAX;
}

// Same gravity update for the int16_t continuation-history entries.
static inline void td_update_history(int16_t& h, int bonus) {
    if (bonus > HISTORY_MAX) bonus = HISTORY_MAX;
    else if (bonus < -HISTORY_MAX) bonus = -HISTORY_MAX;
    int v = (int)h + bonus - (int)h * (bonus < 0 ? -bonus : bonus) / HISTORY_MAX;
    h = (int16_t)v;
}

// Move-ordering score bands (well separated so the additive quiet histories,
// range about +/-2*HISTORY_MAX, never overlap the "special" move scores):
//   TT move > good captures > killers > counter-move > quiet history > bad captures
#define SCORE_TT_MOVE       2000000
#define SCORE_GOOD_CAPTURE   700000
#define SCORE_KILLER0        600000
#define SCORE_KILLER1        590000
#define SCORE_COUNTER        580000
#define SCORE_BAD_CAPTURE   (-700000)

// Pezzo catturato sulla casa target (en passant -> P, coerente con lo scoring).
static inline int td_captured_piece(ThreadData& td, int target) {
    int start = (td.side == white) ? p : P;
    int end   = (td.side == white) ? k : K;
    for (int pc = start; pc <= end; pc++)
        if (get_bit(td.bitboards[pc], target)) return pc;
    return P;
}

static inline int td_score_move(ThreadData& td, int move, int tt_move) {
    if (move == tt_move) return SCORE_TT_MOVE;

    int piece = get_move_piece(move);
    int target = get_move_target(move);

    // Le promozioni sono bombe tattiche: vanno ordinate insieme alle catture
    // forti, non confuse con le mosse silenziose lente.
    if (get_move_promoted(move)) {
        return SCORE_GOOD_CAPTURE + 50000;
    }

    if (get_move_capture(move)) {
        int victim = P;
        int start = (td.side == white) ? p : P;
        int end = (td.side == white) ? k : K;
        for (int pc = start; pc <= end; pc++) {
            if (get_bit(td.bitboards[pc], target)) {
                victim = pc;
                break;
            }
        }

        int caphist = td.capture_history[piece][target][victim];//qui era diviso 16

        // Lazy SEE: capturing a piece of equal-or-greater value is good by
        // definition (no SEE needed). Only run SEE for "attacker > victim"
        // captures, which might be losing. Losing captures (SEE < 0) are
        // ordered AFTER all quiets (still searched, just last).
        if (see_piece_values[piece] <= see_piece_values[victim] || td_see(td, move) >= 0)
            return SCORE_GOOD_CAPTURE + mvv_lva[piece][victim] + caphist;
        else
            return SCORE_BAD_CAPTURE + mvv_lva[piece][victim] + caphist;
    }

    // Quiet moves
    if (td.killer_moves[0][td.ply] == move) return SCORE_KILLER0;
    if (td.killer_moves[1][td.ply] == move) return SCORE_KILLER1;

    int prev = td.move_stack[td.ply];
    if (prev && td.counter_moves[get_move_piece(prev)][get_move_target(prev)] == move)
        return SCORE_COUNTER;

    // Sum butterfly history with continuation history (the move scored in the
    // context of the previous move) at full weight.
    int h = td.history_moves[piece][target];
    if (prev)
        h += td.continuation_history[get_move_piece(prev)][get_move_target(prev)][piece][target];
    return h;
}

// Aggiunti i parametri opzionali per la Policy
static inline void td_sort_moves(ThreadData& td, moves* move_list, int tt_move, float* policy_scores = nullptr, bool policy_used = false) {
    int scores[256];

    // Codifica indici policy CANONICA sul lato che muove (deve combaciare con
    // evaluate_policy / chess_encoding.py): Bianco -> (sq ^ 56), Nero -> sq.
    const bool wtm = (td.side == white);

    for (int i = 0; i < move_list->count; i++) {
        int move = move_list->moves[i];

        // Punteggio base (TT, catture, history, killer)
        scores[i] = td_score_move(td, move, tt_move);

        // Se la Policy   attiva e la mossa   tranquilla, aggiungiamo il super-potere!
        if (policy_used && policy_scores != nullptr && !get_move_capture(move) && move != tt_move) {
            int src = get_move_source(move), tgt = get_move_target(move);
            int cs = wtm ? (src ^ 56) : src;
            int ct = wtm ? (tgt ^ 56) : tgt;
            int p_idx = cs * 64 + ct;
            // Peso ridotto (era *500, tarato sulla VECCHIA rete col pooling).
            // La rete nuova ha logit ~[-8,+11]: *50 rende il bonus una spinta
            // che asseconda la history senza dominarla.
            int policy_bonus = (int)(policy_scores[p_idx] * 250.0f);
            scores[i] += policy_bonus;
        }
    }

    // --- Ordinamento standard (Selection Sort) ---
    for (int i = 0; i < move_list->count - 1; i++) {
        int best = i;
        for (int j = i + 1; j < move_list->count; j++) {
            if (scores[j] > scores[best]) best = j;
        }
        if (best != i) {
            int tmp_move = move_list->moves[i];
            move_list->moves[i] = move_list->moves[best];
            move_list->moves[best] = tmp_move;

            int tmp_score = scores[i];
            scores[i] = scores[best];
            scores[best] = tmp_score;
        }
    }
}

// Nuova funzione che calcola solo gli score ma NON ordina
static inline void td_score_all_moves(ThreadData& td, moves* move_list, int tt_move, int* scores, float* policy_scores = nullptr, bool policy_used = false) {
    const bool wtm = (td.side == white);
    for (int i = 0; i < move_list->count; i++) {
        int move = move_list->moves[i];
        scores[i] = td_score_move(td, move, tt_move);

        // (La logica della tua policy rimane identica qui)
        if (policy_used && policy_scores != nullptr && !get_move_capture(move) && move != tt_move) {
            int src = get_move_source(move), tgt = get_move_target(move);
            int cs = wtm ? (src ^ 56) : src;
            int ct = wtm ? (tgt ^ 56) : tgt;
            int p_idx = cs * 64 + ct;
            scores[i] += (int)(policy_scores[p_idx] * 250.0f);
        }
    }
}

// ============================================================================
// REPETITION DETECTION
// ============================================================================

static inline int td_is_repetition(ThreadData& td) {
    for (int i = 0; i < td.repetition_index; i++) {
        if (td.repetition_table[i] == td.hash_key)
            return 1;
    }
    return 0;
}

// ============================================================================
// QUIESCENCE SEARCH
// ============================================================================

static int td_quiescence(ThreadData& td, int alpha, int beta) {
    // Illegal-position / king-capture guard (see td_negamax_abdada for the full
    // rationale): never search a position where the side not to move is in check,
    // because making the king capture desyncs the NNUE accumulator and crashes.
    {
        int opp_king_sq = get_ls1b_index((td.side == white) ? td.bitboards[k] : td.bitboards[K]);
        if (td_is_square_attacked(td, opp_king_sq, td.side))
            return mate_value - td.ply;
    }
    if ((td.nodes & 4095) == 0) {
        if (stop_threads.load(std::memory_order_relaxed)) return 0;
        if (timeset && get_time_ms() > stoptime) {
            stop_threads.store(true, std::memory_order_relaxed);
            return 0;
        }
    }

    td.nodes++;

    if (td.ply >= max_ply) return td_evaluate(td);

    bool pv_node = (beta - alpha > 1);

    // TT probe (qsearch entries stored at depth 0)
    int tt_move = 0, tt_score = 0, tt_depth = 0, tt_flag = hash_flag_alpha;
    bool tt_busy = false;
    bool tt_hit = probe_tt(td.hash_key, tt_move, tt_score, tt_depth, tt_flag, tt_busy);
    if (tt_hit && !pv_node) {
        if (tt_score < -mate_score) tt_score += td.ply;
        if (tt_score > mate_score) tt_score -= td.ply;
        if (tt_flag == hash_flag_exact) return tt_score;
        if (tt_flag == hash_flag_alpha && tt_score <= alpha) return tt_score;
        if (tt_flag == hash_flag_beta && tt_score >= beta) return tt_score;
    }

    // Sotto scacco: niente stand-pat, si cercano TUTTE le evasioni.
    int king_sq = get_ls1b_index((td.side == white) ? td.bitboards[K] : td.bitboards[k]);
    bool in_check = td_is_square_attacked(td, king_sq, td.side ^ 1);

    int best_score;
    if (in_check) {
        best_score = -infinity;
    }
    else {
        int stand_pat = td_evaluate(td);
        best_score = stand_pat;
        if (stand_pat >= beta) return stand_pat;

        const int DELTA = 1000;
        if (stand_pat + DELTA < alpha) return stand_pat;

        if (stand_pat > alpha) alpha = stand_pat;
    }

    moves move_list[1];
    // Se non siamo sotto scacco (!in_check), passa true per generare SOLO le catture!
    td_generate_moves(td, move_list, !in_check);

    // --- NUOVA FASE: Calcola punteggi senza ordinare (Pick-Next) ---
    int move_scores[256];
    // Chiamiamo la funzione senza Policy, tanto in Q-Search esploriamo perlopi� catture
    td_score_all_moves(td, move_list, tt_move, move_scores);

    int best_move = 0;
    int legal_moves = 0;

    for (int count = 0; count < move_list->count; count++) {

        // --- INIZIO PICK-NEXT: Cerca la mossa migliore tra quelle rimaste ---
        int best_idx = count;
        for (int i = count + 1; i < move_list->count; i++) {
            if (move_scores[i] > move_scores[best_idx]) {
                best_idx = i;
            }
        }

        // Scambiamo mossa e score per portarli in cima
        if (best_idx != count) {
            int tmp_move = move_list->moves[count];
            move_list->moves[count] = move_list->moves[best_idx];
            move_list->moves[best_idx] = tmp_move;

            int tmp_score = move_scores[count];
            move_scores[count] = move_scores[best_idx];
            move_scores[best_idx] = tmp_score;
        }

        // Ora move � garantita essere la migliore disponibile
        int move = move_list->moves[count];
        // --- FINE PICK-NEXT ---

        if (!in_check) {
            if (!get_move_capture(move)) continue;
            if (!get_move_promoted(move) && td_see(td, move) < 0) continue;
        }

        UndoInfo undo;
        td.ply++;
        td.repetition_table[++td.repetition_index] = td.hash_key;

        if (!td_make_move(td, move, undo)) {
            td.ply--;
            td.repetition_index--;
            continue;
        }

        legal_moves++;

        int score = -td_quiescence(td, -beta, -alpha);

        td_unmake_move(td, move, undo);
        td.ply--;
        td.repetition_index--;

        if (stop_threads.load(std::memory_order_relaxed)) return 0;

        if (score > best_score) {
            best_score = score;
            best_move = move;
            if (score > alpha) {
                alpha = score;
                if (score >= beta) break;
            }
        }
    }

    if (in_check && legal_moves == 0)
        return -mate_value + td.ply;

    int store_flag = (best_score >= beta) ? hash_flag_beta : hash_flag_alpha;
    store_tt(td.hash_key, best_move, best_score, 0, store_flag, td.ply);

    return best_score;
}

// ============================================================================
// ABDADA NEGAMAX - The key innovation!
// ============================================================================

// ABDADA parameters
#define ABDADA_THRESHOLD 3  // Only use ABDADA at depth >= this

int td_negamax_abdada(ThreadData& td, int alpha, int beta, int depth, bool is_cut_node, int excluded_move = 0) {
    td.pv_length[td.ply] = td.ply;

    // Illegal-position / king-capture guard. If the side-to-move can capture the
    // opponent's king (i.e. the side NOT to move is in check), the position is
    // illegal - typically a malformed input FEN (e.g. a tablebase test position
    // with the enemy king left in check). Searching it would let us actually make
    // the king-capturing move; td_make_move accepts it (our own king is safe),
    // sf_pos_do then removes the enemy king from the NNUE mirror, and the next
    // eval indexes a king-bucket feature for a now-kingless side -> access
    // violation. Return an immediate winning score instead. In legal play the
    // side not to move is never in check, so this costs one attack probe and
    // never fires.
    {
        int opp_king_sq = get_ls1b_index((td.side == white) ? td.bitboards[k] : td.bitboards[K]);
        if (td_is_square_attacked(td, opp_king_sq, td.side))
            return mate_value - td.ply;
    }

    if (td.ply && (td_is_repetition(td) || td.fifty >= 100)) return 0;

    bool pv_node = (beta - alpha > 1);
    int tt_move = 0;
    int tt_score = 0;
    int tt_depth = 0;
    int tt_flag = hash_flag_alpha;
    bool tt_busy = false;
    int score;

    // TT probe
    bool tt_hit = probe_tt(td.hash_key, tt_move, tt_score, tt_depth, tt_flag, tt_busy);

    // Skip the TT cutoff during a singular search (excluded_move set): we are
    // deliberately re-searching this position without the TT move.
    if (tt_hit && td.ply && !excluded_move) {
        if (tt_depth >= depth && !pv_node) {
            if (tt_score < -mate_score) tt_score += td.ply;
            if (tt_score > mate_score) tt_score -= td.ply;

            if (tt_flag == hash_flag_exact) return tt_score;
            if (tt_flag == hash_flag_alpha && tt_score <= alpha) return tt_score; // fail-soft
            if (tt_flag == hash_flag_beta && tt_score >= beta) return tt_score; // fail-soft
        }
    }

    // Time check
    if ((td.nodes & 4095) == 0) {
        if (stop_threads.load(std::memory_order_relaxed)) return 0;
        if (timeset && get_time_ms() > stoptime) {
            stop_threads.store(true, std::memory_order_relaxed);
            return 0;
        }
    }

    if (depth <= 0) return td_quiescence(td, alpha, beta);

    // Ply ceiling. Set pv_length here so the parent's PV-copy loop reads a sane
    // (terminating) bound from pv_length[ply+1] instead of OOB garbage.
    if (td.ply >= max_ply) { td.pv_length[td.ply] = td.ply; return td_evaluate(td); }

    td.nodes++;

    // Syzygy tablebase WDL probe. For a non-root node with no castling rights
    // and few enough pieces, the tablebase gives the exact game value (side-to-
    // move relative), so we short-circuit the search with it. Cursed/blessed
    // results map to 0 (50-move-rule draws); the actual win conversion in the
    // reached position is guaranteed by the root DTZ probe. count_bits runs
    // only when tablebases are loaded (syzygy_max_pieces() > 0).
    {
        unsigned tb_men = syzygy_max_pieces();
        if (tb_men && td.ply && !excluded_move && td.castle == 0 &&
            (unsigned)count_bits(td.occupancies[both]) <= tb_men) {
            int tb_score;
            if (syzygy_probe_wdl(td, td.ply, tb_score))
                return tb_score;
        }
    }

    int king_sq = get_ls1b_index((td.side == white) ? td.bitboards[K] : td.bitboards[k]);
    bool in_check = td_is_square_attacked(td, king_sq, td.side ^ 1);

    if (in_check) depth++;

    // Internal Iterative Reduction: senza TT move l'ordinamento e' scadente,
    // riduciamo di 1 ply per ottenere a basso costo una hash move.
    if (depth >= 4 && !tt_move && !excluded_move) depth--;

    int static_eval = td_evaluate(td);

    // Reverse futility pruning (skip when beta is a mate bound: don't cut a
    // potential mate search short based on a static evaluation).
    if (!pv_node && !in_check && depth <= 6 && beta < mate_score &&
        static_eval - 80 * depth >= beta) {
        return static_eval - 80 * depth;
    }

    // Null move pruning (not when beta is a mate score)
    if (!pv_node && !in_check && td.ply && depth >= 3 && beta < mate_score && static_eval >= beta) {
        U64 our_pieces = (td.side == white) ?
            (td.bitboards[N] | td.bitboards[B] | td.bitboards[R] | td.bitboards[Q]) :
            (td.bitboards[n] | td.bitboards[b] | td.bitboards[r] | td.bitboards[q]);

        if (our_pieces) {
            int old_ep = td.enpassant;
            U64 old_hash = td.hash_key;

            if (td.enpassant != no_sq) td.hash_key ^= enpassant_keys[td.enpassant];
            td.enpassant = no_sq;
            td.side ^= 1;
            td.hash_key ^= side_key;

            td.ply++;
            td.repetition_table[++td.repetition_index] = td.hash_key;

            // Null move: the child has no "previous move" for counter-move.
            td.move_stack[td.ply] = 0;

            int R = 3 + depth / 4;
            if (R > depth - 1) R = depth - 1;

            sf_pos_do_null(td.sfpos, td.fifty);
            score = -td_negamax_abdada(td, -beta, -beta + 1, depth - 1 - R, !is_cut_node);
            sf_pos_undo(td.sfpos);

            td.ply--;
            td.repetition_index--;
            td.side ^= 1;
            td.hash_key = old_hash;
            td.enpassant = old_ep;

            if (stop_threads.load(std::memory_order_relaxed)) return 0;
            // fail-soft: ritorna lo score reale, ma non un matto "finto" da null move.
            if (score >= beta) return (score >= mate_score) ? beta : score;
        }
    }

    // Razoring
    if (!pv_node && !in_check && depth <= 3) {
        int razor_margin = 300 + 100 * depth;
        if (static_eval + razor_margin < alpha) {
            score = td_quiescence(td, alpha, beta);
            if (score < alpha) return score;
        }
    }

    // 1. Generiamo le mosse PRIMA, cosi' possiamo decidere se la CNN serve.
    moves move_list[1];
    td_generate_moves(td, move_list);

    // 2. Conta le mosse tranquille: in posizioni puramente tattiche (solo
    //    catture / scacchi forzati) la policy non aggiunge ordinamento utile,
    //    quindi saltiamo la forward CNN.
    int quiet_count = 0;
    for (int i = 0; i < move_list->count; i++)
        if (!get_move_capture(move_list->moves[i])) quiet_count++;

    // --- HYBRID: query the policy net SOLO nei nodi della Variante Principale
    // (pv_node) e solo con depth sufficiente. I nodi PV sono pochissimi (~uno
    // per ply lungo la linea principale, NON esponenziali come ply<=3): cosi' le
    // forward CNN restano ~10-20/mossa e gli nps non crollano, ma miglioriamo
    // l'ordinamento proprio dove i cutoff si propagano meglio.
    // (A controllo di tempo, ply<=3 affossava gli nps ~60x -> Hybrid perdeva.)
    static thread_local float policy_scores_by_ply[max_ply][4096];
    float* current_policy_scores = policy_scores_by_ply[td.ply];
    bool policy_used = false;

    // 3. Cache per chiave Zobrist: una posizione distinta paga la forward CNN
    //    UNA sola volta, anche se rivisitata ad ogni iterazione di iterative
    //    deepening o re-search di aspiration.
    // TEST #1: policy SOLO alla radice (ply==0), solo per l'ordinamento delle
    // mosse. E' l'uso dove una rete di predizione-mossa ha piu' senso e costa
    // ~zero (una forward per iterazione ID). Nessun aggiustamento LMR (vedi sotto).
    if (g_use_policy && td.ply == 0 && quiet_count >= 3) {
        static constexpr int PCACHE_SIZE = 1 << 8;   // 256 entry ~4MB/thread (policy root-only: bastano pochissime entry)
        struct PolicyCacheEntry { U64 key; bool valid; float scores[4096]; };
        static thread_local PolicyCacheEntry pcache[PCACHE_SIZE] = {};
        PolicyCacheEntry& slot = pcache[td.hash_key & (PCACHE_SIZE - 1)];
        if (slot.valid && slot.key == td.hash_key) {
            // HIT: copia nel buffer del ply corrente (stabile durante il loop).
            memcpy(current_policy_scores, slot.scores, sizeof(slot.scores));
        }
        else {
            // MISS: una sola forward CNN, poi memorizza in cache.
            evaluate_policy(td, current_policy_scores);
            slot.key = td.hash_key;
            slot.valid = true;
            memcpy(slot.scores, current_policy_scores, sizeof(slot.scores));
        }
        policy_used = true;
    }

    // --- NUOVA FASE: Assegna punteggi senza ordinare l'array (Pick-Next) ---
    int move_scores[256];
    td_score_all_moves(td, move_list, tt_move, move_scores, current_policy_scores, policy_used);

    // 4b. Soglie RANK-BASED per la LMR guidata dalla policy. Usiamo i ranghi
    // (top-3 / quartile inferiore) invece di soglie assolute sul logit, perche'
    // la scala dei logit varia tra versioni della rete. Calcolate UNA volta per
    // nodo; policy_used e' vero solo alla radice, quindi il costo e' trascurabile.
    float policy_hi_cut = 1e9f;    // default: nessuna mossa "alta"
    float policy_lo_cut = -1e9f;   // default: nessuna mossa "bassa"
    if (policy_used) {
        float qlog[256];
        int qn = 0;
        const bool wtm = (td.side == white);
        for (int i = 0; i < move_list->count; i++) {
            int m = move_list->moves[i];
            if (get_move_capture(m) || get_move_promoted(m)) continue;   // solo quiet (come is_quiet)
            int src = get_move_source(m), tgt = get_move_target(m);
            int idx = (wtm ? (src ^ 56) : src) * 64 + (wtm ? (tgt ^ 56) : tgt);
            qlog[qn++] = current_policy_scores[idx];
        }
        if (qn >= 4) {
            std::sort(qlog, qlog + qn, std::greater<float>());
            int hi_i = (qn > 3) ? 2 : (qn - 1);   // confine top-3
            int lo_i = (qn * 3) / 4;              // confine quartile inferiore
            if (lo_i >= qn) lo_i = qn - 1;
            policy_hi_cut = qlog[hi_i];
            policy_lo_cut = qlog[lo_i];
        }
    }

    int best_move = 0;
    int best_score = -infinity;
    int hash_flag = hash_flag_alpha;
    int legal_moves = 0;
    int moves_searched = 0;
    int quiets_searched = 0;

    // Quiet moves searched at this node (for history malus on a beta cutoff).
    int searched_quiets[64];
    int n_searched_quiets = 0;

    int searched_captures[64];
    int n_searched_captures = 0;

    // ABDADA: Deferred moves list
    int deferred[256];
    int deferred_count = 0;

    // First pass: search moves, deferring busy ones
    for (int count = 0; count < move_list->count; count++) {

        // --- INIZIO PICK-NEXT: Cerca la mossa migliore tra quelle rimaste ---
        int best_idx = count;
        for (int i = count + 1; i < move_list->count; i++) {
            if (move_scores[i] > move_scores[best_idx]) {
                best_idx = i;
            }
        }

        // Scambiamo sia la mossa che lo score per portarli in cima
        if (best_idx != count) {
            int tmp_move = move_list->moves[count];
            move_list->moves[count] = move_list->moves[best_idx];
            move_list->moves[best_idx] = tmp_move;

            int tmp_score = move_scores[count];
            move_scores[count] = move_scores[best_idx];
            move_scores[best_idx] = tmp_score;
        }
        // Ora move � garantita essere la migliore
        int move = move_list->moves[count];
        // --- FINE PICK-NEXT ---

        if (move == excluded_move) continue;   // singular search: skip the TT move
        bool is_capture = get_move_capture(move);
        bool is_promotion = get_move_promoted(move);
        bool is_quiet = !is_capture && !is_promotion;

        // LMP
        if (!pv_node && !in_check && depth <= 8 && is_quiet && best_score > -mate_score) {
            int lmp_idx = (depth < 8) ? depth : 8;
            int lmp_threshold = lmp_table[lmp_idx];
            if (quiets_searched >= lmp_threshold) continue;
        }

        // Futility pruning
        if (!pv_node && !in_check && depth <= 6 && is_quiet && best_score > -mate_score) {
            int futility_margin = 100 + 80 * depth;
            if (static_eval + futility_margin <= alpha) {
                quiets_searched++;
                continue;
            }
        }

        // SEE pruning: scarta a bassa profondita' le catture in perdita oltre un
        // margine, prima di cercarle (riduttore di nodi). Promozioni escluse.
        if (!pv_node && !in_check && !is_promotion && depth <= 8 && best_score > -mate_score) {
            int see_margin = is_capture ? (-90 * depth) : (-50 * depth * depth);
            if (td_see(td, move) < see_margin) {
                if (is_quiet) quiets_searched++;
                continue;
            }
        }

        // Singular extension: before searching the TT move, check whether it is
        // much better than every alternative. Re-search this position EXCLUDING
        // the TT move at reduced depth with a window just below tt_score; if all
        // other moves fail low, the TT move is "singular" and gets +1 ply.
        int extension = 0;
        if (excluded_move == 0 && move == tt_move && depth >= 8 && td.ply &&
            tt_hit && tt_depth >= depth - 3 && tt_flag != hash_flag_alpha &&
            tt_score > -mate_score && tt_score < mate_score) {
            int singular_beta = tt_score - 2 * depth;
            int singular_depth = (depth - 1) / 2;
            int s = td_negamax_abdada(td, singular_beta - 1, singular_beta, singular_depth, is_cut_node, tt_move);
            if (s < singular_beta) extension = 1;
        }

        UndoInfo undo;
        td.ply++;
        td.repetition_table[++td.repetition_index] = td.hash_key;

        if (!td_make_move(td, move, undo)) {
            td.ply--;
            td.repetition_index--;
            continue;
        }

        legal_moves++;
        if (is_quiet) quiets_searched++;

        // Does this move give check? After make_move the side to move is the
        // opponent, so their king being attacked means our move checks. Checking
        // moves must NOT be reduced/pruned, otherwise forced mates and tactics
        // are found far too late (Stockfish keeps checks at full depth).
        int stm_king_sq = get_ls1b_index((td.side == white) ? td.bitboards[K] : td.bitboards[k]);
        bool gives_check = td_is_square_attacked(td, stm_king_sq, td.side ^ 1);

        // Record the move that leads to the child node (for counter-move).
        td.move_stack[td.ply] = move;

        // *** ABDADA: Check if child node is busy ***
        // Only for non-first moves at sufficient depth
        if (moves_searched > 0 && depth >= ABDADA_THRESHOLD && num_threads > 1) {
            if (is_node_busy(td.hash_key)) {
                // Defer this move - another thread is working on it
                td_unmake_move(td, move, undo);
                td.ply--;
                td.repetition_index--;
                deferred[deferred_count++] = move;
                continue;
            }
        }

        // Mark node as busy before searching
        if (depth >= ABDADA_THRESHOLD && num_threads > 1) {
            mark_busy(td.hash_key, depth);
        }

        // PVS + LMR
        if (moves_searched == 0) {
            score = -td_negamax_abdada(td, -beta, -alpha, depth - 1 + extension, false);
        }
        else {
            int reduction = 0;

            if (depth >= 3 && moves_searched >= 3 && is_quiet && !in_check && !gives_check) {
                int d_idx = (depth < 63) ? depth : 63;
                int m_idx = (moves_searched < 63) ? moves_searched : 63;
                reduction = lmr_table[d_idx][m_idx];   // assegna alla variabile ESTERNA (niente 'int' -> niente shadowing che disattivava la LMR)

                if (!pv_node) reduction++;
                if (move == td.killer_moves[0][td.ply] || move == td.killer_moves[1][td.ply])
                    reduction--;
                if (static_eval + 100 < alpha) reduction++;
                if (is_cut_node) reduction++;

                // History-based reduction: search good-history quiets less and
                // bad-history quiets more. Clamped to +/-1 ply to stay
                // conservative; the divisor is the knob to tune in matches.
                int hist_r = td.history_moves[get_move_piece(move)][get_move_target(move)] / 3500;
                if (hist_r > 1) hist_r = 1;
                else if (hist_r < -1) hist_r = -1;
                reduction -= hist_r;

                // --- LMR guidata dalla policy (RANK-BASED), TEST #2 ---
                // Le mosse quiet nel top-3 della policy vengono ridotte di meno
                // (la rete le ritiene candidate forti); quelle nel quartile
                // inferiore vengono ridotte di piu'. Soglie per-rango, robuste alla
                // scala dei logit. current_policy_scores e' valido solo dove la
                // policy e' stata interrogata (oggi: radice, ply 0).
                if (policy_used) {
                    const bool wtm = (td.side == white);
                    int src = get_move_source(move), tgt = get_move_target(move);
                    int p_idx = (wtm ? (src ^ 56) : src) * 64 + (wtm ? (tgt ^ 56) : tgt);
                    float p_score = current_policy_scores[p_idx];
                    if (p_score >= policy_hi_cut) reduction--;
                    else if (p_score <= policy_lo_cut) reduction++;
                }

                if (reduction < 0) reduction = 0;
                if (reduction > depth - 2) reduction = depth - 2;
            }

            score = -td_negamax_abdada(td, -alpha - 1, -alpha, depth - 1 - reduction, true);

            if (score > alpha && reduction > 0) {
                score = -td_negamax_abdada(td, -alpha - 1, -alpha, depth - 1, !is_cut_node);
            }

            if (score > alpha && score < beta) {
                score = -td_negamax_abdada(td, -beta, -alpha, depth - 1, false);
            }
        }

        // Unmark busy
        if (depth >= ABDADA_THRESHOLD && num_threads > 1) {
            unmark_busy(td.hash_key);
        }

        td_unmake_move(td, move, undo);
        td.ply--;
        td.repetition_index--;

        if (stop_threads.load(std::memory_order_relaxed)) return 0;

        moves_searched++;
        if (is_quiet && n_searched_quiets < 64)
            searched_quiets[n_searched_quiets++] = move;
        else if (is_capture && n_searched_captures < 64)
            searched_captures[n_searched_captures++] = move;

        if (score > best_score) {
            best_score = score;
            best_move = move;

            if (score > alpha) {
                alpha = score;
                hash_flag = hash_flag_exact;

                td.pv_table[td.ply][td.ply] = move;
                for (int i = td.ply + 1; i < td.pv_length[td.ply + 1]; i++)
                    td.pv_table[td.ply][i] = td.pv_table[td.ply + 1][i];
                td.pv_length[td.ply] = td.pv_length[td.ply + 1];

                if (score >= beta) {
                    if (!excluded_move) store_tt(td.hash_key, move, best_score, depth, hash_flag_beta, td.ply);

                    if (is_quiet) {
                        // Evita di clonare la stessa mossa in entrambi gli slot
                        // killer (dimezzerebbe le chance di cutoff).
                        if (move != td.killer_moves[0][td.ply]) {
                            td.killer_moves[1][td.ply] = td.killer_moves[0][td.ply];
                            td.killer_moves[0][td.ply] = move;
                        }
                        int prev_cm = td.move_stack[td.ply];
                        int pcp = prev_cm ? get_move_piece(prev_cm) : 0;
                        int pct = prev_cm ? get_move_target(prev_cm) : 0;
                        if (prev_cm)
                            td.counter_moves[pcp][pct] = move;
                        int bonus = depth * depth;
                        td_update_history(td.history_moves[get_move_piece(move)][get_move_target(move)], bonus);
                        if (prev_cm)
                            td_update_history(td.continuation_history[pcp][pct][get_move_piece(move)][get_move_target(move)], bonus);
                        // malus: quiet moves tried before the cutoff get penalized
                        for (int q = 0; q < n_searched_quiets; q++) {
                            int qm = searched_quiets[q];
                            if (qm == move) continue;
                            td_update_history(td.history_moves[get_move_piece(qm)][get_move_target(qm)], -bonus);
                            if (prev_cm)
                                td_update_history(td.continuation_history[pcp][pct][get_move_piece(qm)][get_move_target(qm)], -bonus);
                        }
                    }
                    else if (is_capture) {
                        // Capture history: bonus alla cattura del cutoff, malus
                        // alle catture provate prima senza riuscirci.
                        int bonus = depth * depth;
                        int vic = td_captured_piece(td, get_move_target(move));
                        td_update_history(td.capture_history[get_move_piece(move)][get_move_target(move)][vic], bonus);
                        for (int c = 0; c < n_searched_captures; c++) {
                            int cm = searched_captures[c];
                            if (cm == move) continue;
                            int cvic = td_captured_piece(td, get_move_target(cm));
                            td_update_history(td.capture_history[get_move_piece(cm)][get_move_target(cm)][cvic], -bonus);
                        }
                    }

                    return best_score;
                }
            }
        }
    }

    // *** ABDADA: Second pass - search deferred moves ***
    for (int i = 0; i < deferred_count; i++) {
        if (stop_threads.load(std::memory_order_relaxed)) break;

        int move = deferred[i];
        bool is_capture = get_move_capture(move);
        bool is_promotion = get_move_promoted(move);
        bool is_quiet = !is_capture && !is_promotion;

        UndoInfo undo;
        td.ply++;
        td.repetition_table[++td.repetition_index] = td.hash_key;

        if (!td_make_move(td, move, undo)) {
            td.ply--;
            td.repetition_index--;
            continue;
        }

        // Record the move that leads to the child node (for counter-move).
        td.move_stack[td.ply] = move;

        // Mark busy
        mark_busy(td.hash_key, depth);

        // Search with null window first
        score = -td_negamax_abdada(td, -alpha - 1, -alpha, depth - 1, true);

        if (score > alpha && score < beta) {
            score = -td_negamax_abdada(td, -beta, -alpha, depth - 1, false);
        }

        unmark_busy(td.hash_key);

        td_unmake_move(td, move, undo);
        td.ply--;
        td.repetition_index--;

        if (stop_threads.load(std::memory_order_relaxed)) return 0;

        moves_searched++;
        if (is_quiet && n_searched_quiets < 64)
            searched_quiets[n_searched_quiets++] = move;
        else if (is_capture && n_searched_captures < 64)
            searched_captures[n_searched_captures++] = move;

        if (score > best_score) {
            best_score = score;
            best_move = move;

            if (score > alpha) {
                alpha = score;
                hash_flag = hash_flag_exact;

                td.pv_table[td.ply][td.ply] = move;
                for (int i = td.ply + 1; i < td.pv_length[td.ply + 1]; i++)
                    td.pv_table[td.ply][i] = td.pv_table[td.ply + 1][i];
                td.pv_length[td.ply] = td.pv_length[td.ply + 1];

                if (score >= beta) {
                    if (!excluded_move) store_tt(td.hash_key, move, best_score, depth, hash_flag_beta, td.ply);

                    if (is_quiet) {
                        // Evita di clonare la stessa mossa in entrambi gli slot
                        // killer (dimezzerebbe le chance di cutoff).
                        if (move != td.killer_moves[0][td.ply]) {
                            td.killer_moves[1][td.ply] = td.killer_moves[0][td.ply];
                            td.killer_moves[0][td.ply] = move;
                        }
                        int prev_cm = td.move_stack[td.ply];
                        int pcp = prev_cm ? get_move_piece(prev_cm) : 0;
                        int pct = prev_cm ? get_move_target(prev_cm) : 0;
                        if (prev_cm)
                            td.counter_moves[pcp][pct] = move;
                        int bonus = depth * depth;
                        td_update_history(td.history_moves[get_move_piece(move)][get_move_target(move)], bonus);
                        if (prev_cm)
                            td_update_history(td.continuation_history[pcp][pct][get_move_piece(move)][get_move_target(move)], bonus);
                        // malus: quiet moves tried before the cutoff get penalized
                        for (int q = 0; q < n_searched_quiets; q++) {
                            int qm = searched_quiets[q];
                            if (qm == move) continue;
                            td_update_history(td.history_moves[get_move_piece(qm)][get_move_target(qm)], -bonus);
                            if (prev_cm)
                                td_update_history(td.continuation_history[pcp][pct][get_move_piece(qm)][get_move_target(qm)], -bonus);
                        }
                    }
                    else if (is_capture) {
                        // Capture history: bonus alla cattura del cutoff, malus
                        // alle catture provate prima senza riuscirci.
                        int bonus = depth * depth;
                        int vic = td_captured_piece(td, get_move_target(move));
                        td_update_history(td.capture_history[get_move_piece(move)][get_move_target(move)][vic], bonus);
                        for (int c = 0; c < n_searched_captures; c++) {
                            int cm = searched_captures[c];
                            if (cm == move) continue;
                            int cvic = td_captured_piece(td, get_move_target(cm));
                            td_update_history(td.capture_history[get_move_piece(cm)][get_move_target(cm)][cvic], -bonus);
                        }
                    }

                    return best_score;
                }
            }
        }
    }

    // Checkmate / Stalemate
    if (legal_moves == 0) {
        if (in_check) return -mate_value + td.ply;
        return 0;
    }

    if (!excluded_move) store_tt(td.hash_key, best_move, best_score, depth, hash_flag, td.ply);

    return best_score;
}

// ============================================================================
// UCI INFO
// ============================================================================

static void print_search_info(ThreadData& td, int depth, int score) {
    int elapsed = get_time_ms() - search_start_time;
    if (elapsed == 0) elapsed = 1;

    U64 total = 0;
    for (int i = 0; i < num_threads; i++) {
        total += thread_data[i].nodes;
    }

    U64 nps = (total * 1000) / elapsed;

    std::lock_guard<std::mutex> lock(output_mutex);

    if (score > -mate_value && score < -mate_score) {
        printf("info depth %d score mate %d nodes %llu nps %llu time %d pv ",
            depth, -(score + mate_value) / 2 - 1, total, nps, elapsed);
    }
    else if (score > mate_score && score < mate_value) {
        printf("info depth %d score mate %d nodes %llu nps %llu time %d pv ",
            depth, (mate_value - score) / 2 + 1, total, nps, elapsed);
    }
    else {
        printf("info depth %d score cp %d nodes %llu nps %llu time %d pv ",
            depth, score, total, nps, elapsed);
    }

    for (int i = 0; i < td.pv_length[0]; i++) {
        print_move(td.pv_table[0][i]);
        printf(" ");
    }
    printf("\n");
    fflush(stdout);
}

// ============================================================================
// ITERATIVE DEEPENING
// ============================================================================

static void thread_search(int thread_id, int max_depth) {
    ThreadData& td = thread_data[thread_id];

    copy_board_to_thread(td);

    memset(td.killer_moves, 0, sizeof(td.killer_moves));
    // RIMUOVE L'AMNESIA: la history persiste tra le mosse della partita, cosi'
    // l'ordinamento e' intelligente fin dal primo ms (la gravity in
    // td_update_history evita la saturazione).
    // memset(td.history_moves, 0, sizeof(td.history_moves));
    memset(td.counter_moves, 0, sizeof(td.counter_moves));
    // memset(td.continuation_history, 0, sizeof(td.continuation_history));
    memset(td.pv_table, 0, sizeof(td.pv_table));
    memset(td.pv_length, 0, sizeof(td.pv_length));
    td.move_stack[0] = 0;  // root has no previous move

    int prev_score = 0;
    int prev_best_move = 0;

    // Stagger starting depths for thread diversity
    int start_depth = 1 + (thread_id % 2);

    for (int current_depth = start_depth; current_depth <= max_depth; current_depth++) {
        if (stop_threads.load(std::memory_order_relaxed)) break;

        // Re-sync the incremental NNUE mirror to the root board (full refresh)
        // so a previously aborted iteration cannot leave the accumulator stack
        // out of step with the search.
        sf_root_sync(td);

        // Aspiration window: start narrow around the previous score and widen
        // incrementally on failures, instead of re-searching the full window.
        int alpha = -infinity, beta = infinity, delta = 25;
        if (current_depth >= 4) {
            alpha = (prev_score - delta > -infinity) ? prev_score - delta : -infinity;
            beta  = (prev_score + delta <  infinity) ? prev_score + delta :  infinity;
        }

        int score;
        while (true) {
            score = td_negamax_abdada(td, alpha, beta, current_depth, false);
            if (stop_threads.load(std::memory_order_relaxed)) break;

            if (score <= alpha) {            // fail low: lower alpha, pull beta toward midpoint
                beta = (alpha + beta) / 2;
                alpha = (score - delta > -infinity) ? score - delta : -infinity;
                delta += delta;
            }
            else if (score >= beta) {        // fail high: raise beta
                beta = (score + delta < infinity) ? score + delta : infinity;
                delta += delta;
            }
            else {
                break;                       // score is inside the window
            }
        }

        if (stop_threads.load(std::memory_order_relaxed)) break;

        prev_score = score;

        if (td.pv_length[0] > 0) {
            td.best_move = td.pv_table[0][0];
            td.best_score = score;
            td.depth = current_depth;

            // Only main thread prints
            if (thread_id == 0) {
                print_search_info(td, current_depth, score);
            }
        }

        // Soft time management: the main thread decides whether to start another
        // iteration. If the best move is unstable (just changed), allow more
        // time, up to the hard cap (stoptime).
        if (thread_id == 0 && timeset) {
            int soft = soft_time_limit;
            if (current_depth > start_depth && td.best_move != prev_best_move)
                soft += (stoptime - soft_time_limit) / 2;
            prev_best_move = td.best_move;
            if (get_time_ms() >= soft) {
                stop_threads.store(true, std::memory_order_relaxed);
                break;
            }
        }
    }
}

// ============================================================================
// THREAD MANAGEMENT
// ============================================================================

void stop_search_threads() {
    stop_threads.store(true, std::memory_order_relaxed);
}

void wait_for_threads() {
    for (auto& t : search_threads) {
        if (t.joinable()) t.join();
    }
    search_threads.clear();
}

// ============================================================================
// ASYNCHRONOUS SEARCH DRIVER
//
// The UCI "go" must not block the input loop, otherwise "stop" and
// "go infinite" cannot work. So search_position_mt() runs on its own master
// thread (which in turn spawns the helper threads), while the UCI loop keeps
// reading stdin.
// ============================================================================

std::thread search_master;

// Join the master search thread if it is running/finished. Callers that may
// invoke this while an (possibly infinite) search is in progress must first
// call stop_search_threads() so the search actually terminates.
void wait_for_search_done() {
    if (search_master.joinable())
        search_master.join();
}

// Stop any search in progress, wait for it to fully finish, then start a new
// search on a background thread.
void launch_search(int depth) {
    stop_search_threads();          // stop a previous search (if any)
    wait_for_search_done();         // join it and its helpers

    // Syzygy ROOT probe (DTZ): if the GLOBAL (root) position is covered by the
    // installed tablebases, play the perfect move immediately instead of
    // searching. Done HERE, on the main UCI thread and BEFORE spawning the
    // worker, so it cannot race with parse_position() of the next command.
    // Needs .rtbz (DTZ) files; with WDL-only tables it simply fails and we fall
    // through to a normal search (whose in-search WDL probe already fixes the
    // evaluation of covered endgames).
    {
        int tb_move = 0, tb_score = 0;
        if (syzygy_probe_root(tb_move, tb_score)) {
            new_search();
            printf("info depth 1 score cp %d pv ", tb_score);
            print_move(tb_move);
            printf("\nbestmove ");
            print_move(tb_move);
            printf("\n");
            fflush(stdout);
            return;
        }
    }

    stop_threads.store(false, std::memory_order_relaxed);  // clear before spawning
    search_master = std::thread(search_position_mt, depth);
}

// ============================================================================
// MAIN SEARCH ENTRY
// ============================================================================

void search_position_mt(int depth) {
    search_start_time = get_time_ms();

    // Increment TT age
    new_search();

    // NOTE: stop_threads is cleared by launch_search() BEFORE this thread is
    // spawned, so that a "stop" arriving right after "go" is not lost here.
    // NOTE: the Syzygy ROOT probe is NOT done here. It reads the GLOBAL board and
    // must run on the main (UCI) thread BEFORE this worker is spawned - otherwise
    // it races with parse_position() of the next command. See launch_search().
    stopped = 0;

    for (int i = 0; i < num_threads; i++) {
        thread_data[i].nodes = 0;
        thread_data[i].best_move = 0;
        thread_data[i].best_score = -infinity;
        thread_data[i].depth = 0;
    }

    search_threads.clear();

    // Start helper threads
    for (int i = 1; i < num_threads; i++) {
        search_threads.emplace_back(thread_search, i, depth);
    }

    // Main thread search
    thread_search(0, depth);

    stop_threads.store(true, std::memory_order_relaxed);
    wait_for_threads();

    // Get best result from all threads
    int best_move = thread_data[0].best_move;
    int best_score = thread_data[0].best_score;
    int best_depth = thread_data[0].depth;

    for (int i = 1; i < num_threads; i++) {
        if (thread_data[i].depth > best_depth ||
            (thread_data[i].depth == best_depth && thread_data[i].best_score > best_score)) {
            if (thread_data[i].best_move != 0) {
                best_move = thread_data[i].best_move;
                best_score = thread_data[i].best_score;
                best_depth = thread_data[i].depth;
            }
        }
    }

    // Self-play data logging (Phase 1): record the searched root position and
    // the chosen move before emitting it. Uses the global board (= root).
    if (g_data_log_enabled && best_move) {
        log_search_record(best_move, best_score, best_depth);
    }

    printf("bestmove ");
    if (best_move) {
        print_move(best_move);
    }
    else if (thread_data[0].pv_table[0][0]) {
        print_move(thread_data[0].pv_table[0][0]);
    }
    else {
        printf("(none)");
    }
    printf("\n");
    fflush(stdout);
}