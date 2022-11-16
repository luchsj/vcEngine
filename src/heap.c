#include "heap.h"
#include "debug.h"
#include "heap.h"
#include "tlsf/tlsf.h"
#include "mutex.h"
#include <stddef.h>
#include <stdio.h>

#include <Windows.h>
#include <Windowsx.h>

#define TRACE_SIZE 8

typedef struct arena_t
{
	pool_t pool;
	struct arena_t* next;
}arena_t;

typedef struct heap_t
{
	tlsf_t tlsf;
	size_t grow_increment;
	arena_t* arena;
	mutex_t* mutex;
	debug_system_t* debug_sys;
}heap_t;

heap_t* heap_create(size_t grow_increment)
{
	//call system to allocate memory
	heap_t* heap = VirtualAlloc(NULL, sizeof(heap_t) + tlsf_size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!heap)
	{
		debug_print(k_print_error, "out of memory!\n");
		return NULL;
	}
	heap->mutex = mutex_create();
	heap->grow_increment = grow_increment;
	heap->tlsf = tlsf_create(heap+1);
	heap->arena = NULL;
	//heap->debug_sys = sys; 

	return heap;
}

void* heap_alloc(heap_t* heap, size_t size, size_t alignment)
{
	mutex_lock(heap->mutex);
	size_t old_size = size;
	size += debug_get_trace_size(); //allocate additional memory for trace system
	void* address = tlsf_memalign(heap->tlsf, alignment, size);
	if (!address)
	{
		size_t arena_size = __max(heap->grow_increment, size * 2) + sizeof(arena_t);
		arena_t* arena = VirtualAlloc(NULL, arena_size + tlsf_pool_overhead(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!arena)
		{
			debug_print(k_print_error, "out of memory!\n");
			return NULL;
		}
		arena->pool = tlsf_add_pool(heap->tlsf, arena+1, arena_size);
		arena->next = heap->arena;
		heap->arena = arena;

		address = tlsf_memalign(heap->tlsf, alignment, size);
		//if(heap->debug_sys) //write trace to memory after allocation
		//{ 
		//}
	}
	debug_record_trace(address, old_size);
	debug_print(k_print_debug, "memory allocated at address %p with size %d\n", address, old_size);
	mutex_unlock(heap->mutex);

	return address;
}

void* heap_realloc(heap_t* heap, void* prev, size_t size, size_t alignment)
{
	mutex_lock(heap->mutex);
	//if(heap->debug_sys)
		//debug_remove_trace(heap->debug_sys, prev);
	void* temp = tlsf_realloc(heap->tlsf, prev, size);
	//if(heap->debug_sys)
		//debug_record_trace(heap->debug_sys, temp, size);
	mutex_unlock(heap->mutex);
	return temp;
}

void heap_free(heap_t* heap, void* address)
{
	mutex_lock(heap->mutex);
	//if(heap->debug_sys)
		//debug_remove_trace(heap->debug_sys, address);
	tlsf_free(heap->tlsf, address);
	mutex_unlock(heap->mutex);
}

void heap_walk(void* ptr, size_t size, int used, void* user)
{
	if (used)
	{
		debug_print(k_print_debug, "leak detected at address %p!\n", ptr);
		debug_print_trace(ptr, size);
	}
}

void heap_destroy(heap_t* heap)
{
	tlsf_destroy(heap->tlsf);

	arena_t* arena = heap->arena;
	while (arena)
	{
		arena_t* next = arena->next;
		tlsf_walk_pool(arena->pool, heap_walk, NULL);
		
		VirtualFree(arena, 0, MEM_RELEASE);
		arena = next;
	}

	mutex_destroy(heap->mutex);

	VirtualFree(heap, 0, MEM_RELEASE);
}
