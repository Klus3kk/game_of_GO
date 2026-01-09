#include "net.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// these are just copy/paste from my placeholder client
static int send_all(int fd, const char *p, size_t n) {
    while (n > 0) {
        ssize_t w = send(fd, p, n, 0);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += (size_t)w;
        n -= (size_t)w;
    }
    return 0;
}

int net_connect(Net *n, const char *ip, int port) {
    n->fd = -1;
    n->len = 0;

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    
    if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) {
        close(s);
        return -1;
    }

    if (connect(s, (struct sockaddr *)&a, sizeof(a)) < 0) {
        close(s);
        return -1;
    }

    n->fd = s;
    return 0;
}

int net_send_line(Net *n, const char *line) {
    if (!n || n->fd < 0) return -1;

    size_t L = strlen(line);
    if (L == 0) return 0;

    if (send_all(n->fd, line, L) < 0) return -1;
    if (line[L - 1] != '\n') {
        if (send_all(n->fd, "\n", 1) < 0) return -1;
    }
    return 0;
}

int net_recv_into_buffer(Net *n) {
    if (!n || n->fd < 0) return -1;
    if (n->len >= sizeof(n->buf)) n->len = 0;

    ssize_t r = recv(n->fd, n->buf + n->len, sizeof(n->buf) - n->len, 0);
    if (r == 0) return 0;
    if (r < 0) {
        if (errno == EINTR) return 1;
        return -1;
    }
    n->len += (size_t)r;
    return 1;
}

static void rstrip_cr(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n')) {
        s[n - 1] = '\0';
        n--;
    }
}

int net_next_line(Net *n, char *out, size_t outsz) {
    if (!n || outsz == 0) return 0;

    for (size_t i = 0; i < n->len; i++) {
        if (n->buf[i] == '\n') {
            size_t take = i + 1;
            if (take >= outsz) take = outsz - 1;

            memcpy(out, n->buf, take);
            out[take] = '\0';
            rstrip_cr(out);

            size_t remain = n->len - (i + 1);
            memmove(n->buf, n->buf + i + 1, remain);
            n->len = remain;

            return 1;
        }
    }
    return 0;
}

void net_close(Net *n) {
    if (!n) return;
    if (n->fd >= 0) close(n->fd);
    n->fd = -1;
    n->len = 0;
}
