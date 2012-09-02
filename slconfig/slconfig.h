#ifndef _SLCONFIG_H
#define _SLCONFIG_H

#include <stdlib.h>
#include <stdbool.h>

typedef struct
{
	const char* start;
	const char* end;
} SLCONFIG_STRING;

typedef struct
{
	void* (*realloc)(void*, size_t);
	void (*stderr)(SLCONFIG_STRING s);
	void* (*fopen)(SLCONFIG_STRING filename, SLCONFIG_STRING mode);
	int (*fclose)(void*);
	size_t (*fread)(void*, size_t, size_t, void*);
} SLCONFIG_VTABLE;

size_t slc_string_length(SLCONFIG_STRING str);
bool slc_string_equal(SLCONFIG_STRING a, SLCONFIG_STRING b);
SLCONFIG_STRING slc_from_c_str(const char* str);
char* slc_to_c_str(SLCONFIG_STRING str);
void slc_append_to_string(SLCONFIG_STRING* dest, SLCONFIG_STRING new_str, void* (*custom_realloc)(void*, size_t));

typedef struct SLCONFIG SLCONFIG;

typedef struct SLCONFIG_NODE SLCONFIG_NODE;
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
	
	void* user_data;
	SLCONFIG_NODE* parent;
	bool is_aggregate;
	SLCONFIG* config;
};

SLCONFIG* slc_load_config(SLCONFIG_STRING filename);
SLCONFIG* slc_load_config_vtable(SLCONFIG_STRING filename, SLCONFIG_VTABLE vtable);
SLCONFIG* slc_load_config_string(SLCONFIG_STRING filename, SLCONFIG_STRING file);
SLCONFIG* slc_load_config_string_vtable(SLCONFIG_STRING filename, SLCONFIG_STRING file, SLCONFIG_VTABLE vtable);
SLCONFIG_NODE* slc_get_root(SLCONFIG* config);
SLCONFIG_STRING slc_get_full_name(SLCONFIG_NODE* node);

SLCONFIG_NODE* slc_add_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING type, bool own_type, SLCONFIG_STRING name, bool own_name, bool is_aggregate);
SLCONFIG_NODE* slc_get_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING name);
bool slc_set_value(SLCONFIG_NODE* node, SLCONFIG_STRING value, bool own);

void slc_destroy_config(SLCONFIG* config);
void slc_destroy_node(SLCONFIG_NODE* node);

#endif
