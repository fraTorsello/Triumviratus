﻿#pragma once
#ifndef DEFS_H
#define DEFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <unordered_map>

#include "nnue_eval.h"

// Define version information
#define VERSION " - 2.0 NNUE"
#define AUTHOR "Francesco Torsello"
#define NAME "Triumviratus"

// Define the size of a 64-bit unsigned integer
#define U64 unsigned long long

// FEN debug positions for testing
#define empty_board "8/8/8/8/8/8/8/8 b - - "
#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define tricky_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 "
#define killer_position "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define cmk_position "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NPP/PPP1NPB1/R1BQ1RK1 b - - 0 9 "
#define repetitions "2r3k1/R7/8/1R6/8/8/P4KPP/8 w - - 0 40 "

// Enumeration for chessboard squares
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

// Enumeration for piece types
enum { P, N, B, R, Q, K, p, n, b, r, q, k };

// Enumeration for colors
enum { white, black, both };

// Enumeration for special chess pieces (rook or bishop)
enum { rook, bishop };

// Castle permissions flags
enum { wk = 1, wq = 2, bk = 4, bq = 8 };

// Convert squares to coordinates
extern const char* square_to_coordinates[];

// ASCII representation of chess pieces
extern char ascii_pieces[13];

// Unicode representation of chess pieces
extern const char* unicode_pieces[12];

// Function to map a character to a chess piece
extern int mapCharToPiece(char c);

// Function to map a chess piece to its promotion character
extern char mapPieceToPromotion(int piece);

// Chessboard variables
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

// Time control variables
extern int quit;
extern int movestogo;
extern int movetime;
extern int time_uci;
extern int inc;
extern int starttime;
extern int stoptime;
extern int timeset;
extern int stopped;

// Bit manipulations macros
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))
extern inline int count_bits(U64 bitboard);
extern inline int get_ls1b_index(U64 bitboard);

// Variables for Zobrist hashing
extern U64 piece_keys[12][64];
extern U64 enpassant_keys[64];
extern U64 castle_keys[16];
extern U64 side_key;

extern void init_random_keys();
extern U64 generate_hash_key();

// Move list structure
typedef struct {
    int moves[256];
    int count;
} moves;

// Initialization function
extern void init_bitboards();

// Sorting function (Not used)
extern void quicksort(std::vector<int>& move_scores, int* moves, int low, int high);


#endif
