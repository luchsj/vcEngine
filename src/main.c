#include "wm.h"
#include "heap.h"
#include "debug.h"

#include <stdio.h>

static void hw1_test();

int main(int argc, const char* argv[])
{
	debug_install_exception_handler();

	hw1_test();

	debug_set_print_mask(k_print_warning | k_print_error);

	heap_t* heap = heap_create(2 * 1024 * 1024);
	wm_window_t* window = wm_create(heap);

	uint32_t mask = wm_get_mouse_mask(window);

	// THIS IS THE MAIN LOOP!
	while (!wm_pump(window))
	{
		int x, y;
		wm_get_mouse_move(window, &x, &y);
		uint32_t mask = wm_get_mouse_mask(window);
		debug_print(k_print_info, "MOUSE mask=%x move=%dx%x\n", wm_get_mouse_mask(window), x, y);
	}

	wm_destroy(window);
	heap_destroy(heap);

	return 0;
}

static void* hw1_alloc1(heap_t* heap){return heap_alloc(heap, 16*1024, 8);}
static void* hw1_alloc2(heap_t* heap){return heap_alloc(heap, 256, 8);}
static void* hw1_alloc3(heap_t* heap){return heap_alloc(heap, 32*1024, 8);}

static void hw1_test()
{
	heap_t* heap = heap_create(4096);
	void* block1 = hw1_alloc1(heap);
	hw1_alloc2(heap);
	hw1_alloc3(heap);
	heap_free(heap, block1);
	heap_destroy(heap);
}
