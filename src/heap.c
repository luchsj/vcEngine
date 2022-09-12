#include "heap.h"
#include "tlsf/tlsf.h"
#include <stddef.h>
#include <stdio.h>

#include <Windows.h>
#include <Windowsx.h>

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
}heap_t;

heap_t* heap_create(size_t grow_increment)
{
	//call system to allocate memory
	heap_t* heap = VirtualAlloc(NULL, sizeof(heap_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!heap)
	{
		fprintf(stderr, "out of memory!");
		return NULL;
	}
	heap->grow_increment = grow_increment;
	heap->tlsf = tlsf_create(heap+1);
	heap->arena = NULL;
	return heap;
}

void* heap_alloc(heap_t* heap, size_t size, size_t alignment)
{
	void* address = tlsf_memalign(heap->tlsf, alignment, size);
	if (!address)
	{
		//at least grow_increment memory
		size_t arena_size = max(heap->grow_increment, size * 2) + sizeof(arena_t);
		arena_t* arena = VirtualAlloc(NULL, arena_size + tlsf_pool_overhead(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!arena)
		{
			printf("ran out of memory!!\n");
			return NULL;
		}
		arena->pool = tlsf_add_pool(heap->tlsf, arena+1, arena_size);
		arena->next = heap->arena;
		heap->arena = arena;

		address = tlsf_memalign(heap->tlsf, alignment, size);
	}
	return address;
}

void heap_free(heap_t* heap, void* address)
{
	tlsf_free(heap->tlsf, address);
}

void heap_destroy(heap_t* heap)
{
	arena_t* arena = heap->arena;
	while (arena)
	{
		arena_t* next = arena->next;
		//if(arena->)
		
		VirtualFree(arena, 0, MEM_RELEASE);
		arena = next;
	}
	tlsf_destroy(heap->tlsf);
	VirtualFree(heap, 0, MEM_RELEASE);
}
