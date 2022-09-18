#include "debug.h"
#include "heap.h"
#include "tlsf/tlsf.h"
#include <stddef.h>
#include <stdio.h>

#include <Windows.h>
#include <Windowsx.h>

#define STACK_SIZE 10
#define HASH_SIZE 10

typedef struct arena_t
{
	pool_t pool;
	struct arena_t* next;
}arena_t;

typedef struct heap_t
{
	tlsf_t tlsf;
	void*** stack_record;
	size_t grow_increment;
	arena_t* arena;
}heap_t;

uint64_t addr_hash(void* addr)
{
	return (uint64_t) addr % HASH_SIZE;
}

heap_t* heap_create(size_t grow_increment)
{
	//call system to allocate memory
	heap_t* heap = VirtualAlloc(NULL, sizeof(heap_t) + tlsf_size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!heap)
	{
		debug_print(k_print_error, "out of memory!\n");
		return NULL;
	}
	heap->grow_increment = grow_increment;
	heap->tlsf = tlsf_create(heap+1);
	heap->arena = NULL;
	heap->stack_record = malloc(sizeof(void**) * HASH_SIZE);
	return heap;
}

void* heap_alloc(heap_t* heap, size_t size, size_t alignment)
{
	//will need to get current stack trace and store it
	void* address = tlsf_memalign(heap->tlsf, alignment, size);
	if (!address)
	{
		//at least grow_increment memory
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
	}

	//uint64_t place = addr_hash(address);
	//debug_print(k_print_warning, "memory allocated at address %x\n", address);

	return address;
}

void heap_free(heap_t* heap, void* address)
{
	tlsf_free(heap->tlsf, address);
}

void heap_walk(void* ptr, size_t size, int used, void* user)
{
	if (used)
	{
		//debug_print(k_print_warning | k_print_error, "leak detected at address %x!\n", ptr);
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
		//where do we actually call CaptureStackBackTrace here? how does it know
		//void** stack[5];
		//debug_backtrace(stack, 4);
		//for(int i = 0; i < 4; i++)
			//debug_print(k_print_info, "wow %p\n", stack[i]);
		
		VirtualFree(arena, 0, MEM_RELEASE);
		arena = next;
	}
	VirtualFree(heap, 0, MEM_RELEASE);
}
