/*
 * TRIUMVIRATUS - Transposition Table with ABDADA Support
 *
 * ABDADA = Alpha-Beta Distributed Avoiding Duplicate Analysis
 *
 * Key concept: When a thread starts searching a node, it marks it as "busy".
 * Other threads seeing this node skip it (for non-first moves) to avoid
 * duplicating work.
 */

#ifndef TT_ABDADA_H
#define TT_ABDADA_H

#include "defs.h"
#include <atomic>

 // Hash flags
#define hash_flag_exact 0
#define hash_flag_alpha 1
#define hash_flag_beta 2

// ABDADA busy flag - indicates node is being searched
#define TT_BUSY_DEPTH 255

/*
 * TT Entry structure with ABDADA support
 *
 * Layout (16 bytes for cache alignment):
 * - key: 8 bytes (position hash XOR data for lockless)
 * - data: 8 bytes packed (move, score, depth, flag, busy)
 */
struct alignas(16) tt_entry {
    U64 key;           // hash_key XOR data (for lockless verification)

    // Packed data
    U64 data;          // Contains: move (21 bits) | score (16 bits) | depth (8 bits) | flag (2 bits) | busy (8 bits) | age (8 bits)
};

// Data packing/unpacking
inline U64 pack_tt_data(int move, int score, int depth, int flag, int busy, int age) {
    return ((U64)(move & 0x1FFFFF)) |
        ((U64)((score + 32768) & 0xFFFF) << 21) |
        ((U64)(depth & 0xFF) << 37) |
        ((U64)(flag & 0x3) << 45) |
        ((U64)(busy & 0xFF) << 47) |
        ((U64)(age & 0xFF) << 55);
}

inline int unpack_move(U64 data) { return data & 0x1FFFFF; }
inline int unpack_score(U64 data) { return ((data >> 21) & 0xFFFF) - 32768; }
inline int unpack_depth(U64 data) { return (data >> 37) & 0xFF; }
inline int unpack_flag(U64 data) { return (data >> 45) & 0x3; }
inline int unpack_busy(U64 data) { return (data >> 47) & 0xFF; }
inline int unpack_age(U64 data) { return (data >> 55) & 0xFF; }

// Global TT
extern tt_entry* hash_table;
extern U64 hash_entries;
extern int current_age;

// External variables needed for compatibility functions
extern U64 hash_key;
extern U64 piece_keys[12][64];
extern U64 enpassant_keys[64];
extern U64 castle_keys[16];
extern U64 side_key;

// Constants (define if not already defined)
#ifndef mate_value
#define mate_value 31000
#endif
#ifndef mate_score
#define mate_score 30000
#endif

// Initialize hash table
inline void init_hash_table(int mb) {
    U64 size = (U64)mb * 1024 * 1024;
    hash_entries = size / sizeof(tt_entry);

    // Free old table if exists
    if (hash_table) {
        delete[] hash_table;
    }

    hash_table = new tt_entry[hash_entries]();

    // Clear table
    for (U64 i = 0; i < hash_entries; i++) {
        hash_table[i].key = 0;
        hash_table[i].data = 0;
    }
}

// Clear hash table
inline void clear_hash_table() {
    for (U64 i = 0; i < hash_entries; i++) {
        hash_table[i].key = 0;
        hash_table[i].data = 0;
    }
    current_age = 0;
}

// Increment age (call at start of each search)
inline void new_search() {
    current_age = (current_age + 1) & 0xFF;
}

/*
 * Probe TT with ABDADA support
 *
 * Returns: true if valid entry found
 * Sets: tt_move, tt_score, tt_depth, tt_flag, is_busy
 */
inline bool probe_tt(U64 hash_key, int& tt_move, int& tt_score, int& tt_depth, int& tt_flag, bool& is_busy) {
    U64 index = hash_key % hash_entries;
    tt_entry* entry = &hash_table[index];

    U64 data = entry->data;
    U64 stored_key = entry->key;

    // Lockless verification: stored_key should equal hash_key XOR data
    if ((stored_key ^ data) != hash_key) {
        tt_move = 0;
        tt_score = 0;
        tt_depth = 0;
        tt_flag = hash_flag_alpha;
        is_busy = false;
        return false;
    }

    // Valid entry - unpack
    tt_move = unpack_move(data);
    tt_score = unpack_score(data);
    tt_depth = unpack_depth(data);
    tt_flag = unpack_flag(data);
    is_busy = (unpack_busy(data) > 0);

    return true;
}

/*
 * Quick check if node is busy (for ABDADA skip decision)
 */
inline bool is_node_busy(U64 hash_key) {
    U64 index = hash_key % hash_entries;
    tt_entry* entry = &hash_table[index];

    U64 data = entry->data;
    U64 stored_key = entry->key;

    // Verify it's the same position
    if ((stored_key ^ data) != hash_key) {
        return false;
    }

    return (unpack_busy(data) > 0);
}

/*
 * Mark node as busy (called when starting to search a node)
 * Only marks if entry doesn't exist or is from different position
 */
inline void mark_busy(U64 hash_key, int depth) {
    U64 index = hash_key % hash_entries;
    tt_entry* entry = &hash_table[index];

    U64 old_data = entry->data;
    U64 stored_key = entry->key;

    // If same position exists, increment busy counter
    if ((stored_key ^ old_data) == hash_key) {
        int old_busy = unpack_busy(old_data);
        if (old_busy < 255) {
            U64 new_data = pack_tt_data(
                unpack_move(old_data),
                unpack_score(old_data),
                unpack_depth(old_data),
                unpack_flag(old_data),
                old_busy + 1,
                unpack_age(old_data)
            );
            entry->data = new_data;
            entry->key = hash_key ^ new_data;
        }
    }
    else {
        // New position - mark as busy with no real data yet
        U64 new_data = pack_tt_data(0, 0, 0, hash_flag_alpha, 1, current_age);
        entry->data = new_data;
        entry->key = hash_key ^ new_data;
    }
}

/*
 * Unmark node as busy (called when done searching a node)
 */
inline void unmark_busy(U64 hash_key) {
    U64 index = hash_key % hash_entries;
    tt_entry* entry = &hash_table[index];

    U64 old_data = entry->data;
    U64 stored_key = entry->key;

    // Verify same position
    if ((stored_key ^ old_data) == hash_key) {
        int old_busy = unpack_busy(old_data);
        if (old_busy > 0) {
            U64 new_data = pack_tt_data(
                unpack_move(old_data),
                unpack_score(old_data),
                unpack_depth(old_data),
                unpack_flag(old_data),
                old_busy - 1,
                unpack_age(old_data)
            );
            entry->data = new_data;
            entry->key = hash_key ^ new_data;
        }
    }
}

/*
 * Store result in TT (and clear busy flag for this thread)
 *
 * Replacement scheme:
 * 1. Always replace if same position
 * 2. Replace if new depth >= old depth
 * 3. Replace if old entry is from different age
 */
inline void store_tt(U64 hash_key, int move, int score, int depth, int flag, int ply = 0) {
    // Normalize mate scores to be relative to THIS node before storing
    // (value_to_tt). The probe side performs the inverse adjustment. Without
    // this, a "mate in N from the root" would be cached as if it were "mate in
    // N from here", giving wrong mate distances on TT hits.
    if (score > mate_score) score += ply;
    else if (score < -mate_score) score -= ply;

    U64 index = hash_key % hash_entries;
    tt_entry* entry = &hash_table[index];

    U64 old_data = entry->data;
    U64 stored_key = entry->key;

    int old_busy = 0;
    bool same_position = ((stored_key ^ old_data) == hash_key);

    // Decide whether to replace
    bool should_replace = true;

    if (same_position) {
        int old_depth = unpack_depth(old_data);
        int old_age = unpack_age(old_data);
        old_busy = unpack_busy(old_data);

        // Decrement busy counter
        if (old_busy > 0) old_busy--;

        // Don't replace deeper entries from same age unless exact score
        if (old_age == current_age && old_depth > depth && flag != hash_flag_exact) {
            // Just update busy counter
            U64 new_data = pack_tt_data(
                unpack_move(old_data),
                unpack_score(old_data),
                old_depth,
                unpack_flag(old_data),
                old_busy,
                old_age
            );
            entry->data = new_data;
            entry->key = hash_key ^ new_data;
            return;
        }
    }

    // Store new entry
    U64 new_data = pack_tt_data(move, score, depth, flag, old_busy, current_age);
    entry->data = new_data;
    entry->key = hash_key ^ new_data;
}

/*
 * Get TT move only (for move ordering)
 */
inline int get_tt_move(U64 hash_key) {
    U64 index = hash_key % hash_entries;
    tt_entry* entry = &hash_table[index];

    U64 data = entry->data;
    U64 stored_key = entry->key;

    if ((stored_key ^ data) == hash_key) {
        return unpack_move(data);
    }
    return 0;
}

// ============================================================================
// COMPATIBILITY LAYER - Old API functions
// ============================================================================

#define no_hash_entry 100000

// Need ply as extern for compatibility
extern int ply;

/*
 * read_hash_entry - Compatible with old API (4 arguments)
 * Returns score if valid entry found, no_hash_entry otherwise
 */
inline int read_hash_entry(int alpha, int beta, int* best_move, int depth) {
    int tt_move, tt_score, tt_depth, tt_flag;
    bool tt_busy;

    if (!probe_tt(hash_key, tt_move, tt_score, tt_depth, tt_flag, tt_busy)) {
        return no_hash_entry;
    }

    *best_move = tt_move;

    if (tt_depth >= depth) {
        // Adjust mate scores using global ply
        if (tt_score < -mate_score) tt_score += ply;
        if (tt_score > mate_score) tt_score -= ply;

        if (tt_flag == hash_flag_exact) return tt_score;
        if (tt_flag == hash_flag_alpha && tt_score <= alpha) return alpha;
        if (tt_flag == hash_flag_beta && tt_score >= beta) return beta;
    }

    return no_hash_entry;
}

/*
 * write_hash_entry - Compatible with old API
 */
inline void write_hash_entry(int score, int best_move, int depth, int flag) {
    store_tt(hash_key, best_move, score, depth, flag);
}

#endif // TT_ABDADA_H