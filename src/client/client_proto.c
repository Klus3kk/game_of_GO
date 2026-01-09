#include "client_proto.h"
#include "client_state.h"
#include <string.h>
#include <stdio.h>

void parse_server_line(const char *line, Screen *screen) {
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

    int go_id;
    char winner[32];
    char reason[32];

    if (sscanf(line, "GAME_OVER %d %31s %31s", &go_id, winner, reason) == 3) {
        if (go_id == my_game_id) {
            my_hosting = 0;
            my_game_id = -1;
            my_game_size = 0;
            my_color[0] = '\0';
            *screen = SCREEN_PLAY;
        }
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
