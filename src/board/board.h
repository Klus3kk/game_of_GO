#ifndef BOARD_H
#define BOARD_H

typedef enum {
    CELL_EMPTY = 0, // no piece, no color
    CELL_BLACK = 1, // black piece, black color
    CELL_WHITE = 2, // white piece, white color
    CELL_CURSOR = 3, // blue color 
    CELL_SELECT = 4, // red color 
} Cell;

typedef struct {
    int size; // board size (lets say 7x7 is the smallest)
    Cell *cells; // pointer to array of cells

    int cur_x; // positon x
    int cur_y; // position y
} Board;

Board* board_create(int size); // creating a board of given size 
void board_destroy(Board *b); // destroying a board for memory cleanup 

int board_set(Board *b, int x, int y, Cell c); // setting cell at (x,y) to value c
Cell board_get(const Board *b, int x, int y); // getter: cell value at (x,y)

void board_clear(Board *b); // clearing the board (setting all cells to CELL_EMPTY)
void board_draw_ascii(const Board *b); // drawing the board in ascii format 

#endif