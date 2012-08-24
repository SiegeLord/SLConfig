#include "dconfig/internal/utils.h"

#include <string.h>
#include <stdio.h>

void append_to_string(DCONFIG_STRING* dest, DCONFIG_STRING new_str, DCONFIG_VTABLE* vtable)
{
	size_t old_length = dcfg_string_length(*dest);
	size_t new_length = old_length + dcfg_string_length(new_str);
	dest->start = vtable->realloc((char*)dest->start, new_length);
	memcpy((char*)dest->start + old_length, new_str.start, new_length - old_length);
	dest->end = dest->start + new_length;
}

void _dcfg_print_error_prefix(DCONFIG_STRING filename, size_t line, DCONFIG_VTABLE* table)
{
	table->stderr(filename);
	table->stderr(dcfg_from_c_str(":"));
	char buf[32];
	snprintf(buf, 32, "%zu", line);
	table->stderr(dcfg_from_c_str(buf));
	table->stderr(dcfg_from_c_str(": "));
}
