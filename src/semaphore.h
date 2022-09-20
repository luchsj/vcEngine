//Conting semaphore thread synchronization

//handle to a semathore
typedef struct semapthore_t semaphore_t;

//creates a new semaphore
semaphore_t* semaphore_create(int initial_count, int max_count);

//destroy a previously created semaphore
void semaphore_destroy(semaphore_t* semaphore);

//lowers the semaphore count by one
//if the count is zero, blocks until another thread releases
void semaphore_aquire(semaphore_t* semaphore);

//raises the semaphore count by one
void semaphore_release(semaphore_t* semaphore);
