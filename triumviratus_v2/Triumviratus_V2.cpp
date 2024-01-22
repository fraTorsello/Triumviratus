#include <iostream>
#include "defs.h"
#include "uci.h"
#include "tt.h"
#include "presentation.h"
#include <thread>

void present();
void welcome_msg();
void init_hash_nnue();

int main()
{
    // init all
    init_bitboards();

   
    present();
    init_hash_nnue();
    welcome_msg();

    
    // connect to GUI
    uci_loop();

    // free hash table memory on exit
    free(hash_table);
}


void welcome_msg() {

    const char* resetAttributes = "\033[0m";
    // ANSI escape code for dark blue text
    const char* darkBlue = "\033[1;34m";
    // ANSI escape code for bold text
    const char* bold = "\033[1m";
    // ANSI escape code for italic text
    const char* italic = "\033[3m";
    // Print the welcome message with "Triumviratus" in bold, italic, and dark blue
    std::cout <<std::endl<< "Welcome to " << darkBlue << bold << italic << "Triumviratus" << resetAttributes
        << "! Type '" << italic << "uci" << resetAttributes << "' to connect and then '"
        << italic << "ucinewgame" << resetAttributes << "' to start\n\n\n";
}


void present() {
   

    ascii_art();
    std::cout << std::endl;
    printLine();
    std::cout << std::endl;

    int core_count = 1;
    if (std::thread::hardware_concurrency() > 0)
        core_count = std::thread::hardware_concurrency();
    printf("\n");
    printf("Name:                  \033[38;2;255;165;0m%s%s\033[0m\n", NAME, VERSION);
    printf("Author:                \033[38;2;255;165;0m%s\033[0m\n", AUTHOR);
    printf("Threads detected:      \033[38;2;255;165;0m%d\033[0m\n", core_count); // \033[38;2;255;165;0m for orange
   
}

void init_hash_nnue() {
    init_hash_table(64);
    init_nnue("nn-eba324f53044.nnue");
}

