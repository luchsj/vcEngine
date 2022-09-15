#include <stdlib.h>

//heap memory manager
//main object, heap_t, represents a dynamic memory heap
//once created, memory can be allocated and free from the heap

//handle to heap
typedef struct heap_t heap_t;

//creates a new memory heap, returns pointer to it
//grow increment is the default size with which the heap grows (should be a multiple of OS page size)
heap_t* heap_create(size_t grow_increment);

//allocate memory from a heap
void* heap_alloc(heap_t* heap, size_t size, size_t alignment);

//free memory previously allocated from a heap
void heap_free(heap_t* heap, void* address);

//destroy a previously created heap
void heap_destroy(heap_t* heap);
