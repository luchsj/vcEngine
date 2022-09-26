#include "fs.h"
#include "heap.h"
#include "thread.h"
#include "event.h"
#include "queue.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdbool.h>

typedef struct fs_t
{
	heap_t* heap;
	queue_t* file_queue;
	thread_t* file_thread;
}fs_t;

typedef enum fs_work_op_t
{
	k_fs_work_op_read,
	k_fs_work_op_write
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
	event_t* done;
	int result;
} fs_work_t;

fs_t* fs_create(heap_t* heap, int queue_capacity)
{
	fs_t* fs = heap_alloc(heap, sizeof(fs_t), 8);
	fs->heap = heap;
	fs->file_queue = queue_create(heap, queue_capacity);
	fs->file_thread = thread_create(file_thread_func, fs);
	return fs;
}

void fs_destroy(fs_t* fs)
{
	queue_push(fs->file_queue, NULL);
	thread_destroy(fs->file_thread);
	queue_destroy(fs->file_queue);
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
	work->buffer = NULL;
	work->size = 0;
	work->done = event_create();
	work->result = 0;
	work->null_terminate = false;
	work->use_compression = use_compression;
	if(use_compression)
	{
		//hw2
	}
	else
		queue_push(fs->file_queue, work);

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
	return work ? work->buffer:NULL;
}

size_t fs_work_get_size(fs_work_t* work)
{
	if (work)
	{
		event_wait(work->done);
		event_destroy(work->done);
		heap_free(work->heap, work);
	}
}

fs_work_destroy(fs_work_t* work)
{
	if (work)
	{
		event_wait(work->done);
		event_destroy(work->done);
		heap_free(work->heap, work);
	}
}

static void file_read(fs_work_t* item)
{
	wchar_t wide_path[1024];
	if (MultiByteToWideChar(CP_UTF8, 0, item->path, -1, wide_path, sizeof(wide_path)) <= 0)
	{
		item->result = -1;
		return;
	}
	HANDLE handle = CreateFile(item->path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		item->result = GetLastError();
		return;
	}

	if (!GetFileSize(handle, (PLARGE_INTEGER)&item->size))
	{
		item->result = GetLastError();
		CloseHandle(handle);
		return;
	}
	item->buffer = heap_alloc(item->heap, item->size, 8);

	DWORD bytes_read = 0; 
	if(!ReadFile(handle, item->buffer, item->size, &bytes_read, NULL));
	{
		item->result = GetLastError();
		CloseHandle(handle);
		return;
	}

	item->size = bytes_read;
	CloseHandle(handle);

	if (item->use_compression)
	{
		//hw2
	}
	else
	{
		event_signal(item->done);
	}

	//optionally decompress data in another function
	//push data onto decompression queue

	//optionally compress in fs_write
}

static void file_write(fs_work_t* item)
{
	wchar_t wide_path[1024];
	if (0 >= MultiByteToWideChar(CP_UTF8, 0, item->path, -1, wide_path, sizeof(wide_path)))
	{
		item->result = -1;
		return;
	}
	HANDLE handle = CreateFile(item->path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		item->result = GetLastError();
		return;
	}

	if (!GetFileSize(handle, (PLARGE_INTEGER)&item->size))
	{
		item->result = GetLastError();
		CloseHandle(handle);
		return;
	}
	item->buffer = heap_alloc(item->heap, item->size, 8);

	DWORD bytes_read = 0; 
	if(!WriteFile(handle, item->buffer, item->size, &bytes_read, NULL))
	{
		item->result = GetLastError();
		CloseHandle(handle);
		return;
	}

	item->size = bytes_read;
	CloseHandle(handle);

	if (item->use_compression)
	{
		//hw2 compress
	}
	else
	{
		event_signal(item->done);
	}

	//optionally compress data in another function
	//push data onto compression queue
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
				file_read(work);
				break;
			case k_fs_work_op_write:
				file_write(work);
				break;
		}
	}

	return 0;
}