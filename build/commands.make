ifdef SystemRoot
    OS              = "Windows"
    STATIC_LIB_EXT  = .lib
    SHARED_LIB_EXT = .dll
    EXE             =.exe
    PATH_SEP        =\
    message         = @(echo $1)
    SHELL           = cmd.exe
else
    SHELL           = sh
    PATH_SEP        =/
    EXE             =
    ifeq ($(shell uname), Linux)
        OS              = "Linux"
        STATIC_LIB_EXT  = .a
        SHARED_LIB_EXT = .so
        message         = @(echo \033[31m $1 \033[0;0m1)
    else ifeq ($(shell uname), Solaris)
        STATIC_LIB_EXT  = .a
        SHARED_LIB_EXT = .so
        OS              = "Solaris"
        message         = @(echo \033[31m $1 \033[0;0m1)
    else ifeq ($(shell uname),FreeBSD)
        STATIC_LIB_EXT  = .a
        SHARED_LIB_EXT = .so
        OS              = "FreeBSD"
        FixPath         = $1
        message         = @(echo \033[31m $1 \033[0;0m1)
    else ifeq ($(shell uname),Darwin)
        STATIC_LIB_EXT  = .a
        SHARED_LIB_EXT = .so
        OS              = "Darwin"
        message         = @(echo \033[31m $1 \033[0;0m1)
    endif
endif

# Define command for copy, remove and create file/dir
ifeq ($(OS),"Windows")
    RM    = del /Q
    RMDIR = del /Q
    CP    = copy /Y
    MKDIR = mkdir
    MV    = move
    LN    = mklink
else ifeq ($(OS),"Linux")
    RM    = rm -f
    RMDIR = rm -rf
    CP    = cp -fr
    MKDIR = mkdir -p
    MV    = mv
    LN    = ln -s
else ifeq ($(OS),"FreeBSD")
    RM    = rm -f
    RMDIR = rm -rf
    MKDIR = mkdir -p
    MV    = mv
    LN    = ln -s
else ifeq ($(OS),"Solaris")
    RM    = rm -f
    RMDIR = rm -rf
    MKDIR = mkdir -p
    MV    = mv
    LN    = ln -s
else ifeq ($(OS),"Darwin")
    RM    = rm -f
    RMDIR = rm -rf
    MKDIR = mkdir -p
    MV    = mv
    LN    = ln -s
endif 
