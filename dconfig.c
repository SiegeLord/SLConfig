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

DCONFIG_VTABLE default_vtable =
{
	&realloc,
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
	
	for(size_t ii = 0; ii < config->num_files; ii++)
		config->vtable.realloc((void*)config->files[ii].start, 0);
	
	config->vtable.realloc(config->files, 0);
	
	dcfg_destroy_node(config->root);
	
	config->vtable.realloc(config, 0);
}

void dcfg_destroy_node(DCONFIG_NODE* node)
{
	if(!node)
		return;
	
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
	
	node->config->vtable.realloc(node, 0);
}

DCONFIG_NODE* dcfg_get_node(DCONFIG_NODE* aggregate, DCONFIG_STRING name)
{
	if(!aggregate)
		return 0;
	
	for(size_t ii = 0; ii < aggregate->num_children; ii++)
	{
		if(dcfg_string_equal(name, aggregate->children[ii]->name))
			return aggregate->children[ii];
	}
	
	return dcfg_get_node(aggregate->parent, name);
}

DCONFIG_NODE* dcfg_add_node(DCONFIG_NODE* aggregate, DCONFIG_STRING type, bool own_type, DCONFIG_STRING name, bool own_name, bool is_aggregate)
{
	if(!aggregate)
		return 0;
	if(!aggregate->is_aggregate)
		return 0;
	for(size_t ii = 0; ii < aggregate->num_children; ii++)
	{
		DCONFIG_NODE* child = aggregate->children[ii];
		if(dcfg_string_equal(name, child->name))
		{
			if(dcfg_string_equal(child->type, type))
				return child;
			else
				return 0;
		}
	}
	
	DCONFIG_NODE* child = aggregate->config->vtable.realloc(0, sizeof(DCONFIG_NODE));
	memset(child, 0, sizeof(DCONFIG_NODE));
	child->is_aggregate = is_aggregate;
	child->parent = aggregate;
	child->name = name;
	child->own_name = own_name;
	child->type = type;
	child->own_type = own_type;
	child->config = aggregate->config;
	
	aggregate->children = aggregate->config->vtable.realloc(aggregate->children, aggregate->num_children + 1);
	aggregate->children[aggregate->num_children] = child;
	aggregate->num_children++;
	
	return child;
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
	DCONFIG_STRING ret = {0, 0};
	get_name_impl(node, &ret);
	return ret;
}
