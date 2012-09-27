/* Copyright 2012 Pavel Sountsov
 * 
 * This file is part of SLConfig.
 *
 * SLConfig is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SLConfig is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser Public License for more details.
 *
 * You should have received a copy of the GNU Lesser Public License
 * along with SLConfig.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "slconfig/slconfig.h"
#include "slconfig/internal/slconfig.h"
#include "slconfig/internal/parser.h"
#include "slconfig/internal/tokenizer.h"

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
void* default_fopen(SLCONFIG_STRING filename, bool read)
{
	char* filename_str = slc_to_c_str(filename);
	void* ret = fopen(filename_str, read ? "rb" : "wb");
	free(filename_str);
	return ret;
}

static
int default_fclose(void* f)
{
	return fclose(f);
}

static
size_t default_fread(void* buf, size_t size, void* f)
{
	return fread(buf, 1, size, f);
}

static
size_t default_fwrite(const void* buf, size_t size, void* f)
{
	return fwrite(buf, 1, size, f);
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
	&default_fread,
	&default_fwrite
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
	FILL(fwrite);
#undef FILL
}

SLCONFIG_NODE* slc_create_root_node(const SLCONFIG_VTABLE* vtable_ptr)
{
	SLCONFIG_VTABLE vtable;
	if(vtable_ptr)
		memcpy(&vtable, vtable_ptr, sizeof(SLCONFIG_VTABLE));
	else
		memset(&vtable, 0, sizeof(SLCONFIG_VTABLE));
	fill_vtable(&vtable);
	
	CONFIG* config = vtable.realloc(0, sizeof(CONFIG));
	config->vtable = vtable;
	config->files = NULL;
	config->num_files = 0;
	config->root = vtable.realloc(0, sizeof(SLCONFIG_NODE));
	memset(config->root, 0, sizeof(SLCONFIG_NODE));
	config->root->is_aggregate = true;
	config->root->config = config;
	config->num_includes = 0;
	config->include_list = NULL;
	config->include_lines = NULL;
	config->include_ownerships = NULL;
	config->num_search_dirs = 0;
	config->search_dirs = NULL;
	config->search_dir_ownerships = 0;
	
	return config->root;
}

bool _slc_load_file(CONFIG* config, SLCONFIG_STRING filename, SLCONFIG_STRING* file)
{
	assert(config);
	
	#define BUF_SIZE (1024)
	size_t total_bytes_read = 0;
	size_t bytes_read;
	char* buff = NULL;
	void* f = config->vtable.fopen(filename, true);
	if(!f)
	{
		for(size_t ii = 0; ii < config->num_search_dirs && !f; ii++)
		{
			SLCONFIG_STRING test_file = {0, 0};
			slc_append_to_string(&test_file, config->search_dirs[ii], config->vtable.realloc);
			slc_append_to_string(&test_file, slc_from_c_str("/"), config->vtable.realloc);
			slc_append_to_string(&test_file, filename, config->vtable.realloc);
			
			f = config->vtable.fopen(test_file, true);
			slc_destroy_string(&test_file, config->vtable.realloc);
		}
		
		if(!f)
			return false;
	}
	
	do
	{
		buff = config->vtable.realloc(buff, total_bytes_read + BUF_SIZE);
		bytes_read = config->vtable.fread(buff + total_bytes_read, BUF_SIZE, f);
		total_bytes_read += bytes_read;
	} while(bytes_read == BUF_SIZE);
	
	config->vtable.fclose(f);
	
	file->start = buff;
	file->end = buff + total_bytes_read;
	
	_slc_add_file(config, *file);
	return true;
}

bool slc_load_nodes(SLCONFIG_NODE* aggregate, SLCONFIG_STRING filename)
{
	assert(aggregate);
	assert(aggregate->is_aggregate);
	if(!aggregate->is_aggregate)
		return false;
	CONFIG* config = aggregate->config;
	_slc_add_include(config, filename, false, 0);
	SLCONFIG_STRING file = {0, 0};
	bool ret = _slc_load_file(config, filename, &file);
	if(ret)
		ret = _slc_parse_file(config, aggregate, filename, file);
	_slc_clear_includes(config);
	return ret;
}

bool slc_load_nodes_string(SLCONFIG_NODE* aggregate, SLCONFIG_STRING filename, SLCONFIG_STRING file, bool copy)
{	
	assert(aggregate);
	assert(aggregate->is_aggregate);
	if(!aggregate->is_aggregate)
		return false;
	CONFIG* config = aggregate->config;
	SLCONFIG_STRING new_file = {0, 0};
	if(copy)
	{
		slc_append_to_string(&new_file, file, config->vtable.realloc);
		_slc_add_file(config, new_file);
	}
	else
	{
		new_file = file;
	}
	
	_slc_add_include(config, filename, false, 0);
	bool ret = _slc_parse_file(config, aggregate, filename, new_file);
	_slc_clear_includes(config);
	return ret;
}

static
void destroy_config(CONFIG* config)
{
	if(!config)
		return;
	
	for(size_t ii = 0; ii < config->num_files; ii++)
		slc_destroy_string(&config->files[ii], config->vtable.realloc);
	
	_slc_free(config, config->files);
	
	slc_clear_search_directories(config->root);
}

void _slc_add_file(CONFIG* config, SLCONFIG_STRING new_file)
{
	assert(config);
	config->files = config->vtable.realloc(config->files, (config->num_files + 1) * sizeof(SLCONFIG_STRING));
	config->files[config->num_files++] = new_file;
}

static
void detach_node(SLCONFIG_NODE* node)
{
	if(node->parent)
	{
		SLCONFIG_NODE* parent = node->parent;
		node->parent = NULL;
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
		slc_destroy_string(&node->type, node->config->vtable.realloc);

	if(node->own_name)
		slc_destroy_string(&node->name, node->config->vtable.realloc);

	if(node->own_value)
		slc_destroy_string(&node->value, node->config->vtable.realloc);
	
	if(node->own_comment)
		slc_destroy_string(&node->comment, node->config->vtable.realloc);
	
	for(size_t ii = 0; ii < node->num_children; ii++)
		_slc_destroy_node(node->children[ii], false);
	
	if(node->children)
		_slc_free(node->config, node->children);
	
	if(node->user_destructor)
		node->user_destructor(node->user_data);
	
	if(node->parent)
	{
		if(detach)
			detach_node(node);
		
		_slc_free(node->config, node);
	}
	else
	{
		CONFIG* config = node->config;
		destroy_config(config);
		_slc_free(config, node);
		_slc_free(config, config);
	}
}

void slc_destroy_node(SLCONFIG_NODE* node)
{
	_slc_destroy_node(node, true);
}

SLCONFIG_NODE* _slc_search_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING name)
{
	if(!aggregate)
		return NULL;
	
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
	
	return NULL;
}

SLCONFIG_NODE* _slc_add_node_no_attach(SLCONFIG_NODE* aggregate, SLCONFIG_STRING type, bool copy_type, SLCONFIG_STRING name, bool copy_name, bool is_aggregate)
{
	if(!aggregate)
		return NULL;
	if(!aggregate->is_aggregate)
		return NULL;
	
	SLCONFIG_NODE* child = slc_get_node(aggregate, name);
	if(child)
	{
		if(slc_string_equal(child->type, type) && child->is_aggregate == is_aggregate)
			return child;
		else
			return NULL;
	}
	
	child = aggregate->config->vtable.realloc(0, sizeof(SLCONFIG_NODE));
	memset(child, 0, sizeof(SLCONFIG_NODE));
	child->is_aggregate = is_aggregate;
	if(copy_name)
		slc_append_to_string(&child->name, name, aggregate->config->vtable.realloc);
	else
		child->name = name;
	child->own_name = copy_name;
	
	if(copy_type)
		slc_append_to_string(&child->type, type, aggregate->config->vtable.realloc);
	else
		child->type = type;
	child->own_type = copy_type;
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
	assert(node->parent == NULL);
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
	slc_append_to_string(&ret, slc_from_c_str(":"), node->config->vtable.realloc);
	get_name_impl(node, &ret);
	return ret;
}

bool slc_set_value(SLCONFIG_NODE* string_node, SLCONFIG_STRING value, bool copy)
{
	assert(string_node);
	if(string_node->is_aggregate)
		return false;
	if(string_node->own_value)
		slc_destroy_string(&string_node->value, string_node->config->vtable.realloc);
	if(copy)
	{
		string_node->value.start = string_node->value.end = 0;
		slc_append_to_string(&string_node->value, value, string_node->config->vtable.realloc);
	}
	else
	{
		string_node->value = value;
	}
	string_node->own_value = copy;
	return true;
}

void slc_set_comment(SLCONFIG_NODE* node, SLCONFIG_STRING comment, bool copy)
{
	assert(node);
	if(node->own_comment)
		slc_destroy_string(&node->comment, node->config->vtable.realloc);
	if(copy)
	{
		node->comment.start = node->comment.end = 0;
		slc_append_to_string(&node->comment, comment, node->config->vtable.realloc);
	}
	else
	{
		node->comment = comment;
	}
	node->own_comment = copy;
}

SLCONFIG_STRING slc_get_value(SLCONFIG_NODE* string_node)
{
	assert(string_node);
	if(string_node->is_aggregate)
	{
		SLCONFIG_STRING ret = {0, 0};
		return ret;
	}
	else
	{
		return string_node->value;
	}
}

bool slc_is_aggregate(SLCONFIG_NODE* node)
{
	assert(node);
	return node->is_aggregate;
}

SLCONFIG_STRING slc_get_type(SLCONFIG_NODE* node)
{
	assert(node);
	return node->type;
}

SLCONFIG_STRING slc_get_name(SLCONFIG_NODE* node)
{
	assert(node);
	return node->name;
}

SLCONFIG_STRING slc_get_comment(SLCONFIG_NODE* node)
{
	assert(node);
	return node->comment;
}

SLCONFIG_NODE* slc_get_node_by_index(SLCONFIG_NODE* aggregate, size_t idx)
{
	assert(aggregate);
	assert(aggregate->is_aggregate);
	assert(idx < aggregate->num_children);
	
	if(aggregate->is_aggregate)
		return aggregate->children[idx];
	else
		return NULL;
}

size_t slc_get_num_children(SLCONFIG_NODE* node)
{
	assert(node);
	if(node->is_aggregate)
		return node->num_children;
	else
		return 0;
}

void _slc_copy_into(SLCONFIG_NODE* dest, SLCONFIG_NODE* src)
{
	dest->type = src->type;
	dest->own_type = src->own_type;
	dest->name = src->name;
	dest->own_name = src->own_name;
	
	dest->value.start = NULL;
	dest->value.end = NULL;
	slc_append_to_string(&dest->value, src->value, src->config->vtable.realloc);
	dest->own_value = true;
	
	dest->is_aggregate = src->is_aggregate;
	dest->num_children = 0;
	dest->children = NULL;
	
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

bool _slc_add_include(CONFIG* config, SLCONFIG_STRING filename, bool own, size_t line)
{
	assert(config);
	
	for(size_t ii = 0; ii < config->num_includes; ii++)
	{
		if(slc_string_equal(config->include_list[ii], filename))
			return false;
	}
	
	config->include_list = config->vtable.realloc(config->include_list, sizeof(SLCONFIG_STRING) * (config->num_includes + 1));
	config->include_ownerships = config->vtable.realloc(config->include_ownerships, sizeof(bool) * (config->num_includes + 1));
	config->include_lines = config->vtable.realloc(config->include_lines, sizeof(size_t) * config->num_includes);
	
	config->include_list[config->num_includes] = filename;
	config->include_ownerships[config->num_includes] = own;
	if(config->num_includes > 0)
		config->include_lines[config->num_includes - 1] = line;
	
	config->num_includes++;
	
	return true;
}

void _slc_pop_include(CONFIG* config)
{
	assert(config);
	assert(config->num_includes > 0);
	if(config->num_includes > 0)
		config->num_includes--;
}

void _slc_clear_includes(CONFIG* config)
{
	assert(config);
	
	for(size_t ii = 0; ii < config->num_includes; ii++)
	{
		if(config->include_ownerships[ii])
			slc_destroy_string(&config->include_list[ii], config->vtable.realloc);
	}
	
	if(config->include_list)
	{
		config->vtable.realloc(config->include_list, 0);
		config->vtable.realloc(config->include_ownerships, 0);
		config->vtable.realloc(config->include_lines, 0);
	}
	
	config->include_list = NULL;
	config->include_ownerships = NULL;
	config->include_lines = NULL;
	config->num_includes = 0;
}

void slc_set_user_data(SLCONFIG_NODE* node, intptr_t data, void (*user_destructor)(intptr_t))
{
	assert(node);
	if(node->user_destructor)
		node->user_destructor(node->user_data);
	
	node->user_data = data;
	node->user_destructor = user_destructor;
}

intptr_t slc_get_user_data(SLCONFIG_NODE* node)
{
	assert(node);
	return node->user_data;
}

void slc_add_search_directory(SLCONFIG_NODE* node, SLCONFIG_STRING directory, bool copy)
{
	assert(node);
	CONFIG* config = node->config;
	config->search_dirs = config->vtable.realloc(config->search_dirs, sizeof(SLCONFIG_STRING) * (config->num_search_dirs + 1));
	config->search_dir_ownerships = config->vtable.realloc(config->search_dir_ownerships, sizeof(bool) * (config->num_search_dirs + 1));
	
	SLCONFIG_STRING str = {0, 0};
	if(copy)
		slc_append_to_string(&str, directory, config->vtable.realloc);
	else
		str = directory;

	config->search_dirs[config->num_search_dirs] = str;
	config->search_dir_ownerships[config->num_search_dirs] = copy;
	
	config->num_search_dirs++;
}

void slc_clear_search_directories(SLCONFIG_NODE* node)
{
	assert(node);
	CONFIG* config = node->config;
	for(size_t ii = 0; ii < config->num_search_dirs; ii++)
	{
		if(config->search_dir_ownerships[ii])
			slc_destroy_string(&config->search_dirs[ii], config->vtable.realloc);
	}
	
	if(config->search_dirs)
	{
		config->vtable.realloc(config->search_dirs, 0);
		config->vtable.realloc(config->search_dir_ownerships, 0);
	}
	
	config->search_dirs = NULL;
	config->search_dir_ownerships = NULL;
	config->num_search_dirs = 0;
}

#define SENTINEL_CHAR ('-')
#define SENTINEL_STRING ("-")

static
size_t get_sentinel_size(SLCONFIG_STRING string)
{
	if(string.start == 0)
		return 0;
	
	bool need_escaping = false;
	bool first_was_slash = false;

	size_t idx = 0;
	size_t ret = 0;
	size_t max_ret = 0;
	while(string.start < string.end)
	{
		if(*string.start == SENTINEL_CHAR)
			ret++;
		else
			ret = 0;
		
		if(ret > max_ret)
			max_ret = ret;
		
		if(!_slc_is_naked_string_character(*string.start))
			need_escaping = true;
		
		if(idx == 0 && *string.start == '/')
			first_was_slash = true;
		else if(first_was_slash && idx == 1 && *string.start == '/')
			need_escaping = true;
		
		idx++;
		string.start++;
	}

	if(need_escaping)
		return max_ret + 1;
	else
		return 0;
}

static
void node_writer(SLCONFIG_NODE* node, SLCONFIG_STRING line_end, SLCONFIG_STRING indentation, void* output, void (*writer)(void* output, const void* data, size_t size),  size_t indent_level)
{
	SLCONFIG_STRING sentinel_string = {SENTINEL_STRING, SENTINEL_STRING + 1};
	
	#define WRITE_C_STRING(s) writer(output, s, strlen(s));
	#define WRITE_STRING(s) writer(output, s.start, slc_string_length(s));
	#define REPEAT(s, n) for(size_t _ii = 0; _ii < (n); _ii++) { WRITE_STRING(s); }
	#define INDENT REPEAT(indentation, indent_level);
	#define ESCAPED_STRING(s)                                                  \
	do                                                                         \
	{                                                                          \
		size_t sentinel_size = get_sentinel_size(s);                           \
		if(sentinel_size > 0)                                                  \
		{                                                                      \
			REPEAT(sentinel_string, sentinel_size);                            \
			WRITE_C_STRING("\"");                                              \
			WRITE_STRING(s);                                                   \
			WRITE_C_STRING("\"");                                              \
			REPEAT(sentinel_string, sentinel_size);                            \
		}                                                                      \
		else                                                                   \
		{                                                                      \
			WRITE_STRING(s);                                                   \
		}                                                                      \
	} while(0)
	
	if(node->parent && slc_string_length(node->comment))
	{
		INDENT;
		WRITE_C_STRING("/**");
		WRITE_STRING(node->comment);
		WRITE_C_STRING("*/");
		WRITE_STRING(line_end);
	}
	
	INDENT;
	if(slc_string_length(node->type) > 0)
	{
		ESCAPED_STRING(node->type);
		WRITE_C_STRING(" ");
	}
	ESCAPED_STRING(node->name);
	
	if(node->is_aggregate)
	{
		if(!node->parent)
		{
			for(size_t ii = 0; ii < node->num_children; ii++)
			{
				node_writer(node->children[ii], line_end, indentation, output, writer, indent_level);
			}
		}
		else if(node->num_children > 0)
		{
			WRITE_STRING(line_end);
			INDENT;
			WRITE_C_STRING("{");
			WRITE_STRING(line_end);
			for(size_t ii = 0; ii < node->num_children; ii++)
			{
				node_writer(node->children[ii], line_end, indentation, output, writer, indent_level+1);
			}
			INDENT;
			WRITE_C_STRING("}");
		}
		else
		{
			WRITE_C_STRING(" {}");
		}
	}
	else
	{
		if(slc_string_length(node->value))
		{
			WRITE_C_STRING(" = ");
			ESCAPED_STRING(node->value);
		}
		WRITE_C_STRING(";");
	}
	
	if(node->parent)
		WRITE_STRING(line_end);
}

typedef struct
{
	SLCONFIG_STRING* str;
	CONFIG* config;
} STRING_WRITER_DATA;

static
void string_writer(void* output, const void* data, size_t size)
{
	STRING_WRITER_DATA* writer_data = (STRING_WRITER_DATA*)output;
	SLCONFIG_STRING new_str = {data, data + size};
	slc_append_to_string(writer_data->str, new_str, writer_data->config->vtable.realloc);
}

SLCONFIG_STRING slc_save_node_string(SLCONFIG_NODE* node, SLCONFIG_STRING line_end, SLCONFIG_STRING indentation)
{
	SLCONFIG_STRING ret = {0, 0};
	
	STRING_WRITER_DATA data = {&ret, node->config};
	
	node_writer(node, line_end, indentation, &data, &string_writer, 0);
	
	return ret;
}

typedef struct
{
	void* output;
	CONFIG* config;
	bool error;
} FILE_WRITER_DATA;

static
void file_writer(void* output, const void* data, size_t size)
{
	FILE_WRITER_DATA* writer_data = (FILE_WRITER_DATA*)output;
	writer_data->error |= size != writer_data->config->vtable.fwrite(data, size, writer_data->output);
}

bool slc_save_node(SLCONFIG_NODE* node, SLCONFIG_STRING filename, SLCONFIG_STRING line_end, SLCONFIG_STRING indentation)
{
	bool ret = false;
	SLCONFIG_VTABLE vtable = node->config->vtable;
	void* file = vtable.fopen(filename, false);
	if(file)
	{
		FILE_WRITER_DATA data = {file, node->config, false};
		node_writer(node, line_end, indentation, &data, &file_writer, 0);
		ret = !data.error;
		vtable.fclose(file);
	}
	return ret;
}
