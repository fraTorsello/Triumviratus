#include "stdio.h"
#include <string>  // Include the <string> header for std::wstring
#include <sstream> // Include the <sstream> header for std::wistringstream
#include <iostream>
#include <iomanip>
#include <Windows.h>
#include "presentation.h"
int getConsoleWidth() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int columns;

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;

    return columns;
#else
    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);

    return size.ws_col;
#endif
}


void printLine() {
    int consoleWidth = getConsoleWidth();
    std::string line(consoleWidth, '_');

    // Print the line in the specified color
    std::cout << "\033[38;2;205;133;63m" << line << "\033[0m" << std::endl;
}


void ascii_art() {
    std::wstring asciiArt = LR"(
 _____________________________________________________________________
|                                                                     |
|                                                           (\=,      |
|                                                          //  .\     |
|                                                         (( \_  \    |
|      _____     _                      ____    ___        ))  `\_)   |
|     |_   _| __(_)_   _ _ __ _____   _|___ \  / _ \      (/     \    |
|       | || '__| | | | | '_ ` _ \ \ / / __) || | | |      | _.-'|    |
|       | || |  | | |_| | | | | | \ V / / __/ | |_| |       )___(     |
|       |_||_|  |_|\__,_|_| |_| |_|\_/ |_____(_)___/       (=====)    |
|                                                          }====={    |
|    _______________________________________________      (_______)   |
|                                                                     |
|                         By: Francesco                               |
|                                                                     |
|_____________________________________________________________________|
    )";

    // Calculate the number of spaces needed to center the ASCII art
    int consoleWidth = 80; // Adjust this value based on your console window width
    int padding = (consoleWidth - 1 - asciiArt.find_first_of(L'\n')) / 3.5;

    // Output with leading spaces for centering
    std::wistringstream iss(asciiArt);
    std::wstring line;
    while (std::getline(iss, line, L'\n')) {
        std::wcout << "\033[38;2;205;133;63m" << std::setw(padding + line.length()) << line << "\033[0m" << std::endl;
    }
}