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
		SLCONFIG_NODE* child = slc_get_children(node)[ii];

		SLCONFIG_STRING type = slc_get_type(child);
		SLCONFIG_STRING comment = slc_get_comment(child);
		
		printf("%s/*\n%s%.*s\n%s*/\n", indent, indent, (int)slc_string_length(comment), comment.start, indent);
		
		SLCONFIG_STRING full_name = slc_get_full_name(child);
		printf("%s(%.*s) %.*s", indent, (int)slc_string_length(type), type.start,
		       (int)slc_string_length(full_name), full_name.start
		      );
		free((void*)full_name.start);
		
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
