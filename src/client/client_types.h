#pragma once
#include <stdbool.h>

typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_SETTINGS = 1,
    SCREEN_PLAY = 2,
    SCREEN_WAIT = 3,
    SCREEN_GAME = 4,
    SCREEN_GAMEOVER = 5
} Screen;


typedef struct {
    bool mouse_support;
    bool colours;
    int theme;               // 0..2
    char nickname[32];
} Settings;

#define BOARD_MAX_SIZE 19
