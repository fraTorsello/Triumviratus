#include "defs.h"
#include "misc.h"
#include "movegen.h"
#include "perft.h"


// perft driver
 inline void perft_driver(int depth)
{
    // reccursion escape condition
    if (depth == 0)
    {
        // increment nodes count (count reached positions)
        nodes++;
        return;
    }

    // create move list instance
    moves move_list[1];

    // generate moves
    generate_moves(move_list);

    // loop over generated moves
    for (int move_count = 0; move_count < move_list->count; move_count++)
    {
        // preserve board state
        copy_board();

        // make move
        if (!make_move(move_list->moves[move_count], all_moves))
            // skip to the next move
            continue;

        // call perft driver recursively
        perft_driver(depth - 1);

        // take back
        take_back();
    }
}

// perft test
void perft_test(int depth)
{
    printf("\n     Performance test\n\n");

    // create move list instance
    moves move_list[1];

    // generate moves
    generate_moves(move_list);

    // init start time
    long start = get_time_ms();

    // loop over generated moves
    for (int move_count = 0; move_count < move_list->count; move_count++)
    {
        // preserve board state
        copy_board();

        // make move
        if (!make_move(move_list->moves[move_count], all_moves))
            // skip to the next move
            continue;

        // cummulative nodes
        long cummulative_nodes = nodes;

        // call perft driver recursively
        perft_driver(depth - 1);

        // old nodes
        long old_nodes = nodes - cummulative_nodes;

        // take back
        take_back();

        // print move
        printf("     move: %s%s%c  nodes: %ld\n", square_to_coordinates[get_move_source(move_list->moves[move_count])],
            square_to_coordinates[get_move_target(move_list->moves[move_count])],
            get_move_promoted(move_list->moves[move_count]) ? mapPieceToPromotion(get_move_promoted(move_list->moves[move_count])) : ' ',
            old_nodes);
    }

    // print results
    printf("\n    Depth: %d\n", depth);
    printf("    Nodes: %lld\n", nodes);
    printf("     Time: %ld\n\n", get_time_ms() - start);
}
