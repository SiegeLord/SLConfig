#ifndef INTERNAL_DCONFIG_H
#define INTERNAL_DCONFIG_H

#include "dconfig/dconfig.h"

struct DCONFIG
{
	DCONFIG_STRING* files;
	size_t num_files;
	
	DCONFIG_NODE* root;
	
	DCONFIG_VTABLE vtable;
};

#endif
