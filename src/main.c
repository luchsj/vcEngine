#include "wm.h"
#include "heap.h"
#include "debug.h"
#include "thread.h"

#include <stdio.h>

static void hw1_test();

typedef struct thread_data_t
{
	int* counter;
	//mutex_t* mutex;
}thread_data_t;

static int thread_function(void* data)
{
	int* counter = data;
	for (int i = 0; i < 1000; i++)
	{
		
		(*counter)++;
	}
	return 0;
}

int main(int argc, const char* argv[])
{
	debug_install_exception_handler();
	debug_system_init();

	hw1_test();

	debug_set_print_mask(k_print_info | k_print_warning | k_print_error);

	heap_t* heap = heap_create(2 * 1024 * 1024);

	wm_window_t* window = wm_create(heap);

	/*
	int counter = 0;
	thread_t* threads[8];
	for (int i = 0; i < _countof(threads); i++)
	{
		threads[i] = thread_create(thread_function, &counter);
	}

	for (int i = 0; i < _countof(threads); i++)
	{
		thread_destroy(threads[i]);
	}
	*/

	uint32_t mask = wm_get_mouse_mask(window);

	// THIS IS THE MAIN LOOP!
	/*
	while (!wm_pump(window))
	{
		int x, y;
		wm_get_mouse_move(window, &x, &y);
		uint32_t mask = wm_get_mouse_mask(window);
		debug_print(k_print_info, "MOUSE mask=%x move=%dx%x\n", wm_get_mouse_mask(window), x, y);
	}
	*/

	wm_destroy(window);
	heap_destroy(heap);

	debug_system_uninit();

	return EXIT_SUCCESS;
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
