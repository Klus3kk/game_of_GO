// server.h
#pragma once

// Multi-client TCP server (GNU/Linux, BSD sockets)

#include <stdbool.h>
#include <stddef.h>
#include <netinet/in.h>

#define MAX_CLIENTS 50
#define BUF_SIZE 4096
#define NICK_SIZE 32
#define BOARD_MAX_SIZE 19

typedef struct {
    int fd;                  // -1 if unused
    char nick[NICK_SIZE];    // nickname
    char buf[BUF_SIZE];      // receive buffer
    size_t len;              // length of data in buffer
    struct sockaddr_in addr; // client address
    bool subscribed;
} Client;

// send all data in s of length n
ssize_t send_str(int fd, const char *s);

// send formatted message: prefix + body + '\n'
void send_fmt(int fd, const char *prefix, const char *body);

// client helpers
void client_init(Client *c);
void client_close(Client *c);
int  add_client(Client clients[], int fd, struct sockaddr_in *peer);
const char* nick_of_fd(Client clients[], int fd);

// broadcast helper
void broadcast_subscribed(Client clients[], const char *msg);
