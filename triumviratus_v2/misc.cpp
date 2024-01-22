/**********************************\
 ==================================

       Miscellaneous functions
          forked from VICE
         by Richard Allbert

 ==================================
\**********************************/
#include "Windows.h"
#include <io.h>
#include <fcntl.h>
#include <cstdio>
#include <intrin.h>
#include "defs.h"


// get time in milliseconds
int get_time_ms()
{

    return GetTickCount();

}

/*

  Function to "listen" to GUI's input during search.
  It's waiting for the user input from STDIN.
  OS dependent.

  First Richard Allbert aka BluefeverSoftware grabbed it from somewhere...
  And then Code Monkey King has grabbed it from VICE)

*/
#include <conio.h>

int input_waiting()
{
    return _kbhit();
}
/*int input_waiting()
{
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(_fileno(stdin), &readfds);  // Use _fileno instead of fileno
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    select(16, &readfds, 0, 0, &tv);

    return (FD_ISSET(_fileno(stdin), &readfds));
}
*/

// read GUI/user input
void read_input()
{
    // bytes to read holder
    int bytes;

    // GUI/user input
    char input[256] = "", * endc;

    // "listen" to STDIN
    if (input_waiting())
    {
        // tell engine to stop calculating
        stopped = 1;

        // loop to read bytes from STDIN
        do
        {
            // read bytes from STDIN
            bytes = _read(_fileno(stdin), input, 256);

        }

        // until bytes available
        while (bytes < 0);

        // searches for the first occurrence of '\n'
        endc = strchr(input, '\n');

        // if found new line set value at pointer to 0
        if (endc) *endc = 0;

        // if input is available
        if (strlen(input) > 0)
        {
            // match UCI "quit" command
            if (!strncmp(input, "quit", 4))
                // tell engine to terminate exacution    
                quit = 1;

            // // match UCI "stop" command
            else if (!strncmp(input, "stop", 4))
                // tell engine to terminate exacution
                quit = 1;
        }
    }
}

// a bridge function to interact between search and GUI input
 void communicate() {
    // if time is up break here
    if (timeset == 1 && get_time_ms() > stoptime) {
        // tell engine to stop calculating
        stopped = 1;
    }

    // read GUI input
    read_input();
}


// count bits within a bitboard (Brian Kernighan's way)
 inline int count_bits(U64 bitboard)
{
    // bit counter
    int count = 0;

    // consecutively reset least significant 1st bit
    while (bitboard)
    {
        // increment count
        count++;

        // reset least significant 1st bit
        bitboard &= bitboard - 1;
    }

    // return bit count
    return count;
}



 // get least significant 1st bit index
 inline int get_ls1b_index(U64 bitboard)
 {
     // make sure bitboard is not 0
     if (bitboard)
     {
         unsigned long index;
         _BitScanForward64(&index, bitboard);
         return static_cast<int>(index);
     }

     // otherwise
     else
         // return illegal index
         return -1;
 }