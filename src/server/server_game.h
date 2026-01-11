#pragma once
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_CLIENTS 50
#define BUF_SIZE 4096
#define NICK_SIZE 32
#define MAX_GAMES 25
#define BOARD_MAX_SIZE 19
#define GAME_NAME_SIZE 64

typedef enum
{
    GAME_OPEN,
    GAME_RUNNING
} GameStatus;


typedef struct
{
    int id;
    int size;
    int host_fd;
    int guest_fd;
    int host_color;
    GameStatus status;
    unsigned char board[BOARD_MAX_SIZE * BOARD_MAX_SIZE];
    int to_move;
    unsigned char prev_board[BOARD_MAX_SIZE * BOARD_MAX_SIZE];
    int cap_black;
    int cap_white;
    int consecutive_passes;
    char game_name[GAME_NAME_SIZE];  
} Game;

typedef struct
{
    int fd;                  // -1 if unused
    char nick[NICK_SIZE];    // nickname
    char buf[BUF_SIZE];      // receive buffer
    size_t len;              // length of data in buffer
    struct sockaddr_in addr; // client address
    bool subscribed;
} Client;

Game *find_game_by_id(int id);
void remove_games_of_client(Client clients[], int fd, const char *reason);
int host_has_game(int fd);

// core Helpers
void game_clear_board(Game *g);
int game_idx(Game *g, int x, int y);
int fd_color_in_game(const Game *g, int fd);
int opponent_fd(const Game *g, int fd);
const char *color_name(int c);

void copy_board(Game *g, unsigned char *dst, const unsigned char *src);
int boards_equal(Game *g, const unsigned char *a, const unsigned char *b);
int in_bounds(Game *g, int x, int y);

int collect_group(Game *g, int sx, int sy, unsigned char color, int *stones, int max_stones, int *out_liberties);
int remove_group(Game *g, const int *stones, int count);

void list_games(Client clients[], int to_fd);
int cancel_open_games_of_host(Client clients[], int host_fd);
int create_game(Client clients[], int host_fd, int size, char pref, const char *custom_name);
void remove_single_game_of_client(Client clients[], int fd, int gid, const char *reason);
int leave_game(Client clients[], int fd, int gid, const char *reason);
