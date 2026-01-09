#include "lobby.h"

void lobby_init(Lobby *L) {
    L->n = 0;
    L->sel = 0;
}

void lobby_clear(Lobby *L) {
    L->n = 0;
    if (L->sel < 0) L->sel = 0;
}

static int find_idx(Lobby *L, int id) {
    for (int i = 0; i < L->n; i++) if (L->g[i].id == id) return i;
    return -1;
}

void lobby_upsert(Lobby *L, int id, int size, int players, LobbyStatus st) {
    int i = find_idx(L, id);
    if (i >= 0) {
        L->g[i].size = size;
        L->g[i].players = players;
        L->g[i].st = st;
        return;
    }
    if (L->n >= LOBBY_MAX) return;
    L->g[L->n++] = (LobbyGame){ .id=id, .size=size, .players=players, .st=st };
    if (L->sel >= L->n) L->sel = L->n - 1;
}

void lobby_remove(Lobby *L, int id) {
    int i = find_idx(L, id);
    if (i < 0) return;
    L->g[i] = L->g[L->n - 1];
    L->n--;
    if (L->sel >= L->n) L->sel = (L->n > 0 ? L->n - 1 : 0);
}

void lobby_set_running(Lobby *L, int id) {
    int i = find_idx(L, id);
    if (i < 0) return;
    L->g[i].st = LOBBY_RUNNING;
    L->g[i].players = 2;
}
