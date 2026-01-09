#pragma once
#include "client_types.h"

void init_ui(void);
void shutdown_ui(void);

int read_nav_key(int ch);

void draw_wait_screen(void);
void draw_main_menu(int selected);
void draw_settings_screen(const Settings *st, int selected);
void edit_nickname(Settings *st);

void draw_play_screen(int selected);
void draw_play_lobby(void);

int host_popup(int *out_size, char *out_pref);

void apply_theme(int theme);
void draw_game_screen(void);
