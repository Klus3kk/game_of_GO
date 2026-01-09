#include "client_ui.h"
#include "client_state.h"
#include <stdio.h>
#include <string.h>
#include <ncurses.h>
#include <locale.h>

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

void init_ui(void) {
    setlocale(LC_ALL, "");
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
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    mouseinterval(0);
}

void shutdown_ui(void) {
    endwin();
}

int read_nav_key(int ch) {
    // Returns: -1 up, +1 down, 0 none, 10 enter, 27 esc/back, 100 left, 101 right
    if (ch == KEY_UP || ch == 'w' || ch == 'W') return -1;
    if (ch == KEY_DOWN || ch == 's' || ch == 'S') return +1;
    if (ch == KEY_LEFT || ch == 'a' || ch == 'A') return 100;
    if (ch == KEY_RIGHT || ch == 'd' || ch == 'D') return 101;
    if (ch == 10 || ch == KEY_ENTER) return 10;
    if (ch == 27) return 27; // ESC 
    return 0;
}

void draw_wait_screen(void) {
    clear();
    int base_y = 2;
    draw_logo(base_y);

    int y = base_y + 16;

    attron(COLOR_PAIR(5) | A_BOLD);
    center_print(y, "WAITING FOR OPPONENT");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 2;

    char info[128];
    snprintf(info, sizeof(info), "Game #%d  size=%d  you=%s", my_game_id, my_game_size, my_color[0]?my_color:"?");
    attron(COLOR_PAIR(2));
    center_print(y, info);
    attroff(COLOR_PAIR(2));
    y += 2;

    attron(COLOR_PAIR(6));
    center_print(y, "C=CANCEL   R=REFRESH   Q/ESC=BACK");
    attroff(COLOR_PAIR(6));

    refresh();
}

void draw_main_menu(int selected) {
    clear();
    int base_y = 2;

    draw_logo(base_y);
    int menu_y = base_y + 16;

    main_menu_y0 = menu_y;
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

void draw_settings_screen(const Settings *st, int selected) {
    clear();
    int base_y = 2;

    draw_logo(base_y);
    int y = base_y + 16;

    attron(COLOR_PAIR(5) | A_BOLD);
    center_print(y, "SETTINGS");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 2;

    attron(COLOR_PAIR(6));
    center_print(y, "Space: toggle  |  Enter: edit/select  |  Q/ESC: back");
    attroff(COLOR_PAIR(6));
    y += 2;

    // Layout
    int left = (COLS - 40) / 2;
    if (left < 0) left = 0;

    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(y, left, "BASIC");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 2;

    const char *mouse_line = st->mouse_support ? "[x] Mouse Support" : "[ ] Mouse Support";
    const char *col_line   = st->colours       ? "[x] Colours"       : "[ ] Colours";

    const char *lines_basic[] = { mouse_line, col_line };
    for (int i = 0; i < 2; i++) {
        if (selected == i) attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(y + i, left, "%s", lines_basic[i]);
        if (selected == i) attroff(COLOR_PAIR(3) | A_BOLD);
    }
    y += 3;

    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(y, left, "THEMES");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 2;

    // themes
    for (int i = 0; i < 3; i++) {
        char tline[64];
        snprintf(tline, sizeof(tline), "[%c] Theme %d", (st->theme == i ? 'x' : ' '), i + 1);
        int idx = 2 + i;
        if (selected == idx) attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(y + i, left, "%s", tline);
        if (selected == idx) attroff(COLOR_PAIR(3) | A_BOLD);
    }
    y += 4;

    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(y, left, "NICKNAME");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 2;

    char nline[64];
    snprintf(nline, sizeof(nline), "%s", st->nickname[0] ? st->nickname : "(not set)");
    int nick_idx = 5;
    if (selected == nick_idx) attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(y, left, "%s", nline);
    if (selected == nick_idx) attroff(COLOR_PAIR(3) | A_BOLD);

    refresh();
}

void edit_nickname(Settings *st) {
    int left = (COLS - 40) / 2;
    if (left < 0) left = 0;
    int y = LINES - 3;

    attron(COLOR_PAIR(6));
    mvprintw(y, left, "Enter nickname (max 31 chars): ");
    attroff(COLOR_PAIR(6));
    clrtoeol();

    echo();
    curs_set(1);

    char buf[32] = {0};

    // ncurses input
    mvgetnstr(y, left + 31, buf, 31);

    for (int i = 0; buf[i]; i++) {
        if ((unsigned char)buf[i] <= 32) { buf[i] = '\0'; break; }
    }
    strncpy(st->nickname, buf, sizeof(st->nickname) - 1);
    st->nickname[sizeof(st->nickname) - 1] = '\0';

    noecho();
    curs_set(0);
}

void draw_play_screen(int selected) {
    clear();
    timeout(200);
    int base_y = 2;

    draw_logo(base_y);
    int y = base_y + 12;

    attron(COLOR_PAIR(5) | A_BOLD);
    center_print(y, "PLAY");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 2;

    attron(COLOR_PAIR(6));
    center_print(y, "Enter: select  //  Q/ESC: back");
    attroff(COLOR_PAIR(6));
    y += 3;

    const char *items[] = {"HOST", "JOIN", "START"};
    int n = 3;

    for (int i = 0; i < n; i++) {
        int row = y + i * 2;
        if (i == selected) {
            attron(COLOR_PAIR(3) | A_BOLD);
            center_print(row, items[i]);
            attroff(COLOR_PAIR(3) | A_BOLD);
        } else {
            attron(COLOR_PAIR(2));
            center_print(row, items[i]);
            attroff(COLOR_PAIR(2));
        }
    }

    refresh(); // this should work so that it refreshes every 5-10 seconds for example, or, even better, if the user presses a key like 'r' for refresh
}

void draw_play_lobby(void) {
    clear();
    int base_y = 2;

    draw_logo(base_y);
    int y = base_y + 16;

    attron(COLOR_PAIR(5) | A_BOLD);
    center_print(y, "PLAY");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 2;

    attron(COLOR_PAIR(6));
    center_print(y, "Enter=JOIN  H=HOST  R=REFRESH  Q/ESC=BACK");
    attroff(COLOR_PAIR(6));
    y += 2;

    if (!net_ready) {
        attron(COLOR_PAIR(6));
        center_print(y + 2, "Not connected to server.");
        attroff(COLOR_PAIR(6));
        refresh();
        return;
    }

    int left = (COLS - 44) / 2;
    if (left < 0) left = 0;

    if (lobby.n == 0) {
        mvprintw(y + 2, left, "(no open games)");
        refresh();
        return;
    }

    int max_show = 12;
    int start = 0;

    lobby_list_y0 = y;
    lobby_list_x0 = left;
    lobby_list_visible = max_show;
    lobby_list_start = start;

    if (lobby.sel >= max_show) start = lobby.sel - (max_show - 1);
    if (start < 0) start = 0;

    for (int i = 0; i < max_show; i++) {
        int idx = start + i;
        if (idx >= lobby.n) break;

        LobbyGame *g = &lobby.g[idx];
        const char *st = (g->st == LOBBY_RUNNING) ? "RUNNING" : "OPEN";

        char row[128];
        snprintf(row, sizeof(row), "#%d  size=%d  P=%d  %s", g->id, g->size, g->players, st);

        if (idx == lobby.sel) attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(y + i, left, "%s", row);
        if (idx == lobby.sel) attroff(COLOR_PAIR(3) | A_BOLD);
    }

    refresh();
}

int host_popup(int *out_size, char *out_pref) {
    int size = 9;
    int sel = 2; // 0=B,1=W,2=R

    int w = 46;
    int h = 11;
    int y = (LINES - h) / 2;
    int x = (COLS - w) / 2;
    if (y < 0) y = 0;
    if (x < 0) x = 0;

    WINDOW *win = newwin(h, w, y, x);
    keypad(win, TRUE);

    while (1) {
        werase(win);
        box(win, 0, 0);

        wattron(win, A_BOLD);
        mvwprintw(win, 1, 2, "HOST GAME");
        wattroff(win, A_BOLD);

        mvwprintw(win, 3, 2, "Board size: %d", size);
        mvwprintw(win, 4, 2, "Use A/D or LEFT/RIGHT to change");

        const char *opt0 = "BLACK";
        const char *opt1 = "WHITE";
        const char *opt2 = "RANDOM";

        mvwprintw(win, 6, 2, "Host color:");
        for (int i = 0; i < 3; i++) {
            int yy = 7;
            int xx = 2 + i * 14;
            if (sel == i) wattron(win, A_REVERSE | A_BOLD);
            mvwprintw(win, yy, xx, "%s", (i==0?opt0:(i==1?opt1:opt2)));
            if (sel == i) wattroff(win, A_REVERSE | A_BOLD);
        }

        mvwprintw(win, 9, 2, "Enter=OK   Q/ESC=Cancel");

        wrefresh(win);

        int ch = wgetch(win);
        if (ch == 'q' || ch == 'Q' || ch == 27) {
            delwin(win);
            return 0;
        }
        if (ch == 10 || ch == KEY_ENTER) {
            *out_size = size;
            *out_pref = (sel==0?'B':(sel==1?'W':'R'));
            delwin(win);
            return 1;
        }

        // size: A/D or arrows
        if (ch == 'a' || ch == 'A' || ch == KEY_LEFT) {
            size -= 2;
            if (size < 7) size = 19;
        } else if (ch == 'd' || ch == 'D' || ch == KEY_RIGHT) {
            size += 2;
            if (size > 19) size = 7;
        }

        // color select: W/S or up/down
        if (ch == 'w' || ch == 'W' || ch == KEY_UP) {
            sel = (sel + 2) % 3;
        } else if (ch == 's' || ch == 'S' || ch == KEY_DOWN) {
            sel = (sel + 1) % 3;
        }
    }
}

void apply_theme(int theme) {
    if (!has_colors()) return;
    start_color();
    use_default_colors();

    if (theme == 0) {
        init_pair(1, COLOR_CYAN, -1);
        init_pair(3, COLOR_BLACK, COLOR_CYAN);
        init_pair(5, COLOR_MAGENTA, -1);
        init_pair(6, COLOR_YELLOW, -1);
    } else if (theme == 1) {
        init_pair(1, COLOR_GREEN, -1);
        init_pair(3, COLOR_BLACK, COLOR_GREEN);
        init_pair(5, COLOR_CYAN, -1);
        init_pair(6, COLOR_WHITE, -1);
    } else {
        init_pair(1, COLOR_WHITE, -1);
        init_pair(3, COLOR_BLACK, COLOR_WHITE);
        init_pair(5, COLOR_YELLOW, -1);
        init_pair(6, COLOR_CYAN, -1);
    }
}

static void draw_board_grid(int top, int left, int size) {
    const int cell_w = 3;     // "   "
    // board geometry:
    // width  = 1 + size*(cell_w) + size*(1 vertical) = 1 + size*(cell_w+1)
    // height = 1 + size*(1 content) + (size-1)*(1 separator) = 2*size
    // plus bottom border -> 2*size + 1
    const int board_h = 2*size + 1;
    const int board_w = 1 + size*(cell_w + 1);

    const char *TL="┌", *TR="┐", *BL="└", *BR="┘";
    const char *HZ="─", *VT="│";
    const char *TJ="┬", *BJ="┴", *LJ="├", *RJ="┤", *XJ="┼";

    // top border
    mvaddstr(top, left, TL);
    for (int x = 0; x < size; x++) {
        for (int k = 0; k < cell_w; k++) addstr(HZ);
        addstr(x == size-1 ? TR : TJ);
    }

    // rows
    for (int y = 0; y < size; y++) {
        int ry = top + 1 + y*2;

        // content line
        mvaddstr(ry, left, VT);
        for (int x = 0; x < size; x++) {
            int idx = y*size + x;

            // cell interior x origin (first char inside cell)
            int cx = left + 1 + x*(cell_w+1);

            // background/cursor highlight for whole cell
            if (x == cur_x && y == cur_y) attron(COLOR_PAIR(3) | A_BOLD);

            // cell fill (spaces)
            mvaddch(ry, cx + 0, ' ');
            mvaddch(ry, cx + 1, ' ');
            mvaddch(ry, cx + 2, ' ');

            // stone in middle
            if (g_board[idx] == 1) {
                // B
                attron(COLOR_PAIR(4) | A_BOLD);
                mvaddch(ry, cx + 1, 'B');
                attroff(COLOR_PAIR(4) | A_BOLD);
            } else if (g_board[idx] == 2) {
                // W
                attron(COLOR_PAIR(2) | A_BOLD);
                mvaddch(ry, cx + 1, 'W');
                attroff(COLOR_PAIR(2) | A_BOLD);
            }

            if (x == cur_x && y == cur_y) attroff(COLOR_PAIR(3) | A_BOLD);

            // right separator
            mvaddstr(ry, cx + cell_w, VT);
        }

        // separator line
        if (y != size-1) {
            int sy = ry + 1;
            mvaddstr(sy, left, LJ);
            for (int x = 0; x < size; x++) {
                for (int k = 0; k < cell_w; k++) addstr(HZ);
                addstr(x == size-1 ? RJ : XJ);
            }
        }
    }

    // bottom border
    int by = top + board_h - 1;
    mvaddstr(by, left, BL);
    for (int x = 0; x < size; x++) {
        for (int k = 0; k < cell_w; k++) addstr(HZ);
        addstr(x == size-1 ? BR : BJ);
    }

    // update mouse mapping origins for this grid:
    // content area starts at (top+1, left+1)
    game_inner_y0 = top + 1;
    game_inner_x0 = left + 1;
    game_box_top = top;
    game_box_left = left;
}

static void draw_top_bar(void) {
    int bm = black_secs / 60, bs = black_secs % 60;
    int wm = white_secs / 60, ws = white_secs % 60;

    attron(COLOR_PAIR(6));
    mvhline(0, 0, ' ', COLS);

    // score placeholders (score_b/score_w)
    mvprintw(0, 1,
        "Game #%d  You=%s  ToMove=%s   Score B %d - W %d   Time B %02d:%02d | W %02d:%02d",
        my_game_id, my_color,
        (g_to_move==0?"BLACK":"WHITE"),
        score_b, score_w,
        bm, bs, wm, ws
    );
    attroff(COLOR_PAIR(6));
}

static void draw_bottom_bar(void) {
    int y = LINES-1;
    attron(COLOR_PAIR(6));
    mvhline(y, 0, ' ', COLS);
    mvprintw(y, 1, "Mouse: click=select  double=place  Arrows/WASD move  Enter=place  P=pass  Q/ESC=exit");
    attroff(COLOR_PAIR(6));
}

void draw_game_screen(void) {
    clear();
    draw_top_bar();
    draw_bottom_bar();

    const int cell_w = 3;
    const int board_h = 2*my_game_size + 1;
    const int board_w = 1 + my_game_size*(cell_w + 1);

    int top = (LINES - board_h) / 2;
    int left = (COLS - board_w) / 2;

    // keep inside screen with top+bottom bars
    if (top < 1) top = 1;
    if (left < 0) left = 0;
    if (top + board_h >= LINES-1) top = 1;

    // optional: color frame/lines stronger
    attron(COLOR_PAIR(5) | A_BOLD);
    draw_board_grid(top, left, my_game_size);
    attroff(COLOR_PAIR(5) | A_BOLD);

    refresh();
}
