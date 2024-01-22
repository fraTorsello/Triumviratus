
/**********************************\
 ==================================

               Perft

 ==================================
\**********************************/


#pragma once
#ifndef PERFT_H
#define PERFT_H

#include "defs.h"
#include "movegen.h"
#include "misc.h"

// leaf nodes (number of positions reached during the test of the move generator at a given depth)
extern U64 nodes;

extern inline void perft_driver(int depth);

// Function prototype for the perft test
extern void perft_test(int depth);

#endif // PERFT_H
