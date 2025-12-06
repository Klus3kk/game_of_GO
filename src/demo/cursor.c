// this is a test for mowing a cursor on the board using a keyboard

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>

#include "board.h"

void clear_screen() {
    printf("\033[2J\033[H"); // clear screen and move cursor to home position
    fflush(stdout);
}

// Function to place the cursor on the board
void place_cursor(Board *b) {
    board_clear(b);
    board_set(b, b->cur_x, b->cur_y, CELL_CURSOR);
}

int main() {
    // this is for not seeing the input characters on terminal
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    // Turn off canonical mode + echo
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);


    int size;
    printf("Write board size (minimum 7): ");
    if (scanf("%d", &size) != 1 || size < 7) {
        printf("Error!\n");
        return 1;
    }

    // flush newline after scanf
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}

    Board *b = board_create(size);
    if (!b) {
        printf("Cannot create board.\n");
        return 1;
    }

    b->cur_x = 0;
    b->cur_y = 0;

    clear_screen();
    board_draw_ascii(b);

    // After scanf, a newline remains, so we need to remove it:
    getchar();

    while (1) {
        int ch = getchar();
        if (ch == '\n') continue;

        ch = tolower(ch);

        if (ch == 'q') break;
        

        if (ch == 'w' && b->cur_y > 0) b->cur_y--;
        if (ch == 's' && b->cur_y < b->size - 1) b->cur_y++;
        if (ch == 'a' && b->cur_x > 0) b->cur_x--;
        if (ch == 'd' && b->cur_x < b->size - 1) b->cur_x++;
        else if (ch == 'x') board_set(b, b->cur_x, b->cur_y, CELL_BLACK);
        else if (ch == 'o') board_set(b, b->cur_x, b->cur_y, CELL_WHITE);

        // place_cursor(b);
        clear_screen();
        board_draw_ascii(b);
    }

    board_destroy(b);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return 0;
}
