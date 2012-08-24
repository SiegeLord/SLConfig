#ifndef DCONFIG_H
#define DCONFIG_H

#include <stdlib.h>
#include <stdbool.h>

typedef struct
{
	const char* start;
	const char* end;
} DCONFIG_STRING;

typedef struct
{
	void* (*realloc)(void*, size_t);
	void (*stderr)(DCONFIG_STRING s);
} DCONFIG_VTABLE;

size_t dcfg_string_length(DCONFIG_STRING str);
bool dcfg_string_equal(DCONFIG_STRING a, DCONFIG_STRING b);
DCONFIG_STRING dcfg_from_c_str(const char* str);

#endif
