#include "server_game.h"
#include "server_proto.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static Game games[MAX_GAMES];
static int game_count = 0;
static int next_game_id = 1;

const char* color_name(int c) { return c==0 ? "BLACK" : "WHITE"; }

int opponent_fd(const Game *g, int fd) {
    if (g->host_fd == fd) return g->guest_fd;
    if (g->guest_fd == fd) return g->host_fd;
    return -1;
}

int fd_color_in_game(const Game *g, int fd) {
    // return 0 black / 1 white / -1 not in game
    if (g->host_fd == fd) return g->host_color;
    if (g->guest_fd == fd) return (g->host_color == 0 ? 1 : 0);
    return -1;
}

void game_clear_board(Game *g) {
    int n = g->size * g->size;
    for (int i = 0; i < n; i++) {
        g->board[i] = 0;
        g->prev_board[i] = 0;
    }
    g->to_move = 0; // black to move
    g->cap_black = 0;
    g->cap_white = 0;
    g->consecutive_passes = 0;
}

int game_idx(Game *g, int x, int y) {
    return y * g->size + x;
}

int in_bounds(Game *g, int x, int y) {
    return x >= 0 && y >= 0 && x < g->size && y < g->size;
}

void copy_board(Game *g, unsigned char *dst, const unsigned char *src) {
    int n = g->size * g->size;
    for (int i = 0; i < n; i++) dst[i] = src[i];
}

int boards_equal(Game *g, const unsigned char *a, const unsigned char *b) {
    int n = g->size * g->size;
    for (int i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

Game *find_game_by_id(int id) {
    for (int i = 0; i < game_count; i++) {
        if (games[i].id == id)
            return &games[i];
    }
    return NULL;
}

int host_has_game(int fd) {
    for (int i = 0; i < game_count; i++) {
        if (games[i].host_fd == fd) return 1;
    }
    return 0;
}

// BFS group + liberties
int collect_group(Game *g, int sx, int sy, unsigned char color,
                  int *stones, int max_stones, int *out_liberties) {
    int n = g->size * g->size;
    unsigned char seen[BOARD_MAX_SIZE * BOARD_MAX_SIZE];
    for (int i = 0; i < n; i++) seen[i] = 0;

    int qx[BOARD_MAX_SIZE * BOARD_MAX_SIZE];
    int qy[BOARD_MAX_SIZE * BOARD_MAX_SIZE];
    int qh = 0, qt = 0;

    int start = game_idx(g, sx, sy);
    if (g->board[start] != color) { *out_liberties = 0; return 0; }

    qx[qt] = sx; qy[qt] = sy; qt++;
    seen[start] = 1;

    int count = 0;
    int liberties = 0;

    while (qh < qt) {
        int x = qx[qh], y = qy[qh]; qh++;
        int idx = game_idx(g, x, y);

        if (count < max_stones) stones[count] = idx;
        count++;

        const int dx[4] = {1,-1,0,0};
        const int dy[4] = {0,0,1,-1};

        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k], ny = y + dy[k];
            if (!in_bounds(g, nx, ny)) continue;
            int nidx = game_idx(g, nx, ny);

            if (g->board[nidx] == 0) {
                if (seen[nidx] != 3) { seen[nidx] = 3; liberties++; }
            } else if (g->board[nidx] == color) {
                if (!seen[nidx]) {
                    seen[nidx] = 1;
                    qx[qt] = nx; qy[qt] = ny; qt++;
                }
            }
        }
    }

    *out_liberties = liberties;
    return count;
}

int remove_group(Game *g, const int *stones, int count) {
    int removed = 0;
    int n = g->size * g->size;
    for (int i = 0; i < count; i++) {
        int idx = stones[i];
        if (idx >= 0 && idx < n && g->board[idx] != 0) {
            g->board[idx] = 0;
            removed++;
        }
    }
    return removed;
}

void remove_games_of_client(Client clients[], int fd, const char *reason) {
    for (int i = 0; i < game_count; ) {
        if (games[i].host_fd == fd || games[i].guest_fd == fd) {
            int removed_id = games[i].id;

            if (games[i].status == GAME_RUNNING) {
                int opp = opponent_fd(&games[i], fd);
                if (opp != -1) {
                    int opp_color = fd_color_in_game(&games[i], opp);
                    send_game_over(opp, removed_id, color_name(opp_color), reason);
                }
            }

            games[i] = games[game_count - 1];
            game_count--;

            char ev[64];
            snprintf(ev, sizeof(ev), "EVENT GAME_REMOVED %d\n", removed_id);
            broadcast_subscribed(clients, ev);
            continue;
        }
        i++;
    }
}

void list_games(Client clients[], int to_fd) {
    send_str(to_fd, "GAMES_BEGIN\n");

    for (int i = 0; i < game_count; i++) {
        char buf[128];
        const char *status =
            (games[i].status == GAME_OPEN) ? "OPEN" : "RUNNING";

        int players = (games[i].guest_fd == -1) ? 1 : 2;

        snprintf(buf, sizeof(buf),
                 "GAME %d %d %d %s\n",
                 games[i].id,
                 games[i].size,
                 players,
                 status);

        send_str(to_fd, buf);
    }

    send_str(to_fd, "GAMES_END\n");
}

int cancel_open_games_of_host(Client clients[], int host_fd) {
    int removed_any = 0;

    for (int i = 0; i < game_count; ) {
        if (games[i].host_fd == host_fd && games[i].status == GAME_OPEN) {
            int removed_id = games[i].id;

            games[i] = games[game_count - 1];
            game_count--;

            char ev[64];
            snprintf(ev, sizeof(ev), "EVENT GAME_REMOVED %d\n", removed_id);
            broadcast_subscribed(clients, ev);

            removed_any = 1;
            continue;
        }
        i++;
    }

    return removed_any;
}

int create_game(Client clients[], int host_fd, int size, char pref) {
    if (game_count >= MAX_GAMES) return -1;
    if (host_has_game(host_fd)) return -2;

    int host_color = 0;
    if (pref == 'B') host_color = 0;
    else if (pref == 'W') host_color = 1;
    else host_color = rand() % 2;

    Game *g = &games[game_count++];
    g->id = next_game_id++;
    g->size = size;
    g->host_fd = host_fd;
    g->guest_fd = -1;
    g->status = GAME_OPEN;
    g->host_color = host_color;

    char buf[64];
    snprintf(buf, sizeof(buf), "HOSTED %d %s\n",
             g->id, host_color == 0 ? "BLACK" : "WHITE");
    send_str(host_fd, buf);

    char ev[64];
    snprintf(ev, sizeof(ev), "EVENT GAME_CREATED %d %d\n", g->id, g->size);
    broadcast_subscribed(clients, ev);

    return g->id;
}
