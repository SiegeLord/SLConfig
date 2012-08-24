#include "dconfig/dconfig.h"

#include <string.h>

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
