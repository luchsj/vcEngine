#include "debug.h"
#include "fs.h"
#include "heap.h"
#include "timer.h"
#include "trace.h"
#include "semaphore.h"

#include <stddef.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define TRACE_BUFFER_INIT_SIZE 2048
#define TRACE_TEMP_BUFFER_SIZE 512

typedef struct duration_t
{
	char* name;
	char ph;
	uint32_t time;
	uint32_t process_id;
	uint32_t thread_id;
}duration_t;

typedef struct trace_t
{
	duration_t** durations;
	duration_t** active_durations;
	uint32_t duration_count;
	uint32_t active_duration_count;
	uint32_t duration_cap;
	semaphore_t* semaphore;
	heap_t* heap;
	fs_t* fs;
	char* write_path;
	int trace_active;
}trace_t;

trace_t* trace_create(heap_t* heap, fs_t* fs, int event_capacity)
{
	trace_t* trace = heap_alloc(heap, sizeof(trace_t), 8);
	trace->duration_cap = event_capacity;
	trace->duration_count = 0;
	trace->active_duration_count = 0;
	trace->durations = heap_alloc(heap, sizeof(duration_t*) * trace->duration_cap, 8);
	trace->active_durations = heap_alloc(heap, sizeof(duration_t*) * trace->duration_cap, 8);
	trace->trace_active = 0;
	trace->heap = heap;
	trace->fs = fs;
	trace->semaphore = semaphore_create(1, 1);

	return trace;
}

void trace_destroy(trace_t* trace)
{
	for(uint32_t k = 0; k < trace->active_duration_count; k++)
	{
		heap_free(trace->heap, trace->active_durations[k]->name);
		heap_free(trace->heap, trace->active_durations[k]);
	}
	heap_free(trace->heap, trace->active_durations);
	for(uint32_t k = 0; k < trace->duration_count; k++)
	{
		heap_free(trace->heap, trace->durations[k]->name);
		heap_free(trace->heap, trace->durations[k]);
	}
	heap_free(trace->heap, trace->durations);
	heap_free(trace->heap, trace->write_path);
	semaphore_destroy(trace->semaphore);

	heap_free(trace->heap, trace);
}

void trace_duration_push(trace_t* trace, const char* name)
{
	if (trace->duration_count >= trace->duration_cap)
	{
		debug_print(k_print_warning | k_print_error, "Failed to begin trace for duration %s; maximum number of durations reached\n", name);
		return;
	}

	if (trace->trace_active == 0)
	{
		debug_print(k_print_warning | k_print_error, "Failed to begin trace for duration %s, trace system is not active\n", name);
		return;
	}

	duration_t* temp = heap_alloc(trace->heap, sizeof(duration_t), 8);
	temp->name = heap_alloc(trace->heap, strlen(name) + 1, 8);
	strcpy_s(temp->name, strlen(name) +1, name);
	temp->ph = 'B';
	temp->time = timer_ticks_to_ms(timer_get_ticks());
	temp->process_id = 0;
	temp->thread_id = GetCurrentThreadId();

	semaphore_aquire(trace->semaphore);
	trace->durations[trace->duration_count] = temp;
	trace->duration_count++;
	trace->active_durations[trace->active_duration_count] = temp;
	trace->active_duration_count++;
	semaphore_release(trace->semaphore);
}

void trace_duration_pop(trace_t* trace)
{
	if (trace->trace_active == 0)
	{
		debug_print(k_print_warning | k_print_error, "Failed to end trace for duration, trace system is not active\n");
		return;
	}

	if (trace->active_duration_count == 0)
	{
		debug_print(k_print_warning | k_print_error, "Failed to end trace, no durations in stack\n");
		return;
	}

	duration_t* temp = heap_alloc(trace->heap, sizeof(duration_t), 8);
	temp->time = timer_ticks_to_ms(timer_get_ticks());
	semaphore_aquire(trace->semaphore);
	trace->active_duration_count--;
	temp->name = heap_alloc(trace->heap, strlen(trace->active_durations[trace->active_duration_count]->name) + 1, 8);
	strcpy_s(temp->name, strlen(trace->active_durations[trace->active_duration_count]->name) +1, trace->active_durations[trace->active_duration_count]->name);
	temp->ph = 'E';
	temp->process_id = 0;
	temp->thread_id = trace->active_durations[trace->active_duration_count]->thread_id;
	trace->durations[trace->duration_count] = temp;
	trace->active_durations[trace->active_duration_count] = NULL;
	trace->duration_count++;
	semaphore_release(trace->semaphore);
}

void trace_capture_start(trace_t* trace, const char* path)
{
	trace->write_path = heap_alloc(trace->heap, strlen(path) + 1, 8);
	strcpy_s(trace->write_path, strlen(path) + 1, path);
	trace->trace_active = 1;
}

void trace_capture_stop(trace_t* trace)
{
	trace->trace_active = 0;
	char* json_buffer = heap_alloc(trace->heap, TRACE_BUFFER_INIT_SIZE, 8);
	uint32_t buffer_capacity = TRACE_BUFFER_INIT_SIZE;
	uint32_t buffer_length = 0;
	sprintf_s(json_buffer, TRACE_TEMP_BUFFER_SIZE, "{\n\t\"displayTimeUnit\": \"ns\", \"traceEvents\": [\n");
	buffer_length = (uint32_t) strlen(json_buffer);
	for (uint32_t k = 0; k < trace->duration_count; k++)
	{
		char temp[TRACE_TEMP_BUFFER_SIZE];
		sprintf_s(temp, TRACE_TEMP_BUFFER_SIZE, "\t\t{\"name\":\"%s\",\"ph\":\"%c\",\"pid\":0,\"tid\":\"%u\",\"ts\":\"%u\"},\n", 
			trace->durations[k]->name, trace->durations[k]->ph, trace->durations[k]->thread_id, trace->durations[k]->time);
		if (buffer_length + strlen(temp)+ 1 > buffer_capacity)
		{
			buffer_capacity *= 2;
			json_buffer = heap_realloc(trace->heap, json_buffer, buffer_capacity, 8);
		}
		memcpy_s(json_buffer + buffer_length, buffer_capacity, temp, strlen(temp) + 1);
		buffer_length = (uint32_t) strlen(json_buffer);
	}
	buffer_length -= 2;
	char* temp = "\n\t]\t\n}\n";
	memcpy_s(json_buffer + buffer_length, buffer_capacity, temp, strlen(temp) + 1);
	buffer_length = (uint32_t) strlen(json_buffer);
	fs_work_t* work = fs_write(trace->fs, trace->write_path, json_buffer, buffer_length, false);
	fs_work_wait(work); 
	fs_work_destroy(work);
	heap_free(trace->heap, json_buffer);
}
