# SLConfig

SLConfig was born out of my need to have a simple configuration file format that
supported user defined types, had nice syntax and allowed for some manipulation
of the configuration data from within the configuration file itself. SLConfig
supports only string types and aggregate types, but because it allows the types
to be named inside the configuration file, static checking is possible by the
user code. Here is a quick example of what the configuration file looks like.
Keep in mind that there is no support for numbers in SLConfig. Even if it looks
like a number, its just a string as far as SLConfig is concerned (it allows
unquoted strings with some restrictions as to their content).

```
// Import nodes from a different file
#include definitions.slc

// This comment is ignored, but the next comment is accessible by user code
/**An ant*/
enemy Ant
{
    // Extracting the values of existing string nodes (it came from the include)
    float health = $MIN_ENEMY_HEALTH;
    
    // Blank types are also allowed by the syntax
    armor = 5;
    
    attack bite_attack;
    
    // Heredocs (the 'd' before the quote is the sentinel)
    description = d"An ant, also known as a "bear" is a dangerous foe."d;
}

enemy YellowAnt
{
    // Import all nodes from the Ant node
    $Ant;
    
    // String concatenation (description now holds "An ant, ... foe. This one... ")
    // Note that single quoted strings support escape sequences ('\"' below)
    description = $description " This one in particular is \"yellow\".";
    
    // Remove a node
    ~bite_attack;
}

// It is possible to affect nested nodes from outside the nodes they are nested in
YellowAnt:health = 10;
```

## Table of Contents

1. [Building](#building)
2. [Format Definition](#format-definition)
2. [Approximate BNF Syntax](#approximate-bnf-syntax)
3. [User API](#user-api)
4. [License](#license)

## Building

You will need a C99 capable compiler. Run `make` to build the library and
examples. Run `make install` to install the library.

Useful targets:

* _all_ - Make the library and the examples
* _static_ - Make just the static library
* _shared_ - Make just the shared library
* _examples_ - Make just the examples
* _documentation_ - Use pandoc to convert this readme into HTML
* _clean_ - Undo the actions of the _all_ target

Useful variables to alter:

* _CC_ - The C compiler
* _INSTALL_PREFIX_ - Where to install the files

## Format Definition

1. [Nodes](#nodes)
2. [Comments](#comments)
3. [Assignment](#assignment)
4. [References](#references)
5. [Node expansion](#node-expansion)
6. [String concatenation](#string-concatenation)
7. [Node deletion](#node-deletion)
8. [File includes](#file-includes)
9. [Strings](#strings)
10. [Docstrings](#docstrings)
11. [File encoding](#file-encoding)
12. [Reserved tokens](#reserved-tokens)

### Nodes

An SLConfig file is a tree of nodes. There are two types of nodes: aggregate 
nodes and string nodes. Aggregate nodes can have zero or more children, while 
string nodes have a string value. The order of children is important.
All nodes regardless of type have a user defined static type and a name. Nodes
inside a SLConfig file are all children of a root aggregate node which has no
name or type. Here are some examples of string nodes:
```
string_node_no_type;
typename string_node;
string_node = value;
type node = value;
```

And here are some examples of aggregate nodes containing other nodes:

```
empty_aggregate {}
type aggregate
{
    other_aggregate
    {
    
    }
    string_node;
}
```

### Comments

Two types of comments are supported. Line comments:

```
//This is a comment
```

And block comments:

```
/*This is a comment*/
```

Unlike C/C++, block comments nest:

```
/*
/*
This is a comment
*/
Still a comment
*/
```

### Assignment

Nodes in SLConfig are mutable and their values can be changed. The type of a 
node, however, cannot be changed. Additionally, a string node cannot be 
changed into aggregate node and vice versa. Here are some examples of valid 
assignments:

```
type var = abc;
var = def;
type var; // Ok, as type is not changed. The value is set to an empty string

type aggr {}
aggr
{
    type var;
    var = def; // Note that this assignment happens to the var inside aggr
}
```

And here are some invalid ones (same nodes as above example):

```
type2 var = abc; // Can't change the type of var from 'type' to 'type2'
var {}           // Can't change var into an aggregate
aggr = abc;      // Can't change aggr into a string
```

### References

It is possible to refer to nodes outside of the current aggregate's scope using 
the colon symbol. Absolute references start with a double colon. Relative 
references are handled by searching the current aggregate, and then searching 
its parent if it is not found. Some examples of valid usage:

```
type var;
type aggr
{
    type var;
    ::var = abc; // Assignment happens to the external var
}

aggr:var = def;
```

It is not possible to insert new nodes into an aggregate from outside the 
aggregate. So, this is illegal:

```
type aggr {}
type aggr:var = abc;
```

### Node expansion

Nodes can be expanded to assign their values to other nodes by prefixing 
their name with the dollar symbol (`$`). This can be done with both with aggregate and
string nodes. When an aggregate is expanded, it's contents get pasted into the
current aggregate. Some examples:

```
type var1 = abc;
type var2 = $var1; // var2 now has the value of abc
type aggr1
{
    type var = abc;
}
type aggr2
{
    $aggr1; // aggr2 now has a child var node
}
aggr1
{
    $aggr1;
    type var2;
}
// aggr1 now has two child nodes, var and var2
aggr1
{
    var = def;
    var = $::aggr1:var; // This refers to the yet unmodified aggr1, so the value of var is now abc
}
```

Note that in the above example there are references to `aggr1` while assigning 
to it. This is legal because the actual assignment doesn't happen until the 
entire right hand side expression (the brace block) is finished.

### String concatenation

During string assignment values on the right hand side of the equals sign that 
are separated by whitespace are concatenated. This applies to string 
expansions too. Some examples:

```
type var = abc def; // The value of var is abcdef
var = $var ABC $var; // The value of var is abcdefABCabcdef
```

### Node deletion

It is possible to delete nodes by prefixing their name with a tilde (`~`). Some 
examples:

```
type var;
~var;
type var // Valid, as there is no more var
{
    type var;
}
~var:var; // The aggregate is now empty
```

### File includes

It is possible to include the contents of other files inside aggregates. 
Semantically this is identical to expanding the root aggregate nodes of those 
files. An example:

```
// File1
type var = abc;
```

```
// File2
type aggr
{
    #include File1;
    type var2 = $var; // var2 has the value of abc
}
```

### Strings

In every example so far everything that wasn't a special character was a 
string. The name of the node, its type and the string value are all strings. 
There are three types of strings in SLConfig: naked strings, escaped strings 
and heredocs. So far all examples used naked strings.

Naked strings are just sequences of non-whitespace characters that are not 
special tokens. Here are some examples of naked strings (all defining string nodes):

```
valid;
!@#%;
1.5e-3;
asd//;
asd/*;
```

Note that the last two seem to contain comment starts, but in fact the 
parser will absorb those characters into the naked string.

Escaped strings are sequences of characters between double quotes (`"`). Double 
quotes can be inserted into escaped quotes by escaping them with the backslash 
(`\`). Most escaped codes from the C are also supported. Unknown escape codes 
are passed as is (e.g. `"\8"` becomes `"8"`). Newline characters are 
accepted, but are parsed as is, so note must be taken of what style of line 
endings are used. Here are some examples of escaped strings, again defining 
string nodes (it is perfectly valid to use any type of string for any usage of 
string that has been shown so far):

```
"a b c d";
"a
b";
"a\"b\"c";
"\0\a\b\f\n\r\t\v"; // All known escape codes
```

Heredocs are strings with a user defined delimeter that have no escape 
semantics. The delimeter is defined by specifying a naked string sentinel 
followed by a double quote. The parser will then absorb any and all characters 
into the string until it finds a double quote followed by that same sentinel. 
Some examples of heredoc strings defining string nodes:

```
a"d"\a"a; // The name of this node is d"\a
abc"ab""abc; // The name of this node is ab"
```

Here are some uses of the above types of string to define some nodes:

```
"ty pe" "var" = "abc " "def"; // The value of this node is "abc def"
"with spaces" = d"""d;
var = $"with spaces"; // The value of var is now "
```

### Docstrings

Comments whose first character is an asterisk (`*`) are docstrings, and are 
accessible from the user code. They are bound to the node definition 
that follows them, or that is on the same line before them. Multiple 
docstrings are concatenated together with the line feed character. If a 
docstring does not fit that description, it is ignored.

```
//*You can read me in the host code
/**Me too!*/
node; //*This and the two comments above attach to this node
//*This comment is lost
```

### File Encoding

SLConfig expects files in the UTF-8 encoding. Both Windows, Linux and 
OSX style line endings are accepted, but as noted earlier, they are not 
converted at all during parsing, which means that the user application
may need to take them into account.

### Reserved tokens

The following tokens cannot be present in naked strings, or heredoc 
sentinels.

```
~
#
{
}
;
$
=
:
::
```

## Approximate BNF Syntax

Here is a [GOLD parser](http://www.goldparser.org) compatible syntax for SLConfig. It describes most of the 
language with a notable exception of heredocs.

```
Comment Line = '//'
Comment Start = '/*'
Comment End = '*/'
Comment Block @= { Nesting = All, Advance = Character}

"Start Symbol" = <AggregateContents>

! -------------------------------------------------
! Character Sets
! -------------------------------------------------

{Quoted String Chars} = {All Valid} - ["\]
{Naked String Chars} = {All Valid} - {Whitespace} - [=#~{}$:;"]
{Naked String Start} = {Naked String Chars} - [/*]

! -------------------------------------------------
! Terminals
! -------------------------------------------------

NakedString = {Naked String Chars}
            | {Naked String Chars}? {Naked String Start} {Naked String Chars}*
EscapedString = '"' ( {Quoted String Chars} | '\' {Printable} )* '"'

! -------------------------------------------------
! Rules
! -------------------------------------------------

! Aggregates
<AggregateContents> ::= <Statements>
                     |

<Statements> ::= <Statement> | <Statements> <Statement>

! Expressions
<String> ::= EscapedString | NakedString

<Reference> ::= <String>
             | '::' <String>
             |  <Reference> ':' <String>

<Expand> ::= '$' <Reference>

<StringSource> ::= <Reference> | <Expand>

<RValue> ::= <StringSource>
          |  <RValue> <StringSource>

<LValue> ::= <Reference>
          |  <String> <String>

! Statements
<NodeStatement> ::= <LValue> ';'
                 |  <LValue> '=' <RValue> ';'
                 |  <LValue> '{' <AggregateContents> '}'


<DestroyStatement> ::= '~' <Reference> ';'

<IncludeStatement> ::= '#include' <String> ';'

<ExpandStatement> ::= <Expand> ';'

<Statement> ::= <NodeStatement>
             |  <DestroyStatement>
             |  <IncludeStatement>
             |  <ExpandStatement>
             |  ';'
```

## User API

###Types:

[SLCONFIG_STRING](#slconfig_string)

[SLCONFIG_VTABLE](#slconfig_vtable)

[SLCONFIG_NODE](#slconfig_node)


###Node IO:

[slc_create_root_node](#slc_create_root_node)

[slc_add_search_directory](#slc_add_search_directory)

[slc_clear_search_directories](#slc_clear_search_directories)

[slc_load_nodes](#slc_load_nodes)

[slc_load_nodes_string](#slc_load_nodes_string)

[slc_save_node](#slc_save_node)

[slc_save_node_string](#slc_save_node_string)


###Node creation/destruction:

[slc_add_node](#slc_add_node)

[slc_destroy_node](#slc_destroy_node)

###Node access:

[slc_get_node](#slc_get_node)

[slc_get_node_by_index](#slc_get_node_by_index)

[slc_get_node_by_reference](#slc_get_node_by_reference)

###Node properties:

[slc_get_name](#slc_get_name)

[slc_get_type](#slc_get_type)

[slc_get_full_name](#slc_get_full_name)

[slc_is_aggregate](#slc_is_aggregate)

[slc_get_num_children](#slc_get_num_children)

[slc_get_value](#slc_get_value)

[slc_set_value](#slc_set_value)

[slc_get_user_data](#slc_get_user_data)

[slc_set_user_data](#slc_set_user_data)

[slc_get_comment](#slc_get_comment)

[slc_set_comment](#slc_set_comment)

###String handling:

[slc_string_length](#slc_string_length)

[slc_string_equal](#slc_string_equal)

[slc_from_c_str](#slc_from_c_str)

[slc_to_c_str](#slc_to_c_str)

[slc_append_to_string](#slc_append_to_string)

[slc_destroy_string](#slc_destroy_string)

###SLCONFIG_STRING
```c
typedef struct
{
	const char* start;
	const char* end;
} SLCONFIG_STRING;
```

A string type used by the SLConfig library. C strings were not used to minimize 
unnecessary memory allocations for the zero byte. All strings 
returned by the API should be treated as immutable.

_Fields_:

* _start_ - pointer to the start of the string
* _end_ - pointer one byte past the end of the string

###SLCONFIG_VTABLE
```c
typedef struct
{
	void* (*realloc)(void* buf, size_t size);
	void (*stderr)(SLCONFIG_STRING s);
	void* (*fopen)(SLCONFIG_STRING filename, bool read);
	int (*fclose)(void* file);
	size_t (*fread)(void* buf, size_t size, void* file);
	size_t (*fwrite)(const void* buf, size_t size, void* file);
} SLCONFIG_VTABLE;
```

A vtable that can be used to customize how SLConfig interacts with files and 
memory. The user specified functions should follow the same semantics as the 
default implementations (see code blocks after each field documentation).

_Fields_:

* _realloc_ - Memory management.

```c
    void* default_realloc(void* buf, size_t size)
    {
        return realloc(buf, size);
    }
```

* _stderr_ - Error output. A copy of the passed string should be made if it is
retained for longer than the duration of the call.

```c
    void default_stderr(SLCONFIG_STRING s)
    {
        fprintf(stderr, "%.*s", (int)slc_string_length(s), s.start);
    }
```

* _fopen_ - Opening file objects for reading and writing.

```c
    void* default_fopen(SLCONFIG_STRING filename, bool read)
    {
        char* filename_str = slc_to_c_str(filename);
        void* ret = fopen(filename_str, read ? "rb" : "wb");
        free(filename_str);
        return ret;
    }
```

* _fclose_ - Close file objects created by fopen.

```c
    int default_fclose(void* f)
    {
        return fclose(f);
    }
```

* _fread_ - Read from file objects.

```c
    size_t default_fread(void* buf, size_t size, void* f)
    {
        return fread(buf, 1, size, f);
    }
```

* _fwrite_ - Write to file objects.

```c
    size_t default_fwrite(const void* buf, size_t size, void* f)
    {
        return fwrite(buf, 1, size, f);
    }
```

###SLCONFIG_NODE
```c
typedef struct SLCONFIG_NODE SLCONFIG_NODE;
```

An opaque struct representing an SLConfig node.

###slc_create_root_node
```c
SLCONFIG_NODE* slc_create_root_node(const SLCONFIG_VTABLE* vtable);
```

Creates an empty root node using the passed vtable. All nodes added to this 
root will use the same vtable. If the `vtable` is `NULL` then the default 
implementations are used. If any field of the vtable is `NULL` then the 
default implementation is used for that field.

_Arguments_:

* _vtable_ - vtable to use for all future operations.

_Returns_:

Newly created root node, or `NULL` if there is an error.

###slc_add_search_directory
```c
void slc_add_search_directory(SLCONFIG_NODE* node, SLCONFIG_STRING directory,
                              bool copy);
```

Adds a directory to search when loading files. This is used for both `#include` 
statements and the loading functions. Directories are searched in order of their 
addition with the current directory always being searched first. All directories
are shared between all nodes in the tree.

_Arguments_:

* _node_ - any node in the tree
* _directory_ - the directory to search
* _copy_ - whether to make a copy of the directory string or just reference it

###slc_clear_search_directories
```c
void slc_clear_search_directories(SLCONFIG_NODE* node);
```

Erase all the search directories used by the tree.

_Arguments_:

* _node_ - any node in the tree

###slc_load_nodes
```c
bool slc_load_nodes(SLCONFIG_NODE* aggregate, SLCONFIG_STRING filename);
```

Opens a file, parses all of the nodes inside it and inserts them into the 
aggregate.

_Arguments_:

* _aggregate_ - an aggregate node
* _filename_ - path to the file

_Returns_:

`true` if the file was loaded successfully, `false` if there was an error. 
Possible errors include `aggregate` not being an aggregate, there being syntax 
errors in the file, or the file not being found.

###slc_load_nodes_string
```c
bool slc_load_nodes_string(SLCONFIG_NODE* aggregate, SLCONFIG_STRING filename,
                           SLCONFIG_STRING file, bool copy);
```

Like [slc_load_nodes](#slc_load_nodes) but with a passed string instead of an 
external file. Note that the function may still perform file operations if the 
passed string has `#include` statements.

_Arguments_:

* _aggregate_ - an aggregate node
* _filename_ - path to the file. This is only used for error reporting
* _file_ - contents of the file to parse
* _copy_ - whether to make a copy of the file string or just reference it

_Returns_:

`true` if the file was parsed successfully, `false` if there was an error. 
Possible errors include `aggregate` not being an aggregate or there being syntax 
errors in the file.

###slc_save_node
```c
bool slc_save_node(SLCONFIG_NODE* node, SLCONFIG_STRING filename,
                   SLCONFIG_STRING line_end, SLCONFIG_STRING indentation);
```

Saves a node to a file. Block comments are used for docstrings. Heredocs are 
used for strings that cannot be naked strings; naked strings are used 
otherwise.

_Arguments_:

* _node_ - any node. This doesn't have to be the root node
* _filename_ - filename to save the node to
* _line_end_ - string to append at the end of every statement. Can be empty
* _indentation_ - string to prepend to statements for every level of 
indentation. Can be empty

_Returns_:

`true` if the node was saved successfully. `false` if there was an error with 
writing the file.

###slc_save_node_string
```c
SLCONFIG_STRING slc_save_node_string(SLCONFIG_NODE* node, SLCONFIG_STRING line_end,
                                   SLCONFIG_STRING indentation);
```

Like [slc_save_node](#slc_save_node) but with redirecting the output to a 
string.

_Arguments_:

* _node_ - any node. This doesn't have to be the root node
* _line_end_ - string to append at the end of every statement. Can be empty
* _indentation_ - string to prepend to statements for every level of 
indentation. Can be empty

_Returns_:

The string holding the representation of the passed node. This string is newly 
allocated and will need to be destroyed.

###slc_add_node
```c
SLCONFIG_NODE* slc_add_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING type,
                            bool copy_type, SLCONFIG_STRING name,
                            bool copy_name, bool is_aggregate);
```

Adds a node to an aggregate. If an aggregate already has a node which matches 
the type string, the name and the type (aggregate vs. not aggregate) then that 
node is returned instead without making a new node. The node is initialized to 
have no children if it is an aggregate, or an empty string if it is not. If an 
existing node is returned then it is not altered in any way.

_Arguments_:

* _aggregate_ - aggregate to add the node to
* _type_ - type string of the new node. Can be empty
* _copy_type_ - whether to make a copy of the type string or just reference it
* _name_ - name of the new node. Can be empty
* _copy_name_ - whether to make a copy of the name string or just reference it
* _is_aggregate_ - whether the newly added node is an aggregate

_Returns_:

Newly added node, existing node or `NULL` if there is an error. Possible 
sources of error include `aggregate` not being an aggregate, or there already 
existing a node with the same name but differing type.

###slc_destroy_node
```c
void slc_destroy_node(SLCONFIG_NODE* node);
```

Destroys a node, destroying any of its children and detaching itself from its 
parent, if any. The order of its sibling nodes is not affected. All references 
to the children to this node become invalid.

_Arguments_:

* _node_ - any node

###slc_get_node
```c
SLCONFIG_NODE* slc_get_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING name);
```

Searches the children of the passed aggregate for a node with a certain name.

_Arguments_:

* _aggregate_ - an aggregate to search
* _name_ - name of the node to search for

_Returns_:

The found node or `NULL` if no such child node exists.

###slc_get_node_by_index
```c
SLCONFIG_NODE* slc_get_node_by_index(SLCONFIG_NODE* aggregate, size_t idx);
```

Gets the child of the passed aggregate by its index.

_Arguments_:

* _aggregate_ - an aggregate to examine
* _idx_ - index of the node to retrieve

_Returns_:

The node that corresponds to `idx` or `NULL` if the index is invalid.

###slc_get_node_by_reference
```c
SLCONFIG_NODE* slc_get_node_by_reference(SLCONFIG_NODE* aggregate, SLCONFIG_STRING reference);
```

Searches for a node using the reference using the aggregate as a starting 
point. Essentially the same syntax for references is used as in the file 
format except that comments are not allowed. Note that if a relative 
reference is passed to this function then the node that is returned might not 
be a child of the aggregate, and in fact could be the aggregate itself.

_Arguments_:

* _aggregate_ - an aggregate to start the search in (if a relative reference 
is used)
* _reference_ - reference to the node to search for

_Returns_:

The found node or `NULL` if no such node exists.

###slc_get_name
```c
SLCONFIG_STRING slc_get_name(SLCONFIG_NODE* node);
```

Gets the name of the node.

_Arguments_:

* _node_ - any node

_Returns_:

The name of the node.


###slc_get_full_name
```c
SLCONFIG_STRING slc_get_full_name(SLCONFIG_NODE* node);
```

Gets the full name of the node. This is essentially the absolute reference to it. 
It is not exactly that because the names of the node and its ancestors are not 
escaped, so this is primarily useful for error reporting.

_Arguments_:

* _node_ - any node

_Returns_:

The full name of the node.

###slc_get_type
```c
SLCONFIG_STRING slc_get_type(SLCONFIG_NODE* node);
```

Gets the type of the node.

_Arguments_:

* _node_ - any node

_Returns_:

The type of the node.

###slc_is_aggregate
```c
bool slc_is_aggregate(SLCONFIG_NODE* node);
```

Checks whether the node is an aggregate or not.

_Arguments_:

* _node_ - any node

_Returns_:

`true` if the node is an aggregate, `false` otherwise.

###slc_get_num_children
```c
size_t slc_get_num_children(SLCONFIG_NODE* node);
```

Returns the number of children of the node.

_Arguments_:

* _node_ - any node

_Returns_:

Number of children of the node if it is an aggregate, `0` otherwise.

###slc_get_value
```c
SLCONFIG_STRING slc_get_value(SLCONFIG_NODE* string_node);
```

Gets the value of a string node.

_Arguments_:

* _string_node_ - any string node

_Returns_:

The value of the string node.

###slc_set_value
```c
bool slc_set_value(SLCONFIG_NODE* string_node, SLCONFIG_STRING value, bool copy);
```

Sets the value of a string node.

_Arguments_:

* _string_node_ - any string node
* _value_ - new value
* _copy_ - whether to make a copy of the value string or just reference it

_Returns_:

`true` if the value was set successfully, `false` otherwise (e.g. the 
`string_node` is actually an aggregate).

###slc_get_user_data
```c
intptr_t slc_get_user_data(SLCONFIG_NODE* node);
```

Gets the user data of the node.

_Arguments_:

* _node_ - any node

_Returns_:

The user data of the node.

###slc_set_user_data
```c
void slc_set_user_data(SLCONFIG_NODE* node, intptr_t data,
                       void (*user_destructor)(intptr_t));
```

Sets the user data of the node, destroying any user data that was previously 
set for this node.

_Arguments_:

* _node_ - any node
* _data_ - user data
* _user_destructor_ - this function will be called with the `data` as the 
argument when the node is destroyed, or this function is called again. Can be 
`NULL`

_Returns_:

The user data of the node.

###slc_get_comment
```c
SLCONFIG_STRING slc_get_comment(SLCONFIG_NODE* node);
```

Gets the docstring of the node.

_Arguments_:

* _node_ - any node

_Returns_:

The docstring of the node.

###slc_set_comment
```c
void slc_set_comment(SLCONFIG_NODE* node, SLCONFIG_STRING comment, bool copy);
```

Sets the docstring of a string node.

_Arguments_:

* _string_node_ - any string node
* _docstring_ - new docstring
* _copy_ - whether to make a copy of the docstring or just reference it

###slc_string_length
```c
size_t slc_string_length(SLCONFIG_STRING str);
```

Gets the length of the string.

_Arguments_:

* _str_ - a string

_Returns_:

Length of the string in bytes.

###slc_string_equal
```c
bool slc_string_equal(SLCONFIG_STRING a, SLCONFIG_STRING b);
```

Checks if two strings are equal.

_Arguments_:

* _a_ - a string
* _b_ - a string

_Returns_:

`true` if the two strings are identical, `false` if they are not.

###slc_from_c_str
```c
SLCONFIG_STRING slc_from_c_str(const char* str);
```

Creates a reference to the passed zero delimited C string.

_Arguments_:

* _str_ - a C string

_Returns_:

A string that references the passed C string.

###slc_to_c_str
```c
char* slc_to_c_str(SLCONFIG_STRING str);
```

Creates a zero delimited C string. The passed string is not modified.

_Arguments_:

* _str_ - a string

_Returns_:

A newly allocated C string. This string should be freed using the `free` 
function.

###slc_append_to_string
```c
void slc_append_to_string(SLCONFIG_STRING* dest, SLCONFIG_STRING new_str, 
                          void* (*custom_realloc)(void*, size_t));
```

Appends a string to a different string using a custom memory allocator if 
needed.

_Arguments_:

* _dest_ - destination string
* _new_str_ - string to append to the destination string
* _custom_realloc_ - a custom realloc function. Can be `NULL` in which case 
the C `realloc` function is used.

###slc_destroy_string
```c
void slc_destroy_string(SLCONFIG_STRING* str,
                        void* (*custom_realloc)(void*, size_t));
```

Destroys a string using a custom memory allocator if needed. For strings
returned by the API the allocator must match the allocator passed in the 
custom vtable.

_Arguments_:

* _str_ - string to destroy
* _custom_realloc_ - a custom realloc function. Can be `NULL` in which case 
the C `realloc` function is used.

## License

The library is licensed under LGPL 3.0 (see LICENSE). The examples are licensed under the Zlib license.
