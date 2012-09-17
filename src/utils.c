/* Copyright 2012 Pavel Sountsov
 * 
 * This file is part of SLConfig.
 *
 * SLConfig is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SLConfig is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser Public License for more details.
 *
 * You should have received a copy of the GNU Lesser Public License
 * along with SLConfig.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "slconfig/internal/utils.h"
#include "slconfig/internal/slconfig.h"

#include <string.h>
#include <stdio.h>

void slc_append_to_string(SLCONFIG_STRING* dest, SLCONFIG_STRING new_str, void* (*custom_realloc)(void*, size_t))
{
	if(!custom_realloc)
		custom_realloc = &realloc;
	size_t old_length = slc_string_length(*dest);
	size_t new_length = old_length + slc_string_length(new_str);
	dest->start = custom_realloc((void*)dest->start, new_length);
	memcpy((char*)dest->start + old_length, new_str.start, new_length - old_length);
	dest->end = dest->start + new_length;
}

void slc_destroy_string(SLCONFIG_STRING* str, void* (*custom_realloc)(void*, size_t))
{
	if(!custom_realloc)
		custom_realloc = &realloc;
	if(str->start)
		custom_realloc((void*)str->start, 0);
	str->start = str->end = 0;
}

/*
 * Just print a standard error prefix
 */
void _slc_print_error_prefix(SLCONFIG* config, SLCONFIG_STRING filename, size_t line, SLCONFIG_VTABLE* table)
{
	char buf[32];
	if(config->num_includes > 1)
	{
		for(size_t ii = 0; ii < config->num_includes - 1; ii++)
		{
			if(ii == 0)
				table->stderr(slc_from_c_str("In file included from "));
			else
				table->stderr(slc_from_c_str("                 from "));
			table->stderr(config->include_list[ii]);
			table->stderr(slc_from_c_str(":"));
			snprintf(buf, 32, "%zu", config->include_lines[ii]);
			table->stderr(slc_from_c_str(buf));
			if(ii == config->num_includes - 2)
				table->stderr(slc_from_c_str(":\n"));
			else
				table->stderr(slc_from_c_str(",\n"));
		}
	}
	table->stderr(filename);
	table->stderr(slc_from_c_str(":"));
	snprintf(buf, 32, "%zu", line);
	table->stderr(slc_from_c_str(buf));
	table->stderr(slc_from_c_str(": "));
}

/*
 * Error: Expected <expected> after '<after>', not '<actual>'
 */
void _slc_expected_after_error(SLCONFIG* config, TOKENIZER_STATE* state, size_t line, SLCONFIG_STRING expected, SLCONFIG_STRING after, SLCONFIG_STRING actual)
{
	_slc_print_error_prefix(config, state->filename, line, state->vtable);
	state->vtable->stderr(slc_from_c_str("Error: Expected "));
	state->vtable->stderr(expected);
	state->vtable->stderr(slc_from_c_str(" after '"));
	state->vtable->stderr(after);
	state->vtable->stderr(slc_from_c_str("', not '"));
	state->vtable->stderr(actual);
	state->vtable->stderr(slc_from_c_str("'.\n"));
}

/*
 * Error: Expected '<expected>' not '<actual>'
 */
void _slc_expected_error(SLCONFIG* config, TOKENIZER_STATE* state, size_t line, SLCONFIG_STRING expected, SLCONFIG_STRING actual)
{
	_slc_print_error_prefix(config, state->filename, line, state->vtable);
	state->vtable->stderr(slc_from_c_str("Error: Expected '"));
	state->vtable->stderr(expected);
	state->vtable->stderr(slc_from_c_str("', not '"));
	state->vtable->stderr(actual);
	state->vtable->stderr(slc_from_c_str("'.\n"));
}

size_t slc_string_length(SLCONFIG_STRING str)
{
	return str.end > str.start ? str.end - str.start : 0;
}

bool slc_string_equal(SLCONFIG_STRING a, SLCONFIG_STRING b)
{
	if(slc_string_length(a) != slc_string_length(b))
		return false;
	
	size_t len = slc_string_length(a);
	for(size_t ii = 0; ii < len; ii++)
	{
		if(a.start[ii] != b.start[ii])
			return false;
	}
	
	return true;
}

SLCONFIG_STRING slc_from_c_str(const char* str)
{
	SLCONFIG_STRING ret;
	ret.start = str;
	ret.end = str + strlen(str);
	return ret;
}

char* slc_to_c_str(SLCONFIG_STRING str)
{
	size_t len = slc_string_length(str);
	char* ret = malloc(len + 1);
	memcpy(ret, str.start, len);
	ret[len] = '\0';
	return ret;
}

/*
 * Just to avoid realloc(0, 0) and make the intention clear
 */
void _slc_free(SLCONFIG* config, void* ptr)
{
	if(ptr)
	{
		config->vtable.realloc(ptr, 0);
	}
}
