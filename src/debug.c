#include "debug.h"
#include "mutex.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <DbgHelp.h>

//also how do i view memory leaks inside of VS? can i use valgrind?
#define STACK_TRACE_SIZE 8
static uint32_t s_mask = 0xffffffff;

typedef struct trace_alloc_t //would it be bad form to move this definition into the header file?
{
	uint64_t mem_size; //are there any memory considerations we need to make here?
	void** trace_stack;
} trace_alloc_t;

typedef struct debug_system_t
{
	uint32_t trace_size;
}debug_system_t;

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

void debug_system_init(uint32_t trace_max)
{
	//initialize symbol system
	if(!SymInitialize(GetCurrentProcess(), NULL, TRUE))
		debug_print(k_print_warning, "Failed to initialize symbol system; traces will not return function information\n");

	/*
	debug_system_t* new_sys = malloc(sizeof(debug_system_t));
	if (!new_sys)
	{
		debug_print(k_print_warning, "Debug init error: memory allocation failed\n");
		return NULL;
	}
	new_sys->trace_size = trace_max;
	*/

	//allocate resources for stack tracing
	//can we use tlsf here?
	//but then we'd have to trace what's happening in the debug system, right?

	debug_print(k_print_debug, "debug_system_init() success\n");
	//return new_sys;
}

void debug_system_uninit()
{
	//for(uint32_t i = 0; i < sys->stack_count; i++)
	//kind of want to implement this using malloc so i can see leaks in VS first
	//then i change the calls to TLSF
	SymCleanup(GetCurrentProcess());
	debug_print(k_print_debug, "debug_system_uninit() success\n");
}

int debug_get_trace_size()
{
	//just returns size of trace information
	return sizeof(uint64_t) + sizeof(void*) * STACK_TRACE_SIZE;
}

void debug_record_trace(void* address, uint64_t mem_size)
{
	debug_print(k_print_debug, "recording trace at address %x\n", (intptr_t) address);
	trace_alloc_t* trace = (trace_alloc_t*)((intptr_t) address + mem_size);
	trace->mem_size = mem_size;
	trace->trace_stack = (void**)((intptr_t)address + mem_size + sizeof(mem_size));
	debug_backtrace(trace->trace_stack, STACK_TRACE_SIZE, 2);
	//debug_print(k_print_debug, "failed to initialize trace!\n");
}
/*

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
*/

void debug_print_trace(void* address, size_t mem_size)
{
	//if trace found, print call stack
	trace_alloc_t* trace = (trace_alloc_t*)((intptr_t) address + mem_size - debug_get_trace_size());
	debug_print(k_print_warning, "Memory leak of size %ul bytes with call stack:\n", trace->mem_size);
	for(uint32_t k = 0; k < STACK_TRACE_SIZE; k++)
	{ 
		//get info from symbols
		char buffer[sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME * sizeof(TCHAR)];
		DWORD64 trace_addr = (DWORD64) trace->trace_stack[k];
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
