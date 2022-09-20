#include "debug.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <DbgHelp.h>

#define HASH_SIZE 20
#define STACK_TRACE_SIZE 10
static uint32_t s_mask = 0xffffffff;

typedef struct trace_alloc_t
{
	uintptr_t address;
	uint32_t mem_size;
	uint32_t trace_size;
	void** trace_stack;
} trace_alloc_t;

trace_alloc_t** stack_record;
uint32_t stack_count[HASH_SIZE];
uint32_t stack_count_max[HASH_SIZE];
HANDLE self_handle; //shouldn't need this anymore

uint32_t addr_hash(void* addr)
{
	return ((uint64_t) addr * 7) % HASH_SIZE;
}

static LONG debug_exception_handler(LPEXCEPTION_POINTERS ExceptionInfo)
{
	debug_print(k_print_error, "caught exception!\n");
	HANDLE file = CreateFile(L"ga2022-crash.dump", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION mini_exception = {0};
		mini_exception.ThreadId = GetCurrentThreadId();
		mini_exception.ExceptionPointers = ExceptionInfo;
		mini_exception.ClientPointers = FALSE;

		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpWithThreadInfo, &mini_exception, NULL, NULL);
		CloseHandle(file);
	}
	return EXCEPTION_EXECUTE_HANDLER;
}

void debug_install_exception_handler()
{
	//1. __try{} __except{}
	//2, vectored exception handler
	AddVectoredExceptionHandler(TRUE, debug_exception_handler);
}

void debug_set_print_mask(uint32_t mask)
{
	s_mask = mask;
}

void debug_print(uint32_t type, _Printf_format_string_ const char* format, ...)
{
	if((s_mask & type) == 0)
		return;

	va_list args;
	va_start(args, format);
	char buffer[256];
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	OutputDebugStringA(buffer);

	DWORD bytes = (DWORD) strlen(buffer);
	DWORD written = 0;
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

	WriteConsoleA(out, buffer, bytes, &written, NULL);
}

int debug_backtrace(void** stack, int stack_cap, int offset)
{
	return CaptureStackBackTrace(offset, stack_cap, stack, NULL);
}

void debug_system_init()
{
	SymInitialize(GetCurrentProcess(), NULL, TRUE);
	//if(!SymInitialize(self_handle, NULL, TRUE))
		//debug_print(k_print_error, "debug_system_init(): SymInitialize failed");

	stack_record = calloc(HASH_SIZE, sizeof(void***)); //using regular calloc so we aren't manipulating tlsf`
	for (int k = 0; k < HASH_SIZE; k++)
	{
		stack_count[k] = 0;
		stack_count_max[k] = 16;
		stack_record[k] = malloc(sizeof(trace_alloc_t) * stack_count_max[k]);
	}

	debug_print(k_print_info, "debug_system_init() success\n");
}

void debug_system_uninit()
{
	SymCleanup(GetCurrentProcess);
	debug_print(k_print_info, "debug_system_uninit() success\n");
}

void debug_record_trace(void* address, uint32_t mem_size)
{
	uint32_t place = addr_hash(address);
	//debug_print(k_print_warning, "memory allocated at address %x\n", address);
	void* temp_stack[STACK_TRACE_SIZE];

	if (!stack_record)
	{
		debug_print(k_print_error, "record_trace aborted, unititialized stack_record - did you call debug_system_init()?\n");
		return;
	}

	stack_record[place][stack_count[place]].address = (uintptr_t) address;
	stack_record[place][stack_count[place]].mem_size = mem_size;
	stack_record[place][stack_count[place]].trace_size = debug_backtrace(temp_stack, STACK_TRACE_SIZE, 3);
	stack_record[place][stack_count[place]].trace_stack = malloc(sizeof(void*) * stack_record[place][stack_count[place]].trace_size);

	for (unsigned int k = 0; k < stack_record[place][stack_count[place]].trace_size; k++)
	{
		stack_record[place][stack_count[place]].trace_stack[k] = temp_stack[k];
	}

	stack_count[place]++;
	/*
	if (stack_count[place] == stack_count_max[place]) //filled traces at this hash, double space
	{
		stack_count_max[place] *= 2;
		//what is happening here.
		trace_alloc_t* temp_alloc = (trace_alloc_t*) realloc(stack_record[place], stack_count_max[place]);
		if (temp_alloc)
		{
			debug_print(k_print_error, "debug_record_trace failed to reallocate trace record\n");
		}
		stack_record[place] = temp_alloc;
	}
	*/
	//debug_print(k_print_warning, "record_trace failed to record stack trace\n");
}

void debug_remove_trace(void* address)
{
	uint32_t place = addr_hash(address);
	for (unsigned int k = 0; k < stack_count[place]; k++)
	{
		if (stack_record[place][k].address == (uintptr_t) address)
		{
			free(stack_record[place][k].trace_stack);
			stack_record[place][k].address = NULL;
			stack_record[place][k].trace_size = 0;
			stack_record[place][k].mem_size = 0;
			break;
		}
	}
}

void debug_print_trace(void* address)
{
	//first, find the trace in the record
	uint32_t place = addr_hash(address);
	void** trace = NULL;
	uint32_t trace_size = 0;
	uint32_t mem_size = 0;
	for (unsigned int k = 0; k < stack_count[place]; k++)
	{
		if (stack_record[place][k].address == (uintptr_t) address)
		{
			trace = stack_record[place][k].trace_stack;
			trace_size = stack_record[place][k].trace_size;
			mem_size = stack_record[place][k].mem_size;
			break;
		}
	}
	if (!trace)
	{
		debug_print(k_print_warning, "debug_print_trace failed to find address in stack record");
		return;
	}

	debug_print(k_print_warning, "memory leak of %lu bytes with callstack\n", mem_size);
	for(unsigned int k = 0; k < trace_size; k++)
	{ 
		char buffer[sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME * sizeof(TCHAR)];

		DWORD64 trace_addr = (DWORD64) trace[k];
		DWORD64 displacement;
		IMAGEHLP_SYMBOL64 *sym = (IMAGEHLP_SYMBOL64*) buffer;
		sym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
		sym->MaxNameLength = MAX_PATH;
		if (SymGetSymFromAddr64(GetCurrentProcess(), trace_addr, &displacement, sym))
		{
			debug_print(k_print_warning, "[%d] %s\n", k, sym->Name);
		}
		else
		{
			debug_print(k_print_error, "debug_print_trace failed to retrieve symbol info; SymGetSym error %d\n", GetLastError());
		}
		if(strcmp(sym->Name, "main") == 0)
			break;
	}	
}
