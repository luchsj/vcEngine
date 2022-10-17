#include "debug.h"
#include "fs.h"
#include "heap.h"
#include "timer.h"
#include "trace.h"

#include <stddef.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define MAX_BUFFER_SIZE 2048

typedef struct duration_t
{
	char* name;
	char** categories;
	char ph;
	uint64_t begin_time;
	uint64_t end_time;
	uint64_t process_id;
	uint64_t thread_id;
}duration_t;

typedef struct trace_t
{
	duration_t** durations;
	duration_t** active_durations;
	uint64_t duration_count;
	uint64_t active_duration_count;
	uint64_t duration_cap;
	heap_t* heap;
	char* write_path;
	int trace_active;
}trace_t;

trace_t* trace_create(heap_t* heap, int event_capacity)
{
	trace_t* trace = heap_alloc(heap, sizeof(trace_t), 8);
	trace->duration_cap = 100;
	trace->duration_count = 0;
	trace->active_duration_count = 0;
	trace->durations = heap_alloc(heap, sizeof(trace_t*) * trace->duration_cap, 8);
	trace->active_durations = heap_alloc(heap, sizeof(trace_t*) * trace->duration_cap, 8); //FILO array of active durations
	trace->trace_active = 0;

	return trace;
}

void trace_destroy(trace_t* trace)
{
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
		debug_print(k_print_warning | k_print_error, "Failed to begin trace for duration %s, trace system has not started", name);
		return;
	}

	duration_t* temp = heap_alloc(trace->heap, sizeof(duration_t), 8);
	temp->name = heap_alloc(trace->heap, sizeof(strlen(name) + 1), 8);
	strcpy(temp->name, name);
	
	temp->begin_time = timer_ticks_to_ms(timer_get_ticks());
	trace->active_durations[trace->active_duration_count] = temp;
	trace->active_duration_count++;
}

void trace_duration_pop(trace_t* trace)
{
	duration_t* temp = trace->active_durations[trace->active_duration_count];
	temp->end_time = timer_ticks_to_ms(timer_get_ticks());
	trace->durations[trace->duration_count] = trace->active_durations[trace->active_duration_count];
	trace->active_durations[trace->duration_count] = NULL;
	trace->active_duration_count--;
	trace->active_duration_count++;
}

void trace_capture_start(trace_t* trace, const char* path)
{
	trace->write_path = heap_alloc(trace->heap, sizeof(strlen(path) + 1), 8);
	strcpy(trace->write_path, path);
	trace->trace_active = 1;
}

void trace_capture_stop(trace_t* trace)
{
	trace->trace_active = 0;
	//write to file
	char json_buffer[MAX_BUFFER_SIZE];
	int buffer_len;
	sprintf(json_buffer, "{\n\t\"displayTimeUnit\": \"ns\", \"traceEvents\": [\n\0");
	buffer_len = strlen(json_buffer);
	for (int k = 0; k < trace->duration_count; k++)
	{
		int prev_size = buffer_len;
		if (prev_size >= MAX_BUFFER_SIZE)
		{
			debug_print(k_print_error, "Trace write failed: trace data exceeded buffer\n");
			return;
		}
		sprintf(json_buffer + prev_size, "{\"%s\":\",\"ph\":\"%c\",\"pid\":0,\"tid\":IMPLEMENT,\"ts\":\"10\"}\0",
			trace->durations[k]->name, trace->durations[k]->ph);
	}
}
