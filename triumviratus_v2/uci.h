#pragma once
#ifndef  UCI_H
#define UCI_H
#include "defs.h"


// External function prototypes

// parse user/GUI move string input (e.g. "e7e8q")
extern int parse_move(char* move_string);

// parse UCI "position" command
extern void parse_position(char* command);

// reset time control variables
extern void reset_time_control();

// parse UCI command "go"
extern void parse_go(char* command);

// main UCI loop
extern void uci_loop();

// Function prototype for initializing the hash table
extern void init_hash_table(int mb);

// Function prototypes for reading and writing hash entry data
extern int read_hash_entry(int alpha, int beta, int* best_move, int depth);
extern void write_hash_entry(int score, int best_move, int depth, int hash_flag);


#endif
