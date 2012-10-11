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

module slconfig;

import core.memory;
import core.exception;

import tango.stdc.stdint;
import tango.util.Convert;

extern(C)
{

struct SLCONFIG_STRING
{
	const char* start;
	const char* end;
}

struct SLCONFIG_VTABLE
{
	void* function(void* buf, size_t size) realloc;
	void function(SLCONFIG_STRING s) error;
	void* function(SLCONFIG_STRING filename, bool read) fopen;
	int function(void* file) fclose;
	size_t function(void* buf, size_t size, void* file) fread;
	size_t function(const void* buf, size_t size, void* file) fwrite;
}

struct SLCONFIG_NODE {}

/* Node IO */
SLCONFIG_NODE* slc_create_root_node(const SLCONFIG_VTABLE* vtable);
void slc_add_search_directory(SLCONFIG_NODE* node, SLCONFIG_STRING directory, bool copy);
void slc_clear_search_directories(SLCONFIG_NODE* node);
bool slc_load_nodes(SLCONFIG_NODE* aggregate, SLCONFIG_STRING filename);
bool slc_load_nodes_string(SLCONFIG_NODE* aggregate, SLCONFIG_STRING filename, SLCONFIG_STRING file, bool copy);
bool slc_save_node(const SLCONFIG_NODE* node, SLCONFIG_STRING filename, SLCONFIG_STRING line_end, SLCONFIG_STRING indentation);
SLCONFIG_STRING slc_save_node_string(const SLCONFIG_NODE* node, SLCONFIG_STRING line_end, SLCONFIG_STRING indentation);

/* Node creation/destruction */
SLCONFIG_NODE* slc_add_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING type, bool copy_type, SLCONFIG_STRING name, bool copy_name, bool is_aggregate);
void slc_destroy_node(SLCONFIG_NODE* node);

/* Node access */
SLCONFIG_NODE* slc_get_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING name);
SLCONFIG_NODE* slc_get_node_by_index(SLCONFIG_NODE* aggregate, size_t idx);
SLCONFIG_NODE* slc_get_node_by_reference(SLCONFIG_NODE* aggregate, SLCONFIG_STRING reference);

/* Node properties */
SLCONFIG_STRING slc_get_name(const SLCONFIG_NODE* node);
SLCONFIG_STRING slc_get_type(const SLCONFIG_NODE* node);
SLCONFIG_STRING slc_get_full_name(const SLCONFIG_NODE* node);
bool slc_is_aggregate(const SLCONFIG_NODE* node);
size_t slc_get_num_children(const SLCONFIG_NODE* node);
SLCONFIG_STRING slc_get_value(const SLCONFIG_NODE* string_node);
bool slc_set_value(SLCONFIG_NODE* node, SLCONFIG_STRING string_node, bool copy);
intptr_t slc_get_user_data(SLCONFIG_NODE* node);
void slc_set_user_data(SLCONFIG_NODE* node, intptr_t data, void function(intptr_t) user_destructor);
SLCONFIG_STRING slc_get_comment(const SLCONFIG_NODE* node);
void slc_set_comment(SLCONFIG_NODE* node, SLCONFIG_STRING comment, bool copy);

/* String handling */
size_t slc_string_length(SLCONFIG_STRING str);
bool slc_string_equal(SLCONFIG_STRING a, SLCONFIG_STRING b);
SLCONFIG_STRING slc_from_c_str(const char* str);
char* slc_to_c_str(SLCONFIG_STRING str);
void slc_append_to_string(SLCONFIG_STRING* dest, SLCONFIG_STRING new_str, void* function(void*, size_t) custom_realloc);
void slc_destroy_string(SLCONFIG_STRING* str, void* function(void*, size_t) custom_realloc);

private void* Realloc(void* buf, size_t size)
{
	void* ret = null;
	try
		ret = GC.realloc(buf, size);
	catch(OutOfMemoryError e)
		return null;
	return ret;
}

}

private const __gshared VTable = SLCONFIG_VTABLE
(
	&Realloc,
	null,
	null,
	null,
	null,
	null
);

private SLCONFIG_STRING ToStr(const(char)[] str)
{
	return SLCONFIG_STRING(str.ptr, str.ptr + str.length);
}

private const(char)[] FromStr(SLCONFIG_STRING str)
{
	return str.start[0..str.end - str.start];
}

struct SNode
{
	static SNode opCall(const SLCONFIG_VTABLE* vtable = null)
	{
		return SNode(slc_create_root_node(vtable is null ? &VTable : vtable));
	}
	
	static SNode opCall(SLCONFIG_NODE* node)
	{
		SNode ret;
		ret.Node = node;
		return ret;
	}
	
	void AddSearchDirectory(const(char)[] dir)
	{
		slc_add_search_directory(Node, ToStr(dir), false);
	}
	
	void ClearSearchDirectories()
	{
		slc_clear_search_directories(Node);
	}
	
	bool LoadNodes(const(char)[] filename)
	{
		return slc_load_nodes(Node, ToStr(filename));
	}
	
	bool LoadNodes(const(char)[] filename, const(char)[] str)
	{
		return slc_load_nodes_string(Node, ToStr(filename), ToStr(str), false);
	}
	
	bool Save(const(char)[] filename, const(char)[] line_end = "\n", const(char)[] indentation = "\t")
	{
		return slc_save_node(Node, ToStr(filename), ToStr(line_end), ToStr(indentation));
	}
	
	const(char)[] toString(const(char)[] line_end, const(char)[] indentation)
	{
		return FromStr(slc_save_node_string(Node, ToStr(line_end), ToStr(indentation)));
	}
	
	immutable(char)[] toString()
	{
		return cast(typeof(return))toString("\n", "\t");
	}
	
	SNode AddNode(const(char)[] type, const(char)[] name, bool aggregate)
	{
		return SNode(slc_add_node(Node, ToStr(type), false, ToStr(name), false, aggregate));
	}
	
	void Destroy()
	{
		slc_destroy_node(Node);
		Node = null;
	}
	
	SNode GetNode(const(char)[] name)
	{
		return SNode(slc_get_node(Node, ToStr(name)));
	}
	
	SNode GetNode(size_t idx)
	{
		return SNode(slc_get_node_by_index(Node, idx));
	}
	
	alias GetNode opIndex;
	
	@property
	SNode opDispatch(const(char)[] s)()
	{
		return GetNode(mixin(`"` ~ s ~ `"`));
	}
	
	SNode GetNodeByReference(const(char)[] reference)
	{
		return SNode(slc_get_node_by_reference(Node, ToStr(reference)));
	}
	
	int opApply(scope int delegate(size_t idx, SNode node) dg)
	{
		foreach(idx; 0..NumChildren)
		{
			if(int ret = dg(idx, GetNode(idx)))
				return ret;
		}
		return 0;
	}
	
	int opApply(scope int delegate(SNode node) dg)
	{
		return opApply((idx, node) => dg(node));
	}
	
	@property
	const(char)[] Name() const
	{
		return FromStr(slc_get_name(Node));
	}
	
	@property
	const(char)[] Type() const
	{
		return FromStr(slc_get_type(Node));
	}
	
	@property
	const(char)[] FullName() const
	{
		return FromStr(slc_get_full_name(Node));
	}
	
	@property
	bool IsAggregate() const
	{
		return slc_is_aggregate(Node);
	}
	
	@property
	size_t NumChildren() const
	{
		return slc_get_num_children(Node);
	}
	
	T GetValue(T = const(char)[])(T def = T.init) const
	{
		if(!Valid)
		{
			return def;
		}
		else
		{
			try
				return to!(T)(FromStr(slc_get_value(Node)));
			catch(ConversionException e)
				return def;
		}
	}
	
	T SetValue(T)(T val)
	{
		slc_set_value(Node, ToStr(to!(const(char)[])(val)), false);
		return val;
	}
	
	alias SetValue opAssign;
	
	private struct SUserDataWrapper
	{
		intptr_t Data;
		void function(intptr_t) UserDestructor1;
		void delegate(intptr_t) UserDestructor2;
		extern(C) static void CDeclDestructor(intptr_t data)
		{
			auto wrapper = cast(SUserDataWrapper*)data;
			if(wrapper.UserDestructor1)
				wrapper.UserDestructor1(wrapper.Data);
			else if(wrapper.UserDestructor2)
				wrapper.UserDestructor2(wrapper.Data);
		}
	}
	
	@property
	intptr_t UserData()
	{
		auto wrapper = cast(SUserDataWrapper*)slc_get_user_data(Node);
		if(wrapper)
			return wrapper.Data;
		else
			return 0;
	}
	
	@property
	intptr_t UserData(intptr_t data, void function(intptr_t) user_destructor = null)
	{
		auto wrapper = new SUserDataWrapper;
		wrapper.Data = data;
		wrapper.UserDestructor1 = user_destructor;
		slc_set_user_data(Node, cast(intptr_t)wrapper, &SUserDataWrapper.CDeclDestructor);
		return data;
	}
	
	@property
	intptr_t UserData(intptr_t data, void delegate(intptr_t) user_destructor)
	{
		auto wrapper = new SUserDataWrapper;
		wrapper.Data = data;
		wrapper.UserDestructor2 = user_destructor;
		slc_set_user_data(Node, cast(intptr_t)wrapper, &SUserDataWrapper.CDeclDestructor);
		return data;
	}
	
	@property
	const(char)[] Comment() const
	{
		return FromStr(slc_get_comment(Node));
	}
	
	@property
	const(char)[] Comment(const(char)[] comment)
	{
		slc_set_comment(Node, ToStr(comment), false);
		return comment;
	}
	
	T opCast(T)() const
	{
		return GetValue!(T)();
	}
	
	bool opEquals(T)(const T val) const
	{
		static if(is(T : const(SNode)))
			return GetValue() == val.GetValue();
		else
			return GetValue!(T)() == val;
	}
	
	@property
	bool Valid() const
	{
		return Node != null;
	}
protected:
	SLCONFIG_NODE* Node;
}

unittest
{
	auto root = SNode();
	scope(exit) root.Destroy();
	auto var1 = root.AddNode("type", "var", false);
	auto aggr = root.AddNode("type2", "aggr", true);
	auto var2 = aggr.AddNode("type", "var", false);
	var1 = 5;
	var2 = "abc";
	var2.Comment = "this is a comment";
	
	assert(root.GetNodeByReference("aggr:var") == aggr["var"]);
	
	foreach(idx, node; root)
	{
		if(idx == 0)
			assert(node == var1);
		else if(idx == 1)
			assert(node.NumChildren == 1);
	}
	
	const(char)[] src = "
	type var = 5;
	type2 aggr
	{
		//*this is a comment
		type var = abc;
	}";
	
	auto root2 = SNode();
	scope(exit) root2.Destroy();
	root2.LoadNodes("", src);
	
	assert(root2.toString() == root.toString());
	
	auto root3 = SNode();
	root3.UserData = 1;
	assert(root3.UserData == 1);
	
	int counter = 0;
	void destructor(intptr_t data)
	{
		counter += data;
	}

	root3.UserData(1, &destructor);
	root3.UserData(2, &destructor);
	root3.Destroy();
	assert(counter == 3);
	assert(!root3.Valid);
	
	auto root4 = SNode();
	scope(exit) root2.Destroy();
	root4.AddNode("", "a", false) = true;
	root4.AddNode("", "b", false) = 1.356;
	assert(root4.a == true);
	assert(cast(double)root4.b - 1.356 < 0.01);
	assert(!root.c.Valid);
	assert(cast(double)root.c is double.init);
	assert(root.c.GetValue(5) == 5);
}
