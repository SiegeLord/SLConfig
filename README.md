SLConfig
========

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

Table of Contents
-----------------

1. [Building](#building)
2. [Format Definition](#format-definition)
3. [User API](#user-api)
4. [License](#license)

Building
--------

Run `make` to build the library and examples.

Format Definition
-----------------

User API
--------

License
-------

The library is licensed under LGPL 3.0 (see LICENSE). The examples are licensed under the Zlib license.
