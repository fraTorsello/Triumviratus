#include "defs.h"
#include "magic.h"

// init random hash keys
void init_random_keys()
{
    // update pseudo random number state
    random_state = 1804289383;

    // loop over piece codes
    for (int piece = P; piece <= k; piece++)
    {
        // loop over board squares
        for (int square = 0; square < 64; square++)
            // init random piece keys
            piece_keys[piece][square] = get_random_U64_number();
    }

    // loop over board squares
    for (int square = 0; square < 64; square++)
        // init random enpassant keys
        enpassant_keys[square] = get_random_U64_number();

    // loop over castling keys
    for (int index = 0; index < 16; index++)
        // init castling keys
        castle_keys[index] = get_random_U64_number();

    // init random side key
    side_key = get_random_U64_number();
}

// generate "almost" unique position ID aka hash key from scratch
U64 generate_hash_key()
{
    // final hash key
    U64 final_key = 0ULL;

    // temp piece bitboard copy
    U64 bitboard;

    // loop over piece bitboards
    for (int piece = P; piece <= k; piece++)
    {
        // init piece bitboard copy
        bitboard = bitboards[piece];

        // loop over the pieces within a bitboard
        while (bitboard)
        {
            // init square occupied by the piece
            int square = get_ls1b_index(bitboard);

            // hash piece
            final_key ^= piece_keys[piece][square];

            // pop LS1B
            pop_bit(bitboard, square);
        }
    }

    // if enpassant square is on board
    if (enpassant != no_sq)
        // hash enpassant
        final_key ^= enpassant_keys[enpassant];

    // hash castling rights
    final_key ^= castle_keys[castle];

    // hash the side only if black is to move
    if (side == black) final_key ^= side_key;

    // return generated hash key
    return final_key;
}

