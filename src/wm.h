#include <stdbool.h>
#include <stdint.h>

//window manager
//description.

typedef struct wm_window_t wm_window_t;

wm_window_t* wm_create(); //func desc. (required in headers for all own code going forward!!!)
bool wm_pump(wm_window_t* window);
void wm_destroy(wm_window_t* window);
uint32_t wm_get_mouse_mask(wm_window_t* window);
uint32_t wm_get_key_mask(wm_window_t* window);
void wm_get_mouse_move(wm_window_t* window, int* x, int* y);
