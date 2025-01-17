#include "debug.h"
#include "fs.h"
#include "heap.h"
#include "render.h"
//#include "simple_game.h"
#include "frogger_game.h"
#include "timer.h"
#include "wm.h"
#include "heap.h"
#include "thread.h"

#include <assert.h>
#include <stdio.h>
#include <string.h> 

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int main(int argc, const char* argv[])
{
	debug_set_print_mask(k_print_info | k_print_warning | k_print_error);
	//debug_install_exception_handler();

	timer_startup();
	debug_system_init(8);

	heap_t* heap = heap_create(2 * 1024 * 1024, NULL);
	fs_t* fs = fs_create(heap, 8);
	wm_window_t* window = wm_create(heap);
	render_t* render = render_create(heap, window);

	frogger_game_t* game = frogger_game_create(heap, fs, window, render);

	while (!wm_pump(window))
	{
		frogger_game_update(game);
	}

	/* XXX: Shutdown render before the game. Render uses game resources. */
	render_destroy(render);

	frogger_game_destroy(game);

	wm_destroy(window);
	fs_destroy(fs);
	heap_destroy(heap);

	debug_system_uninit();
}
