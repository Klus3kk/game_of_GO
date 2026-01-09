#include "client_state.h"
#include <string.h>

unsigned char g_board[BOARD_MAX_SIZE * BOARD_MAX_SIZE]; // 0 empty, 1 B, 2 W
int g_to_move = 0; // 0 black, 1 white
int cur_x = 0, cur_y = 0;

Net net;           // network handle
Lobby lobby;       // lobby state

int my_hosting = 0;        // 1 if waiting as host
int my_game_id = -1;       // current game id
int my_game_size = 0;      // current board size
char my_color[16] = "";    // my color string

int join_pending_size = 0; // reserved (if you use it later)
int net_ready = 0;         // 1 if connected
int syncing = 0;           // 1 for being berween GAMES_BEGIN and GAMES_END

int lobby_list_y0 = 0;      // lobby list origin y
int lobby_list_x0 = 0;      // lobby list origin x
int lobby_list_visible = 0; // how many lobby rows visible
int lobby_list_start = 0;   // lobby scroll start index

int main_menu_y0 = 0;      // main menu origin y

int game_box_top = 0, game_box_left = 0; // board box pos
int game_inner_y0 = 0, game_inner_x0 = 0; // board content origin

int ui_enabled_colours = 1; // 1 if colours enabled

time_t last_tick = 0;      // timer last tick
int black_secs = 10*60;    // time left black
int white_secs = 10*60;    // time left white

unsigned char prev_board[BOARD_MAX_SIZE * BOARD_MAX_SIZE]; // previous board snapshot
int prev_board_valid = 0;  // 1 if prev_board valid

int score_b = 0; // captures by BLACK
int score_w = 0; // captures by WHITE

void client_clear_board(int size) {
    int n = size * size;
    for (int i = 0; i < n; i++) g_board[i] = 0;
    g_to_move = 0;
    cur_x = cur_y = 0;
    prev_board_valid = 0;
    score_b = score_w = 0;
}
