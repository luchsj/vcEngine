#include <stdint.h>
#include <stdio.h>

//debug system
//some functions (namely stack tracing) require debug_system_init to be called before they are used

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
int debug_backtrace(void** stack, int stack_cap, int offset);

//record trace starting from func that called this. stores address trace as well as requested memory size.
// should i store strings instead of addresses so i can ignore everything after main?
//must be called after debug_system_init!
void debug_record_trace(void* address, uint32_t mem_size); 

//remove previously created trace for given address
void debug_remove_trace(void* address);

//clear all traces from the record (without releasing structure)
void debug_destroy_traces();

//print the names of functions in the stack associated with the memory at this address
//(if it exists)
void debug_print_trace(void* address);

//initialize resources used by debug system 
void debug_system_init();

//unitinialize debug system 
void debug_system_uninit();