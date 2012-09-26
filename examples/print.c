/* Copyright 2012 Pavel Sountsov
 * 
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <stdio.h>
#include <string.h>

#include "slconfig/slconfig.h"

void print_delegate(SLCONFIG_NODE* node, int level)
{
	char* indent = malloc(level + 1);
	indent[level] = '\0';
	memset(indent, '\t', level);
	
	for(size_t ii = 0; ii < slc_get_num_children(node); ii++)
	{
		SLCONFIG_NODE* child = slc_get_node_by_index(node, ii);

		SLCONFIG_STRING type = slc_get_type(child);
		SLCONFIG_STRING comment = slc_get_comment(child);
		
		if(slc_string_length(comment))
			printf("%s/*\n%s%.*s\n%s*/\n", indent, indent, (int)slc_string_length(comment), comment.start, indent);
		
		SLCONFIG_STRING full_name = slc_get_full_name(child);
		printf("%s(%.*s) %.*s", indent, (int)slc_string_length(type), type.start,
		       (int)slc_string_length(full_name), full_name.start
		      );
		slc_destroy_string(&full_name, 0);
		
		if(slc_is_aggregate(child))
		{
			printf("\n%s{\n", indent);
			print_delegate(child, level + 1);
			printf("%s}\n", indent);
		}
		else
		{
			SLCONFIG_STRING value = slc_get_value(child);
			printf(" = %.*s\n", (int)slc_string_length(value), value.start);
		}
	}
	
	free(indent);
}

static
void print_help(const char* argv0)
{
	fprintf(stderr, "Usage:\n%s [-I<search_directory>...] <file>\n", argv0);
}

int main(int argc, char** argv)
{
	if(argc < 2)
	{
		print_help(argv[0]);
		return -1;
	}
	int ret = -1;
	SLCONFIG_NODE* root = slc_create_config(0);
	
	for(int ii = 1; ii < argc - 1; ii++)
	{
		char* arg = argv[ii];
		if(*arg != '-')
		{
			print_help(argv[0]);
			goto exit;
		}
		arg++;
		if(*arg != 'I')
		{
			print_help(argv[0]);
			goto exit;
		}
		arg++;
		slc_add_search_directory(root, slc_from_c_str(arg), false);
	}
	
	SLCONFIG_NODE* node = slc_add_node(root, slc_from_c_str(""), false, slc_from_c_str("external"), false, false);
	slc_set_value(node, slc_from_c_str("external_value"), false);
	if(slc_load_config(root, slc_from_c_str(argv[argc - 1])))
	{
		print_delegate(root, 0);
		ret = 0;
	}
	else
	{
		fprintf(stderr, "Error loading %s.\n", argv[argc - 1]);
	}
exit:
	slc_destroy_node(root);
	return ret;
}
