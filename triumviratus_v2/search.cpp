#include <vector>
#include "search.h"
#include "defs.h"
#include "tt.h"
#include "movegen.h"
#include "misc.h"
#include "evaluation.h"
#include "perft.h"
#include "tinycthread.h"
// full depth moves counter
const int full_depth_moves = 4;
thrd_t worker_threads[64];

// depth limit to consider reduction
const int reduction_limit = 3;


// enable PV move scoring
static inline void enable_pv_scoring(moves* move_list)
{
    // disable following PV
    follow_pv = 0;

    // loop over the moves within a move list
    for (int count = 0; count < move_list->count; count++)
    {
        // make sure we hit PV move
        if (pv_table[0][ply] == move_list->moves[count])
        {
            // enable move scoring
            score_pv = 1;

            // enable following PV
            follow_pv = 1;
        }
    }
}

/*  =======================
         Move ordering
    =======================

    1. PV move
    2. Captures in MVV/LVA
    3. 1st killer move
    4. 2nd killer move
    5. History moves
    6. Unsorted moves
*/

// score moves
static inline int score_move(int move)
{
    // if PV move scoring is allowed
    if (score_pv)
    {
        // make sure we are dealing with PV move
        if (pv_table[0][ply] == move)
        {
            // disable score PV flag
            score_pv = 0;

            // give PV move the highest score to search it first
            return 20000;
        }
    }

    // score capture move
    if (get_move_capture(move))
    {
        // init source piece
        int piece = get_move_piece(move);

        // init target piece
        int target_piece = P;

        // pick up bitboard piece index ranges depending on side
        int start_piece, end_piece;

        // pick up side to move
        if (side == white) { start_piece = p; end_piece = k; }
        else { start_piece = P; end_piece = K; }

        // loop over bitboards opposite to the current side to move
        for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++)
        {
            // if there's a piece on the target square
            if (get_bit(bitboards[bb_piece], get_move_target(move)))
            {
                // remove it from corresponding bitboard
                target_piece = bb_piece;
                break;
            }
        }

        /* extract move features
        int source_square = get_move_source(move);
        int target_square = get_move_target(move);

        // make the first capture, so that X-ray defender show up
        pop_bit(bitboards[piece], source_square);

        // captures of undefended pieces are good by definition
        if (!is_square_attacked(target_square, side ^ 1)) {
            // restore captured piece
            set_bit(bitboards[piece], source_square);

            // score undefended captures greater than other captures
            return 15000;
        }

        // restore captured piece
        set_bit(bitboards[piece], source_square);*/

        // score move by MVV LVA lookup [source piece][target piece]
        return mvv_lva[piece][target_piece] + 10000;
    }

    // score quiet move
    else
    {
        // score 1st killer move
        if (killer_moves[0][ply] == move)
            return 9000;

        // score 2nd killer move
        else if (killer_moves[1][ply] == move)
            return 8000;

        // score history move
        else
            return history_moves[get_move_piece(move)][get_move_target(move)];
    }

    return 0;
}

// sort moves in descending order
static inline int sort_moves(moves* move_list, int best_move)
{
    // move scores
    std::vector<int> move_scores(move_list->count);

    // score all the moves within a move list
    for (int count = 0; count < move_list->count; count++)
    {
        // if hash move available
        if (best_move == move_list->moves[count])
            // score move
            move_scores[count] = 30000;

        else
            // score move
            move_scores[count] = score_move(move_list->moves[count]);
    }

    // loop over current move within a move list
    for (int current_move = 0; current_move < move_list->count; current_move++)
    {
        // loop over next move within a move list
        for (int next_move = current_move + 1; next_move < move_list->count; next_move++)
        {
            // compare current and next move scores
            if (move_scores[current_move] < move_scores[next_move])
            {
                // swap scores
                int temp_score = move_scores[current_move];
                move_scores[current_move] = move_scores[next_move];
                move_scores[next_move] = temp_score;

                // swap moves
                int temp_move = move_list->moves[current_move];
                move_list->moves[current_move] = move_list->moves[next_move];
                move_list->moves[next_move] = temp_move;
            }
        }
    }
    return 0;
}

// print move scores
void print_move_scores(moves* move_list)
{
    printf("     Move scores:\n\n");

    // loop over moves within a move list
    for (int count = 0; count < move_list->count; count++)
    {
        printf("     move: ");
        print_move(move_list->moves[count]);
        printf(" score: %d\n", score_move(move_list->moves[count]));
    }
}

// position repetition detection
static inline int is_repetition()
{
    // loop over repetition indices range
    for (int index = 0; index < repetition_index; index++)
        // if we found the hash key same with a current
        if (repetition_table[index] == hash_key)
            // we found a repetition
            return 1;

    // if no repetition found
    return 0;
}

// quiescence search
static inline int quiescence(int alpha, int beta)
{
    // every 2047 nodes
    if ((nodes & 2047) == 0)
        // "listen" to the GUI/user input
        communicate();

    // increment nodes count
    nodes++;

    // we are too deep, hence there's an overflow of arrays relying on max ply constant
    if (ply > max_ply - 1)
        // evaluate position
        return evaluate();

    // evaluate position
    int evaluation = evaluate();

    // fail-hard beta cutoff
    if (evaluation >= beta)
    {
        // node (position) fails high
        return beta;
    }

    // found a better move
    if (evaluation > alpha)
    {
        // PV node (position)
        alpha = evaluation;
    }

    // create move list instance
    moves move_list[1];

    // generate moves
    generate_moves(move_list);

    // sort moves
    sort_moves(move_list, 0);

    // loop over moves within a movelist
    for (int count = 0; count < move_list->count; count++)
    {
        // preserve board state
        copy_board();

        // increment ply
        ply++;

        // increment repetition index & store hash key
        repetition_index++;
        repetition_table[repetition_index] = hash_key;


        // make sure to make only legal moves
        if (make_move(move_list->moves[count], only_captures) == 0)
        {
            // decrement ply
            ply--;

            // decrement repetition index
            repetition_index--;

            // skip to next move
            continue;
        }

        // score current move
        int score = -quiescence(-beta, -alpha);

        // decrement ply
        ply--;

        // decrement repetition index
        repetition_index--;

        // take move back
        take_back();

        // reutrn 0 if time is up
        if (stopped == 1) return 0;

        // found a better move
        if (score > alpha)
        {
            // PV node (position)
            alpha = score;

            // fail-hard beta cutoff
            if (score >= beta)
            {
                // node (position) fails high
                return beta;
            }
        }
    }

    // node (position) fails low
    return alpha;
}

// negamax alpha beta search
static inline int negamax(int alpha, int beta, int depth)
{
    // init PV length
    pv_length[ply] = ply;

    // variable to store current move's score (from the static evaluation perspective)
    int score;

    // best move (to store in TT)
    int best_move = 0;

    // define hash flag
    int hash_flag = hash_flag_alpha;

    // if position repetition occurs
    if (ply && is_repetition() || fifty >= 100)
        // return draw score
        return 0;

    // a hack by Pedro Castro to figure out whether the current node is PV node or not 
    int pv_node = beta - alpha > 1;

    // read hash entry if we're not in a root ply and hash entry is available
    // and current node is not a PV node
    if (ply && (score = read_hash_entry(alpha, beta, &best_move, depth)) != no_hash_entry && pv_node == 0)
        // if the move has already been searched (hence has a value)
        // we just return the score for this move without searching it
        return score;

    // every 2047 nodes
    if ((nodes & 2047) == 0)
        // "listen" to the GUI/user input
        communicate();

    // recursion escapre condition
    if (depth == 0)
        // run quiescence search
        return quiescence(alpha, beta);

    // we are too deep, hence there's an overflow of arrays relying on max ply constant
    if (ply > max_ply - 1)
        // evaluate position
        return evaluate();

    // increment nodes count
    nodes++;

    // is king in check
    int in_check = is_square_attacked((side == white) ? get_ls1b_index(bitboards[K]) :
        get_ls1b_index(bitboards[k]),
        side ^ 1);

    // increase search depth if the king has been exposed into a check
    if (in_check) depth++;

    // legal moves counter
    int legal_moves = 0;

    // get static evaluation score
    int static_eval = evaluate();

    // evaluation pruning / static null move pruning
    if (depth < 3 && !pv_node && !in_check && abs(beta - 1) > -infinity + 100)
    {
        // define evaluation margin
        int eval_margin = 120 * depth;

        // evaluation margin substracted from static evaluation score fails high
        if (static_eval - eval_margin >= beta)
            // evaluation margin substracted from static evaluation score
            return static_eval - eval_margin;
    }

    // null move pruning
    if (depth >= 3 && in_check == 0 && ply)
    {
        // preserve board state
        copy_board();

        // increment ply
        ply++;

        // increment repetition index & store hash key
        repetition_index++;
        repetition_table[repetition_index] = hash_key;

        // hash enpassant if available
        if (enpassant != no_sq) hash_key ^= enpassant_keys[enpassant];

        // reset enpassant capture square
        enpassant = no_sq;

        // switch the side, literally giving opponent an extra move to make
        side ^= 1;

        // hash the side
        hash_key ^= side_key;

        /* search moves with reduced depth to find beta cutoffs
           depth - 1 - R where R is a reduction limit */
        score = -negamax(-beta, -beta + 1, depth - 1 - 2);

        // decrement ply
        ply--;

        // decrement repetition index
        repetition_index--;

        // restore board state
        take_back();

        // reutrn 0 if time is up
        if (stopped == 1) return 0;

        // fail-hard beta cutoff
        if (score >= beta)
            // node (position) fails high
            return beta;
    }

    // razoring
    if (!pv_node && !in_check && depth <= 3)
    {
        // get static eval and add first bonus
        score = static_eval + 125;

        // define new score
        int new_score;

        // static evaluation indicates a fail-low node
        if (score < beta)
        {
            // on depth 1
            if (depth == 1)
            {
                // get quiscence score
                new_score = quiescence(alpha, beta);

                // return quiescence score if it's greater then static evaluation score
                return (new_score > score) ? new_score : score;
            }

            // add second bonus to static evaluation
            score += 175;

            // static evaluation indicates a fail-low node
            if (score < beta && depth <= 2)
            {
                // get quiscence score
                new_score = quiescence(alpha, beta);

                // quiescence score indicates fail-low node
                if (new_score < beta)
                    // return quiescence score if it's greater then static evaluation score
                    return (new_score > score) ? new_score : score;
            }
        }
    }

    // create move list instance
    moves move_list[1];

    // generate moves
    generate_moves(move_list);

    // if we are now following PV line
    if (follow_pv)
        // enable PV move scoring
        enable_pv_scoring(move_list);

    // sort moves
    sort_moves(move_list, best_move);

    // number of moves searched in a move list
    int moves_searched = 0;

    // loop over moves within a movelist
    for (int count = 0; count < move_list->count; count++)
    {
        // preserve board state
        copy_board();

        // increment ply
        ply++;

        // increment repetition index & store hash key
        repetition_index++;
        repetition_table[repetition_index] = hash_key;

        // make sure to make only legal moves
        if (make_move(move_list->moves[count], all_moves) == 0)
        {
            // decrement ply
            ply--;

            // decrement repetition index
            repetition_index--;

            // skip to next move
            continue;
        }

        // increment legal moves
        legal_moves++;

        // full depth search
        if (moves_searched == 0)
            // do normal alpha beta search
            score = -negamax(-beta, -alpha, depth - 1);

        // late move reduction (LMR)
        else
        {
            // condition to consider LMR
            if (
                moves_searched >= full_depth_moves &&
                depth >= reduction_limit &&
                in_check == 0 &&
                get_move_capture(move_list->moves[count]) == 0 &&
                get_move_promoted(move_list->moves[count]) == 0
                )
                // search current move with reduced depth:
                score = -negamax(-alpha - 1, -alpha, depth - 2);

            // hack to ensure that full-depth search is done
            else score = alpha + 1;

            // principle variation search PVS
            if (score > alpha)
            {
                /* Once you've found a move with a score that is between alpha and beta,
                   the rest of the moves are searched with the goal of proving that they are all bad.
                   It's possible to do this a bit faster than a search that worries that one
                   of the remaining moves might be good. */
                score = -negamax(-alpha - 1, -alpha, depth - 1);

                /* If the algorithm finds out that it was wrong, and that one of the
                   subsequent moves was better than the first PV move, it has to search again,
                   in the normal alpha-beta manner.  This happens sometimes, and it's a waste of time,
                   but generally not often enough to counteract the savings gained from doing the
                   "bad move proof" search referred to earlier. */
                if ((score > alpha) && (score < beta))
                    /* re-search the move that has failed to be proved to be bad
                       with normal alpha beta score bounds*/
                    score = -negamax(-beta, -alpha, depth - 1);
            }
        }

        // decrement ply
        ply--;

        // decrement repetition index
        repetition_index--;

        // take move back
        take_back();

        // reutrn 0 if time is up
        if (stopped == 1)
            return 0;

        // increment the counter of moves searched so far
        moves_searched++;

        // found a better move
        if (score > alpha)
        {
            // switch hash flag from storing score for fail-low node
            // to the one storing score for PV node
            hash_flag = hash_flag_exact;

            // store best move (for TT)
            best_move = move_list->moves[count];

            // on quiet moves
            if (get_move_capture(move_list->moves[count]) == 0)
                // store history moves
                history_moves[get_move_piece(move_list->moves[count])][get_move_target(move_list->moves[count])] += depth;

            // PV node (position)
            alpha = score;

            // write PV move
            pv_table[ply][ply] = move_list->moves[count];

            // loop over the next ply
            for (int next_ply = ply + 1; next_ply < pv_length[ply + 1]; next_ply++)
                // copy move from deeper ply into a current ply's line
                pv_table[ply][next_ply] = pv_table[ply + 1][next_ply];

            // adjust PV length
            pv_length[ply] = pv_length[ply + 1];

            // fail-hard beta cutoff
            if (score >= beta)
            {
                // store hash entry with the score equal to beta
                write_hash_entry(beta, best_move, depth, hash_flag_beta);

                // on quiet moves
                if (get_move_capture(move_list->moves[count]) == 0)
                {
                    // store killer moves
                    killer_moves[1][ply] = killer_moves[0][ply];
                    killer_moves[0][ply] = move_list->moves[count];
                }

                // node (position) fails high
                return beta;
            }
        }
    }

    // we don't have any legal moves to make in the current postion
    if (legal_moves == 0)
    {
        // king is in check
        if (in_check)
            // return mating score (assuming closest distance to mating position)
            return -mate_value + ply;

        // king is not in check
        else
            // return stalemate score
            return 0;
    }

    // store hash entry with the score equal to alpha
    write_hash_entry(alpha, best_move, depth, hash_flag);

    // node (position) fails low
    return alpha;
}

// search position for the best move
void search_position(int depth)
{
    int start = get_time_ms();
    int score = 0;
    nodes = 0;
    stopped = 0;
    follow_pv = 0;
    score_pv = 0;

    // clear helper data structures for search
    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_moves, 0, sizeof(history_moves));
    memset(pv_table, 0, sizeof(pv_table));
    memset(pv_length, 0, sizeof(pv_length));

    // define initial alpha beta bounds
    int alpha = -infinity;
    int beta = infinity;

    // iterative deepening
    for (int current_depth = 1; current_depth <= depth; current_depth++)
    {
        // if time is up
        if (stopped == 1)
            break;

        follow_pv = 1;

        // find best move within a given position
        score = negamax(alpha, beta, current_depth);

        // we fell outside the window, so try again with a full-width window (and the same depth)
        if ((score <= alpha) || (score >= beta)) {
            alpha = -infinity;
            beta = infinity;
            continue;
        }

        // set up the window for the next iteration
        alpha = score - 50;
        beta = score + 50;

        // if PV is available
        if (pv_length[0])
        {
            if (score > -mate_value && score < -mate_score)
                printf("info score mate %d depth %d nodes %lld time %d pv ", -(score + mate_value) / 2 - 1, current_depth, nodes, get_time_ms() - start);

            else if (score > mate_score && score < mate_value)
                printf("info score mate %d depth %d nodes %lld time %d pv ", (mate_value - score) / 2 + 1, current_depth, nodes, get_time_ms() - start);

            else
                printf("info score cp %d depth %d nodes %lld time %d pv ", score, current_depth, nodes, get_time_ms() - start);

            for (int count = 0; count < pv_length[0]; count++)
            {
                print_move(pv_table[0][count]);
                printf(" ");
            }

            printf("\n");
        }
    }

    // print best move
    printf("bestmove ");

    if (pv_table[0][0])
        print_move(pv_table[0][0]);

    else
        // shouldn't get here
        printf("(none)");

    printf("\n");
}


