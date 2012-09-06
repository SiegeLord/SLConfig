#include "slconfig/slconfig.h"
#include "slconfig/internal/slconfig.h"
#include "slconfig/internal/parser.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static
void default_stderr(SLCONFIG_STRING s)
{
	fprintf(stderr, "%.*s", (int)slc_string_length(s), s.start);
}

static
void* default_fopen(SLCONFIG_STRING filename, SLCONFIG_STRING mode)
{
	char* filename_str = slc_to_c_str(filename);
	char* mode_str = slc_to_c_str(mode);
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

SLCONFIG_VTABLE default_vtable =
{
	&default_realloc,
	&default_stderr,
	&default_fopen,
	&default_fclose,
	&default_fread
};

static
void fill_vtable(SLCONFIG_VTABLE* vtable)
{
#define FILL(a) if(!vtable->a) vtable->a = default_vtable.a;
	FILL(realloc);
	FILL(stderr);
	FILL(fopen);
	FILL(fclose);
	FILL(fread);
#undef FILL
}

SLCONFIG* slc_load_config(SLCONFIG_STRING filename)
{
	return slc_load_config_vtable(filename, default_vtable);
}

SLCONFIG* slc_load_config_vtable(SLCONFIG_STRING filename, SLCONFIG_VTABLE vtable)
{
	SLCONFIG_STRING file;
	
	fill_vtable(&vtable);

#define BUF_SIZE (1024)
	size_t total_bytes_read = 0;
	size_t bytes_read;
	char* buff = 0;
	void* f = vtable.fopen(filename, slc_from_c_str("rb"));
	
	do
	{
		buff = vtable.realloc(buff, total_bytes_read + BUF_SIZE);
		bytes_read = vtable.fread(buff + total_bytes_read, 1, BUF_SIZE, f);
		total_bytes_read += bytes_read;
	} while(bytes_read == BUF_SIZE);
	
	vtable.fclose(f);
	
	file.start = buff;
	file.end = buff + total_bytes_read;
	
	SLCONFIG* ret = slc_load_config_string_vtable(filename, file, false, vtable);
	_slc_add_file(ret, file);
	return ret;
}

SLCONFIG* slc_load_config_string(SLCONFIG_STRING filename, SLCONFIG_STRING file, bool copy)
{
	return slc_load_config_string_vtable(filename, file, copy, default_vtable);
}

SLCONFIG* slc_load_config_string_vtable(SLCONFIG_STRING filename, SLCONFIG_STRING file, bool copy, SLCONFIG_VTABLE vtable)
{
	fill_vtable(&vtable);
	
	SLCONFIG* ret = vtable.realloc(0, sizeof(SLCONFIG));
	ret->vtable = vtable;
	ret->files = 0;
	ret->num_files = 0;
	ret->root = vtable.realloc(0, sizeof(SLCONFIG_NODE));
	memset(ret->root, 0, sizeof(SLCONFIG_NODE));
	ret->root->is_aggregate = true;
	ret->root->config = ret;
	
	SLCONFIG_STRING new_file = {0, 0};
	if(copy)
	{
		slc_append_to_string(&new_file, file, ret->vtable.realloc);
		_slc_add_file(ret, new_file);
	}
	else
	{
		new_file = file;
	}
	
	if(!_slc_parse_file(ret, ret->root, filename, new_file))
	{
		slc_destroy_config(ret);
		return 0;
	}
	
	return ret;
}

void slc_destroy_config(SLCONFIG* config)
{
	if(!config)
		return;
	
	slc_destroy_node(config->root);
	
	for(size_t ii = 0; ii < config->num_files; ii++)
		_slc_free(config, (void*)config->files[ii].start);
	
	_slc_free(config, config->files);
	
	_slc_free(config, config);
}

void _slc_add_file(SLCONFIG* config, SLCONFIG_STRING new_file)
{
	config->files = config->vtable.realloc(config->files, (config->num_files + 1) * sizeof(SLCONFIG_STRING));
	config->files[config->num_files++] = new_file;
}

static
void detach_node(SLCONFIG_NODE* node)
{
	if(node->parent)
	{
		SLCONFIG_NODE* parent = node->parent;
		node->parent = 0;
		size_t ii;
		
		for(ii = 0; ii < parent->num_children; ii++)
		{
			if(parent->children[ii] == node)
				break;
		}
		assert(ii < parent->num_children);
		
		for(size_t jj = ii + 1; jj < parent->num_children; jj++)
		{
			parent->children[jj - 1] = parent->children[jj];
		}
		
		parent->num_children--;
	}
}

void _slc_destroy_node(SLCONFIG_NODE* node, bool detach)
{
	if(!node)
		return;
	
	//printf("Clearing %.*s %zu %d\n", (int)slc_string_length(node->name), node->name.start, node->num_children, node->own_comment);
	if(node->own_type)
		_slc_free(node->config, (void*)node->type.start);

	if(node->own_name)
		_slc_free(node->config, (void*)node->name.start);

	if(node->own_value)
		_slc_free(node->config, (void*)node->value.start);
	
	if(node->own_comment)
		_slc_free(node->config, (void*)node->comment.start);
	
	for(size_t ii = 0; ii < node->num_children; ii++)
		_slc_destroy_node(node->children[ii], false);
	
	if(node->children)
		_slc_free(node->config, node->children);
	
	if(detach)
		detach_node(node);
	
	_slc_free(node->config, node);
}

void slc_destroy_node(SLCONFIG_NODE* node)
{
	_slc_destroy_node(node, true);
}

SLCONFIG_NODE* _slc_search_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING name)
{
	if(!aggregate)
		return 0;
	
	SLCONFIG_NODE* ret = slc_get_node(aggregate, name);
	
	if(ret)
		return ret;
	else
		return _slc_search_node(aggregate->parent, name);
}

SLCONFIG_NODE* slc_get_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING name)
{
	assert(aggregate);
	
	for(size_t ii = 0; ii < aggregate->num_children; ii++)
	{
		if(slc_string_equal(name, aggregate->children[ii]->name))
			return aggregate->children[ii];
	}
	
	return 0;
}

SLCONFIG_NODE* _slc_add_node_no_attach(SLCONFIG_NODE* aggregate, SLCONFIG_STRING type, bool own_type, SLCONFIG_STRING name, bool own_name, bool is_aggregate)
{
	if(!aggregate)
		return 0;
	if(!aggregate->is_aggregate)
		return 0;
	
	SLCONFIG_NODE* child = slc_get_node(aggregate, name);
	if(child)
	{
		if(slc_string_equal(child->type, type) && child->is_aggregate == is_aggregate)
			return child;
		else
			return 0;
	}
	
	child = aggregate->config->vtable.realloc(0, sizeof(SLCONFIG_NODE));
	memset(child, 0, sizeof(SLCONFIG_NODE));
	child->is_aggregate = is_aggregate;
	child->name = name;
	child->own_name = own_name;
	child->type = type;
	child->own_type = own_type;
	child->config = aggregate->config;
	
	//printf("%.*s : %p\n", (int)slc_string_length(name), name.start, child);
	
	return child;
}

void _slc_attach_node(SLCONFIG_NODE* aggregate, SLCONFIG_NODE* node)
{
	if(!aggregate)
		return;
	if(!aggregate->is_aggregate)
		return;
	if(node->parent == aggregate)
		return;
	assert(node->parent == 0);
	//printf("Attaching %.*s to %.*s : %p\n", (int)slc_string_length(node->name), node->name.start, (int)slc_string_length(aggregate->name), aggregate->name.start, aggregate);
	node->parent = aggregate;
	aggregate->children = aggregate->config->vtable.realloc(aggregate->children, (aggregate->num_children + 1) * sizeof(SLCONFIG_NODE*));
	aggregate->children[aggregate->num_children] = node;
	aggregate->num_children++;
}

SLCONFIG_NODE* slc_add_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING type, bool own_type, SLCONFIG_STRING name, bool own_name, bool is_aggregate)
{
	SLCONFIG_NODE* node = _slc_add_node_no_attach(aggregate, type, own_type, name, own_name, is_aggregate);
	if(node)
		_slc_attach_node(aggregate, node);
	return node;
}

SLCONFIG_NODE* slc_get_root(SLCONFIG* config)
{
	assert(config);
	return config->root;
}

static
void get_name_impl(SLCONFIG_NODE* node, SLCONFIG_STRING* out)
{
	if(node->parent)
	{
		get_name_impl(node->parent, out);
		slc_append_to_string(out, slc_from_c_str(":"), node->config->vtable.realloc);
	}
	slc_append_to_string(out, node->name, node->config->vtable.realloc);
}

SLCONFIG_STRING slc_get_full_name(SLCONFIG_NODE* node)
{
	assert(node);
	SLCONFIG_STRING ret = {0, 0};
	get_name_impl(node, &ret);
	return ret;
}

bool slc_set_value(SLCONFIG_NODE* node, SLCONFIG_STRING value, bool copy)
{
	assert(node);
	if(node->is_aggregate)
		return false;
	if(node->own_value)
		_slc_free(node->config, (void*)node->value.start);
	if(copy)
	{
		node->value.start = node->value.end = 0;
		slc_append_to_string(&node->value, value, node->config->vtable.realloc);
	}
	else
	{
		node->value = value;
	}
	node->own_value = copy;
	return true;
}

void _slc_copy_into(SLCONFIG_NODE* dest, SLCONFIG_NODE* src)
{
	dest->type = src->type;
	dest->own_type = src->own_type;
	dest->name = src->name;
	dest->own_name = src->own_name;
	
	dest->value.start = 0;
	dest->value.end = 0;
	slc_append_to_string(&dest->value, src->value, src->config->vtable.realloc);
	dest->own_value = true;
	
	dest->is_aggregate = src->is_aggregate;
	dest->num_children = 0;
	dest->children = 0;
	
	/* Don't touch the parent */
	
	if(src->is_aggregate)
	{
		for(size_t ii = 0; ii < src->num_children; ii++)
		{
			SLCONFIG_NODE* child = src->children[ii];
			SLCONFIG_NODE* new_node = slc_add_node(dest, child->type, false, child->name, false, child->is_aggregate);
			_slc_copy_into(new_node, child);
		}
	}
}
