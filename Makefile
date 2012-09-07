include build/commands.make

CC = gcc
C_FLAGS = -g -Wall -Wextra --std=c99 -I./include

LIB_SOURCES = $(wildcard src/*.c)
LIB_OBJS = $(patsubst src/%.c, .objs/%.o, $(LIB_SOURCES))
LIB_NAME = slconfig
LIB_FILE = lib/libslconfig$(STATIC_LIB_EXT)

EXAMPLE_SOURCES = $(wildcard examples/*.c)
EXAMPLE_OBJS = $(patsubst examples/%.c, .objs/%.o, $(EXAMPLE_SOURCES))
EXAMPLE_FILES = $(patsubst examples/%.c, bin/%$(EXE), $(EXAMPLE_SOURCES))
EXAMPLE_LDFLAGS = -Llib -l$(LIB_NAME)

.PHONY : all
.PHONY : library
.PHONY : examples
.PHONY : clean
.PHONY : FORCE

all : library examples
library : $(LIB_FILE)
examples : $(EXAMPLE_FILES)

.objs : 
	$(MKDIR) .objs

bin :
	$(MKDIR) bin

lib :
	$(MKDIR) lib

$(LIB_FILE) : lib $(LIB_OBJS)
	ar rs $(LIB_FILE) $(LIB_OBJS)

bin/% : .objs/%.o bin library
	$(CC) $< -o $@ $(EXAMPLE_LDFLAGS)

.objs/%.o : src/%.c FORCE .objs
	$(CC) -c $< $(C_FLAGS) -o $@

.objs/%.o : examples/%.c FORCE .objs
	$(CC) -c $< $(C_FLAGS) -o $@

clean : 
	$(RM) $(LIB_FILE)
	$(RM) $(EXAMPLE_FILES)
	$(RM) $(LIB_OBJS) $(EXAMPLE_OBJS)
	$(RMDIR) .objs
	$(RMDIR) bin
	$(RMDIR) lib
