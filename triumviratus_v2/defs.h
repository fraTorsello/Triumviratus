#pragma once
#ifndef DEFS_H
#define DEFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <unordered_map>

#include "nnue_eval.h"

// define version
#define VERSION " - 2.0 NNUE"
#define AUTHOR "Francesco Torsello"
#define NAME "Triumviratus"

#define U64 unsigned long long

// FEN dedug positions
#define empty_board "8/8/8/8/8/8/8/8 b - - "
#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define tricky_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 "
#define killer_position "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define cmk_position "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NPP/PPP1NPB1/R1BQ1RK1 b - - 0 9 "
#define repetitions "2r3k1/R7/8/1R6/8/8/P4KPP/8 w - - 0 40 "

enum {
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1, no_sq
};


enum { P, N, B, R, Q, K, p, n, b, r, q, k };
enum { white, black, both };
enum { rook, bishop };


enum { wk = 1, wq = 2, bk = 4, bq = 8 };

// convert squares to coordinates
extern const char* square_to_coordinates[];
extern char ascii_pieces[13];

// unicode pieces
extern const char* unicode_pieces[12];

// convert ASCII character pieces to encoded constants
// Function to map characters to enum constants
// Function to map a character to a chess piece
extern int mapCharToPiece(char c);

// Function to map a chess piece to its promotion character
extern char mapPieceToPromotion(int piece);




/**********************************\
 ==================================

            Chess board

 ==================================
\**********************************/


extern U64 bitboards[12];
extern U64 occupancies[3];
extern int side;
extern int enpassant;
extern int castle;
extern U64 hash_key;
extern U64 repetition_table[1000];
extern int repetition_index;
extern int ply;
extern int fifty;

/**********************************\
 ==================================

       Time controls variables

 ==================================
\**********************************/

extern int quit;
extern int movestogo;
extern int movetime;
extern int time_uci;
extern int inc;
extern int starttime;
extern int stoptime;
extern int timeset;
extern int stopped;

/**********************************\
 ==================================

          Bit manipulations

 ==================================
\**********************************/

// set/get/pop bit macros
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))
extern inline int count_bits(U64 bitboard);
extern inline int get_ls1b_index(U64 bitboard);



/**********************************\
 ==================================

               Zobrist

 ==================================
\**********************************/

extern U64 piece_keys[12][64];
extern U64 enpassant_keys[64];
extern U64 castle_keys[16];
extern U64 side_key;


extern void init_random_keys();
extern U64 generate_hash_key();


/**********************************\
 ==================================

           Move generator

 ==================================
\**********************************/

// move list structure
typedef struct {
    // moves
    int moves[256];

    // move count
    int count;
} moves;


/**********************************\
 ==================================

              Init all

 ==================================
\**********************************/

extern void init_bitboards();



//SORT
extern void quicksort(std::vector<int>& move_scores, int* moves, int low, int high);


typedef struct
{
	int age;
	U64 smp_data;
	U64 smp_key;
} s_hashentry;

typedef struct
{
	s_hashentry* p_table;
	int num_entries;
	int new_write;
	int over_write;
	int hit;
	int cut;
	int current_age;
} s_hashtable;

typedef struct
{
	int move;
	int castle_perm;
	int en_pas;
	int fifty_move;
	U64 pos_key;
} s_undo;

typedef struct
{

	U64 bitboards[12];
	U64 occupancies[3];
	int side;
	int enpassant;
	int castle;
	U64 hash_key;
	U64 repetition_table[1000];
	int repetition_index;
	int ply;
	int fifty;

	int enPas;
	int fifty_move;
	int his_ply;

	int castlePerm;

	U64 posKey;

	s_undo history[1024];

	// piece list
	int p_list[13][10];

	int pv_array[64];

	int search_history[13][64];
	int search_killers[2][64];

	 U64 piece_keys[12][64];
	 U64 enpassant_keys[64];
	 U64 castle_keys[16];
	 U64 side_key;

} s_board;

typedef struct
{
	int starttime;
	int stoptime;
	int depth;
	int timeset;
	int movestogo;

	long nodes;

	int quit;
	int stopped;

	float fh;
	float fhf;
	int null_cut;
	int movetime;
	int thread_num;
} s_searchinfo;

typedef struct
{
	s_searchinfo* info;
	s_board* original_position;
	s_hashtable* ttable;
} s_search_thread_data;

typedef struct
{
	s_board* pos;
	s_searchinfo* info;
	s_hashtable* ttable;

	int thread_number;
	int depth;
	int best_move;
} s_search_worker_data;



#endif