#pragma once
#ifndef IO_H
#define IO_H

#include "defs.h"

extern void print_bitboard(U64 bitboard);
extern void print_board();
extern void reset_board();
extern void parse_fen(const char* fen);

#endif