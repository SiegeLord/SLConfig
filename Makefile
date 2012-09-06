include build/commands.make

CC = gcc
C_FLAGS = -Wall -Wextra --std=c99 -I.

LIB_SOURCES = $(wildcard src/*.c)
LIB_OBJS = $(patsubst src/%.c, .objs/%.o, $(LIB_SOURCES))
LIB_NAME = slconfig
LIB_FILE = libslconfig$(STATIC_LIB_EXT)

EXAMPLE_SOURCES = $(wildcard example/*.c)
EXAMPLE_OBJS = $(patsubst example/%.c, .objs/%.o, $(EXAMPLE_SOURCES))
EXAMPLE_FILES = $(patsubst example/%.c, %$(EXE), $(EXAMPLE_SOURCES))
EXAMPLE_LDFLAGS = -L. -l$(LIB_NAME)

.PHONY : all
.PHONY : lib
.PHONY : example
.PHONY : clean
.PHONY : FORCE

all : lib example
lib : $(LIB_FILE)
example : $(EXAMPLE_FILES)

.objs : 
	$(MKDIR) .objs

$(LIB_FILE) : $(LIB_OBJS)
	ar rs $(LIB_FILE) $(LIB_OBJS)

% : .objs/%.o lib
	$(CC) $< -o $@ $(EXAMPLE_LDFLAGS)

.objs/%.o : src/%.c FORCE .objs
	$(CC) -c $< $(C_FLAGS) -o $@

.objs/%.o : example/%.c FORCE .objs
	$(CC) -c $< $(C_FLAGS) -o $@

clean : 
	$(RM) $(LIB_FILE)
	$(RM) $(EXAMPLE_FILES)
	$(RM) $(LIB_OBJS) $(EXAMPLE_OBJS)
	$(RMDIR) .objs
