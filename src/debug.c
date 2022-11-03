#include "debug.h"
#include "mutex.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <DbgHelp.h>

#define HASH_SIZE 20
#define STACK_TRACE_SIZE 10
#define STACK_COUNT_MAX 128 //this is extremely inefficient memory-wise. we need to use realloc, but that makes TLSF confused. 
//TLSF has realloc, but would using it in this system mess things up? would it be ok to create a debug-exclusive heap that won't track its own leaks?
//also how do i view memory leaks inside of VS? can i use valgrind?
//what parts of debug systems end up in shipped code? what would we change for a release version?
//what are the system calls to use thread-specific stacks?
static uint32_t s_mask = 0xffffffff;

typedef struct trace_alloc_t
{
	uintptr_t address;
	uint64_t mem_size; //are there any memory considerations we need to make here?
	uint64_t trace_size;
	void** trace_stack;
} trace_alloc_t;

typedef struct debug_system_t
{
	trace_alloc_t*** stack_record;
	mutex_t* mutex;
	uint32_t stack_count[HASH_SIZE];
	uint32_t stack_count_max[HASH_SIZE];
	uint32_t trace_count_max;
} debug_system_t;

uint32_t addr_hash(void* addr)
{
	return ((uint64_t) addr) % HASH_SIZE;
}

static LONG debug_exception_handler(LPEXCEPTION_POINTERS ExceptionInfo)
{
	debug_print(k_print_error, "caught exception!\n");
	HANDLE file = CreateFile(L"ga2022-crash.dmp", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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

debug_system_t* debug_system_init(uint32_t trace_max)
{
	//initialize symbol system
	if(!SymInitialize(GetCurrentProcess(), NULL, TRUE))
		debug_print(k_print_warning, "Failed to initialize symbol system; traces will not return function information\n");

	debug_system_t* new_sys = malloc(sizeof(debug_system_t));
	if (!new_sys)
	{
		debug_print(k_print_warning, "Debug init error: memory allocation failed\n");
		return NULL;
	}

	//allocate resources for stack tracing
	new_sys->stack_record = malloc(HASH_SIZE * sizeof(trace_alloc_t**)); //using regular calloc so we aren't manipulating tlsf
	//can we use tlsf here?
	//but then we'd have to trace what's happening in the debug system, right?

	if (new_sys->stack_record == NULL)
	{
		debug_print(k_print_warning, "Debug init error: memory allocation failed\n");
		return NULL;
	}

	for (int k = 0; k < HASH_SIZE; k++)
	{
		new_sys->stack_count[k] = 0;
		new_sys->stack_count_max[k] = STACK_COUNT_MAX;
		if(new_sys->stack_record)
			new_sys->stack_record[k] = malloc(sizeof(trace_alloc_t*) * new_sys->stack_count_max[k]);
	}

	new_sys->trace_count_max = trace_max;
	new_sys->mutex = mutex_create();

	debug_print(k_print_debug, "debug_system_init() success\n");
	return new_sys;
}

void debug_system_uninit(debug_system_t* sys)
{
	//for(uint32_t i = 0; i < sys->stack_count; i++)
	//kind of want to implement this using malloc so i can see leaks in VS first
	//then i change the calls to TLSF
	SymCleanup(GetCurrentProcess());
	debug_print(k_print_debug, "debug_system_uninit() success\n");
}

void debug_record_trace(debug_system_t* sys, void* address, uint64_t mem_size)
{
	//get memory info
	uint64_t place = addr_hash(address);
	debug_print(k_print_debug, "recording trace at address %x\n", (uintptr_t) address);
	void* temp_stack[STACK_TRACE_SIZE];

	if (!sys->stack_record)
	{
		debug_print(k_print_warning, "record_trace aborted, unititialized stack_record - did you call debug_system_init()?\n");
		return;
	}

	if(sys->stack_count[place] >= sys->stack_count_max[place] - 1)
	{
		debug_print(k_print_warning, "record_trace aborted, over stack trace limit\n");
		return;
	}

	if(sys->stack_record[place][sys->stack_count[place]])
		sys->stack_record[place][sys->stack_count[place]] = calloc(1, sizeof(trace_alloc_t));
	if(sys->stack_record[place][sys->stack_count[place]] != NULL)
	{
		sys->stack_record[place][sys->stack_count[place]]->address = (uintptr_t) address;
		sys->stack_record[place][sys->stack_count[place]]->mem_size = mem_size;
		sys->stack_record[place][sys->stack_count[place]]->trace_size = debug_backtrace(temp_stack, STACK_TRACE_SIZE, 2);
		sys->stack_record[place][sys->stack_count[place]]->trace_stack = malloc(sizeof(void*) * sys->stack_record[place][sys->stack_count[place]]->trace_size);

		for (int k = 0; k < sys->stack_record[place][sys->stack_count[place]]->trace_size; k++)
		{
			sys->stack_record[place][sys->stack_count[place]]->trace_stack[k] = temp_stack[k];
		}

		sys->stack_count[place]++;
	}
	else
	{
		debug_print(k_print_debug, "failed to initialize trace!\n");
	}
}

void debug_remove_trace(debug_system_t* sys, void* address)
{
	debug_print(k_print_debug, "removing trace at address %x\n", (uintptr_t)address);
	uint64_t place = addr_hash(address);
	for (uint32_t k = 0; k < sys->stack_count[place]; k++)
	{
		if (sys->stack_record[place][k]->address == (uintptr_t)address)
		{
			//for(int c = 0; c < stack_record[place][k]->trace_size; c++)
				//free(stack_record[place][k]->trace_stack[c]);
			free(sys->stack_record[place][k]->trace_stack);
			sys->stack_record[place][k]->address = 0;

			sys->stack_count[place]--;
			return;
		}
	}
}

void debug_print_trace(debug_system_t* sys, void* address)
{
	//find address in trace record
	uint32_t place = addr_hash(address);
	void** trace = NULL;
	uint64_t trace_size = 0;
	uint64_t mem_size = 0;
	for (uint32_t k = 0; k < sys->stack_count[place]; k++)
	{
		if (sys->stack_record[place][k]->address == (uintptr_t) address)
		{
			trace = sys->stack_record[place][k]->trace_stack;
			trace_size = sys->stack_record[place][k]->trace_size;
			mem_size = sys->stack_record[place][k]->mem_size;
			break;
		}
	}
	if (!trace)
	{
		debug_print(k_print_warning, "debug_print_trace failed to find given address in stack record\n");
		return;
	}

	//if trace found, print call stack
	debug_print(k_print_warning, "Memory leak of size %u bytes with call stack:\n", mem_size);
	for(int k = 0; k < trace_size; k++)
	{ 
		//get info from symbols
		char buffer[sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME * sizeof(TCHAR)];
		DWORD64 trace_addr = (DWORD64) trace[k];
		DWORD64 displacement;
		IMAGEHLP_SYMBOL64 *sym = (IMAGEHLP_SYMBOL64*) buffer;
		IMAGEHLP_LINE64 line;
		sym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
		sym->MaxNameLength = MAX_PATH;
		line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
		if (!SymGetSymFromAddr64(GetCurrentProcess(), trace_addr, &displacement, sym))
		{
			debug_print(k_print_warning, "debug_print_trace failed to retrieve symbol info; SymGetSym error %d\n", GetLastError());
		}

		if (!SymGetLineFromAddr64(GetCurrentProcess(), trace_addr, (PDWORD) & displacement, &line))
{
			debug_print(k_print_warning, "debug_print_trace failed to retrieve symbol info; SymGetLine error %d\n", GetLastError());
		}

		debug_print(k_print_warning, "[%d] %s at %s:%d\n", k, sym->Name, strrchr(line.FileName, '\\') + 1, line.LineNumber);

		if(strcmp(sym->Name, "main") == 0)
			break;
	}	
}
