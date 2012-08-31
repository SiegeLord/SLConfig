#include <stdio.h>
#include <string.h>

#include "dconfig/dconfig.h"

void print_delegate(DCONFIG_NODE* node, int level)
{
	char* indent = malloc(level + 1);
	indent[level] = '\0';
	memset(indent, '\t', level);
	
	for(size_t ii = 0; ii < node->num_children; ii++)
	{
		DCONFIG_NODE* child = node->children[ii];
		if(child->parent)
		{
			//printf("Parent: %.*s : %p\n", (int)dcfg_string_length(child->parent->name), child->parent->name.start, child->parent);
		}
		printf("%s/*\n%s%.*s\n%s*/\n", indent, indent, (int)dcfg_string_length(child->comment), child->comment.start, indent);
		DCONFIG_STRING full_name = dcfg_get_full_name(child);
		printf("%s(%.*s) %.*s", indent, (int)dcfg_string_length(child->type), child->type.start,
		       (int)dcfg_string_length(full_name), full_name.start
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
			printf(" = %.*s\n", (int)dcfg_string_length(child->value), child->value.start);
		}
	}
	
	free(indent);
}

int main()
{
	DCONFIG* config = dcfg_load_config(dcfg_from_c_str("test.cfg"));
	if(!config)
		return 1;
	printf("Done!\n\n");
	DCONFIG_NODE* root = dcfg_get_root(config);
	print_delegate(root, 0);
	dcfg_destroy_config(config);
	return 0;
}
