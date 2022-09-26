#include "atomic.h"
#include "debug.h"
#include "fs.h"
#include "mutex.h"
#include "wm.h"
#include "heap.h"
#include "thread.h"

#include <assert.h>
#include <stdio.h>
#include <string.h> //VS doesn't tell me when a function is undefined?

static void hw1_test();
static void hw2_test();

typedef struct thread_data_t
{
	int* counter;
	//mutex_t* mutex;
}thread_data_t;

int main(int argc, const char* argv[])
{
	//debug_install_exception_handler();
	debug_set_print_mask(k_print_info | k_print_warning | k_print_error);
	debug_system_init();

	//hw1_test();
	hw2_test();

	heap_t* heap = heap_create(2 * 1024 * 1024);
	wm_window_t* window = wm_create(heap);

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

static void hw2_test_internal(heap_t* heap, fs_t* fs, bool use_compression)
{
	const char* write_data = "hello world!";
//	char compressed_buffer[312]
//	int compressed_size = L4Z_compredd_default(write_data, dst, (int) strlen(write_data), int sizeof(compressed_buffer))
//	char result[123214];
//	lz4_decompress_safe(compressed_buffer, result, compressed_size, sizeof(result));
//	will need to get size of compressed file, meaning we need to store it in the file somehow
	fs_work_t* write_work = fs_write(fs, "foo.bar", write_data, strlen(write_data), use_compression);
	fs_work_t* read_work = fs_read(fs, "foo.bar", heap, true, use_compression);

	assert(fs_work_get_result(write_work) == 0);
	assert(fs_work_get_size(write_work) == 12);

	char* read_data = fs_work_get_buffer(read_work);
	assert(read_data && strcmp(read_data, "hello world!") == 0);
	assert(fs_work_get_result(read_work) == 0);
	assert(fs_work_get_size(read_work) == 12);

	fs_work_destroy(read_work);
	fs_work_destroy(write_work);

	heap_free(heap, read_data);
}

static void hw2_test()
{
	heap_t* heap = heap_create(4096);
	fs_t* fs = fs_create(heap, 16);

	const bool disable_compression = false;
	hw2_test_internal(heap, fs, disable_compression);

	const bool enable_compression = false;
	hw2_test_internal(heap, fs, enable_compression);

	fs_destroy(fs);
	heap_destroy(heap);
}