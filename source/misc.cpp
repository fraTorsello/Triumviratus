#include "Windows.h"
#include <io.h>
#include <fcntl.h>
#include <cstdio>
#include <intrin.h>
#include "defs.h"
#include <conio.h>

// get time in milliseconds
int get_time_ms()
{
    return GetTickCount();
}

int input_waiting()
{
    return _kbhit();
}

// read GUI/user input
void read_input()
{
    int bytes;
    char input[256] = "", * endc;

    if (input_waiting())
    {
        stopped = 1;

        do
        {
            bytes = _read(_fileno(stdin), input, 256);
        } while (bytes < 0);

        endc = strchr(input, '\n');
        if (endc) *endc = 0;

        if (strlen(input) > 0)
        {
            if (!strncmp(input, "quit", 4))
                quit = 1;
            else if (!strncmp(input, "stop", 4))
                quit = 1;
        }
    }
}

// a bridge function to interact between search and GUI input
void communicate() {
    if (timeset == 1 && get_time_ms() > stoptime) {
        stopped = 1;
    }
    read_input();
}

// count bits within a bitboard (Brian Kernighan's way)
int count_bits(U64 bitboard)
{
    int count = 0;
    while (bitboard)
    {
        count++;
        bitboard &= bitboard - 1;
    }
    return count;
}

// get least significant 1st bit index
int get_ls1b_index(U64 bitboard)
{
    if (bitboard)
    {
        unsigned long index;
        _BitScanForward64(&index, bitboard);
        return static_cast<int>(index);
    }
    else
        return -1;
}
