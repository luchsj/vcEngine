#include "fs.h"
#include "debug.h"
#include "heap.h"
#include "thread.h"
#include "event.h"
#include "queue.h"
#include "l4z/lz4.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdbool.h>
#include <string.h>

#define COMPRESS_SIZE_LIMIT 2048
#define DECOMPRESS_SIZE_LIMIT 8192

typedef struct fs_t
{
	heap_t* heap;
	queue_t* file_queue;
	queue_t* compression_queue;
	thread_t* file_thread;
	thread_t* compression_thread;
}fs_t;

typedef enum fs_work_op_t
{
	k_fs_work_op_read,
	k_fs_work_op_write,
}fs_work_op_t;

typedef struct fs_work_t
{
	heap_t* heap;
	fs_work_op_t op;
	char path[1024];	//UTF-8
//	short path[1024];	//UTF-16
//	int path[1024];		//UTF-32
	bool null_terminate;
	bool use_compression;
	void* buffer;
	size_t size;
	size_t compression_size;
	event_t* done;
	int result;
} fs_work_t;

static int file_thread_func(void* user);
static int compression_thread_func(void* user);

fs_t* fs_create(heap_t* heap, int queue_capacity)
{
	fs_t* fs = heap_alloc(heap, sizeof(fs_t), 8);
	fs->heap = heap;
	fs->file_queue = queue_create(heap, queue_capacity);
	fs->compression_queue = queue_create(heap, queue_capacity);
	fs->file_thread = thread_create(file_thread_func, fs);
	fs->compression_thread = thread_create(compression_thread_func, fs);
	return fs;
}

void fs_destroy(fs_t* fs)
{
	queue_push(fs->file_queue, NULL);
	thread_destroy(fs->file_thread);
	queue_destroy(fs->file_queue);
	queue_destroy(fs->compression_queue);
	heap_free(fs->heap, fs);
}

fs_work_t* fs_read(fs_t* fs, const char* path, heap_t* heap, bool null_terminate, bool use_compression)
{
	fs_work_t* work = heap_alloc(fs->heap, sizeof(fs_work_t), 8);
	work->heap = heap;
	work->op = k_fs_work_op_read;
	strcpy_s(work->path, sizeof(work->path), path);
	work->buffer = NULL;
	work->size = 0;
	work->done = event_create();
	work->result = 0;
	work->null_terminate = null_terminate;
	work->use_compression = use_compression;
	queue_push(fs->file_queue, work);
	return work;
}

fs_work_t* fs_write(fs_t* fs, const char* path, const void* buffer, size_t size, bool use_compression)
{
	fs_work_t* work = heap_alloc(fs->heap, sizeof(fs_work_t), 8);
	work->heap = fs->heap;
	work->op = k_fs_work_op_write;
	strcpy_s(work->path, sizeof(work->path), path);
	work->buffer = (void*)buffer;
	work->size = size;
	work->done = event_create();
	work->result = 0;
	work->null_terminate = false;
	work->use_compression = use_compression;
	if(use_compression)
		queue_push(fs->compression_queue, work);
	else
		queue_push(fs->file_queue, work);

	return work;

}

bool fs_work_is_done(fs_work_t* work)
{
	return work ? event_is_raised(work->done) : true;
}

void fs_work_wait(fs_work_t* work)
{
	if(work)
		event_wait(work->done);
}

int fs_work_get_result(fs_work_t* work)
{
	fs_work_wait(work);
	return work ? work->result:-1;
}

void* fs_work_get_buffer(fs_work_t* work)
{
	fs_work_wait(work);
	return work ? work->buffer : NULL;
}

size_t fs_work_get_size(fs_work_t* work)
{
	fs_work_wait(work);
	return work ? work->size : 0;
}

void fs_work_destroy(fs_work_t* work)
{
	if (work)
	{
		if(work->use_compression || work->op == k_fs_work_op_read)
			heap_free(work->heap, work->buffer);
		event_wait(work->done);
		event_destroy(work->done);
		heap_free(work->heap, work);
	}
}

static void file_read(fs_work_t* work, fs_t* fs)
{
	wchar_t wide_path[1024];
	if (MultiByteToWideChar(CP_UTF8, 0, work->path, -1, wide_path, sizeof(wide_path)) <= 0)
	{
		work->result = -1;
		return;
	}
	HANDLE handle = CreateFile(wide_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		work->result = GetLastError();
		return;
	}

	if (!GetFileSizeEx(handle, (PLARGE_INTEGER)&work->size))
	{
		work->result = GetLastError();
		CloseHandle(handle);
		return;
	}
	work->buffer = heap_alloc(work->heap, work->null_terminate ? work->size + 1 : work->size, 8);

	DWORD bytes_read = 0; 
	if(!ReadFile(handle, work->buffer, (DWORD) work->size, &bytes_read, NULL))
	{
		work->result = GetLastError();
		CloseHandle(handle);
		return;
	}

	CloseHandle(handle);

	if (work->use_compression)
	{
		char compression_buffer[16];
		for (int k = 0; k < 16; k++)
		{
			compression_buffer[k] = ((char*) work->buffer)[k];
			if (compression_buffer[k] == '\n')
			{
				compression_buffer[k] = 0;
				break;
			}
		}
		work->compression_size = atoi(compression_buffer);
		work->size -= strlen(compression_buffer) + 1; 
		for (unsigned int k = 0; k < work->size; k++)
			((char*)work->buffer)[k] = ((char*)work->buffer)[k + strlen(compression_buffer) + 1];

		queue_push(fs->compression_queue, work);
	}

	if (work->null_terminate)
	{
		((char*) work->buffer)[bytes_read] = 0;
	}

	if (!work->use_compression)
	{
		event_signal(work->done);
	}
}

static void file_write(fs_work_t* work)
{
	wchar_t wide_path[1024];
	if (MultiByteToWideChar(CP_UTF8, 0, work->path, -1, wide_path, sizeof(wide_path)) <= 0)
	{
		work->result = -1;
		return;
	}
	HANDLE handle = CreateFile(wide_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		work->result = GetLastError();
		return;
	}

	DWORD compress_info_bytes_written = 0;
	if (work->use_compression)
	{
		char uncompressed_size_str[16];
		sprintf_s(uncompressed_size_str, 16, "%zd\n", work->compression_size);
		if(!WriteFile(handle, uncompressed_size_str, (DWORD) strlen(uncompressed_size_str), &compress_info_bytes_written, NULL))
		{
			debug_print(k_print_error, "Failed to write compression data to file, aborting write operation\n");
			work->result = GetLastError();
			CloseHandle(handle);
			return;
		}
		SetEndOfFile(handle);
	}

	DWORD bytes_written = 0;
	if(!WriteFile(handle, work->buffer, (DWORD) work->size, &bytes_written, NULL))
	{
		debug_print(k_print_error, "Failed to write data to file, aborting write operation\n");
		work->result = GetLastError();
		CloseHandle(handle);
		return;
	}
	
	work->size = bytes_written + compress_info_bytes_written;
	CloseHandle(handle);
	event_signal(work->done);
}

static int file_thread_func(void* user)
{
	fs_t* fs = user;
	while (1)
	{
		fs_work_t* work = queue_pop(fs->file_queue);
		if (work == NULL)
		{
			break;
		}

		switch (work->op)
		{
			case k_fs_work_op_read:
				file_read(work, fs);
				break;
			case k_fs_work_op_write:
				file_write(work);
				break;
		}
	}

	return 0;
}

static int compression_thread_func(void* user)
{
	fs_t* fs = user;
	while (1)
	{
		fs_work_t* work = queue_pop(fs->compression_queue);
		if(work == NULL)
			break;

		switch (work->op)
		{
			case k_fs_work_op_write:
			{
				int buffer_size = LZ4_compressBound(work->size);
				void* compression_buffer = heap_alloc(work->heap, buffer_size, 8);
				int compressed_size = LZ4_compress_default(work->buffer, compression_buffer, work->size, buffer_size);
				if (compressed_size == 0)
				{
					debug_print(k_print_error, "Failed to compress file; LZ4 returned 0");
					event_signal(work->done);
					break;
				}
				work->compression_size = work->size;
				work->size = compressed_size;
				work->buffer = compression_buffer;
				queue_push(fs->file_queue, work);
				break;
			}
			case k_fs_work_op_read:
			{
				void* compression_buffer = heap_alloc(work->heap, work->compression_size + 1, 8); // + 1 to accomodate null terminator
				int bytes_decompressed = LZ4_decompress_safe(work->buffer, compression_buffer, work->size, work->compression_size);
				((char*) compression_buffer)[bytes_decompressed] = 0;
				if (bytes_decompressed <= 0)
				{
					debug_print(k_print_error, "Failed to decompress file; LZ4 returned%d\n", bytes_decompressed);
					event_signal(work->done);
					break;
				}
				work->size = bytes_decompressed;
				heap_free(fs->heap, work->buffer);
				work->buffer = compression_buffer;
				event_signal(work->done);
				break;
			}
		}
	}

	return 0;
}