#pragma once
#define LOBBY_MAX 32

typedef enum { LOBBY_OPEN, LOBBY_RUNNING } LobbyStatus;

typedef struct {
    int id;
    int size;
    int players;
    LobbyStatus st;
} LobbyGame;


typedef struct {
    LobbyGame g[LOBBY_MAX];
    int n;
    int sel;
} Lobby;

void lobby_init(Lobby *L);
void lobby_clear(Lobby *L);
void lobby_upsert(Lobby *L, int id, int size, int players, LobbyStatus st);
void lobby_remove(Lobby *L, int id);
void lobby_set_running(Lobby *L, int id);
