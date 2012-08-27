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

void _dcfg_clear_node(DCONFIG_NODE* node);
DCONFIG_NODE* _dcfg_search_node(DCONFIG_NODE* aggregate, DCONFIG_STRING name);

#endif
