#include <stdint.h>
#include <stdio.h>
typedef enum debug_print_t
{
	k_print_info = 1 << 0,
	k_print_warning = 1 << 1,
	k_print_error = 1 << 2
}debug_print_t;

void debug_print_line(uint32_t type, _Printf_format_string_ const char* format, ...);

void debug_set_print_mask(uint32_t mask);