#include "dconfig/dconfig.h"
#include "dconfig/internal/dconfig.h"
#include "dconfig/internal/parser.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static
void default_stderr(DCONFIG_STRING s)
{
	fprintf(stderr, "%.*s", (int)dcfg_string_length(s), s.start);
}

static
void* default_fopen(DCONFIG_STRING filename, DCONFIG_STRING mode)
{
	char* filename_str = dcfg_to_c_str(filename);
	char* mode_str = dcfg_to_c_str(mode);
	void* ret = fopen(filename_str, mode_str);
	free(filename_str);
	free(mode_str);
	return ret;
}

static
int default_fclose(void* f)
{
	return fclose(f);
}

static
size_t default_fread(void* buf, size_t size, size_t nmemb, void* f)
{
	return fread(buf, size, nmemb, f);
}

static
void* default_realloc(void* buf, size_t size)
{
	//printf("%p\n", buf);
	return realloc(buf, size);
}

DCONFIG_VTABLE default_vtable =
{
	&default_realloc,
	&default_stderr,
	&default_fopen,
	&default_fclose,
	&default_fread
};

static
void fill_vtable(DCONFIG_VTABLE* vtable)
{
#define FILL(a) if(!vtable->a) vtable->a = default_vtable.a;
	FILL(realloc);
	FILL(stderr);
	FILL(fopen);
	FILL(fclose);
	FILL(fread);
#undef FILL
}

DCONFIG* dcfg_load_config(DCONFIG_STRING filename)
{
	return dcfg_load_config_vtable(filename, default_vtable);
}

DCONFIG* dcfg_load_config_vtable(DCONFIG_STRING filename, DCONFIG_VTABLE vtable)
{
	DCONFIG_STRING file;
	
	fill_vtable(&vtable);

#define BUF_SIZE (1024)
	size_t total_bytes_read = 0;
	size_t bytes_read;
	char* buff = 0;
	void* f = vtable.fopen(filename, dcfg_from_c_str("rb"));
	
	do
	{
		buff = vtable.realloc(buff, total_bytes_read + BUF_SIZE);
		bytes_read = vtable.fread(buff + total_bytes_read, 1, BUF_SIZE, f);
		total_bytes_read += bytes_read;
	} while(bytes_read == BUF_SIZE);
	
	vtable.fclose(f);
	
	file.start = buff;
	file.end = buff + total_bytes_read;
	
	return dcfg_load_config_string_vtable(filename, file, vtable);
}

DCONFIG* dcfg_load_config_string(DCONFIG_STRING filename, DCONFIG_STRING file)
{
	return dcfg_load_config_string_vtable(filename, file, default_vtable);
}

DCONFIG* dcfg_load_config_string_vtable(DCONFIG_STRING filename, DCONFIG_STRING file, DCONFIG_VTABLE vtable)
{
	fill_vtable(&vtable);
	
	DCONFIG* ret = vtable.realloc(0, sizeof(DCONFIG));
	ret->vtable = vtable;
	ret->files = 0;
	ret->num_files = 0;
	ret->root = vtable.realloc(0, sizeof(DCONFIG_NODE));
	memset(ret->root, 0, sizeof(DCONFIG_NODE));
	ret->root->is_aggregate = true;
	ret->root->config = ret;
	
	if(!_dcfg_parse_file(ret, ret->root, filename, file))
	{
		dcfg_destroy_config(ret);
		return 0;
	}
	
	return ret;
}

void dcfg_destroy_config(DCONFIG* config)
{
	if(!config)
		return;
	
	dcfg_destroy_node(config->root);
	
	for(size_t ii = 0; ii < config->num_files; ii++)
		config->vtable.realloc((void*)config->files[ii].start, 0);
	
	config->vtable.realloc(config->files, 0);
	
	config->vtable.realloc(config, 0);
}

void _dcfg_clear_node(DCONFIG_NODE* node)
{
	//printf("Clearing %.*s\n", (int)dcfg_string_length(node->name), node->name.start);
	if(node->own_type)
		node->config->vtable.realloc((void*)node->type.start, 0);

	if(node->own_name)
		node->config->vtable.realloc((void*)node->name.start, 0);

	if(node->own_value)
		node->config->vtable.realloc((void*)node->value.start, 0);
	
	if(node->own_comment)
		node->config->vtable.realloc((void*)node->comment.start, 0);
	
	for(size_t ii = 0; ii < node->num_children; ii++)
		dcfg_destroy_node(node->children[ii]);
	
	node->config->vtable.realloc(node->children, 0);
}

void dcfg_destroy_node(DCONFIG_NODE* node)
{
	if(!node)
		return;
	
	_dcfg_clear_node(node);
	
	/* TODO: detach */
	
	node->config->vtable.realloc(node, 0);
}

DCONFIG_NODE* _dcfg_search_node(DCONFIG_NODE* aggregate, DCONFIG_STRING name)
{
	if(!aggregate)
		return 0;
	
	DCONFIG_NODE* ret = dcfg_get_node(aggregate, name);
	
	if(ret)
		return ret;
	else
		return _dcfg_search_node(aggregate->parent, name);
}

DCONFIG_NODE* dcfg_get_node(DCONFIG_NODE* aggregate, DCONFIG_STRING name)
{
	assert(aggregate);
	
	for(size_t ii = 0; ii < aggregate->num_children; ii++)
	{
		if(dcfg_string_equal(name, aggregate->children[ii]->name))
			return aggregate->children[ii];
	}
	
	return 0;
}

DCONFIG_NODE* _dcfg_add_node_no_attach(DCONFIG_NODE* aggregate, DCONFIG_STRING type, bool own_type, DCONFIG_STRING name, bool own_name, bool is_aggregate)
{
	if(!aggregate)
		return 0;
	if(!aggregate->is_aggregate)
		return 0;
	
	DCONFIG_NODE* child = dcfg_get_node(aggregate, name);
	if(child)
	{
		if(dcfg_string_equal(child->type, type))
			return child;
		else
			return 0;
	}
	
	child = aggregate->config->vtable.realloc(0, sizeof(DCONFIG_NODE));
	memset(child, 0, sizeof(DCONFIG_NODE));
	child->is_aggregate = is_aggregate;
	child->name = name;
	child->own_name = own_name;
	child->type = type;
	child->own_type = own_type;
	child->config = aggregate->config;
	
	//printf("%.*s : %p\n", (int)dcfg_string_length(name), name.start, child);
	
	return child;
}

void _dcfg_attach_node(DCONFIG_NODE* aggregate, DCONFIG_NODE* node)
{
	if(!aggregate)
		return;
	if(!aggregate->is_aggregate)
		return;
	if(node->parent == aggregate)
		return;
	assert(node->parent == 0);
	//printf("Attaching %.*s to %.*s : %p\n", (int)dcfg_string_length(node->name), node->name.start, (int)dcfg_string_length(aggregate->name), aggregate->name.start, aggregate);
	node->parent = aggregate;
	aggregate->children = aggregate->config->vtable.realloc(aggregate->children, (aggregate->num_children + 1) * sizeof(DCONFIG_NODE*));
	aggregate->children[aggregate->num_children] = node;
	aggregate->num_children++;
}

DCONFIG_NODE* dcfg_add_node(DCONFIG_NODE* aggregate, DCONFIG_STRING type, bool own_type, DCONFIG_STRING name, bool own_name, bool is_aggregate)
{
	DCONFIG_NODE* node = _dcfg_add_node_no_attach(aggregate, type, own_type, name, own_name, is_aggregate);
	if(node)
		_dcfg_attach_node(aggregate, node);
	return node;
}

DCONFIG_NODE* dcfg_get_root(DCONFIG* config)
{
	assert(config);
	return config->root;
}

static
void get_name_impl(DCONFIG_NODE* node, DCONFIG_STRING* out)
{
	if(node->parent)
	{
		get_name_impl(node->parent, out);
		dcfg_append_to_string(out, dcfg_from_c_str(":"), node->config->vtable.realloc);
	}
	dcfg_append_to_string(out, node->name, node->config->vtable.realloc);
}

DCONFIG_STRING dcfg_get_full_name(DCONFIG_NODE* node)
{
	assert(node);
	DCONFIG_STRING ret = {0, 0};
	get_name_impl(node, &ret);
	return ret;
}

bool dcfg_set_value(DCONFIG_NODE* node, DCONFIG_STRING value, bool own)
{
	assert(node);
	if(node->is_aggregate)
		return false;
	if(node->own_value)
		node->config->vtable.realloc((void*)node->value.start, 0);
	node->value = value;
	node->own_value = own;
	return true;
}
