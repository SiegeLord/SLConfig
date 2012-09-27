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

#define TEST(a) if(!(a)) { fprintf(stderr, "Failed at %d!\n", __LINE__); ret = false; }

static
bool test_references()
{
	bool ret = true;

	SLCONFIG_NODE* root = slc_create_root_node(NULL);
	SLCONFIG_NODE* var = slc_add_node(root, slc_from_c_str(""), false, slc_from_c_str("var"), false, false);
	SLCONFIG_NODE* aggr = slc_add_node(root, slc_from_c_str(""), false, slc_from_c_str("aggr"), false, true);
	SLCONFIG_NODE* var2 = slc_add_node(aggr, slc_from_c_str(""), false, slc_from_c_str("var"), false, false);
	
	TEST(slc_get_node_by_reference(root, slc_from_c_str("::var")) == var);
	TEST(slc_get_node_by_reference(root, slc_from_c_str("var")) == var);
	TEST(slc_get_node_by_reference(aggr, slc_from_c_str("::var")) == var);
	TEST(slc_get_node_by_reference(aggr, slc_from_c_str("var")) == var2);
	TEST(slc_get_node_by_reference(aggr, slc_from_c_str("aggr:var")) == var2);
	TEST(slc_get_node_by_reference(aggr, slc_from_c_str("::aggr:var")) == var2);
	TEST(slc_get_node_by_reference(aggr, slc_from_c_str("d\"aggr\"d:\"var\"")) == var2);
	
	slc_destroy_node(root);
	
	return ret;
}

static
bool test_saving()
{
	bool ret = true;
	
	SLCONFIG_NODE* root = slc_create_root_node(NULL);
	SLCONFIG_NODE* var = slc_add_node(root, slc_from_c_str(":test"), false, slc_from_c_str("var"), false, false);
	SLCONFIG_NODE* aggr = slc_add_node(root, slc_from_c_str("\"--:"), false, slc_from_c_str("aggr"), false, true);
	SLCONFIG_NODE* var2 = slc_add_node(aggr, slc_from_c_str(""), false, slc_from_c_str("/abc"), false, false);
	slc_set_value(var, slc_from_c_str("3.15"), false);
	slc_set_value(var2, slc_from_c_str("//esc"), false);
	slc_set_comment(aggr, slc_from_c_str("Test comment\nTest comment"), false);
	
	SLCONFIG_STRING str = slc_save_node_string(root, slc_from_c_str("\n"), slc_from_c_str("\t"));
	//printf("%.*s", (int)slc_string_length(str), str.start);
	
	SLCONFIG_NODE* root2 = slc_create_root_node(NULL);
	slc_load_nodes_string(root2, slc_from_c_str(""), str, false);
	
	SLCONFIG_STRING str2 = slc_save_node_string(root2, slc_from_c_str("\n"), slc_from_c_str("\t"));
	//printf("%.*s", (int)slc_string_length(str2), str2.start);
	//slc_save_node(root2, slc_from_c_str("save_test.cfg"), slc_from_c_str("\n"), slc_from_c_str("\t"));
	
	TEST(slc_string_equal(str, str2));
	
	slc_destroy_node(root);
	slc_destroy_node(root2);
	
	slc_destroy_string(&str, 0);
	slc_destroy_string(&str2, 0);
	
	return ret;
}

int main()
{
	bool ret = true;
	ret &= test_references();
	ret &= test_saving();

	if(ret)
	{
		printf("All tests passed!\n");
		return 0;
	}
	else
	{
		return -1;
	}
}
