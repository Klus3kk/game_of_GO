// gcc client.c client_state.c client_ui.c client_net.c client_proto.c net.c lobby.c -lncurses -o client
#include "client_types.h"
#include "client_state.h"
#include "client_ui.h"
#include "client_net.h"
#include <stdbool.h>
#include <string.h>
#include <ncurses.h>
#include <time.h>
#include <stdio.h>

void reset_game_state(void) {
    // Reset timery
    black_secs = 10 * 60;
    white_secs = 10 * 60;
    last_tick = 0;
    
    // Reset scores
    score_b = 0;
    score_w = 0;
    
    // Reset board state
    prev_board_valid = 0;
    
    // Reset kursor
    cur_x = 0;
    cur_y = 0;
}

int main()
{
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

    while (running)
    {
        if (screen == SCREEN_MAIN)
        {
            draw_main_menu(sel_main);
            int ch = getch();

            if (ch == KEY_MOUSE && st.mouse_support)
            {
                MEVENT ev;
                if (getmouse(&ev) == OK)
                {
                    if (ev.bstate & BUTTON1_CLICKED)
                    {
                        // elementy są na: main_menu_y0 + i*2
                        for (int i = 0; i < 3; i++)
                        {
                            if (ev.y == main_menu_y0 + i * 2)
                            {
                                sel_main = i;
                                break;
                            }
                        }
                    }
                    else if (ev.bstate & BUTTON1_DOUBLE_CLICKED)
                    {
                        for (int i = 0; i < 3; i++)
                        {
                            if (ev.y == main_menu_y0 + i * 2)
                            {
                                sel_main = i;
                                if (sel_main == 0)
                                    screen = SCREEN_PLAY;
                                else if (sel_main == 1)
                                    screen = SCREEN_SETTINGS;
                                else
                                    running = false;
                                break;
                            }
                        }
                    }
                }
                continue;
            }

            int nav = read_nav_key(ch);

            if (nav == -1)
                sel_main = (sel_main + 2) % 3;
            else if (nav == +1)
                sel_main = (sel_main + 1) % 3;
            else if (nav == 10)
            {
                if (sel_main == 0)
                    screen = SCREEN_PLAY;
                else if (sel_main == 1)
                    screen = SCREEN_SETTINGS;
                else
                    running = false;
            }
            else if (nav == 27)
            {
                running = false;
            }
        }
        else if (screen == SCREEN_SETTINGS)
        {
            draw_settings_screen(&st, sel_settings);
            int ch = getch();
            int nav = read_nav_key(ch);

            if (nav == 27)
            {
                screen = SCREEN_MAIN;
                continue;
            }

            if (nav == -1)
                sel_settings = (sel_settings + 4) % 5;
            else if (nav == +1)
                sel_settings = (sel_settings + 1) % 5;
            else if (ch == ' ')
            {
                if (sel_settings == 0) {
                    st.mouse_support = !st.mouse_support;
                } else if (sel_settings >= 1 && sel_settings <= 3) {
                    st.theme = sel_settings - 1;
                    apply_theme(st.theme);
                }
            }
            else if (nav == 10)
            {
                if (sel_settings == 4) {
                    edit_nickname(&st);
                } else if (sel_settings == 0) {
                    st.mouse_support = !st.mouse_support;
                } else if (sel_settings >= 1 && sel_settings <= 3) {
                    st.theme = sel_settings - 1;
                    apply_theme(st.theme);
                }
            }
        }
        else if (screen == SCREEN_PLAY)
        {
            if (!ensure_connected(&st))
            {
            }
            if (my_hosting)
            {
                screen = SCREEN_WAIT;
                continue;
            }

            static int tick = 0;
            timeout(200);
            pump_network(&screen);
            tick++;
            if (tick >= 50)
            { // 50 * 200ms = 10s
                net_send_line(&net, "GAMES");
                tick = 0;
            }

            draw_play_lobby();

            int ch = getch();
            if (ch == ERR)
                continue;

            if (ch == KEY_MOUSE && st.mouse_support)
            {
                MEVENT ev;
                if (getmouse(&ev) == OK)
                {
                    if (ev.bstate & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED))
                    {
                        int row = ev.y - lobby_list_y0;
                        if (row >= 0 && row < lobby_list_visible)
                        {
                            int idx = lobby_list_start + row;
                            if (idx >= 0 && idx < lobby.n)
                            {
                                lobby.sel = idx;

                                if (ev.bstate & BUTTON1_DOUBLE_CLICKED)
                                {
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

            if (nav == 27)
            {
                screen = SCREEN_MAIN;
                timeout(-1);
                continue;
            }

            if (nav == -1 && lobby.n > 0)
            {
                if (lobby.sel > 0)
                    lobby.sel--;
            }
            else if (nav == +1 && lobby.n > 0)
            {
                if (lobby.sel < lobby.n - 1)
                    lobby.sel++;
            }
            else if (nav == 10 && lobby.n > 0)
            {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "JOIN %d", lobby.g[lobby.sel].id);
                net_send_line(&net, cmd);
                tick = 0;
            }
            else if (ch == 'r' || ch == 'R')
            {
                net_send_line(&net, "GAMES");
            }
            else if (ch == 'h' || ch == 'H')
            {
                int size;
                char pref;
                char game_name[64] = {0};
                
                if (host_popup(&size, &pref, game_name))
                {
                    char cmd[128];
                    
                    // Jeśli użytkownik podał nazwę, wyślij ją do serwera
                    if (game_name[0]) {
                        snprintf(cmd, sizeof(cmd), "HOST %d %c %s", size, pref, game_name);
                    } else {
                        snprintf(cmd, sizeof(cmd), "HOST %d %c", size, pref);
                    }
                    
                    my_game_size = size;
                    net_send_line(&net, cmd);
                }
            }
        }
        else if (screen == SCREEN_WAIT)
        {
            timeout(200);
            pump_network(&screen);
            draw_wait_screen();

            int ch = getch();
            if (ch == ERR)
                continue;

            if (ch == 'r' || ch == 'R')
            {
                net_send_line(&net, "GAMES");
            }
            else if (ch == 'c' || ch == 'C')
            {
                net_send_line(&net, "CANCEL");
                // server odeśle OK CANCELLED + EVENT
            }
            else
            {
                int nav = read_nav_key(ch);
                if (nav == 27)
                { // ESC
                    net_send_line(&net, "CANCEL");
                    screen = SCREEN_PLAY; // wróci po OK CANCELLED albo od razu
                }
            }
        }
        else if (screen == SCREEN_GAME)
        {
            timeout(200);
            pump_network(&screen);

            time_t now = time(NULL);
            if (last_tick == 0)
                last_tick = now;
            int dt = (int)(now - last_tick);
            if (dt > 0)
            {
                last_tick = now;
                if (g_to_move == 0)
                    black_secs -= dt;
                else
                    white_secs -= dt;
                if (black_secs < 0)
                    black_secs = 0;
                if (white_secs < 0)
                    white_secs = 0;
            }

            draw_game_screen();

            int ch = getch();
            if (ch == ERR)
                continue;

            if (ch == KEY_MOUSE && st.mouse_support)
            {
                MEVENT ev;
                if (getmouse(&ev) == OK)
                {
                    if (ev.bstate & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED))
                    {
                        int ry = ev.y - game_inner_y0;
                        int rx = ev.x - game_inner_x0;

                        // content lines are even (0,2,4...) because separators are odd
                        if (ry >= 0 && rx >= 0 && (ry % 2 == 0))
                        {
                            int y = ry / 2;

                            // each cell is (cell_w=3) + 1 separator
                            int x = rx / 4;

                            if (x >= 0 && x < my_game_size && y >= 0 && y < my_game_size)
                            {
                                cur_x = x;
                                cur_y = y;

                                if (ev.bstate & BUTTON1_DOUBLE_CLICKED)
                                {
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

            if (nav == 27 || ch == 'q' || ch == 'Q')
            {
                if (net_ready && my_game_id > 0)
                {
                    char cmd[64];
                    snprintf(cmd, sizeof(cmd), "LEAVE %d", my_game_id);
                    net_send_line(&net, cmd);
                }
                screen = SCREEN_PLAY;
                timeout(200);
                continue;
            }

            if (nav == -1 && cur_y > 0)
                cur_y--;
            else if (nav == +1 && cur_y < my_game_size - 1)
                cur_y++;
            else if (nav == 100 && cur_x > 0)
                cur_x--;
            else if (nav == 101 && cur_x < my_game_size - 1)
                cur_x++;
            else if (nav == 10)
            {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "MOVE %d %d %d", my_game_id, cur_x, cur_y);
                net_send_line(&net, cmd);
            }
            else if (ch == 'p' || ch == 'P')
            {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "PASS %d", my_game_id);
                net_send_line(&net, cmd);
            }
        } else if (screen == SCREEN_GAMEOVER) {
            timeout(-1);
            draw_gameover_screen();
            getch();

            // Resetuj stan gry
            reset_game_state();
            
            my_hosting = 0;
            my_game_id = -1;
            my_game_size = 0;
            my_color[0] = '\0';

            gameover_winner[0] = '\0';
            gameover_reason[0] = '\0';
            gameover_id = -1;

            screen = SCREEN_PLAY;
            timeout(200);
        }
    }

    shutdown_ui();
    return 0;
}
