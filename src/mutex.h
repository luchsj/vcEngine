// Recursive mutex thread synchronization

// Handle to a mutex.
typedef struct mutex_t mutex_t;

//creates a new mutex
mutex_t* mutex_create();

//destroys a previously created mutex
void mutex_destroy(mutex_t* m);

//locks a mutex. may block if another thread unlocks it
//if a thread locks a mutex multiple times, it must be unlocked multiple times
void mutex_lock(mutex_t* m);

//unlocks a mutex
void mutex_unlock(mutex_t* m);
