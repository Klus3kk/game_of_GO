// Interactive TCP client.
// - Reads from stdin (lines) and sends to server,
// - Reads from socket and prints server responses.
// Run:   ./client 127.0.0.1 1984

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 4096

// Fatal error
static void fatal_error(const char *msg) {
    perror(msg);
    exit(1);
}

// Send all data in buffer
static void send_all(int fd, const char *data, size_t n) {
    while (n > 0) {
        ssize_t w = send(fd, data, n, 0);
        if (w < 0) {
            if (errno == EINTR) continue;
            fatal_error("send");
        }
        data += (size_t)w;
        n -= (size_t)w;
    }
}

int main(int argc, char **argv) {
    const char *host = "127.0.0.1";
    int port = 1984;

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) fatal_error("socket");

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &serv.sin_addr) != 1) {
        fprintf(stderr, "Invalid host: %s\n", host);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0) fatal_error("connect");

    printf("Connected to %s:%d\n", host, port);
    printf("Type commands (e.g. NICK <nick> \\ GAMES \\ HOST <size> \\ JOIN <id> \\ SUB \\ QUIT)\n");


    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(sock, &rfds);

        int maxfd = (sock > STDIN_FILENO ? sock : STDIN_FILENO);
        int rc = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (rc < 0) {
            if (errno == EINTR) continue;
            fatal_error("select");
        }

        if (FD_ISSET(sock, &rfds)) {
            char buf[BUF_SIZE];
            ssize_t r = recv(sock, buf, sizeof(buf) - 1, 0);
            if (r == 0) {
                printf("\nServer closed connection.\n");
                break;
            }
            if (r < 0) fatal_error("recv");
            buf[r] = '\0';
            printf("%s", buf);
            fflush(stdout);
        }

        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            char line[BUF_SIZE];
            if (!fgets(line, sizeof(line), stdin)) {
                send_all(sock, "QUIT\n", 5);
                break;
            }
            size_t n = strlen(line);

            if (n == 0) continue;

            send_all(sock, line, n);

            if (line[n - 1] != '\n') {
                send_all(sock, "\n", 1);
            }
        }
    }

    close(sock);
    return 0;
}
