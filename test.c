#include <stdio.h>
#include <string.h>

#include "dconfig/dconfig.h"

int main()
{
	DCONFIG* config = dcfg_load_config(dcfg_from_c_str("test.cfg"));
	if(!config)
		return 1;
	printf("Done!\n\n");
	DCONFIG_NODE* root = dcfg_get_root(config);
	for(size_t ii = 0; ii < root->num_children; ii++)
	{
		DCONFIG_STRING full_name = dcfg_get_full_name(root->children[ii]);
		printf("(%.*s) %.*s\n", (int)dcfg_string_length(root->children[ii]->type), root->children[ii]->type.start, (int)dcfg_string_length(full_name), full_name.start);
		free((void*)full_name.start);
	}
	dcfg_destroy_config(config);
	return 0;
}
