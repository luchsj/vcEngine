#include "debug.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <DbgHelp.h>

#define HASH_SIZE 20
#define STACK_TRACE_SIZE 10
static uint32_t s_mask = 0xffffffff;

typedef struct trace_alloc_t
{
	uintptr_t address;
	uint64_t mem_size;
	uint64_t trace_size;
	void** trace_stack;
} trace_alloc_t;

trace_alloc_t** stack_record;
uint64_t stack_count[HASH_SIZE];
uint64_t stack_count_max[HASH_SIZE];

uint64_t addr_hash(void* addr)
{
	return ((uint64_t) addr) % HASH_SIZE;
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
	if(SymInitialize(GetCurrentProcess(), NULL, TRUE))
		//debug_print(k_print_error, "debug_system_init(): SymInitialize failed");

	stack_record = calloc(HASH_SIZE, sizeof(void***)); //using regular calloc so we aren't manipulating tlsf`
	for (int k = 0; k < HASH_SIZE; k++)
	{
		stack_count[k] = 0;
		stack_count_max[k] = 16;
		stack_record[k] = malloc(sizeof(trace_alloc_t) * stack_count_max[k]);
	}

	debug_print(k_print_debug, "debug_system_init() success\n");
}

void debug_system_uninit()
{
	SymCleanup(GetCurrentProcess());
	debug_print(k_print_debug, "debug_system_uninit() success\n");
}

void debug_record_trace(void* address, uint64_t mem_size)
{
	uint64_t place = addr_hash(address);
	debug_print(k_print_debug, "recording trace at address %x\n", address);
	void* temp_stack[STACK_TRACE_SIZE];

	if (!stack_record)
	{
		debug_print(k_print_error, "record_trace aborted, unititialized stack_record - did you call debug_system_init()?\n");
		return;
	}

	stack_record[place][stack_count[place]].address = (uintptr_t) address;
	stack_record[place][stack_count[place]].mem_size = mem_size;
	stack_record[place][stack_count[place]].trace_size = debug_backtrace(temp_stack, STACK_TRACE_SIZE, 2);
	stack_record[place][stack_count[place]].trace_stack = malloc(sizeof(void*) * stack_record[place][stack_count[place]].trace_size);


	for (int k = 0; k < stack_record[place][stack_count[place]].trace_size; k++)
	{
		stack_record[place][stack_count[place]].trace_stack[k] = temp_stack[k];
	}

	stack_count[place]++;
}

void debug_print_trace(void* address)
{
	//first, find the trace in the record
	uint64_t place = addr_hash(address);
	void** trace = NULL;
	uint64_t trace_size = 0;
	uint64_t mem_size = 0;
	for (int k = 0; k < stack_count[place]; k++)
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
		debug_print(k_print_warning, "debug_print_trace failed to find given address in stack record");
		return;
	}

	debug_print(k_print_warning, "Memory leak of size %lu bytes with call stack:\n", mem_size);
	for(int k = 0; k < trace_size; k++)
	{ 
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
			debug_print(k_print_error, "debug_print_trace failed to retrieve symbol info; SymGetSym error %d\n", GetLastError());
		}

		if (!SymGetLineFromAddr64(GetCurrentProcess(), trace_addr, &displacement, &line))
		{
			debug_print(k_print_error, "debug_print_trace failed to retrieve symbol info; SymGetLine error %d\n", GetLastError());
		}

		debug_print(k_print_info, "[%d] %s at %s:%d\n", k, sym->Name, strrchr(line.FileName, '\\') + 1, line.LineNumber);

		if(strcmp(sym->Name, "main") == 0)
			break;
	}	
}
