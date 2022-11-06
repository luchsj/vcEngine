#pragma once

#include <stdlib.h>

//heap memory manager
//main object, heap_t, represents a dynamic memory heap
//once created, memory can be allocated and free from the heap

//handle to heap
typedef struct heap_t heap_t;
typedef struct debug_system_t debug_system_t;

//creates a new memory heap, returns pointer to it
//grow increment is the default size with which the heap grows (should be a multiple of OS page size)
//debug system is optional. if you don't want to use it, just pass in NULL
heap_t* heap_create(size_t grow_increment, debug_system_t* sys);

//allocate memory from a heap
void* heap_alloc(heap_t* heap, size_t size, size_t alignment);

//change the size of previously allocated memory
//not done in place, so it requires size memory besides what's already allocated to be available
//data beyond old size will be uninitialized
void* heap_realloc(heap_t* heap, void* prev, size_t size, size_t alignment);

//free memory previously allocated from a heap
void heap_free(heap_t* heap, void* address);

//destroy a previously created heap
void heap_destroy(heap_t* heap);
