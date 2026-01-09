#include "server_proto.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>



int safe_send(Client clients[], int fd, const char *msg) {
    if (fd < 0) return -1;
    if (send_str(fd, msg) < 0) {
        remove_games_of_client(clients, fd, "DISCONNECT");
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == fd) {
                close(clients[i].fd);
                clients[i].fd = -1;
                clients[i].len = 0;
                clients[i].nick[0] = '\0';
                clients[i].subscribed = false;
                break;
            }
        }
        return -1;
    }
    return 0;
}

void send_game_over(int to_fd, int gid, const char *winner, const char *reason) {
    char msg[128];
    snprintf(msg, sizeof(msg), "GAME_OVER %d %s %s\n", gid, winner, reason);
    send_str(to_fd, msg);
}

void send_board(Game *g) {
    char msg[BUF_SIZE];
    int n = g->size * g->size;

    const char *tm = (g->to_move == 0) ? "BLACK" : "WHITE";

    // "BOARD id to_move <cells>\n"
    int k = snprintf(msg, sizeof(msg), "BOARD %d %s ", g->id, tm);
    if (k < 0) return;

    // append cells
    for (int i = 0; i < n && k + 2 < (int)sizeof(msg); i++) {
        char c = '.';
        if (g->board[i] == 1) c = 'B';
        else if (g->board[i] == 2) c = 'W';
        msg[k++] = c;
    }
    if (k + 1 < (int)sizeof(msg)) msg[k++] = '\n';
    msg[k] = '\0';

    send_str(g->host_fd, msg);
    if (g->guest_fd != -1) send_str(g->guest_fd, msg);
}

void send_captures(Game *g) {
    char msg[128];
    snprintf(msg, sizeof(msg), "CAPTURES %d %d %d\n", g->id, g->cap_black, g->cap_white);
    send_str(g->host_fd, msg);
    if (g->guest_fd != -1) send_str(g->guest_fd, msg);
}
