//thread-safe queue container

typedef struct queue_t queue_t;

typedef struct heap_t heap_t;

//create queue with defined capacity
queue_t* queue_create(heap_t* heap, int capacity);

//destroy previously created queue
void queue_destroy(queue_t* queue);

//push item onto queue. if queue is full, block until space is available
//safe for multiple threads to push at the same time
void queue_push(queue_t* queue, void* item);

//pop an item off the queue (FIFO). if queue is empty, blocks until an item is available
//safe for multiple threads to pop at the same time
void* queue_pop(queue_t* queue);

