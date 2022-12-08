#pragma once

// Frogger game
// Uses engine systems to create a simple recreation of frogger

typedef struct frogger_game_t frogger_game_t;

typedef struct audio_t audio_t;
typedef struct fs_t fs_t;
typedef struct heap_t heap_t;
typedef struct render_t render_t;
typedef struct wm_window_t wm_window_t;

// Create an instance of simple test game.
frogger_game_t* frogger_game_create(heap_t* heap, fs_t* fs, wm_window_t* window, render_t* render, audio_t* audio);

// Destroy an instance of simple test game.
void frogger_game_destroy(frogger_game_t* game);

// Per-frame update for our simple test game.
void frogger_game_update(frogger_game_t* game);
