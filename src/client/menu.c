#include "net.h"
#include "lobby.h"
#include <sys/select.h>
#include <stdbool.h>
#include <string.h>
#include <ncurses.h>
#include <ctype.h>
#include <time.h>
#include <locale.h>

typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_SETTINGS = 1,
    SCREEN_PLAY = 2,
    SCREEN_WAIT = 3,
    SCREEN_GAME = 4
} Screen;


typedef struct {
    bool mouse_support;
    bool colours;
    int theme;               // 0..2
    char nickname[32];
} Settings;

#define BOARD_MAX_SIZE 19
static unsigned char g_board[BOARD_MAX_SIZE * BOARD_MAX_SIZE]; // 0 empty, 1 B, 2 W
static int g_to_move = 0; // 0 black, 1 white
static int cur_x = 0, cur_y = 0;
static Net net;
static Lobby lobby;
static int my_hosting = 0;
static int my_game_id = -1;
static int my_game_size = 0;
static char my_color[16] = "";
static int join_pending_size = 0;
static int net_ready = 0; // 1 if connected
static int syncing = 0; // 1 for being berween GAMES_BEGIN and GAMES_END
static int lobby_list_y0 = 0;
static int lobby_list_x0 = 0;
static int lobby_list_visible = 0;
static int lobby_list_start = 0;
static int main_menu_y0 = 0;
static int game_box_top=0, game_box_left=0;
static int game_inner_y0=0, game_inner_x0=0;
static int ui_enabled_colours = 1;
static time_t last_tick = 0;
static int black_secs = 10*60;
static int white_secs = 10*60;
static unsigned char prev_board[BOARD_MAX_SIZE * BOARD_MAX_SIZE];
static int prev_board_valid = 0;
static int score_b = 0; // captures by BLACK
static int score_w = 0; // captures by WHITE


static void init_ui(void) {
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

static void shutdown_ui(void) {
    endwin();
}

static void client_clear_board(int size) {
    int n = size * size;
    for (int i = 0; i < n; i++) g_board[i] = 0;
    g_to_move = 0;
    cur_x = cur_y = 0;
    prev_board_valid = 0;
    score_b = score_w = 0;
}

static int read_nav_key(int ch) {
    // Returns: -1 up, +1 down, 0 none, 10 enter, 27 esc/back, 100 left, 101 right
    if (ch == KEY_UP || ch == 'w' || ch == 'W') return -1;
    if (ch == KEY_DOWN || ch == 's' || ch == 'S') return +1;
    if (ch == KEY_LEFT || ch == 'a' || ch == 'A') return 100;
    if (ch == KEY_RIGHT || ch == 'd' || ch == 'D') return 101;
    if (ch == 10 || ch == KEY_ENTER) return 10;
    if (ch == 27) return 27; // ESC 
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

static void draw_wait_screen(void) {
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


static void draw_main_menu(int selected) {
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

static void draw_settings_screen(const Settings *st, int selected) {
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

static void edit_nickname(Settings *st) {
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

static void draw_play_screen(int selected) {
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

static int ensure_connected(void) {
    if (net_ready) return 1;

    if (net_connect(&net, "127.0.0.1", 1984) < 0) return 0;  // failure
    net_ready = 1;

    net_send_line(&net, "SUB");
    net_send_line(&net, "GAMES");
    return 1;
}

static void parse_server_line(const char *line, Screen *screen) {
    int gid=0;
    char col[16];

    if (strcmp(line, "GAMES_BEGIN") == 0) {
        lobby_clear(&lobby);
        syncing = 1;
        return;
    }
    if (strcmp(line, "GAMES_END") == 0) {
        syncing = 0;
        return;
    }

    int id=0,size=0,players=0;
    char st[32];

    if (sscanf(line, "GAME %d %d %d %31s", &id, &size, &players, st) == 4) {
        LobbyStatus s = (strcmp(st, "RUNNING") == 0) ? LOBBY_RUNNING : LOBBY_OPEN;
        lobby_upsert(&lobby, id, size, players, s);
        return;
    }

    if (sscanf(line, "EVENT GAME_CREATED %d %d", &id, &size) == 2) {
        lobby_upsert(&lobby, id, size, 1, LOBBY_OPEN);
        return;
    }
    if (sscanf(line, "EVENT GAME_STARTED %d", &id) == 1) {
        lobby_set_running(&lobby, id);
        return;
    }
    if (sscanf(line, "EVENT GAME_REMOVED %d", &id) == 1) {
        lobby_remove(&lobby, id);
        return;
    }

    if (sscanf(line, "HOSTED %d %15s", &gid, col) == 2) {
        my_hosting = 1;
        my_game_id = gid;
        strncpy(my_color, col, sizeof(my_color)-1);
        my_color[sizeof(my_color)-1] = '\0';
        // my_game_size ustawiasz w miejscu gdzie wysyłasz HOST (poniżej)
        return;
    }

    int id2, size2;
    if (sscanf(line, "START %d %d %15s", &id2, &size2, col) == 3) {
        my_game_id = id2;
        my_game_size = size2;
        strncpy(my_color, col, sizeof(my_color)-1);
        my_color[sizeof(my_color)-1] = '\0';

        my_hosting = 0;
        client_clear_board(size2);

        *screen = SCREEN_GAME;
        return;
    }



    if (strcmp(line, "OK CANCELLED") == 0) {
        my_hosting = 0;
        my_game_id = -1;
        my_game_size = 0;
        my_color[0] = '\0';
        return;
    }

    int bid;
    char tm[16];

    if (sscanf(line, "BOARD %d %15s", &bid, tm) == 2) {
        if (bid != my_game_id) return;

        int next_to_move = (strcmp(tm, "BLACK") == 0) ? 0 : 1;

        const char *p = strchr(line, ' ');
        if (!p) return;
        p = strchr(p + 1, ' ');
        if (!p) return;
        p = strchr(p + 1, ' ');
        if (!p) return;
        p++;

        int n = my_game_size * my_game_size;

        unsigned char newb[BOARD_MAX_SIZE * BOARD_MAX_SIZE];
        for (int i = 0; i < n; i++) {
            char c = p[i];
            if (c == '.') newb[i] = 0;
            else if (c == 'B') newb[i] = 1;
            else if (c == 'W') newb[i] = 2;
            else { n = i; break; }
        }

        if (prev_board_valid) {
            int removed_black = 0;
            int removed_white = 0;
            for (int i = 0; i < n; i++) {
                if (prev_board[i] == 1 && newb[i] == 0) removed_black++;
                if (prev_board[i] == 2 && newb[i] == 0) removed_white++;
            }

            // kto zrobił ostatni ruch?
            int last_player = 1 - next_to_move; // 0=BLACK,1=WHITE

            if (last_player == 0) score_b += removed_white;  // BLACK zbił WHITE
            else                 score_w += removed_black;  // WHITE zbił BLACK
        }

        // commit
        memcpy(g_board, newb, n);
        memcpy(prev_board, newb, n);
        prev_board_valid = 1;

        g_to_move = next_to_move;
        return;
    }
}

static void pump_network(Screen *screen) {
    if (!net_ready) return;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(net.fd, &rfds);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    int rc = select(net.fd + 1, &rfds, NULL, NULL, &tv);
    if (rc <= 0) return;

    if (FD_ISSET(net.fd, &rfds)) {
        int ok = net_recv_into_buffer(&net);
        if (ok <= 0) {
            net_close(&net);
            net_ready = 0;
            return;
        }

        char line[1024];
        while (net_next_line(&net, line, sizeof(line))) {
            parse_server_line(line, screen);
        }
    }
}

static void draw_play_lobby(void) {
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

static int host_popup(int *out_size, char *out_pref) {
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

static void draw_ascii_box(int y, int x, int h, int w) {
    // h,w >= 2
    mvaddch(y, x, '/');
    for (int i = 1; i < w-1; i++) addch('_');
    addch('\\');

    for (int r = 1; r < h-1; r++) {
        mvaddch(y+r, x, '|');
        mvaddch(y+r, x+w-1, '|');
    }

    mvaddch(y+h-1, x, '\\');
    for (int i = 1; i < w-1; i++) addch('_');
    addch('/');
}

static void apply_theme(int theme) {
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

static void draw_game_screen(void) {
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


int main() {
    Settings st = {.mouse_support = true, .colours = true, .theme = 0, .nickname = "u1"};
    Screen screen = SCREEN_MAIN;
    int sel_main = 0;
    int sel_settings = 0; // [0..5]
    int sel_play = 0;

    init_ui();

    apply_theme(st.theme);
    bool running = true;
    lobby_init(&lobby);
    net_ready = 0;
    syncing = 0;

    while (running) {
        if (screen == SCREEN_MAIN) {
            draw_main_menu(sel_main);
            int ch = getch();

            if (ch == KEY_MOUSE && st.mouse_support) {
                MEVENT ev;
                if (getmouse(&ev) == OK) {
                    if (ev.bstate & BUTTON1_CLICKED) {
                        // elementy są na: main_menu_y0 + i*2
                        for (int i = 0; i < 3; i++) {
                            if (ev.y == main_menu_y0 + i*2) {
                                sel_main = i;
                                break;
                            }
                        }
                    } else if (ev.bstate & BUTTON1_DOUBLE_CLICKED) {
                        for (int i = 0; i < 3; i++) {
                            if (ev.y == main_menu_y0 + i*2) {
                                sel_main = i;
                                if (sel_main == 0) screen = SCREEN_PLAY;
                                else if (sel_main == 1) screen = SCREEN_SETTINGS;
                                else running = false;
                                break;
                            }
                        }
                    }
                }
                continue;
            }

            int nav = read_nav_key(ch);

            if (nav == -1) sel_main = (sel_main + 2) % 3;
            else if (nav == +1) sel_main = (sel_main + 1) % 3;
            else if (nav == 10) {
                if (sel_main == 0) screen = SCREEN_PLAY;
                else if (sel_main == 1) screen = SCREEN_SETTINGS;
                else running = false;
            } else if (nav == 27) {
                running = false;
            }
        }
        else if (screen == SCREEN_SETTINGS) {
            draw_settings_screen(&st, sel_settings);
            int ch = getch();
            int nav = read_nav_key(ch);

            if (nav == 27) {
                screen = SCREEN_MAIN;
                continue;
            }

            if (nav == -1) sel_settings = (sel_settings + 5) % 6;
            else if (nav == +1) sel_settings = (sel_settings + 1) % 6;
            else if (ch == ' ') {
                // these arent working :)
                if (sel_settings == 0) st.mouse_support = !st.mouse_support;
                else if (sel_settings == 1) st.colours = !st.colours;
                else if (sel_settings >= 2 && sel_settings <= 4) st.theme = sel_settings - 2;
            } else if (nav == 10) {
                if (sel_settings == 5) edit_nickname(&st);
                else if (sel_settings == 0) st.mouse_support = !st.mouse_support;
                else if (sel_settings == 1) st.colours = !st.colours;
                else if (sel_settings >= 2 && sel_settings <= 4) st.theme = sel_settings - 2;
            }
        }
        else if (screen == SCREEN_PLAY) {
            if (!ensure_connected()) {}
            if (my_hosting) { screen = SCREEN_WAIT; continue; }

            static int tick = 0;
            timeout(200);
            pump_network(&screen);
            tick++;
            if (tick >= 50) {      // 50 * 200ms = 10s
                net_send_line(&net, "GAMES");
                tick = 0;
            }

            draw_play_lobby();

            int ch = getch();
            if (ch == ERR) continue;

            if (ch == KEY_MOUSE && st.mouse_support) {
                MEVENT ev;
                if (getmouse(&ev) == OK) {
                    if (ev.bstate & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED)) {
                        int row = ev.y - lobby_list_y0;
                        if (row >= 0 && row < lobby_list_visible) {
                            int idx = lobby_list_start + row;
                            if (idx >= 0 && idx < lobby.n) {
                                lobby.sel = idx;

                                if (ev.bstate & BUTTON1_DOUBLE_CLICKED) {
                                    char cmd[64];
                                    snprintf(cmd, sizeof(cmd), "JOIN %d", lobby.g[lobby.sel].id);
                                    net_send_line(&net, cmd);
                                }
                            }
                        }
                    }
                }
                continue; // po kliknięciu od razu kolejna iteracja
            }


            int nav = read_nav_key(ch);

            if (nav == 27) {
                screen = SCREEN_MAIN;
                timeout(-1);
                continue;
            }

            if (nav == -1 && lobby.n > 0) {
                if (lobby.sel > 0) lobby.sel--;
            } else if (nav == +1 && lobby.n > 0) {
                if (lobby.sel < lobby.n - 1) lobby.sel++;
            } else if (nav == 10 && lobby.n > 0) {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "JOIN %d", lobby.g[lobby.sel].id);
                net_send_line(&net, cmd);
                tick = 0;
            } else if (ch == 'r' || ch == 'R') {
                net_send_line(&net, "GAMES");
            } else if (ch == 'h' || ch == 'H') {
                int size;
                char pref;
                if (host_popup(&size, &pref)) {
                    char cmd[64];
                    snprintf(cmd, sizeof(cmd), "HOST %d %c", size, pref);
                    my_game_size = size;
                    net_send_line(&net, cmd);
                }
            }
        } else if (screen == SCREEN_WAIT) {
            timeout(200);
            pump_network(&screen);
            draw_wait_screen();

            int ch = getch();
            if (ch == ERR) continue;

            if (ch == 'r' || ch == 'R') {
                net_send_line(&net, "GAMES");
            } else if (ch == 'c' || ch == 'C') {
                net_send_line(&net, "CANCEL");
                // server odeśle OK CANCELLED + EVENT
            } else {
                int nav = read_nav_key(ch);
                if (nav == 27) { // ESC
                    net_send_line(&net, "CANCEL");
                    screen = SCREEN_PLAY; // wróci po OK CANCELLED albo od razu
                }
            }
        } else if (screen == SCREEN_GAME) {
            timeout(200);
            pump_network(&screen);

            time_t now = time(NULL);
            if (last_tick == 0) last_tick = now;
            int dt = (int)(now - last_tick);
            if (dt > 0) {
                last_tick = now;
                if (g_to_move == 0) black_secs -= dt;
                else white_secs -= dt;
                if (black_secs < 0) black_secs = 0;
                if (white_secs < 0) white_secs = 0;
            }

            draw_game_screen();

            int ch = getch();
            if (ch == ERR) continue;

            if (ch == KEY_MOUSE && st.mouse_support) {
                MEVENT ev;
                if (getmouse(&ev) == OK) {
                    if (ev.bstate & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED)) {
                        int ry = ev.y - game_inner_y0;
                        int rx = ev.x - game_inner_x0;

                        // content lines are even (0,2,4...) because separators are odd
                        if (ry >= 0 && rx >= 0 && (ry % 2 == 0)) {
                            int y = ry / 2;

                            // each cell is (cell_w=3) + 1 separator
                            int x = rx / 4;

                            if (x >= 0 && x < my_game_size && y >= 0 && y < my_game_size) {
                                cur_x = x;
                                cur_y = y;

                                if (ev.bstate & BUTTON1_DOUBLE_CLICKED) {
                                    char cmd[64];
                                    snprintf(cmd, sizeof(cmd), "MOVE %d %d %d", my_game_id, cur_x, cur_y);
                                    net_send_line(&net, cmd);
                                }
                            }
                        }
                    }
                }
                continue;
            }


            int nav = read_nav_key(ch);

            if (nav == 27 || ch == 'q' || ch == 'Q') {
                screen = SCREEN_PLAY;
                timeout(200);
                continue;
            }

            if (nav == -1 && cur_y > 0) cur_y--;
            else if (nav == +1 && cur_y < my_game_size - 1) cur_y++;
            else if (nav == 100 && cur_x > 0) cur_x--;
            else if (nav == 101 && cur_x < my_game_size - 1) cur_x++;
            else if (nav == 10) {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "MOVE %d %d %d", my_game_id, cur_x, cur_y);
                net_send_line(&net, cmd);
            } else if (ch == 'p' || ch == 'P') {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "PASS %d", my_game_id);
                net_send_line(&net, cmd);
            }
        }
    }

    shutdown_ui();
    return 0;
}
