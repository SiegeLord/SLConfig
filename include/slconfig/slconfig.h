#ifndef _SLCONFIG_H
#define _SLCONFIG_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

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
void slc_destroy_string(SLCONFIG_STRING* str, void* (*custom_realloc)(void*, size_t));

typedef struct SLCONFIG SLCONFIG;
typedef struct SLCONFIG_NODE SLCONFIG_NODE;

SLCONFIG* slc_create_config(const SLCONFIG_VTABLE* vtable);
void slc_destroy_config(SLCONFIG* config);

void slc_add_search_directory(SLCONFIG* config, SLCONFIG_STRING directory, bool copy);
void slc_clear_search_directories(SLCONFIG* config);

bool slc_load_config(SLCONFIG* config, SLCONFIG_STRING filename);
bool slc_load_config_string(SLCONFIG* config, SLCONFIG_STRING filename, SLCONFIG_STRING file, bool copy);

SLCONFIG_NODE* slc_get_root(SLCONFIG* config);
SLCONFIG_STRING slc_get_full_name(SLCONFIG_NODE* node);

SLCONFIG_NODE* slc_add_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING type, bool copy_type, SLCONFIG_STRING name, bool copy_name, bool is_aggregate);
SLCONFIG_NODE* slc_get_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING name);

bool slc_set_value(SLCONFIG_NODE* node, SLCONFIG_STRING value, bool copy);
void slc_set_user_data(SLCONFIG_NODE* node, intptr_t data, void (*user_destructor)(intptr_t));
size_t slc_get_num_children(SLCONFIG_NODE* node);
SLCONFIG_NODE* slc_get_node_by_index(SLCONFIG_NODE* aggregate, size_t idx);
SLCONFIG_NODE* slc_get_node_by_reference(SLCONFIG_NODE* aggregate, SLCONFIG_STRING reference);
SLCONFIG_STRING slc_get_name(SLCONFIG_NODE* node);
SLCONFIG_STRING slc_get_type(SLCONFIG_NODE* node);
bool slc_is_aggregate(SLCONFIG_NODE* node);
SLCONFIG_STRING slc_get_value(SLCONFIG_NODE* node);
SLCONFIG_STRING slc_get_comment(SLCONFIG_NODE* node);

void slc_destroy_node(SLCONFIG_NODE* node);

#endif
