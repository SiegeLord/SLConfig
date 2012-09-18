#ifndef _INTERNAL_SLCONFIG_H
#define _INTERNAL_SLCONFIG_H

#include "slconfig/slconfig.h"

struct SLCONFIG
{
	SLCONFIG_STRING* files;
	size_t num_files;
	
	SLCONFIG_NODE* root;
	
	SLCONFIG_VTABLE vtable;
	
	/* Include business */
	SLCONFIG_STRING* include_list;
	size_t* include_lines;
	bool* include_ownerships;
	size_t num_includes;
};

struct SLCONFIG_NODE
{
	SLCONFIG_STRING type;
	bool own_type;
	SLCONFIG_STRING name;
	bool own_name;
	SLCONFIG_STRING value;
	bool own_value;
	SLCONFIG_STRING comment;
	bool own_comment;
	
	SLCONFIG_NODE** children;
	size_t num_children;
	
	intptr_t user_data;
	void (*user_destructor)(intptr_t);
	
	SLCONFIG_NODE* parent;
	bool is_aggregate;
	SLCONFIG* config;
};

SLCONFIG_NODE* _slc_search_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING name);
SLCONFIG_NODE* _slc_add_node_no_attach(SLCONFIG_NODE* aggregate, SLCONFIG_STRING type, bool copy_type, SLCONFIG_STRING name, bool copy_name, bool is_aggregate);
void _slc_attach_node(SLCONFIG_NODE* aggregate, SLCONFIG_NODE* node);
void _slc_copy_into(SLCONFIG_NODE* dest, SLCONFIG_NODE* src);
void _slc_destroy_node(SLCONFIG_NODE* node, bool detach);
void _slc_free(SLCONFIG* config, void*);
void _slc_add_file(SLCONFIG* config, SLCONFIG_STRING new_file);
bool _slc_load_file(SLCONFIG* config, SLCONFIG_STRING filename, SLCONFIG_STRING* file);

bool _slc_add_include(SLCONFIG* config, SLCONFIG_STRING filename, bool own, size_t line);
void _slc_pop_include(SLCONFIG* config);
void _slc_clear_includes(SLCONFIG* config);

#endif
