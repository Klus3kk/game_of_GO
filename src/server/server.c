// Multi-client TCP server (GNU/Linux, BSD sockets)
// Commands:
//   NICK <name>   -> sets nickname
//   LIST          -> list connected clients
//   QUIT          -> disconnect
// Run:   ./server 9000

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

#define FD_SIZE 1024
#define MAX_CLIENTS 50
#define BUF_SIZE 4096
#define NICK_SIZE 32
#define MAX_GAMES 50

typedef enum { GAME_OPEN, GAME_RUNNING } GameStatus;

typedef struct {
    int id;
    int size;         // board size
    int host_fd;      // who created
    int guest_fd;     // -1 if none
    GameStatus status;
} Game;

static Game games[MAX_GAMES];
static int game_count = 0;
static int next_game_id = 1;



typedef struct {
    int fd;                  // -1 if unused
    char nick[NICK_SIZE];    // nickname
    char buf[BUF_SIZE];      // receive buffer
    size_t len;              // length of data in buffer
    struct sockaddr_in addr; // client address
} Client;

// send all data in s of length n
static void send_str(int fd, const char *s) {
    size_t n = strlen(s);
    while (n > 0) {
        ssize_t w = send(fd, s, n, 0);
        if (w < 0) {
            if (errno == EINTR) continue;
            break;
        }
        s += (size_t)w;
        n -= (size_t)w;
    }
}

// send formatted message: prefix + body + '\n'
static void send_fmt(int fd, const char *prefix, const char *body) {
    char out[BUF_SIZE];
    int k = snprintf(out, sizeof(out), "%s%s\n", prefix, body);
    if (k > 0) send_str(fd, out);
}

// Init client 
static void client_init(Client *c) {
    c->fd = -1;
    c->nick[0] = '\0';
    c->len = 0;
    memset(&c->addr, 0, sizeof(c->addr));
}

// Add new client, return index or -1 if full
static int add_client(Client clients[], int fd, struct sockaddr_in *peer) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == -1) {
            clients[i].fd = fd;
            clients[i].len = 0;
            clients[i].addr = *peer;

            // default nick: u<fd>
            snprintf(clients[i].nick, sizeof(clients[i].nick), "u%d", fd);

            send_fmt(fd, "WELCOME ", "type: NICK <name> | GAMES | HOST <size> | JOIN <id> | QUIT");
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

// Fatal error
static void fatal_error(const char *msg) {
    perror(msg);
    exit(1);
}

static Game *find_game_by_id(int id) {
    for (int i = 0; i < game_count; i++) {
        if (games[i].id == id)
            return &games[i];
    }
    return NULL;
}

static void remove_games_of_client(int fd) {
    for (int i = 0; i < game_count; ) {
        if (games[i].host_fd == fd || games[i].guest_fd == fd) {
            games[i] = games[game_count - 1];
            game_count--;
        } else {
            i++;
        }
    }
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
        client_close(c);
        return;
    }

    if (strcmp(line, "GAMES") == 0) {
        send_str(c->fd, "GAMES_BEGIN\n");

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

            send_str(c->fd, buf);
        }

        send_str(c->fd, "GAMES_END\n");
        return;
    }

    if (strncmp(line, "HOST ", 5) == 0) {
        int size = atoi(line + 5);

        if (size < 7 || size > 19 || size % 2 == 0) {
            send_str(c->fd, "ERR invalid board size\n");
            return;
        }

        if (game_count >= MAX_GAMES) {
            send_str(c->fd, "ERR server full\n");
            return;
        }

        Game *g = &games[game_count++];
        g->id = next_game_id++;
        g->size = size;
        g->host_fd = c->fd;
        g->guest_fd = -1;
        g->status = GAME_OPEN;

        char buf[64];
        snprintf(buf, sizeof(buf), "HOSTED %d\n", g->id);
        send_str(c->fd, buf);
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

        send_str(g->host_fd, "JOINED BLACK\n");
        send_str(g->guest_fd, "JOINED WHITE\n");
        return;
    }

    send_fmt(c->fd, "ERR ", "unknown command");
}

static void process_client_data(Client clients[], int idx) {
    Client *c = &clients[idx];
    if (c->fd < 0) return;

    // read into buffer tail
    ssize_t r = recv(c->fd, c->buf + c->len, (size_t)(BUF_SIZE - c->len), 0);
    if (r == 0) {
        remove_games_of_client(c->fd);
        client_close(c);
        return;
    }
    if (r < 0) {
        if (errno == EINTR) return;
        remove_games_of_client(c->fd);
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

    // compact remaining bytes 
    if (start > 0) {
        size_t remain = c->len - start;
        memmove(c->buf, c->buf + start, remain);
        c->len = remain;
    }

    // if buffer is full without newline -> reset to avoid deadlock
    if (c->len == BUF_SIZE) {
        c->len = 0;
        send_fmt(c->fd, "ERR ", "line too long");
    }
}

int main(int argc, char **argv) {
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