#ifndef _INTERNAL_SLCONFIG_H
#define _INTERNAL_SLCONFIG_H

#include "slconfig/slconfig.h"

struct SLCONFIG
{
	SLCONFIG_STRING* files;
	size_t num_files;
	
	SLCONFIG_NODE* root;
	
	SLCONFIG_VTABLE vtable;
};

SLCONFIG_NODE* _slc_search_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING name);
SLCONFIG_NODE* _slc_add_node_no_attach(SLCONFIG_NODE* aggregate, SLCONFIG_STRING type, bool own_type, SLCONFIG_STRING name, bool own_name, bool is_aggregate);
void _slc_attach_node(SLCONFIG_NODE* aggregate, SLCONFIG_NODE* node);
void _slc_copy_into(SLCONFIG_NODE* dest, SLCONFIG_NODE* src);
void _slc_destroy_node(SLCONFIG_NODE* node, bool detach);
void _slc_free(SLCONFIG* config, void*);

#endif
