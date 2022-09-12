#include "wm.h"
#include "heap.h"
#include "debug.h"

#include <stdio.h>

int main(int argc, const char* argv[])
{
	heap_t* heap = heap_create(2 * 1024 * 1024);
	wm_window_t* window = wm_create(heap);

	uint32_t mask = wm_get_mouse_mask(window);

	// THIS IS THE MAIN LOOP!
	while (!wm_pump(window))
	{
		int x, y;
		wm_get_mouse_move(window, &x, &y);
		//debug_print_line(k_print_info, "MOUSE mask=%x move=%dx%x\n", wm_get_mouse_mask(window), x, y);
	}

	wm_destroy(window);

	return 0;
}
