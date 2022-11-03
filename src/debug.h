#pragma once

#include <stdint.h>
#include <stdio.h>

//debug functions

//bitmask for debug_print
typedef enum debug_print_t
{
	k_print_info = 1 << 0,
	k_print_warning = 1 << 1,
	k_print_error = 1 << 2,
	k_print_debug = 1 << 3
}debug_print_t;

typedef struct debug_system_t debug_system_t;

//log a message to the console if the type is in the active mask
void debug_print(uint32_t type, _Printf_format_string_ const char* format, ...);

//install unhandled exception handler. when unhandled exceptions are caught, it will log an error and capture a memory dump
void debug_install_exception_handler();

//set the mask of debug messages allowed to fire
void debug_set_print_mask(uint32_t mask);

//get the addresses of functions in the current callstack (after given offset)
//returns number of addresses captured
int debug_backtrace(void** stack, int stack_cap, int offset);

//record trace starting from func that called this.
//must be called after debug_system_init!
void debug_record_trace(debug_system_t* sys, void* address, uint64_t mem_size); 

//remove previously recorded trace at the given address.
void debug_remove_trace(debug_system_t* sys, void* address);

//print the names of functions in the stack previously recorded the memory at this address
void debug_print_trace(debug_system_t* sys, void* address);

//initialize debug system resources 
//should be called before any other functions in the debug system
//initializes semaphores too, so call before creating threads!
debug_system_t* debug_system_init(uint32_t trace_max);

//unitinialize debug system, freeing all resources
void debug_system_uninit(debug_system_t* sys);