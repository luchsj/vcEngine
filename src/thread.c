#include "thread.h"

#include "debug.h"
#include <Windows.h>

typedef struct thread_t thread_t;

thread_t* thread_create(int (*func) (void*), void* data)
{
	HANDLE h = CreateThread(NULL, 0, func, data, CREATE_SUSPENDED, NULL);
	if(h == INVALID_HANDLE_VALUE)
	{
		debug_print(k_print_warning, "Thread failed to create\n");
		return NULL;
	}
	ResumeThread(h);
	return (thread_t*)h;
}

int thread_destroy(thread_t* thread)
{
	WaitForSingleObject(thread, INFINITE);
	int exit = 0;
	GetExitCodeThread(thread, &exit);
	CloseHandle(thread);
	return exit;
}

void thread_sleep(uint32_t ms)
{
	Sleep(ms);
}
