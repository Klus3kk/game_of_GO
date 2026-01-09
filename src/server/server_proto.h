#pragma once
#include "server_game.h"
#include <sys/types.h> 

ssize_t send_str(int fd, const char *s);

void send_game_over(int to_fd, int gid, const char *winner, const char *reason);
void send_board(Game *g);
void send_captures(Game *g);

// broadcast helper 
void broadcast_subscribed(Client clients[], const char *msg);

// handle disconnect-on-send
int safe_send(Client clients[], int fd, const char *msg);
