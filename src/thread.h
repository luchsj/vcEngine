// Threading support.

// Handle to a thread.
typedef struct thread_t thread_t;

//create a new thread that performs the given function
thread_t* thread_create(int (*func) (void*), void* data);

//destroy an existing thread
int thread_destroy(thread_t* thread);