#include "dconfig/internal/utils.h"
#include "dconfig/internal/dconfig.h"

#include <string.h>
#include <stdio.h>

void dcfg_append_to_string(DCONFIG_STRING* dest, DCONFIG_STRING new_str, void* (*custom_realloc)(void*, size_t))
{
	if(!custom_realloc)
		custom_realloc = &realloc;
	size_t old_length = dcfg_string_length(*dest);
	size_t new_length = old_length + dcfg_string_length(new_str);
	dest->start = custom_realloc((void*)dest->start, new_length);
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

void _dcfg_expected_after_error(TOKENIZER_STATE* state, size_t line, DCONFIG_STRING expected, DCONFIG_STRING after, DCONFIG_STRING actual)
{
	_dcfg_print_error_prefix(state->filename, line, state->vtable);
	state->vtable->stderr(dcfg_from_c_str("Error: Expected "));
	state->vtable->stderr(expected);
	state->vtable->stderr(dcfg_from_c_str(" after '"));
	state->vtable->stderr(after);
	state->vtable->stderr(dcfg_from_c_str("', not '"));
	state->vtable->stderr(actual);
	state->vtable->stderr(dcfg_from_c_str("'.\n"));
}

void _dcfg_expected_error(TOKENIZER_STATE* state, size_t line, DCONFIG_STRING expected, DCONFIG_STRING actual)
{
	_dcfg_print_error_prefix(state->filename, line, state->vtable);
	state->vtable->stderr(dcfg_from_c_str("Error: Expected '"));
	state->vtable->stderr(expected);
	state->vtable->stderr(dcfg_from_c_str("', not '"));
	state->vtable->stderr(actual);
	state->vtable->stderr(dcfg_from_c_str("'.\n"));
}

size_t dcfg_string_length(DCONFIG_STRING str)
{
	return str.end > str.start ? str.end - str.start : 0;
}

bool dcfg_string_equal(DCONFIG_STRING a, DCONFIG_STRING b)
{
	if(dcfg_string_length(a) != dcfg_string_length(b))
		return false;
	
	size_t len = dcfg_string_length(a);
	for(size_t ii = 0; ii < len; ii++)
	{
		if(a.start[ii] != b.start[ii])
			return false;
	}
	
	return true;
}

DCONFIG_STRING dcfg_from_c_str(const char* str)
{
	DCONFIG_STRING ret;
	ret.start = str;
	ret.end = str + strlen(str);
	return ret;
}

char* dcfg_to_c_str(DCONFIG_STRING str)
{
	size_t len = dcfg_string_length(str);
	char* ret = malloc(len + 1);
	memcpy(ret, str.start, len);
	ret[len] = '\0';
	return ret;
}

void _dcfg_free(DCONFIG* config, void* ptr)
{
	if(ptr)
	{
		config->vtable.realloc(ptr, 0);
	}
}
