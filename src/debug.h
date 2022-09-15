#include <stdint.h>
#include <stdio.h>

typedef enum debug_print_t
{
	k_print_info = 1 << 0,
	k_print_warning = 1 << 1,
	k_print_error = 1 << 2
}debug_print_t;

//log a message to the console if the type is in the active mask
void debug_print(uint32_t type, _Printf_format_string_ const char* format, ...);

//install unhandled exception handler. when unhandled exceptions are caught, it will log an error and capture a memory dump
void debug_install_exception_handler();

//set the mask of debug messages allowed to fire
void debug_set_print_mask(uint32_t mask);

//get the addresses of functions in the current callstack
//returns number of addresses captured
int debug_backtrace(void** stack, int stack_cap);