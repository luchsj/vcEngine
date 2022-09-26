#include <stdbool.h>

//asynchronous read/write file system

//handle to file work
typedef struct fs_work_t fs_work_t;

//handle to file system
typedef struct fs_t fs_t;

typedef struct heap_t heap_t;


//create new file system.
//given heap will be used to allocate space for queue and work buffers
//given queue size will define number of in-flight file operations
fs_t* fs_create(heap_t* heap, int queue_capacity);

//destroy previously created file system
void fs_destroy(fs_t* fs);

//queue file read
//file at the specified path will be read in full
//memory for the file will be allocated out of the provided heap
//it's the call's responsibility to free said memory
//returns a work object
fs_work_t* fs_read(fs_t* fs, const char* path, heap_t* heap, bool null_terminate, bool use_compression);

//queue a file write
//file at the specified path will be read in full
//memory for the file will be allocated out of the allocated heap
fs_work_t* fs_write(fs_t* fs, const char* path, const void* buffer, size_t size, bool use_compression);

//if true, file work is complete
bool fs_work_is_done()