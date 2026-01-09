// server.c
// Multi-client TCP server (GNU/Linux, BSD sockets)
// Commands:
//   NICK <name>   -> sets nickname
//   SUB           -> subscribe to lobby broadcasts
//   GAMES         -> list games
//   HOST <size> <B|W|R>
//   JOIN <id>
//   MOVE <id> <x> <y>
//   PASS <id>
//   CANCEL
//   QUIT          -> disconnect
// Run:   ./server 9000
// gcc server.c server_game.c server_proto.c -o server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <signal.h> // i need to add this so that when the client disconnects it doesn't crash the server
#include <time.h>
#include "server_game.h"
#include "server_proto.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char* nick_of_fd(Client clients[], int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == fd) return clients[i].nick;
    }
    return "?";
}

// send all data in s of length n
// NOTE: not static, because server_proto.c uses it too
ssize_t send_str(int fd, const char *s) {
    size_t n = strlen(s);
    while (n > 0) {
        ssize_t w = send(fd, s, n, 0);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        s += (size_t)w;
        n -= (size_t)w;
    }
    return 0;
}

// send formatted message: prefix + body + '\n'
static void send_fmt(int fd, const char *prefix, const char *body) {
    char out[BUF_SIZE];
    int k = snprintf(out, sizeof(out), "%s%s\n", prefix, body);
    if (k > 0) (void)send_str(fd, out);
}

// Init client
static void client_init(Client *c) {
    c->fd = -1;
    c->nick[0] = '\0';
    c->len = 0;
    c->subscribed = false;
    memset(&c->addr, 0, sizeof(c->addr));
}

// Add new client, return index or -1 if full
static int add_client(Client clients[], int fd, struct sockaddr_in *peer) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == -1) {
            clients[i].fd = fd;
            clients[i].len = 0;
            clients[i].addr = *peer;
            clients[i].subscribed = false;

            // default nick: u<fd>
            snprintf(clients[i].nick, sizeof(clients[i].nick), "u%d", fd);

            return i;
        }
    }
    return -1;
}

// List clients to to_fd
static void list_clients(Client clients[], int to_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1) {
            char ip[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &clients[i].addr.sin_addr, ip, sizeof(ip));
            char line[256];
            snprintf(line, sizeof(line), " - %s (fd=%d, %s:%d)\n",
                     clients[i].nick, clients[i].fd, ip, ntohs(clients[i].addr.sin_port));
            send_str(to_fd, line);
        }
    }
    send_str(to_fd, "\n");
}

// Close client connection and free slot
static void client_close(Client *c) {
    if (c->fd >= 0) close(c->fd);
    client_init(c);
}

void broadcast_subscribed(Client clients[], const char *msg) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1 && clients[i].subscribed) {
            if (send_str(clients[i].fd, msg) < 0) {
                remove_games_of_client(clients, clients[i].fd, "DISCONNECT");
                client_close(&clients[i]);
            }
        }
    }
}

// Fatal error
static void fatal_error(const char *msg) {
    perror(msg);
    exit(1);
}

// Remove trailing \n and \r
static void rstrip(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

// Handle a complete line from client idx
static void handle_line(Client clients[], int idx, char *line) {
    Client *c = &clients[idx];
    rstrip(line);

    if (line[0] == '\0') return;

    if (strncmp(line, "NICK ", 5) == 0) {
        const char *name = line + 5;

        if (*name == '\0') {
            send_fmt(c->fd, "ERR ", "empty nickname");
            return;
        }
        if (strlen(name) >= NICK_SIZE) {
            send_fmt(c->fd, "ERR ", "nickname too long");
            return;
        }
        for (const char *p = name; *p; p++) {
            if (*p <= 32) {
                send_fmt(c->fd, "ERR ", "nickname cannot contain spaces/control chars");
                return;
            }
        }
        snprintf(c->nick, sizeof(c->nick), "%s", name);
        send_fmt(c->fd, "OK ", "NICK set");
        return;
    }

    if (strcmp(line, "QUIT") == 0) {
        send_fmt(c->fd, "OK ", "bye");
        remove_games_of_client(clients, c->fd, "QUIT");
        client_close(c);
        return;
    }

    if (strcmp(line, "CANCEL") == 0) {
        int removed = cancel_open_games_of_host(clients, c->fd);
        if (removed) send_str(c->fd, "OK CANCELLED\n");
        else send_str(c->fd, "ERR nothing to cancel\n");
        return;
    }


    if (strcmp(line, "GAMES") == 0) {
        list_games(clients, c->fd);
        return;
    }


    if (strncmp(line, "HOST ", 5) == 0) {
        int size = 0;
        char pref = 'R'; // B/W/R

        int n = sscanf(line, "HOST %d %c", &size, &pref);
        if (n < 1) {
            send_str(c->fd, "ERR usage: HOST <size> <B|W|R>\n");
            return;
        }

        if (pref != 'B' && pref != 'W' && pref != 'R') pref = 'R';

        if (size < 7 || size > 19 || size % 2 == 0) {
            send_str(c->fd, "ERR invalid board size\n");
            return;
        }

        int gid = create_game(clients, c->fd, size, pref);
        if (gid == -1) {
            send_str(c->fd, "ERR server full\n");
            return;
        }
        if (gid == -2) {
            send_str(c->fd, "ERR already hosting a game\n");
            return;
        }
        return;
    }


    if (strncmp(line, "JOIN ", 5) == 0) {
        int id = atoi(line + 5);
        Game *g = find_game_by_id(id);

        if (!g) {
            send_str(c->fd, "ERR no such game\n");
            return;
        }

        if (g->status != GAME_OPEN || g->guest_fd != -1) {
            send_str(c->fd, "ERR game not available\n");
            return;
        }

        if (g->host_fd == c->fd) {
            send_str(c->fd, "ERR cannot join own game\n");
            return;
        }

        g->guest_fd = c->fd;
        g->status = GAME_RUNNING;

        const char *hc = (g->host_color == 0) ? "BLACK" : "WHITE";
        const char *gc = (g->host_color == 0) ? "WHITE" : "BLACK";

        game_clear_board(g);

        // START do obu
        char sh[64], sg[64];
        snprintf(sh, sizeof(sh), "START %d %d %s\n", g->id, g->size, hc);
        snprintf(sg, sizeof(sg), "START %d %d %s\n", g->id, g->size, gc);
        send_str(g->host_fd, sh);
        send_str(g->guest_fd, sg);

        // pierwsza plansza (pusta)
        send_board(g);
        send_captures(g);

        int black_fd = (g->host_color == 0) ? g->host_fd : g->guest_fd;
        int white_fd = (g->host_color == 0) ? g->guest_fd : g->host_fd;

        char nn[128];
        snprintf(nn, sizeof(nn), "NICKS %d %s %s\n", g->id,
                nick_of_fd(clients, black_fd),
                nick_of_fd(clients, white_fd));
        send_str(g->host_fd, nn);
        send_str(g->guest_fd, nn);

        char ev[64];
        snprintf(ev, sizeof(ev), "EVENT GAME_STARTED %d\n", g->id);
        broadcast_subscribed(clients, ev);

        return;
    }

    if (strcmp(line, "SUB") == 0) {
        c->subscribed = true;
        send_fmt(c->fd, "OK ", "subscribed to broadcasts");
        return;
    }

    if (strncmp(line, "MOVE ", 5) == 0) {
        int id, x, y;
        if (sscanf(line, "MOVE %d %d %d", &id, &x, &y) != 3) {
            send_str(c->fd, "ERR usage: MOVE <id> <x> <y>\n");
            return;
        }

        Game *g = find_game_by_id(id);
        if (!g) { send_str(c->fd, "ERR no such game\n"); return; }
        if (g->status != GAME_RUNNING) { send_str(c->fd, "ERR game not running\n"); return; }

        int myc = fd_color_in_game(g, c->fd);
        if (myc < 0) { send_str(c->fd, "ERR not in that game\n"); return; }

        if (!in_bounds(g, x, y)) { send_str(c->fd, "ERR out of bounds\n"); return; }
        if (myc != g->to_move) { send_str(c->fd, "ERR not your turn\n"); return; }

        int idxb = game_idx(g, x, y);
        if (g->board[idxb] != 0) { send_str(c->fd, "ERR occupied\n"); return; }

        unsigned char before[BOARD_MAX_SIZE * BOARD_MAX_SIZE];
        copy_board(g, before, g->board);

        g->board[idxb] = (myc == 0 ? 1 : 2);

        unsigned char opp = (myc == 0 ? 2 : 1);
        const int dx[4] = {1,-1,0,0};
        const int dy[4] = {0,0,1,-1};

        int stones[BOARD_MAX_SIZE * BOARD_MAX_SIZE];
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k], ny = y + dy[k];
            if (!in_bounds(g, nx, ny)) continue;
            int nidx = game_idx(g, nx, ny);
            if (g->board[nidx] == opp) {
                int libs = 0;
                int cnt = collect_group(g, nx, ny, opp, stones,
                                        (int)(BOARD_MAX_SIZE*BOARD_MAX_SIZE), &libs);
                if (cnt > 0 && libs == 0) {
                    int removed = remove_group(g, stones, cnt);
                    if (myc == 0) g->cap_black += removed;
                    else         g->cap_white += removed;
                }
            }
        }

        int mylibs = 0;
        int mycnt = collect_group(g, x, y, (myc == 0 ? 1 : 2), stones,
                                  (int)(BOARD_MAX_SIZE*BOARD_MAX_SIZE), &mylibs);
        if (mycnt > 0 && mylibs == 0) {
            copy_board(g, g->board, before);
            send_str(c->fd, "ERR suicide\n");
            return;
        }

        if (boards_equal(g, g->board, g->prev_board)) {
            copy_board(g, g->board, before);
            send_str(c->fd, "ERR ko\n");
            return;
        }

        copy_board(g, g->prev_board, before);

        g->to_move = (g->to_move == 0 ? 1 : 0);

        char msg[128];
        snprintf(msg, sizeof(msg), "MOVED %d %d %d %s\n",
                 g->id, x, y, (myc==0?"BLACK":"WHITE"));
        send_str(g->host_fd, msg);
        send_str(g->guest_fd, msg);

        send_board(g);
        send_captures(g);
        return;
    }

    if (strncmp(line, "PASS ", 5) == 0) {
        int id;
        if (sscanf(line, "PASS %d", &id) != 1) {
            send_str(c->fd, "ERR usage: PASS <id>\n");
            return;
        }

        Game *g = find_game_by_id(id);
        if (!g) { send_str(c->fd, "ERR no such game\n"); return; }
        if (g->status != GAME_RUNNING) { send_str(c->fd, "ERR game not running\n"); return; }

        int myc = fd_color_in_game(g, c->fd);
        if (myc < 0) { send_str(c->fd, "ERR not in that game\n"); return; }
        if (myc != g->to_move) { send_str(c->fd, "ERR not your turn\n"); return; }

        unsigned char before[BOARD_MAX_SIZE * BOARD_MAX_SIZE];
        copy_board(g, before, g->board);
        copy_board(g, g->prev_board, before);

        g->to_move = (g->to_move == 0 ? 1 : 0);

        char m[64];
        snprintf(m, sizeof(m), "PASSED %d %s\n", g->id, (myc==0?"BLACK":"WHITE"));
        send_str(g->host_fd, m);
        send_str(g->guest_fd, m);

        send_board(g);
        return;
    }

    send_fmt(c->fd, "ERR ", "unknown command");
}

static void process_client_data(Client clients[], int idx) {
    Client *c = &clients[idx];
    if (c->fd < 0) return;

    ssize_t r = recv(c->fd, c->buf + c->len, (size_t)(BUF_SIZE - c->len), 0);
    if (r == 0) {
        remove_games_of_client(clients, c->fd, "DISCONNECT");
        client_close(c);
        return;
    }
    if (r < 0) {
        if (errno == EINTR) return;
        remove_games_of_client(clients, c->fd, "DISCONNECT");
        client_close(c);
        return;
    }

    c->len += (size_t)r;
    size_t start = 0;
    for (size_t i = 0; i < c->len; i++) {
        if (c->buf[i] == '\n') {
            size_t line_len = i - start + 1;
            char line[BUF_SIZE];
            if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
            memcpy(line, c->buf + start, line_len);
            line[line_len] = '\0';

            handle_line(clients, idx, line);

            if (clients[idx].fd == -1) return;

            start = i + 1;
        }
    }

    if (start > 0) {
        size_t remain = c->len - start;
        memmove(c->buf, c->buf + start, remain);
        c->len = remain;
    }

    if (c->len == BUF_SIZE) {
        c->len = 0;
        send_fmt(c->fd, "ERR ", "line too long");
    }
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    srand((unsigned)time(NULL));

    int port = 1984;
    if (argc >= 2) port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) fatal_error("socket");

    int one = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        fatal_error("setsockopt(SO_REUSEADDR)");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) fatal_error("bind");
    if (listen(server_fd, 64) < 0) fatal_error("listen");

    Client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) client_init(&clients[i]);

    printf("Server listening: %d\n", port);

    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(server_fd, &rfds);

        int maxfd = server_fd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1) {
                FD_SET(clients[i].fd, &rfds);
                if (clients[i].fd > maxfd) maxfd = clients[i].fd;
            }
        }

        int rc = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (rc < 0) {
            if (errno == EINTR) continue;
            fatal_error("select");
        }

        if (FD_ISSET(server_fd, &rfds)) {
            struct sockaddr_in peer;
            socklen_t peerlen = sizeof(peer);
            int cfd = accept(server_fd, (struct sockaddr *)&peer, &peerlen);
            if (cfd >= 0) {
                int idx = add_client(clients, cfd, &peer);
                if (idx < 0) {
                    send_fmt(cfd, "ERR ", "server full");
                    close(cfd);
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, &rfds)) {
                process_client_data(clients, i);
            }
        }
    }

    close(server_fd);
    return 0;
}
