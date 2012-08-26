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
	void* (*fopen)(DCONFIG_STRING filename, DCONFIG_STRING mode);
	int (*fclose)(void*);
	size_t (*fread)(void*, size_t, size_t, void*);
} DCONFIG_VTABLE;

size_t dcfg_string_length(DCONFIG_STRING str);
bool dcfg_string_equal(DCONFIG_STRING a, DCONFIG_STRING b);
DCONFIG_STRING dcfg_from_c_str(const char* str);
char* dcfg_to_c_str(DCONFIG_STRING str);
void dcfg_append_to_string(DCONFIG_STRING* dest, DCONFIG_STRING new_str, void* (*custom_realloc)(void*, size_t));

typedef struct DCONFIG DCONFIG;

typedef struct DCONFIG_NODE DCONFIG_NODE;
struct DCONFIG_NODE
{
	DCONFIG_STRING type;
	bool own_type;
	DCONFIG_STRING name;
	bool own_name;
	DCONFIG_STRING value;
	bool own_value;
	DCONFIG_STRING comment;
	bool own_comment;
	
	DCONFIG_NODE** children;
	size_t num_children;
	
	void* user_data;
	DCONFIG_NODE* parent;
	bool is_aggregate;
	DCONFIG* config;
};

DCONFIG* dcfg_load_config(DCONFIG_STRING filename);
DCONFIG* dcfg_load_config_vtable(DCONFIG_STRING filename, DCONFIG_VTABLE vtable);
DCONFIG* dcfg_load_config_string(DCONFIG_STRING filename, DCONFIG_STRING file);
DCONFIG* dcfg_load_config_string_vtable(DCONFIG_STRING filename, DCONFIG_STRING file, DCONFIG_VTABLE vtable);
DCONFIG_NODE* dcfg_get_root(DCONFIG* config);
DCONFIG_STRING dcfg_get_full_name(DCONFIG_NODE* node);

DCONFIG_NODE* dcfg_add_node(DCONFIG_NODE* aggregate, DCONFIG_STRING type, bool own_type, DCONFIG_STRING name, bool own_name, bool is_aggregate);
DCONFIG_NODE* dcfg_get_node(DCONFIG_NODE* aggregate, DCONFIG_STRING name);

void dcfg_destroy_config(DCONFIG* config);
void dcfg_destroy_node(DCONFIG_NODE* node);

#endif
