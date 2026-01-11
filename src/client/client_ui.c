#include "client_ui.h"
#include "client_state.h"
#include <stdio.h>
#include <string.h>
#include <ncurses.h>
#include <locale.h>

static void draw_logo(int y)
{
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
        NULL};

    int width = 0;
    for (int i = 0; logo[i]; i++)
    {
        int len = (int)strlen(logo[i]);
        if (len > width)
            width = len;
    }

    int x = (COLS - width) / 2;
    if (x < 0)
        x = 0;

    attron(COLOR_PAIR(1) | A_BOLD);
    for (int i = 0; logo[i]; i++)
    {
        mvprintw(y + i, x, "%s", logo[i]);
    }
    attroff(COLOR_PAIR(1) | A_BOLD);
}

static void center_print(int y, const char *s)
{
    int x = (COLS - (int)strlen(s)) / 2;
    mvprintw(y, (x < 0 ? 0 : x), "%s", s);
}

void init_ui(void)
{
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors())
    {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_CYAN, -1);          // title
        init_pair(2, COLOR_WHITE, -1);         // normal
        init_pair(3, COLOR_BLACK, COLOR_CYAN); // highlight
        init_pair(4, COLOR_GREEN, -1);         // ok/selected
        init_pair(5, COLOR_MAGENTA, -1);       // section
        init_pair(6, COLOR_YELLOW, -1);        // hints
        init_pair(7, COLOR_CYAN, -1);          // info panel border
    }
    mousemask(BUTTON1_CLICKED | BUTTON1_PRESSED | BUTTON1_RELEASED, NULL);

    mouseinterval(0);
}

void shutdown_ui(void)
{
    endwin();
}

int read_nav_key(int ch)
{
    if (ch == KEY_UP || ch == 'w' || ch == 'W')
        return -1;
    if (ch == KEY_DOWN || ch == 's' || ch == 'S')
        return +1;
    if (ch == KEY_LEFT || ch == 'a' || ch == 'A')
        return 100;
    if (ch == KEY_RIGHT || ch == 'd' || ch == 'D')
        return 101;
    if (ch == 10 || ch == KEY_ENTER)
        return 10;
    if (ch == 27)
        return 27;
    return 0;
}

void draw_wait_screen(void)
{
    clear();
    int base_y = 2;
    draw_logo(base_y);

    int y = base_y + 16;

    attron(COLOR_PAIR(5) | A_BOLD);
    center_print(y, "WAITING FOR OPPONENT");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 2;

    char info[128];
    snprintf(info, sizeof(info), "Game #%d  size=%d  you=%s", my_game_id, my_game_size, my_color[0] ? my_color : "?");
    attron(COLOR_PAIR(2));
    center_print(y, info);
    attroff(COLOR_PAIR(2));
    y += 2;

    attron(COLOR_PAIR(6));
    center_print(y, "C=CANCEL   R=REFRESH   Q/ESC=BACK");
    attroff(COLOR_PAIR(6));

    refresh();
}

void draw_main_menu(int selected)
{
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

    for (int i = 0; i < n; i++)
    {
        int y = menu_y + i * 2;
        if (i == selected)
        {
            attron(COLOR_PAIR(3) | A_BOLD);
            center_print(y, items[i]);
            attroff(COLOR_PAIR(3) | A_BOLD);
        }
        else
        {
            attron(COLOR_PAIR(2));
            center_print(y, items[i]);
            attroff(COLOR_PAIR(2));
        }
    }

    refresh();
}

void draw_settings_screen(const Settings *st, int selected)
{
    clear();
    
    int base_y = 1;
    
    // Logo zawsze na górze
    draw_logo(base_y);
    
    int y = base_y + 14;

    attron(COLOR_PAIR(5) | A_BOLD);
    center_print(y, "SETTINGS");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 1;

    attron(COLOR_PAIR(6));
    center_print(y, "Space/Enter=select | Q/ESC=back");
    attroff(COLOR_PAIR(6));
    y += 2;

    // BASIC
    attron(COLOR_PAIR(5) | A_BOLD);
    center_print(y, "BASIC");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 1;

    const char *mouse_line = st->mouse_support ? "[x] Mouse Support" : "[ ] Mouse Support";

    if (selected == 0) attron(COLOR_PAIR(3) | A_BOLD);
    center_print(y, mouse_line);
    if (selected == 0) attroff(COLOR_PAIR(3) | A_BOLD);

    y += 2;

    // THEMES
    attron(COLOR_PAIR(5) | A_BOLD);
    center_print(y, "THEMES");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 1;

    for (int i = 0; i < 3; i++)
    {
        char tline[64];
        snprintf(tline, sizeof(tline), "[%c] Theme %d", (st->theme == i ? 'x' : ' '), i + 1);

        int idx = 1 + i;
        if (selected == idx) attron(COLOR_PAIR(3) | A_BOLD);
        center_print(y + i, tline);
        if (selected == idx) attroff(COLOR_PAIR(3) | A_BOLD);
    }

    y += 4;

    // NICKNAME
    attron(COLOR_PAIR(5) | A_BOLD);
    center_print(y, "NICKNAME");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 1;

    char nline[64];
    snprintf(nline, sizeof(nline), "%s", st->nickname[0] ? st->nickname : "(not set)");

    int nick_idx = 4;
    if (selected == nick_idx) attron(COLOR_PAIR(3) | A_BOLD);
    center_print(y, nline);
    if (selected == nick_idx) attroff(COLOR_PAIR(3) | A_BOLD);

    refresh();
}

void edit_nickname(Settings *st)
{
    int left = (COLS - 40) / 2;
    if (left < 0)
        left = 0;
    int y = LINES - 3;

    attron(COLOR_PAIR(6));
    mvprintw(y, left, "Enter nickname (max 31 chars): ");
    attroff(COLOR_PAIR(6));
    clrtoeol();

    echo();
    curs_set(1);

    char buf[32] = {0};

    mvgetnstr(y, left + 31, buf, 31);

    for (int i = 0; buf[i]; i++)
    {
        if ((unsigned char)buf[i] <= 32)
        {
            buf[i] = '\0';
            break;
        }
    }
    strncpy(st->nickname, buf, sizeof(st->nickname) - 1);
    st->nickname[sizeof(st->nickname) - 1] = '\0';

    noecho();
    curs_set(0);
}

void draw_play_screen(int selected)
{
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

    for (int i = 0; i < n; i++)
    {
        int row = y + i * 2;
        if (i == selected)
        {
            attron(COLOR_PAIR(3) | A_BOLD);
            center_print(row, items[i]);
            attroff(COLOR_PAIR(3) | A_BOLD);
        }
        else
        {
            attron(COLOR_PAIR(2));
            center_print(row, items[i]);
            attroff(COLOR_PAIR(2));
        }
    }

    refresh();
}

void draw_play_lobby(void)
{
    clear();

    int base_y = 2;
    draw_logo(base_y);
    
    int y = base_y + 14;

    attron(COLOR_PAIR(5) | A_BOLD);
    center_print(y, "PLAY");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 2;

    attron(COLOR_PAIR(6));
    center_print(y, "Enter=JOIN  H=HOST  R=REFRESH  Q/ESC=BACK");
    attroff(COLOR_PAIR(6));
    y += 2;

    if (!net_ready)
    {
        attron(COLOR_PAIR(6));
        center_print(y + 2, "Not connected to server.");
        attroff(COLOR_PAIR(6));
        refresh();
        return;
    }

    if (lobby.n == 0) {
        center_print(y + 2, "(no open games)");
        refresh();
        return;
    }

    int max_show = 12;
    int start = 0;

    lobby_list_y0 = y;
    lobby_list_visible = max_show;
    lobby_list_start = start;

    if (lobby.sel >= max_show)
        start = lobby.sel - (max_show - 1);
    if (start < 0)
        start = 0;

    for (int i = 0; i < max_show; i++)
    {
        int idx = start + i;
        if (idx >= lobby.n)
            break;

        LobbyGame *g = &lobby.g[idx];
        const char *st = (g->st == LOBBY_RUNNING) ? "RUNNING" : "OPEN";

        char row[128];
        snprintf(row, sizeof(row), "#%d  size=%d  P=%d  %s", g->id, g->size, g->players, st);

        if (idx == lobby.sel)
            attron(COLOR_PAIR(3) | A_BOLD);
        int x = (COLS - (int)strlen(row)) / 2;
        if (x < 0)
            x = 0;
        mvprintw(y + i, x, "%s", row);
        if (idx == lobby.sel)
            attroff(COLOR_PAIR(3) | A_BOLD);
    }

    refresh();
}

int host_popup(int *out_size, char *out_pref, char *out_game_name)
{
    int size = 9;
    int sel = 2; // 0=B, 1=W, 2=R
    char game_name[64] = "";

    int w = 50;
    int h = 15;
    int y = (LINES - h) / 2;
    int x = (COLS - w) / 2;
    if (y < 0) y = 0;
    if (x < 0) x = 0;

    clear();
    refresh();

    WINDOW *win = newwin(h, w, y, x);
    keypad(win, TRUE);

    while (1)
    {
        werase(win);
        box(win, 0, 0);

        wattron(win, A_BOLD | COLOR_PAIR(5));
        mvwprintw(win, 1, 2, "HOST GAME");
        wattroff(win, A_BOLD | COLOR_PAIR(5));

        mvwprintw(win, 3, 2, "Board size: %d", size);
        wattron(win, COLOR_PAIR(6));
        mvwprintw(win, 4, 2, "Use A/D or LEFT/RIGHT to change");
        wattroff(win, COLOR_PAIR(6));

        const char *opt0 = "BLACK";
        const char *opt1 = "WHITE";
        const char *opt2 = "RANDOM";

        mvwprintw(win, 6, 2, "Host color:");
        for (int i = 0; i < 3; i++)
        {
            int yy = 7;
            int xx = 2 + i * 14;
            if (sel == i)
                wattron(win, A_REVERSE | A_BOLD);
            mvwprintw(win, yy, xx, "%s", (i == 0 ? opt0 : (i == 1 ? opt1 : opt2)));
            if (sel == i)
                wattroff(win, A_REVERSE | A_BOLD);
        }

        mvwprintw(win, 9, 2, "Game name:");
        wattron(win, COLOR_PAIR(3));
        mvwprintw(win, 10, 2, "%s", game_name[0] ? game_name : "<default: your_nick game>");
        wattroff(win, COLOR_PAIR(3));

        wattron(win, COLOR_PAIR(6));
        mvwprintw(win, 12, 2, "N=Edit name  Enter=OK  Q/ESC=Cancel");
        wattroff(win, COLOR_PAIR(6));

        wrefresh(win);

        int ch = wgetch(win);
        
        if (ch == 'q' || ch == 'Q' || ch == 27)
        {
            delwin(win);
            clear();
            refresh();
            return 0;
        }
        
        if (ch == 10 || ch == KEY_ENTER)
        {
            *out_size = size;
            *out_pref = (sel == 0 ? 'B' : (sel == 1 ? 'W' : 'R'));
            
            // Skopiuj nazwę jeśli została ustawiona
            if (game_name[0]) {
                strncpy(out_game_name, game_name, 63);
                out_game_name[63] = '\0';
            } else {
                out_game_name[0] = '\0';
            }
            
            delwin(win);
            clear();
            refresh();
            return 1;
        }

        if (ch == 'n' || ch == 'N')
        {
            // Edycja nazwy gry
            werase(win);
            box(win, 0, 0);
            
            wattron(win, A_BOLD);
            mvwprintw(win, 1, 2, "EDIT GAME NAME");
            wattroff(win, A_BOLD);
            
            mvwprintw(win, 3, 2, "Enter game name (max 30 chars):");
            mvwprintw(win, 4, 2, "Leave empty for default");
            
            echo();
            curs_set(1);
            
            char buf[64] = {0};
            mvwgetnstr(win, 6, 2, buf, 30);
            
            // Usuń białe znaki z nazwy
            int j = 0;
            for (int i = 0; buf[i] && j < 30; i++) {
                if (buf[i] > 32) {
                    game_name[j++] = buf[i];
                }
            }
            game_name[j] = '\0';
            
            noecho();
            curs_set(0);
            continue;
        }

        if (ch == 'a' || ch == 'A' || ch == KEY_LEFT)
        {
            size -= 2;
            if (size < 7) size = 19;
        }
        else if (ch == 'd' || ch == 'D' || ch == KEY_RIGHT)
        {
            size += 2;
            if (size > 19) size = 7;
        }

        if (ch == 'w' || ch == 'W' || ch == KEY_UP)
        {
            sel = (sel + 2) % 3;
        }
        else if (ch == 's' || ch == 'S' || ch == KEY_DOWN)
        {
            sel = (sel + 1) % 3;
        }
    }
}

void apply_theme(int theme)
{
    if (!has_colors())
        return;
    start_color();
    use_default_colors();

    if (theme == 0)
    {
        init_pair(1, COLOR_CYAN, -1);
        init_pair(3, COLOR_BLACK, COLOR_CYAN);
        init_pair(5, COLOR_MAGENTA, -1);
        init_pair(6, COLOR_YELLOW, -1);
        init_pair(7, COLOR_CYAN, -1);
    }
    else if (theme == 1)
    {
        init_pair(1, COLOR_GREEN, -1);
        init_pair(3, COLOR_BLACK, COLOR_GREEN);
        init_pair(5, COLOR_CYAN, -1);
        init_pair(6, COLOR_WHITE, -1);
        init_pair(7, COLOR_GREEN, -1);
    }
    else
    {
        init_pair(1, COLOR_WHITE, -1);
        init_pair(3, COLOR_BLACK, COLOR_WHITE);
        init_pair(5, COLOR_YELLOW, -1);
        init_pair(6, COLOR_CYAN, -1);
        init_pair(7, COLOR_WHITE, -1);
    }
}

static void draw_board_grid(int top, int left, int size, int cell_w)
{
    const int board_h = 2 * size + 1;
    const int board_w = 1 + size*(cell_w + 1);

    const char *TL = "┌", *TR = "┐", *BL = "└", *BR = "┘";
    const char *HZ = "─", *VT = "│";
    const char *TJ = "┬", *BJ = "┴", *LJ = "├", *RJ = "┤", *XJ = "┼";

    attron(COLOR_PAIR(7));
    
    // Zewnętrzna ramka dookoła całej planszy
    mvaddch(top - 1, left - 1, ACS_ULCORNER);
    mvaddch(top - 1, left + board_w, ACS_URCORNER);
    mvaddch(top + board_h, left - 1, ACS_LLCORNER);
    mvaddch(top + board_h, left + board_w, ACS_LRCORNER);
    
    for (int i = 0; i < board_w; i++) {
        mvaddch(top - 1, left + i, ACS_HLINE);
        mvaddch(top + board_h, left + i, ACS_HLINE);
    }
    
    for (int i = 0; i < board_h; i++) {
        mvaddch(top + i, left - 1, ACS_VLINE);
        mvaddch(top + i, left + board_w, ACS_VLINE);
    }
    
    attroff(COLOR_PAIR(7));
    
    // Siatka wewnętrzna
    attron(COLOR_PAIR(2));
    
    mvaddstr(top, left, TL);
    for (int x = 0; x < size; x++)
    {
        for (int k = 0; k < cell_w; k++)
            addstr(HZ);
        addstr(x == size - 1 ? TR : TJ);
    }

    for (int y = 0; y < size; y++)
    {
        int ry = top + 1 + y * 2;

        mvaddstr(ry, left, VT);
        for (int x = 0; x < size; x++)
        {
            int idx = y * size + x;
            int cx = left + 1 + x * (cell_w + 1);

            if (x == cur_x && y == cur_y)
            {
                attron(COLOR_PAIR(3) | A_BOLD);
                for (int k = 0; k < cell_w; k++)
                    mvaddch(ry, cx + k, ' ');
            }
            else
            {
                for (int k = 0; k < cell_w; k++)
                    mvaddch(ry, cx + k, ' ');
            }

            if (g_board[idx] == 1)
            {
                attron(COLOR_PAIR(4) | A_BOLD);
                mvaddch(ry, cx + 1, 'B');
                attroff(COLOR_PAIR(4) | A_BOLD);
            }
            else if (g_board[idx] == 2)
            {
                attron(COLOR_PAIR(2) | A_BOLD);
                mvaddch(ry, cx + 1, 'W');
                attroff(COLOR_PAIR(2) | A_BOLD);
            }

            if (x == cur_x && y == cur_y)
                attroff(COLOR_PAIR(3) | A_BOLD);

            attron(COLOR_PAIR(2));
            mvaddstr(ry, cx + cell_w, VT);
        }

        if (y != size - 1)
        {
            int sy = ry + 1;
            mvaddstr(sy, left, LJ);
            for (int x = 0; x < size; x++)
            {
                for (int k = 0; k < cell_w; k++)
                    addstr(HZ);
                addstr(x == size - 1 ? RJ : XJ);
            }
        }
    }

    int by = top + board_h - 1;
    mvaddstr(by, left, BL);
    for (int x = 0; x < size; x++)
    {
        for (int k = 0; k < cell_w; k++)
            addstr(HZ);
        addstr(x == size - 1 ? BR : BJ);
    }
    
    attroff(COLOR_PAIR(2));

    game_inner_y0 = top + 1;
    game_inner_x0 = left + 1;
    game_box_top = top;
    game_box_left = left;
}

static void draw_info_panel(int panel_x, int panel_y, int panel_w, int panel_h)
{
    // Rysuj ramkę panelu
    attron(COLOR_PAIR(7));
    
    mvaddch(panel_y, panel_x, ACS_ULCORNER);
    mvaddch(panel_y, panel_x + panel_w - 1, ACS_URCORNER);
    mvaddch(panel_y + panel_h - 1, panel_x, ACS_LLCORNER);
    mvaddch(panel_y + panel_h - 1, panel_x + panel_w - 1, ACS_LRCORNER);
    
    for (int i = 1; i < panel_w - 1; i++) {
        mvaddch(panel_y, panel_x + i, ACS_HLINE);
        mvaddch(panel_y + panel_h - 1, panel_x + i, ACS_HLINE);
    }
    
    for (int i = 1; i < panel_h - 1; i++) {
        mvaddch(panel_y + i, panel_x, ACS_VLINE);
        mvaddch(panel_y + i, panel_x + panel_w - 1, ACS_VLINE);
    }
    
    attroff(COLOR_PAIR(7));

    int y = panel_y + 1;
    int x = panel_x + 2;
    
    // Nagłówek
    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(y, x, "GAME INFO");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 2;
    
    // You
    attron(COLOR_PAIR(6));
    mvprintw(y++, x, "You:");
    attroff(COLOR_PAIR(6));
    attron(A_BOLD);
    mvprintw(y++, x, "  %s", my_color[0] ? my_color : "?");
    attroff(A_BOLD);
    y++;
    
    // To Move
    attron(COLOR_PAIR(6));
    mvprintw(y++, x, "To Move:");
    attroff(COLOR_PAIR(6));
    attron(COLOR_PAIR(4) | A_BOLD);
    mvprintw(y++, x, "  %s", (g_to_move == 0 ? "BLACK" : "WHITE"));
    attroff(COLOR_PAIR(4) | A_BOLD);
    y += 2;
    
    // BLACK
    attron(COLOR_PAIR(4) | A_BOLD);
    mvprintw(y++, x, "BLACK:");
    attroff(COLOR_PAIR(4) | A_BOLD);
    
    mvprintw(y++, x, "  %s", black_nick[0] ? black_nick : "?");
    
    int bm = black_secs / 60, bs = black_secs % 60;
    attron(COLOR_PAIR(6));
    mvprintw(y, x, "  Time:");
    attroff(COLOR_PAIR(6));
    mvprintw(y++, x + 8, "%02d:%02d", bm, bs);
    
    attron(COLOR_PAIR(6));
    mvprintw(y, x, "  Caps:");
    attroff(COLOR_PAIR(6));
    mvprintw(y++, x + 8, "%d", score_b);
    y += 2;
    
    // WHITE
    attron(A_BOLD);
    mvprintw(y++, x, "WHITE:");
    attroff(A_BOLD);
    
    mvprintw(y++, x, "  %s", white_nick[0] ? white_nick : "?");
    
    int wm = white_secs / 60, ws = white_secs % 60;
    attron(COLOR_PAIR(6));
    mvprintw(y, x, "  Time:");
    attroff(COLOR_PAIR(6));
    mvprintw(y++, x + 8, "%02d:%02d", wm, ws);
    
    attron(COLOR_PAIR(6));
    mvprintw(y, x, "  Caps:");
    attroff(COLOR_PAIR(6));
    mvprintw(y++, x + 8, "%d", score_w);
}

void draw_game_screen(void)
{
    clear();

    int cell_w = 3;
    int board_w = 1 + my_game_size * (cell_w + 1);
    const int board_h = 2 * my_game_size + 1;
    
    // Panel info - taki sam jak plansza
    int panel_w = 24;
    int panel_h = board_h + 2; // Dopasuj do wysokości planszy
    
    // Dostosowanie do małego okna
    int min_width_needed = board_w + panel_w + 6;
    
    if (min_width_needed > COLS)
    {
        cell_w = 1;
        board_w = 1 + my_game_size * (cell_w + 1);
        min_width_needed = board_w + panel_w + 6;
        
        if (min_width_needed > COLS) {
            panel_w = COLS - board_w - 6;
            if (panel_w < 18) panel_w = 18;
        }
    }

    int total_w = board_w + panel_w + 4;
    int start_x = (COLS - total_w) / 2;
    if (start_x < 1) start_x = 1;
    
    // Zarezerwuj miejsce na dolną linijkę
    int available_h = LINES - 1;
    int start_y = (available_h - (board_h + 2)) / 2;
    if (start_y < 1) start_y = 1;
    
    // Rysuj planszę z ramką
    draw_board_grid(start_y + 1, start_x + 1, my_game_size, cell_w);
    
    // Rysuj panel info na tej samej wysokości co plansza
    int panel_x = start_x + board_w + 4;
    int panel_y = start_y;
    
    if (panel_x + panel_w < COLS) {
        draw_info_panel(panel_x, panel_y, panel_w, panel_h);
    }
    
    // Dolna wskazówka zawsze widoczna
    attron(COLOR_PAIR(6));
    mvprintw(LINES - 1, 1, "Arrows/WASD: move | Enter: place | P: pass | Q/ESC: exit");
    attroff(COLOR_PAIR(6));

    refresh();
}

void draw_gameover_screen(void)
{
    clear();
    
    // Dostosowanie do rozmiaru okna
    int compact = (LINES < 30) ? 1 : 0;
    int base_y = compact ? 1 : 2;
    
    if (!compact) {
        draw_logo(base_y);
        base_y += 14;
    }

    int y = base_y;

    attron(COLOR_PAIR(5) | A_BOLD);
    center_print(y, "GAME OVER");
    attroff(COLOR_PAIR(5) | A_BOLD);
    y += 2;

    char line1[128];
    snprintf(line1, sizeof(line1), "Game #%d", gameover_id);
    attron(COLOR_PAIR(2));
    center_print(y, line1);
    attroff(COLOR_PAIR(2));
    y += compact ? 1 : 2;

    char line2[128];
    snprintf(line2, sizeof(line2), "Winner: %s", gameover_winner[0] ? gameover_winner : "?");
    attron(COLOR_PAIR(4) | A_BOLD);
    center_print(y, line2);
    attroff(COLOR_PAIR(4) | A_BOLD);
    y += 1;

    if (my_color[0] && gameover_winner[0])
    {
        int you_win = (strcmp(my_color, gameover_winner) == 0);
        attron(COLOR_PAIR(you_win ? 4 : 2) | A_BOLD);
        center_print(y, you_win ? "YOU WIN" : "YOU LOSE");
        attroff(COLOR_PAIR(you_win ? 4 : 2) | A_BOLD);
        y += compact ? 1 : 2;
    }
    else
    {
        y += compact ? 1 : 2;
    }

    char line3[128];
    snprintf(line3, sizeof(line3), "Reason: %s", gameover_reason[0] ? gameover_reason : "?");
    attron(COLOR_PAIR(2));
    center_print(y, line3);
    attroff(COLOR_PAIR(2));
    y += compact ? 2 : 3;

    attron(COLOR_PAIR(6));
    center_print(y, "Press any key to return to PLAY");
    attroff(COLOR_PAIR(6));

    refresh();
}