cmake_minimum_required(VERSION 3.5)
project(ServalRPC)

set(CMAKE_INSTALL_SYSCONFDIR CMAKE_INSTALL_PREFIX/etc)

# sysconfdir definition
set(CMAKE_C_FLAGS
        "-DSYSCONFDIR=\"$(sysconfdir)\""
)

# Optimisation, position indipendent code, security check for functions like printf, and make it impossible to compile potential vulnerable code.
# Also add security checks for functions like memcpy, strcpy, etc.
set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS}
        -O3 -fPIC -Wformat -Werror=format-security -D_FORTIFY_SOURCE=2"
)

# Security (stack protection)
set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS}
        -fstack-protector-all --param=ssp-buffer-size=4"
)

# SQLite (disable data functions, disable interactive compiling, no deprecated functions, disable extension loading functions, no virtual tables, no authorization)
set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS}
        -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_DATETIME_FUNCS -DSQLITE_OMIT_COMPILEOPTION_DIAGS -DSQLITE_OMIT_DEPRECATED -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_VIRTUALTABLE -DSQLITE_OMIT_AUTHORIZATION"
)

# Enable some functions defined in POSIX 600 standard and definitions normally not available or deprecated in macOS (i.e bzero).
set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS}
        -D_XOPEN_SOURCE=600 -D_DARWIN_C_SOURCE"
)

# Enable all and even more warnings, treat them as errors and show all errors.
set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS}
        -Wall -Wextra -Werror -ferror-limit=0"
)

set(CMAKE_INCLUDE_SYSTEM_FLAG_C "-isystem ")

# Include search paths
include_directories(
        "SYSTEM"
        "${CMAKE_SOURCE_DIR}/serval/general"
        "${CMAKE_SOURCE_DIR}/serval/sqlite-amalgamation-3100200"
        "${CMAKE_SOURCE_DIR}/serval/nacl"
)

file(GLOB_RECURSE SRCS *.c)
file(GLOB_RECURSE HDRS *.h)

LINK_DIRECTORIES(${.})
ADD_EXECUTABLE(ServalRPC ${SRCS} ${HDRS})
TARGET_LINK_LIBRARIES(ServalRPC servalrpc curl)