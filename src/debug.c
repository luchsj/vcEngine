#include "debug.h"
#include <stdarg.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static uint32_t s_mask = 0;

void debug_set_print_mask(uint32_t mask)
{
	s_mask = mask;
}

void debug_print_line(uint32_t type, _Printf_format_string_ const char* format, ...)
{
	if((s_mask & type) == 0)
		return;

	va_list args;
	va_start(args, format);
	char buffer[256];
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	DWORD bytes = (DWORD) strlen(buffer);
	DWORD written = 0;
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

	WriteConsoleA(out, buffer, bytes, &written, NULL);
}