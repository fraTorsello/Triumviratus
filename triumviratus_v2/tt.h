#pragma once
#ifndef  TT_H
#define TT_H
#include "defs.h"
/**********************************\
 ==================================

        Transposition table

 ==================================
\**********************************/

// number hash table entries
extern int hash_entries;

// no hash entry found constant
#define no_hash_entry 100000

// transposition table hash flags
#define hash_flag_exact 0
#define hash_flag_alpha 1
#define hash_flag_beta 2

// transposition table data structure
typedef struct {
    U64 hash_key;   // "almost" unique chess position identifier
    int depth;      // current search depth
    int flag;       // flag the type of node (fail-low/fail-high/PV) 
    int score;      // score (alpha/beta/PV)
    int best_move;
} tt;               // transposition table (TT aka hash table)

// define TT instance
extern tt* hash_table;

// PROTOTYPES
extern void init_hash_table(int mb);
extern int read_hash_entry(int alpha, int beta, int* best_move, int depth);
extern void write_hash_entry(int score, int best_move, int depth, int hash_flag);
extern void clear_hash_table();

#endif // ! TT_H
