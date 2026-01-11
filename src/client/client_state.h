#pragma once
#include "client_types.h"
#include "net.h"
#include "lobby.h"
#include <time.h>

extern unsigned char g_board[BOARD_MAX_SIZE * BOARD_MAX_SIZE]; // 0 empty, 1 B, 2 W
extern int g_to_move; // 0 black, 1 white
extern int cur_x, cur_y;

extern Net net;       // network handle
extern Lobby lobby;   // lobby state

extern int my_hosting;        // 1 if waiting as host
extern int my_game_id;        // current game id
extern int my_game_size;      // current board size
extern char my_color[16];     // my color string

extern int join_pending_size; // reserved (if you use it later)
extern int net_ready;         // 1 if connected
extern int syncing;           // 1 for being berween GAMES_BEGIN and GAMES_END

extern int lobby_list_y0;      // lobby list origin y
extern int lobby_list_x0;      // lobby list origin x
extern int lobby_list_visible; // how many lobby rows visible
extern int lobby_list_start;   // lobby scroll start index

extern int main_menu_y0;      // main menu origin y

extern int game_box_top;      // board box top
extern int game_box_left;     // board box left
extern int game_inner_y0;     // board content origin y
extern int game_inner_x0;     // board content origin x

extern int ui_enabled_colours; // 1 if colours enabled

extern time_t last_tick;      // timer last tick
extern int black_secs;        // time left black
extern int white_secs;        // time left white

extern unsigned char prev_board[BOARD_MAX_SIZE * BOARD_MAX_SIZE]; // previous board snapshot
extern int prev_board_valid;  // 1 if prev_board valid

extern char gameover_winner[16];
extern char gameover_reason[32];
extern int gameover_id;

extern char black_nick[32];
extern char white_nick[32];
extern int cap_black;
extern int cap_white;

extern int score_b; // captures by BLACK
extern int score_w; // captures by WHITE

void client_clear_board(int size);
