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

## Table of Contents

1. [Building](#building)
2. [Format Definition](#format-definition)
3. [User API](#user-api)
4. [License](#license)

## Building

Run `make` to build the library and examples.

## Format Definition

### File Encoding

SLConfig expects files in the UTF-8 encoding. Both Windows, Linux and 
OSX style line endings are accepted, but as noted later, they are not 
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

### Strings

BNF definition:

```
{Quoted String Chars} = {All Valid} - ["\]
{Naked String Chars} = {All Valid} - {Whitespace} - [=#~{}$:;"]
{Naked String Start} = {Naked String Chars} - [/*]

NakedString = {Naked String Chars}
            | {Naked String Chars}? {Naked String Start} {Naked String Chars}*
QuotedString = '"' ( {Quoted String Chars} | '\' {Printable} )* '"'
```

Aside from the special tokens, whitespace and comments, everything inside a 
SLConfig file is a string. There are 3 types of strings: naked 
strings, escaped strings and heredocs.

Naked strings are just sequences of characters (that are not reserved 
tokens). Here are 5 valid strings:

```
valid
!@#%
1.5e-3
asd//
asd/*
```

Note that the last two seem to contain comment starts, but in fact the 
parser will absorb those characters.

## User API

## License

The library is licensed under LGPL 3.0 (see LICENSE). The examples are licensed under the Zlib license.
