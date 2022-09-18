typedef struct mutex_t mutex_t;

mutex_t* mutex_create();
void mutex_destroy(mutex_t m);
void mutex_lock(mutex_t m);
void mutex_unlock(mutex_t m);
