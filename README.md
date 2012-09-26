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

Run `make` to build the library and examples.

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
string nodes have a string value. All nodes regardless of type have a user 
defined static type and a name. Nodes inside a SLConfig file are all children 
of a root aggregate node which has no name or type. Here are some examples of 
string nodes:

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
the colon symbol. Absolute references start with a double colon. Some examples 
of valid usage:

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
their name with the dollar symbol. This can be done with both with aggregate and
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

It is possible to delete nodes by prefixing their name with a tilde. Some 
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

Escaped strings are sequences of characters between double quotes. Double 
quotes can be inserted into escaped quotes by escaping them with the backslash 
('\'). Most escaped codes from the C are also supported. Unknown escape codes 
are passed as is (e.g. "\8" becomes "8"). Newline characters are 
accepted, but are parsed as is, so you must be mindful of what style of line 
endings you use. Here are some examples of escaped strings, again defining 
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

Comments whose first character is a `*` are docstrings, and are 
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

[slc_create_config](#slc_create_config)

[slc_add_search_directory](#slc_add_search_directory)

[slc_clear_search_directories](#slc_clear_search_directories)

[slc_load_config](#slc_load_config)

[slc_load_config_string](#slc_load_config_string)

[slc_node_to_string](#slc_node_to_string)


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

###SLCONFIG_VTABLE
```c
typedef struct
{
	void* (*realloc)(void*, size_t);
	void (*stderr)(SLCONFIG_STRING s);
	void* (*fopen)(SLCONFIG_STRING filename, bool read);
	int (*fclose)(void*);
	size_t (*fread)(void*, size_t, void*);
} SLCONFIG_VTABLE;
```

###SLCONFIG_NODE
```c
typedef struct SLCONFIG_NODE SLCONFIG_NODE;
```

###slc_create_config
```c
SLCONFIG_NODE* slc_create_config(const SLCONFIG_VTABLE* vtable);
```

###slc_add_search_directory
```c
void slc_add_search_directory(SLCONFIG_NODE* node, SLCONFIG_STRING directory,
                              bool copy);
```

###slc_clear_search_directories
```c
void slc_clear_search_directories(SLCONFIG_NODE* node);
```

###slc_load_config
```c
bool slc_load_config(SLCONFIG_NODE* aggregate, SLCONFIG_STRING filename);
```

###slc_load_config_string
```c
bool slc_load_config_string(SLCONFIG_NODE* aggregate, SLCONFIG_STRING filename,
                            SLCONFIG_STRING file, bool copy);
```
###slc_node_to_string
```c
SLCONFIG_STRING slc_node_to_string(SLCONFIG_NODE* node, SLCONFIG_STRING line_end,
                                   SLCONFIG_STRING indentation);
```

###slc_add_node
```c
SLCONFIG_NODE* slc_add_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING type,
                            bool copy_type, SLCONFIG_STRING name,
                            bool copy_name, bool is_aggregate);
```

###slc_destroy_node
```c
void slc_destroy_node(SLCONFIG_NODE* node);
```

###slc_get_node
```c
SLCONFIG_NODE* slc_get_node(SLCONFIG_NODE* aggregate, SLCONFIG_STRING name);
```

###slc_get_node_by_index
```c
SLCONFIG_NODE* slc_get_node_by_index(SLCONFIG_NODE* aggregate, size_t idx);
```

###slc_get_node_by_reference
```c
SLCONFIG_NODE* slc_get_node_by_reference(SLCONFIG_NODE* aggregate, SLCONFIG_STRING reference);
```

###slc_get_name
```c
SLCONFIG_STRING slc_get_name(SLCONFIG_NODE* node);
```

###slc_get_full_name
```c
SLCONFIG_STRING slc_get_full_name(SLCONFIG_NODE* node);
```

###slc_get_type
```c
SLCONFIG_STRING slc_get_type(SLCONFIG_NODE* node);
```

###slc_is_aggregate
```c
bool slc_is_aggregate(SLCONFIG_NODE* node);
```

###slc_get_num_children
```c
size_t slc_get_num_children(SLCONFIG_NODE* node);
```

###slc_get_value
```c
SLCONFIG_STRING slc_get_value(SLCONFIG_NODE* string_node);
```

###slc_set_value
```c
bool slc_set_value(SLCONFIG_NODE* string_node, SLCONFIG_STRING value, bool copy);
```

###slc_get_user_data
```c
intptr_t slc_get_user_data(SLCONFIG_NODE* node);
```

###slc_set_user_data
```c
void slc_set_user_data(SLCONFIG_NODE* node, intptr_t data,
                       void (*user_destructor)(intptr_t));
```

###slc_get_comment
```c
SLCONFIG_STRING slc_get_comment(SLCONFIG_NODE* node);
```

###slc_set_comment
```c
void slc_set_comment(SLCONFIG_NODE* node, SLCONFIG_STRING comment, bool copy);
```

###slc_string_length
```c
size_t slc_string_length(SLCONFIG_STRING str);
```

###slc_string_equal
```c
bool slc_string_equal(SLCONFIG_STRING a, SLCONFIG_STRING b);
```

###slc_from_c_str
```c
SLCONFIG_STRING slc_from_c_str(const char* str);
```

###slc_to_c_str
```c
char* slc_to_c_str(SLCONFIG_STRING str);
```

###slc_append_to_string
```c
void slc_append_to_string(SLCONFIG_STRING* dest, SLCONFIG_STRING new_str, 
                          void* (*custom_realloc)(void*, size_t));
```

###slc_destroy_string
```c
void slc_destroy_string(SLCONFIG_STRING* str,
                        void* (*custom_realloc)(void*, size_t));
```

## License

The library is licensed under LGPL 3.0 (see LICENSE). The examples are licensed under the Zlib license.
