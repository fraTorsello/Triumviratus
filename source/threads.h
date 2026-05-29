#pragma once
#ifndef THREADS_H
#define THREADS_H

#include "defs.h"
#include "search.h"
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>

#define MAX_THREADS 64

// Thread-local data structure
struct ThreadData {
    int thread_id;
    
    // Board state copy
    U64 bitboards[12];
    U64 occupancies[3];
    int side;
    int enpassant;
    int castle;
    U64 hash_key;
    int fifty;
    
    // Repetition detection
    U64 repetition_table[1000];
    int repetition_index;
    
    // Search state
    int ply;
    U64 nodes;

    // Move that led to each ply (0 = root / null move). Used by the
    // counter-move heuristic to know the "previous move" at a node.
    int move_stack[max_ply + 8];

    // Move ordering. +8 slack on the ply dimension: a node entering at
    // ply == max_ply-1 increments td.ply (and reads pv_length[ply+1]), so the
    // ply index can transiently reach max_ply. Without the slack that is an OOB
    // write that aliases pv_table[0][0] -> wild loop bound -> access violation.
    int killer_moves[2][max_ply + 8];
    int history_moves[12][64];
    // Counter-move heuristic: counter_moves[prev_piece][prev_to] = the quiet
    // reply that most recently caused a beta cutoff after that previous move.
    int counter_moves[12][64];
    // Continuation history: [prev_piece][prev_to][piece][to]. A 2-ply history
    // that scores a quiet move in the context of the move that led to this
    // node. int16_t keeps it ~1.1 MB/thread (values are bounded to +/-HISTORY_MAX).
    int16_t continuation_history[12][64][12][64];
    // Capture history: [moving piece][to square][captured piece].
    int capture_history[12][64][12];

    // PV table. +8 slack on the ply dimension for the same reason as
    // killer_moves above (pv_length[ply+1] / pv_table[ply+1] lookahead at the
    // deepest searched node).
    int pv_length[max_ply + 8];
    int pv_table[max_ply + 8][max_ply + 8];
    
    // Results
    int best_move;
    int best_score;
    int depth;

    // Opaque per-thread handle for the incremental NNUE mirror (see sf_bridge).
    // Owned here: created in init_threads, destroyed on re-init / shutdown.
    void* sfpos = nullptr;
};

// Global thread management
extern std::vector<std::thread> search_threads;
extern std::vector<ThreadData> thread_data;
extern std::atomic<bool> stop_threads;
extern std::atomic<U64> total_nodes;
extern std::mutex cout_mutex;
extern int num_threads;

// Thread management functions
extern void init_threads(int thread_count);
extern void copy_board_to_thread(ThreadData& td);
extern void start_search_threads(int depth);
extern void stop_search_threads();
extern void wait_for_threads();

// Multi-threaded search
extern void search_position_mt(int depth);

// Asynchronous driver: run the search on a background thread so the UCI loop
// stays responsive to "stop" / "go infinite".
extern std::thread search_master;
extern void launch_search(int depth);
extern void wait_for_search_done();

// Soft time limit (absolute ms, like stoptime). The main thread stops starting
// new iterative-deepening iterations once it passes this; stoptime stays the
// hard cap checked inside the search. Set by parse_go alongside stoptime.
extern int soft_time_limit;

// Self-play data logging (Phase 1: policy-net training data). When enabled, the
// engine appends "FEN<TAB>bestmove<TAB>score<TAB>depth" for each completed root
// search to the dataset file. Controlled via the DataLog / DataFile UCI options.
extern void set_data_log_enabled(bool enabled);
extern void set_data_log_file(const char* path);

// Policy on/off (UCI option "UsePolicy") — A/B the hybrid policy vs pure NNUE.
extern void set_use_policy(bool enabled);

#endif
