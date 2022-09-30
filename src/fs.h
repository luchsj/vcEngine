#include <stdlib.h>
#include <stdbool.h>

//asynchronous read/write file system with LZ4 compression
//compressed files will be stored with the size of the uncompressed file and a newline character preceeding the compressed data

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
//if use_compression, fs_read will attempt to parse compression data before decompressing the file and returning the uncompressed data
//returns a work object
fs_work_t* fs_read(fs_t* fs, const char* path, heap_t* heap, bool null_terminate, bool use_compression);

//queue a file write
//file at the specified path will be read in full
//memory for the file will be allocated out of the allocated heap
//if use_compression, the data will be compressed and the uncompressed size of the data will be written to the target file before the actual data
fs_work_t* fs_write(fs_t* fs, const char* path, const void* buffer, size_t size, bool use_compression);

//if true, file work is complete
bool fs_work_is_done(fs_work_t* work);

//block for the file work to complete
void fs_work_wait(fs_work_t* work);

//get error code for file work. value of zero generally indicates success
int fs_work_get_result(fs_work_t* work);

//get buffer associated with file operation
void* fs_work_get_buffer(fs_work_t* work);

//get size associated with file operation
size_t fs_work_get_size(fs_work_t* work);

//free file work object
void fs_work_destroy(fs_work_t* work);
