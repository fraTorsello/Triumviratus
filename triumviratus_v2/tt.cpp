#include "tt.h"
#include "defs.h"
#include "search.h"

void init_hash_table(int mb)
{
    // init hash size
    int hash_size = 0x100000 * mb;

    // init number of hash entries
    hash_entries = hash_size / sizeof(tt);

    // free hash table if not empty
    if (hash_table != NULL)
    {
        printf("    Clearing hash memory...\n");

        // free hash table dynamic memory
        free(hash_table);
    }

    // allocate memory
    hash_table = (tt*)malloc(hash_entries * sizeof(tt));

    // if allocation has failed
    if (hash_table == NULL)
    {
        printf("    Couldn't allocate memory for hash table, tryinr %dMB...", mb / 2);

        // try to allocate with half size
        init_hash_table(mb / 2);
    }

    // if allocation succeeded
    else
    {
        // clear hash table
        clear_hash_table();

        printf("\nHash table entries:    \033[38;2;205;133;63m%d\033[0m \n", hash_entries);

    }


}

// read hash entry data
 inline int read_hash_entry(int alpha, int beta, int* best_move, int depth)
{
    // create a TT instance pointer to particular hash entry storing
    // the scoring data for the current board position if available
    tt* hash_entry = &hash_table[hash_key % hash_entries];

    // make sure we're dealing with the exact position we need
    if (hash_entry->hash_key == hash_key)
    {
        // make sure that we match the exact depth our search is now at
        if (hash_entry->depth >= depth)
        {
            // extract stored score from TT entry
            int score = hash_entry->score;

            // retrieve score independent from the actual path
            // from root node (position) to current node (position)
            if (score < -mate_score) score += ply;
            if (score > mate_score) score -= ply;

            // match the exact (PV node) score 
            if (hash_entry->flag == hash_flag_exact)
                // return exact (PV node) score
                return score;

            // match alpha (fail-low node) score
            if ((hash_entry->flag == hash_flag_alpha) &&
                (score <= alpha))
                // return alpha (fail-low node) score
                return alpha;

            // match beta (fail-high node) score
            if ((hash_entry->flag == hash_flag_beta) &&
                (score >= beta))
                // return beta (fail-high node) score
                return beta;
        }

        // store best move
        *best_move = hash_entry->best_move;
    }

    // if hash entry doesn't exist
    return no_hash_entry;
}

// write hash entry data
 inline void write_hash_entry(int score, int best_move, int depth, int hash_flag)
{
    // create a TT instance pointer to particular hash entry storing
    // the scoring data for the current board position if available
    tt* hash_entry = &hash_table[hash_key % hash_entries];

    // store score independent from the actual path
    // from root node (position) to current node (position)
    if (score < -mate_score) score -= ply;
    if (score > mate_score) score += ply;

    // write hash entry data 
    hash_entry->hash_key = hash_key;
    hash_entry->score = score;
    hash_entry->flag = hash_flag;
    hash_entry->depth = depth;
    hash_entry->best_move = best_move;
}

// clear TT (hash table)
void clear_hash_table()
{
    // init hash table entry pointer
    tt* hash_entry;

    // loop over TT elements
    for (hash_entry = hash_table; hash_entry < hash_table + hash_entries; hash_entry++)
    {
        // reset TT inner fields
        hash_entry->hash_key = 0;
        hash_entry->depth = 0;
        hash_entry->flag = 0;
        hash_entry->score = 0;
    }
}

