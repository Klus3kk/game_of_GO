#include <ncurses.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_SETTINGS = 1,
    SCREEN_PLAY = 2
} Screen;

typedef struct {
    bool mouse_support;
    bool colours;
    int theme;               // 0..2
    char nickname[32];
} Settings;

static void init_ui(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_CYAN,   -1);  // title
        init_pair(2, COLOR_WHITE,  -1);  // normal
        init_pair(3, COLOR_BLACK,  COLOR_CYAN); // highlight
        init_pair(4, COLOR_GREEN,  -1);  // ok/selected
        init_pair(5, COLOR_MAGENTA,-1);  // section
        init_pair(6, COLOR_YELLOW, -1);  // hints
    }
}

static void shutdown_ui(void) {
    endwin();
}

static int read_nav_key(int ch) {
    // Returns: -1 up, +1 down, 0 none, 10 enter, 27 esc/back, 100 left, 101 right
    if (ch == KEY_UP || ch == 'w' || ch == 'W') return -1;
    if (ch == KEY_DOWN || ch == 's' || ch == 'S') return +1;
    if (ch == KEY_LEFT || ch == 'a' || ch == 'A') return 100;
    if (ch == KEY_RIGHT || ch == 'd' || ch == 'D') return 101;
    if (ch == 10 || ch == KEY_ENTER) return 10;
    if (ch == 27 || ch == 'q' || ch == 'Q') return 27; // ESC or q
    return 0;
}

static void draw_logo(int y) {
    const char *logo[] = {
        "      _____            _____    ",
        "  ___|\\    \\      ____|\\    \\   ",
        " /    /\\    \\    /     /\\    \\  ",
        "|    |  |____|  /     /  \\    \\ ",
        "|    |    ____ |     |    |    |",
        "|    |   |    ||     |    |    |",
        "|    |   |_,  ||\\     \\  /    /|",
        "|\\ ___\\___/  /|| \\_____\\/____/ |",
        "| |   /____ / | \\ |    ||    | /",
        " \\|___|    | /   \\|____||____|/ ",
        "   \\( |____|/       \\(    )/    ",
        "    '   )/           '    '     ",
        "        '                       ",
        NULL
    };

    int width = 0;
    for (int i = 0; logo[i]; i++) {
        int len = (int)strlen(logo[i]);
        if (len > width) width = len;
    }

    int x = (COLS - width) / 2;
    if (x < 0) x = 0;

    attron(COLOR_PAIR(1) | A_BOLD);
    for (int i = 0; logo[i]; i++) {
        mvprintw(y + i, x, "%s", logo[i]);
    }
    attroff(COLOR_PAIR(1) | A_BOLD);
}


static void center_print(int y, const char *s) {
    int x = (COLS - (int)strlen(s)) / 2;
    mvprintw(y, (x < 0 ? 0 : x), "%s", s);
}

static void draw_main_menu(int selected) {
    clear();
    int base_y = 2;

    draw_logo(base_y);
    int menu_y = base_y + 16;

    attron(COLOR_PAIR(6));
    center_print(menu_y - 2, "Use up arrow/down arrow or W/S, Enter to select, Q/ESC to quit");
    attroff(COLOR_PAIR(6));

    const char *items[] = {"PLAY", "SETTINGS", "EXIT"};
    int n = 3;

    for (int i = 0; i < n; i++) {
        int y = menu_y + i * 2;
        if (i == selected) {
            attron(COLOR_PAIR(3) | A_BOLD);
            center_print(y, items[i]);
            attroff(COLOR_PAIR(3) | A_BOLD);
        } else {
            attron(COLOR_PAIR(2));
            center_print(y, items[i]);
            attroff(COLOR_PAIR(2));
        }
    }

    refresh();
}
