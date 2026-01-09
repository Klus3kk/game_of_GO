#include "net.h"
#include "lobby.h"
#include <sys/select.h>
#include <stdbool.h>
#include <string.h>
#include <ncurses.h>
#include <ctype.h>

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

static Net net;
static Lobby lobby;
static int net_ready = 0; // 1 if connected
static int syncing = 0; // 1 for being berween GAMES_BEGIN and GAMES_END

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
    int base_y = 2;

    draw_logo(base_y);
    int y = base_y + 16;

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

static void parse_server_line(const char *line) {
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
}

static void pump_network(void) {
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
            parse_server_line(line);
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

int main(void) {
    Settings st = {.mouse_support = true, .colours = true, .theme = 0, .nickname = "u1"};
    Screen screen = SCREEN_MAIN;
    int sel_main = 0;
    int sel_settings = 0; // [0..5]
    int sel_play = 0;

    init_ui();

    bool running = true;
    lobby_init(&lobby);
    net_ready = 0;
    syncing = 0;

    while (running) {
        if (screen == SCREEN_MAIN) {
            draw_main_menu(sel_main);
            int ch = getch();
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

            timeout(0); 
            pump_network();
            draw_play_lobby();

            int ch = getch();
            if (ch == ERR) continue;

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
            } else if (ch == 'r' || ch == 'R') {
                net_send_line(&net, "GAMES");
            } else if (ch == 'h' || ch == 'H') {
                // for test its 9x9 for now
                net_send_line(&net, "HOST 9");
            }
        }
    }

    shutdown_ui();
    return 0;
}
