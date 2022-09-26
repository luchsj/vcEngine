// event thread synchronization
#include <stdbool.h>

//handle to an event
typedef struct event_t event_t;

//creates new event
event_t* event_create();

//destroy previously created event
void event_destroy(event_t* event);

//signals an event
//all threads waiting on this event will resume
void event_signal(event_t* event);

//waits for an event to be signaled
void event_wait(event_t* event);

//determine if an event is signaled
bool event_is_raised(event_t* event);
