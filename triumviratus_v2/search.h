#pragma once

/**********************************\
 ==================================

               Search

 ==================================
\**********************************/


#ifndef SERACH_H
#define SEARCH_H
#include "defs.h"
#include "movegen.h"



/*
     These are the score bounds for the range of the mating scores
   [-infinity, -mate_value ... -mate_score, ... score ... mate_score ... mate_value, infinity]
*/

#define infinity 50000
#define mate_value 49000
#define mate_score 48000

// max ply that we can reach within a search
#define max_ply 64

// most valuable victim & less valuable attacker

/*

	(Victims) Pawn Knight Bishop   Rook  Queen   King
  (Attackers)
		Pawn   105    205    305    405    505    605
	  Knight   104    204    304    404    504    604
	  Bishop   103    203    303    403    503    603
		Rook   102    202    302    402    502    602
	   Queen   101    201    301    401    501    601
		King   100    200    300    400    500    600

*/

// MVV LVA [attacker][victim]
extern int mvv_lva[12][12];

// killer moves [id][ply]
extern int killer_moves[2][max_ply];

// history moves [piece][square]
extern int history_moves[12][64];

/*
      ================================
            Triangular PV table
      --------------------------------
        PV line: e2e4 e7e5 g1f3 b8c6
      ================================

           0    1    2    3    4    5

      0    m1   m2   m3   m4   m5   m6

      1    0    m2   m3   m4   m5   m6

      2    0    0    m3   m4   m5   m6

      3    0    0    0    m4   m5   m6

      4    0    0    0    0    m5   m6

      5    0    0    0    0    0    m6
*/

// PV length [ply]
extern int pv_length[max_ply];

// PV table [ply][ply]
extern int pv_table[max_ply][max_ply];

// follow PV & score PV move
extern int follow_pv, score_pv;


extern void search_position(int depth);



#endif
