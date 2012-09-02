#include <stdio.h>
#include <string.h>

#include "slconfig/slconfig.h"

void print_delegate(SLCONFIG_NODE* node, int level)
{
	char* indent = malloc(level + 1);
	indent[level] = '\0';
	memset(indent, '\t', level);
	
	for(size_t ii = 0; ii < node->num_children; ii++)
	{
		SLCONFIG_NODE* child = node->children[ii];
		if(child->parent)
		{
			//printf("Parent: %.*s : %p\n", (int)slc_string_length(child->parent->name), child->parent->name.start, child->parent);
		}
		printf("%s/*\n%s%.*s\n%s*/\n", indent, indent, (int)slc_string_length(child->comment), child->comment.start, indent);
		SLCONFIG_STRING full_name = slc_get_full_name(child);
		printf("%s(%.*s) %.*s", indent, (int)slc_string_length(child->type), child->type.start,
		       (int)slc_string_length(full_name), full_name.start
		      );
		free((void*)full_name.start);
		if(child->is_aggregate)
		{
			printf("\n%s{\n", indent);
			print_delegate(child, level + 1);
			printf("%s}\n", indent);
		}
		else
		{
			printf(" = %.*s\n", (int)slc_string_length(child->value), child->value.start);
		}
	}
	
	free(indent);
}

int main()
{
	SLCONFIG* config = slc_load_config(slc_from_c_str("test.cfg"));
	if(!config)
		return 1;
	printf("Done!\n\n");
	SLCONFIG_NODE* root = slc_get_root(config);
	print_delegate(root, 0);
	slc_destroy_config(config);
	return 0;
}
