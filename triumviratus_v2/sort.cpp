#include <algorithm>
#include "defs.h"


// Function to partition the move_scores and move_list based on pivot
int partition(std::vector<int>& move_scores, int* moves, int low, int high) {
    int pivot_score = move_scores[high];
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (move_scores[j] >= pivot_score) {
            i++;
            // Swap move_scores
            std::swap(move_scores[i], move_scores[j]);
            // Swap moves
            std::swap(moves[i], moves[j]);
        }
    }

    // Swap move_scores and moves with pivot
    std::swap(move_scores[i + 1], move_scores[high]);
    std::swap(moves[i + 1], moves[high]);

    return i + 1;
}

// Recursive function to perform quicksort on move_scores and move_list
void quicksort(std::vector<int>& move_scores, int* moves, int low, int high) {
    if (low < high) {
        int partition_index = partition(move_scores, moves, low, high);

        // Recursive call on the left and right subarrays
        quicksort(move_scores, moves, low, partition_index - 1);
        quicksort(move_scores, moves, partition_index + 1, high);
    }
}