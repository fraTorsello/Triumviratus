/**********************************\
 ==================================

             Evaluation

 ==================================
\**********************************/
#include "defs.h"
#include "movegen.h"
#include "evaluation.h"
#include "nnue_eval.h"



// position evaluation
 inline int evaluate()
{
    // current pieces bitboard copy
    U64 bitboard;

    // init piece & square
    int piece, square;

    // array of piece codes converted to Stockfish piece codes
    int pieces[33];

    // array of square indices converted to Stockfish square indices
    int squares[33];

    // pieces and squares current index to write next piece square pair at
    int index = 2;

    // loop over piece bitboards
    for (int bb_piece = P; bb_piece <= k; bb_piece++)
    {
        // init piece bitboard copy
        bitboard = bitboards[bb_piece];

        // loop over pieces within a bitboard
        while (bitboard)
        {
            // init piece
            piece = bb_piece;

            // init square
            square = get_ls1b_index(bitboard);

            /*
                Code to initialize pieces and squares arrays
                to serve the purpose of direct probing of NNUE
            */

            // case white king
            if (piece == K)
            {
                /* convert white king piece code to stockfish piece code and
                   store it at the first index of pieces array
                */
                pieces[0] = nnue_pieces[piece];

                /* convert white king square index to stockfish square index and
                   store it at the first index of pieces array
                */
                squares[0] = nnue_squares[square];
            }

            // case black king
            else if (piece == k)
            {
                /* convert black king piece code to stockfish piece code and
                   store it at the second index of pieces array
                */
                pieces[1] = nnue_pieces[piece];

                /* convert black king square index to stockfish square index and
                   store it at the second index of pieces array
                */
                squares[1] = nnue_squares[square];
            }

            // all the rest pieces
            else
            {
                /*  convert all the rest of piece code with corresponding square codes
                    to stockfish piece codes and square indices respectively
                */
                pieces[index] = nnue_pieces[piece];
                squares[index] = nnue_squares[square];
                index++;
            }

            // pop ls1b
            pop_bit(bitboard, square);
        }
    }

    // set zero terminating characters at the end of pieces & squares arrays
    pieces[index] = 0;
    squares[index] = 0;

    /*
        We need to make sure that fifty rule move counter gives a penalty
        to the evaluation, otherwise it won't be capable of mating in
        simple endgames like KQK or KRK! This expression is used:
                        nnue_score * (100 - fifty) / 100
    */

    // get NNUE score (final score! No need to adjust by the side!)
    return (evaluate_nnue(side, pieces, squares) * (100 - fifty) / 100);
}
