#include <stdio.h>
#include <stdlib.h>

#include "board.h"

int main() {
    int size;
    printf("Enter the board size (minimum is 7): ");
    if (scanf("%d", &size) != 1) {
        printf("Error reading size.\n");
        return 1;
    }

    Board *b = board_create(size);
    if (!b) {
        printf("Failed to create board.\n");
        return 1;
    }

    // Example of piece placements 
    if (size >= 7) {
        board_set(b, 3, 3, CELL_BLACK);
        board_set(b, 4, 4, CELL_WHITE);
        board_set(b, 2, 2, CELL_CURSOR);
    }

    board_draw_ascii(b);

    board_destroy(b);
    return 0;
}