/*
 * Static Exchange Evaluation (SEE)
 * Determines if a capture wins or loses material
 */

#include "see.h"
#include "defs.h"
#include "movegen.h"
#include "magic.h"
#include "attacks.h"

// Get least valuable attacker
static inline int get_lva(U64 bb[12], U64 occ[3], int square, int side, int* from_sq) {
    U64 attackers;
    
    // Pawns first (least valuable)
    if (side == white) {
        attackers = pawn_attacks[black][square] & bb[P];
    } else {
        attackers = pawn_attacks[white][square] & bb[p];
    }
    if (attackers) {
        *from_sq = get_ls1b_index(attackers);
        return (side == white) ? P : p;
    }
    
    // Knights
    int knight = (side == white) ? N : n;
    attackers = knight_attacks[square] & bb[knight];
    if (attackers) {
        *from_sq = get_ls1b_index(attackers);
        return knight;
    }
    
    // Bishops
    int bishop = (side == white) ? B : b;
    attackers = get_bishop_attacks(square, occ[both]) & bb[bishop];
    if (attackers) {
        *from_sq = get_ls1b_index(attackers);
        return bishop;
    }
    
    // Rooks
    int rook = (side == white) ? R : r;
    attackers = get_rook_attacks(square, occ[both]) & bb[rook];
    if (attackers) {
        *from_sq = get_ls1b_index(attackers);
        return rook;
    }
    
    // Queens
    int queen = (side == white) ? Q : q;
    attackers = get_queen_attacks(square, occ[both]) & bb[queen];
    if (attackers) {
        *from_sq = get_ls1b_index(attackers);
        return queen;
    }
    
    // King (last resort)
    int king = (side == white) ? K : k;
    attackers = king_attacks[square] & bb[king];
    if (attackers) {
        *from_sq = get_ls1b_index(attackers);
        return king;
    }
    
    return -1;
}

// SEE implementation
int td_see(ThreadData& td, int move) {
    int from = get_move_source(move);
    int to = get_move_target(move);
    int piece = get_move_piece(move);
    int captured = -1;
    
    // Find captured piece
    int start = (td.side == white) ? p : P;
    int end = (td.side == white) ? k : K;
    
    for (int pc = start; pc <= end; pc++) {
        if (get_bit(td.bitboards[pc], to)) {
            captured = pc;
            break;
        }
    }
    
    // En passant
    if (get_move_enpassant(move)) {
        captured = (td.side == white) ? p : P;
    }
    
    // Not a capture
    if (captured == -1) return 0;
    
    // Local copy for simulation
    U64 bb[12], occ[3];
    memcpy(bb, td.bitboards, sizeof(td.bitboards));
    memcpy(occ, td.occupancies, sizeof(td.occupancies));
    
    int gain[32];
    int depth = 0;
    int current_side = td.side;
    
    // Initial gain
    gain[0] = see_piece_values[captured];
    
    // Remove attacker from source
    pop_bit(bb[piece], from);
    pop_bit(occ[current_side], from);
    pop_bit(occ[both], from);
    
    // Remove captured from target
    pop_bit(bb[captured], to);
    pop_bit(occ[current_side ^ 1], to);
    
    // Attacker now on target
    set_bit(bb[piece], to);
    set_bit(occ[current_side], to);
    set_bit(occ[both], to);
    
    int attacker_value = see_piece_values[piece];
    current_side ^= 1;
    
    // Simulate exchanges
    while (depth < 31) {
        depth++;
        
        int from_sq;
        int attacker = get_lva(bb, occ, to, current_side, &from_sq);
        
        if (attacker == -1) break;
        
        gain[depth] = attacker_value - gain[depth - 1];
        
        // Stand-pat pruning
        if (gain[depth] < 0 && gain[depth - 1] < 0) break;
        
        attacker_value = see_piece_values[attacker];
        
        // Remove this attacker
        pop_bit(bb[attacker], from_sq);
        pop_bit(occ[current_side], from_sq);
        pop_bit(occ[both], from_sq);
        
        // Remove previous piece from target
        for (int pc = P; pc <= k; pc++) {
            if (get_bit(bb[pc], to)) {
                pop_bit(bb[pc], to);
                break;
            }
        }
        
        // Add attacker to target
        set_bit(bb[attacker], to);
        set_bit(occ[current_side], to);
        set_bit(occ[both], to);
        
        current_side ^= 1;
    }
    
    // Negamax the gain stack
    while (--depth > 0) {
        gain[depth - 1] = -((-gain[depth - 1] > gain[depth]) ? -gain[depth - 1] : gain[depth]);
    }
    
    return gain[0];
}
