#include "defs.h"
#include "uci.h"
#include "movegen.h"
#include "search.h"
#include "tt.h"
#include "misc.h"
#include "io.h"
// parse user/GUI move string input (e.g. "e7e8q")
int parse_move(char* move_string)
{
    // create move list instance
    moves move_list[1];

    // generate moves
    generate_moves(move_list);

    // parse source square
    int source_square = (move_string[0] - 'a') + (8 - (move_string[1] - '0')) * 8;

    // parse target square
    int target_square = (move_string[2] - 'a') + (8 - (move_string[3] - '0')) * 8;

    // loop over the moves within a move list
    for (int move_count = 0; move_count < move_list->count; move_count++)
    {
        // init move
        int move = move_list->moves[move_count];

        // make sure source & target squares are available within the generated move
        if (source_square == get_move_source(move) && target_square == get_move_target(move))
        {
            // init promoted piece
            int promoted_piece = get_move_promoted(move);

            // promoted piece is available
            if (promoted_piece)
            {
                // promoted to queen
                if ((promoted_piece == Q || promoted_piece == q) && move_string[4] == 'q')
                    // return legal move
                    return move;

                // promoted to rook
                else if ((promoted_piece == R || promoted_piece == r) && move_string[4] == 'r')
                    // return legal move
                    return move;

                // promoted to bishop
                else if ((promoted_piece == B || promoted_piece == b) && move_string[4] == 'b')
                    // return legal move
                    return move;

                // promoted to knight
                else if ((promoted_piece == N || promoted_piece == n) && move_string[4] == 'n')
                    // return legal move
                    return move;

                // continue the loop on possible wrong promotions (e.g. "e7e8f")
                continue;
            }

            // return legal move
            return move;
        }
    }

    // return illegal move
    return 0;
}

// parse UCI "position" command
void parse_position(char* command)
{
    // shift pointer to the right where next token begins
    command += 9;

    // init pointer to the current character in the command string
    char* current_char = command;

    // parse UCI "startpos" command
    if (strncmp(command, "startpos", 8) == 0)
        // init chess board with start position
        parse_fen(start_position);

    // parse UCI "fen" command 
    else
    {
        // make sure "fen" command is available within command string
        current_char = strstr(command, "fen");

        // if no "fen" command is available within command string
        if (current_char == NULL)
            // init chess board with start position
            parse_fen(start_position);

        // found "fen" substring
        else
        {
            // shift pointer to the right where next token begins
            current_char += 4;

            // init chess board with position from FEN string
            parse_fen(current_char);
        }
    }

    // parse moves after position
    current_char = strstr(command, "moves");

    // moves available
    if (current_char != NULL)
    {
        // shift pointer to the right where next token begins
        current_char += 6;

        // loop over moves within a move string
        while (*current_char)
        {
            // parse next move
            int move = parse_move(current_char);

            // if no more moves
            if (move == 0)
                // break out of the loop
                break;

            // increment repetition index
            repetition_index++;

            // wtire hash key into a repetition table
            repetition_table[repetition_index] = hash_key;

            // make move on the chess board
            make_move(move, all_moves);

            // move current character mointer to the end of current move
            while (*current_char && *current_char != ' ') current_char++;

            // go to the next move
            current_char++;
        }
    }

    // print board
    print_board();
}

// reset time control variables
void reset_time_control()
{
    // reset timing
    quit = 0;
    movestogo = 30;
    movetime = -1;
    time_uci = -1;
    inc = 0;
    starttime = 0;
    stoptime = 0;
    timeset = 0;
    stopped = 0;
}

// parse UCI command "go"
void parse_go(char* command)
{
    // reset time control
    reset_time_control();

    // init parameters
    int depth = -1;

    // init argument
    char* argument = NULL;

    // infinite search
    if ((argument = strstr(command, "infinite"))) {}

    // match UCI "binc" command
    if ((argument = strstr(command, "binc")) && side == black)
        // parse black time increment
        inc = atoi(argument + 5);

    // match UCI "winc" command
    if ((argument = strstr(command, "winc")) && side == white)
        // parse white time increment
        inc = atoi(argument + 5);

    // match UCI "wtime" command
    if ((argument = strstr(command, "wtime")) && side == white)
        // parse white time limit
        time_uci = atoi(argument + 6);

    // match UCI "btime" command
    if ((argument = strstr(command, "btime")) && side == black)
        // parse black time limit
        time_uci = atoi(argument + 6);

    // match UCI "movestogo" command
    if ((argument = strstr(command, "movestogo")))
        // parse number of moves to go
        movestogo = atoi(argument + 10);

    // match UCI "movetime" command
    if ((argument = strstr(command, "movetime")))
        // parse amount of time allowed to spend to make a move
        movetime = atoi(argument + 9);

    // match UCI "depth" command
    if ((argument = strstr(command, "depth")))
        // parse search depth
        depth = atoi(argument + 6);

    // if move time is not available
    if (movetime != -1)
    {
        // set time equal to move time
        time_uci = movetime;

        // set moves to go to 1
        movestogo = 1;
    }

    // init start time
    starttime = get_time_ms();

    // init search depth
    depth = depth;

    // if time control is available
    if (time_uci != -1)
    {
        // flag we're playing with time control
        timeset = 1;

        // set up timing
        time_uci /= movestogo;

        // lag compensation
        time_uci -= 50;

        // if time is up
        if (time_uci < 0)
        {
            // restore negative time to 0
            time_uci = 0;

            // inc lag compensation on 0+inc time controls
            inc -= 50;

            // timing for 0 seconds left and no inc
            if (inc < 0) inc = 1;
        }

        // init stoptime
        stoptime = starttime + time_uci + inc;
    }

    // if depth is not available
    if (depth == -1)
        // set depth to 64 plies (takes ages to complete...)
        depth = 64;

    // print debug info
    printf("time: %d  inc: %d  start: %u  stop: %u  depth: %d  timeset:%d\n",
        time_uci, inc, starttime, stoptime, depth, timeset);

    // search position
    search_position(depth);
}

// main UCI loop
void uci_loop()
{
    // just make it big enough
#define INPUT_BUFFER 10000

// max hash MB
    int max_hash = 128;

    // default MB value
    int mb = 64;

    // reset STDIN & STDOUT buffers
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    // define user / GUI input buffer
    char input[INPUT_BUFFER];

    // main loop
    while (1)
    {
        // reset user /GUI input
        memset(input, 0, sizeof(input));

        // make sure output reaches the GUI
        fflush(stdout);

        // get user / GUI input
        if (!fgets(input, INPUT_BUFFER, stdin))
            // continue the loop
            continue;

        // make sure input is available
        if (input[0] == '\n')
            // continue the loop
            continue;

        // parse UCI "isready" command
        if (strncmp(input, "isready", 7) == 0)
        {
            printf("readyok\n");
            continue;
        }

        // parse UCI "position" command
        else if (strncmp(input, "position", 8) == 0)
        {
            // call parse position function
            parse_position(input);

            // clear hash table
            clear_hash_table();
        }
        // parse UCI "ucinewgame" command
        else if (strncmp(input, "ucinewgame", 10) == 0)
        {
            char startpos_string[18]= "position startpos";
            // call parse position function
            parse_position(startpos_string);

            // clear hash table
            clear_hash_table();
        }
        // parse UCI "go" command
        else if (strncmp(input, "go", 2) == 0)
            // call parse go function
            parse_go(input);

        // parse UCI "quit" command
        else if (strncmp(input, "quit", 4) == 0)
            // quit from the UCI loop (terminate program)
            break;

        // parse UCI "uci" command
        else if (strncmp(input, "uci", 3) == 0)
        {
            // print engine info
            printf("id name %s %s\n", NAME, VERSION);
            printf("id author %s\n", AUTHOR);
            printf("option name Hash type spin default 64 min 4 max %d\n", max_hash);
            printf("uciok\n");
        }

        else if (!strncmp(input, "setoption name Hash value ", 26)) {
            // init MB
            int result = sscanf_s(input, "%*s %*s %*s %*s %d", &mb);

            // Check if sscanf_s successfully parsed the integer value
            if (result == 1) {
                // adjust MB if going beyond the allowed bounds
                if (mb < 4) mb = 4;
                if (mb > max_hash) mb = max_hash;

                // set hash table size in MB
                printf("    Set hash table size to %dMB\n", mb);
                init_hash_table(mb);
            }
        }

    }
}

