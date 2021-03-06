include build/commands.make

CC = gcc
C_FLAGS = -g -O2 -Wall -Wextra --std=c99 -I./include

LIB_SOURCES = $(wildcard src/*.c)
STATIC_OBJS = $(patsubst src/%.c, .objs/%_static.o, $(LIB_SOURCES))
STATIC_NAME = slconfig-static
STATIC_FILE = lib/$(LIB_PREFIX)$(STATIC_NAME)$(STATIC_LIB_EXT)

SHARED_FLAGS = $(C_FLAGS) -fPIC
SHARED_OBJS = $(patsubst src/%.c, .objs/%_shared.o, $(LIB_SOURCES))
SHARED_NAME = slconfig
SHARED_FILE = lib/$(LIB_PREFIX)$(SHARED_NAME)$(SHARED_LIB_EXT)

ifeq ($(OS),"Windows")
	IMPORT_LIBRARY_FILE = lib/$(LIB_PREFIX)$(SHARED_NAME)$(STATIC_LIB_EXT)
	IMPORT_LIBRARY_FLAGS = -Wl,--out-implib,$(IMPORT_LIBRARY_FILE)
	INSTALL_IMPORT = $(INSTALL_PREFIX)/$(IMPORT_LIBRARY_FILE)
else
	IMPORT_LIBRARY_FILE = 
	IMPORT_LIBRARY_FLAGS = 
	INSTALL_IMPORT = 
endif

EXAMPLE_SOURCES = $(wildcard examples/*.c)
EXAMPLE_FILES = $(patsubst examples/%.c, bin/%$(EXE), $(EXAMPLE_SOURCES))
EXAMPLE_LDFLAGS = -Llib -l$(STATIC_NAME)

DOC_SOURCE = README.md
DOC_FILE = doc/doc.html

INSTALL_PREFIX = /usr/local
INSTALL_SOURCES = $(wildcard include/slconfig/*.h)
INSTALL_HEADERS = $(patsubst %.h, $(INSTALL_PREFIX)/%.h, $(INSTALL_SOURCES))
INSTALL_LIBS = $(INSTALL_PREFIX)/$(STATIC_FILE) $(INSTALL_PREFIX)/$(SHARED_FILE) $(INSTALL_IMPORT)

.PHONY : all
.PHONY : static
.PHONY : shared
.PHONY : examples
.IGNORE : clean
.PHONY : FORCE
.PHONY : install

all : static shared examples
static : $(STATIC_FILE)
shared : $(SHARED_FILE)
examples : $(EXAMPLE_FILES)
documentation : $(DOC_FILE)
install : $(INSTALL_HEADERS) $(INSTALL_LIBS)

.objs : 
	$(MKDIR) .objs

bin :
	$(MKDIR) bin

lib :
	$(MKDIR) lib

doc :
	$(MKDIR) doc

$(INSTALL_PREFIX)/include/%.h : include/%.h
	$(INSTALL_SRC) $(subst /,$(PATH_SEP), $<) $(subst /,$(PATH_SEP), $@)

$(INSTALL_PREFIX)/lib/%$(STATIC_LIB_EXT) : lib/%$(STATIC_LIB_EXT)
	$(INSTALL_BIN) $(subst /,$(PATH_SEP), $<) $(subst /,$(PATH_SEP), $@)

$(INSTALL_PREFIX)/lib/%$(SHARED_LIB_EXT) : lib/%$(SHARED_LIB_EXT)
	$(INSTALL_BIN) $(subst /,$(PATH_SEP), $<) $(subst /,$(PATH_SEP), $@)

$(DOC_FILE) : doc $(DOC_SOURCE)
	pandoc -o $(DOC_FILE) $(DOC_SOURCE)

$(STATIC_FILE) : lib $(STATIC_OBJS)
	ar rs $(STATIC_FILE) $(STATIC_OBJS)

$(SHARED_FILE) : lib $(SHARED_OBJS)
	$(CC) --shared $(SHARED_FLAGS) -o $(SHARED_FILE) $(IMPORT_LIBRARY_FLAGS) $(SHARED_OBJS)

bin/%$(EXE) : examples/%.c bin static FORCE
	$(CC) $< -o $@ $(C_FLAGS) $(EXAMPLE_LDFLAGS)

.objs/%_static.o : src/%.c FORCE .objs
	$(CC) -c $< $(C_FLAGS) -o $@

.objs/%_shared.o : src/%.c FORCE .objs
	$(CC) -c $< $(SHARED_FLAGS) -o $@

clean : 
	$(RM) $(subst /,$(PATH_SEP), $(DOC_FILE))
	$(RM) $(subst /,$(PATH_SEP), $(STATIC_FILE))
	$(RM) $(subst /,$(PATH_SEP), $(SHARED_FILE))
	$(RM) $(subst /,$(PATH_SEP), $(IMPORT_LIBRARY_FILE))
	$(RM) $(subst /,$(PATH_SEP), $(EXAMPLE_FILES))
	$(RM) $(subst /,$(PATH_SEP), $(STATIC_OBJS) $(SHARED_OBJS))
	$(RMDIR) .objs
	$(RMDIR) bin
	$(RMDIR) lib
	$(RMDIR) doc
