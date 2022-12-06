#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Threading support.

// Handle to a thread.
typedef struct thread_t thread_t;

//create a new thread that performs the given function
thread_t* thread_create(int (*func) (void*), void* data);

// Waits for a thread to complete and destroys it.
// Returns the thread's exit code.
int thread_destroy(thread_t* thread);

// Puts the calling thread to sleep for the specified number of milliseconds.
// Thread will sleep for *approximately* the specified time.
void thread_sleep(uint32_t ms);

#ifdef __cplusplus
}
#endif
