#pragma once 
#include <stddef.h>

typedef struct {
    int fd;
    char buf[4096];
    size_t len;
} Net;

int net_connect(Net *n, const char *ip, int port);
int net_send_line(Net *n, const char *line);   
int net_recv_into_buffer(Net *n);
int net_next_line(Net *n, char *out, size_t outsz);
void net_close(Net *n);
