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

DCONFIG_NODE* _dcfg_search_node(DCONFIG_NODE* aggregate, DCONFIG_STRING name);
DCONFIG_NODE* _dcfg_add_node_no_attach(DCONFIG_NODE* aggregate, DCONFIG_STRING type, bool own_type, DCONFIG_STRING name, bool own_name, bool is_aggregate);
void _dcfg_attach_node(DCONFIG_NODE* aggregate, DCONFIG_NODE* node);
void _dcfg_copy_into(DCONFIG_NODE* dest, DCONFIG_NODE* src);
void _dcfg_destroy_node(DCONFIG_NODE* node, bool detach);
void _dcfg_free(DCONFIG* config, void*);

#endif
