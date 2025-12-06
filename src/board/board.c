#include <stdlib.h>
#include <stdio.h>

#include "board.h"

// Calculate the index in the 1D array for coordinates (x, y)
static int idx(const Board *b, int x, int y) {
    return y * b->size + x;
}

// Create a new board of given size
Board* board_create(int size) {
    if (size < 7) return NULL; // Minimum board size is 7x7 (for now at least?)

    Board *b = (Board *)malloc(sizeof(Board));  
    if (!b) return NULL; // If memolry allocation fails

    b->size = size; 
    b->cells = (Cell *)malloc(size * size * sizeof(Cell));
    b->cur_x = 0; 
    b->cur_y = 0;
    
    // Check if memory allocation for cells was successful
    if (!b->cells) {
        free(b);
        return NULL;
    }

    board_clear(b);
    return b;
}

void board_destroy(Board *b) {
    // If board is NULL
    if (!b) return; 

    free(b->cells); 
    free(b); 
}

// Clearing the board 
void board_clear(Board *b) {
    for (int i = 0; i < b->size * b->size; i++)
        b->cells[i] = CELL_EMPTY; 
}

// Setting cell at (x,y) to value c
int board_set(Board *b, int x, int y, Cell c) {
    // Check for out of bounds
    if (x < 0 || y < 0 || x >= b->size || y >= b->size) 
        return -1; 

    b->cells[idx(b, x, y)] = c;  
    return 0; 
}

// Getting the cell value at (x,y)
Cell board_get(const Board *b, int x, int y) {
    // Check for out of bounds
    if (x < 0 || y < 0 || x >= b->size || y >= b->size) 
        return CELL_EMPTY; 

    return b->cells[idx(b, x, y)];  // Return the cell value
}


// Drawing the board (its a test for now, i will change it later)
void board_draw_ascii(const Board *b) {
    printf("\n   ");
    for (int x = 0; x < b->size; x++)
        printf("%2d", x);
    printf("\n");

    for (int y = 0; y < b->size; y++) {
        printf("%2d ", y);
        for (int x = 0; x < b->size; x++) {
            char ch = '.';
            switch (board_get(b, x, y)) {
                case CELL_EMPTY: ch = '.'; break;
                case CELL_BLACK: ch = 'B'; break;
                case CELL_WHITE: ch = 'W'; break;
                case CELL_CURSOR: ch = 'C'; break;
                case CELL_SELECT: ch = 'S'; break;
                default: ch = '?'; break;
            }
            printf(" %c", ch);
        }
        printf("\n");
    }
}