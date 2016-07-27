cmake_minimum_required(VERSION 3.5)
project(ServalRPC)

set(CMAKE_INSTALL_SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/etc")

# sysconfdir definition
set(CMAKE_C_FLAGS -DSYSCONFDIR=\\"${CMAKE_INSTALL_SYSCONFDIR}\\")

# Optimisation, position indipendent code, security check for functions like printf, and make it impossible to compile potential vulnerable code.
# Also add security checks for functions like memcpy, strcpy, etc.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fPIC -Wformat -Werror=format-security -D_FORTIFY_SOURCE=2")

# Security (stack protection)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-all --param=ssp-buffer-size=4")

# SQLite (disable data functions, disable interactive compiling, no deprecated functions, disable extension loading functions, no virtual tables, no authorization)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_DATETIME_FUNCS -DSQLITE_OMIT_COMPILEOPTION_DIAGS -DSQLITE_OMIT_DEPRECATED -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_VIRTUALTABLE -DSQLITE_OMIT_AUTHORIZATION")

# Enable some functions defined in POSIX 600 standard and definitions normally not available or deprecated in macOS (i.e bzero).
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=600 -D_DARWIN_C_SOURCE")

# Enable all and even more warnings, treat them as errors and show all errors.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")





add_definitions(-DPACKAGE_NAME=\"servalrpc\" -DPACKAGE_TARNAME=\"servalrpc\" -DPACKAGE_VERSION=\"0.1\" -DPACKAGE_STRING=\"servalrpc\ 0.1\" -DPACKAGE_BUGREPORT=\"\" -DPACKAGE_URL=\"\" -DHAVE_FUNC_ATTRIBUTE_ALIGNED=1 -DHAVE_FUNC_ATTRIBUTE_UNUSED=1 -DHAVE_FUNC_ATTRIBUTE_USED=1 -DHAVE_VAR_ATTRIBUTE_SECTION_SEG=1 -DHAVE_BCOPY=1 -DHAVE_BZERO=1 -DHAVE_BCMP=1 -DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 -DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_UNISTD_H=1 -DSIZEOF_OFF_T=8 -DHAVE_CURL_CURL_H=1 -DHAVE_STDDEF_H=1 -DHAVE_STDINT_H=1 -DHAVE_ERRNO_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STDARG_H=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_CTYPE_H=1 -DHAVE_FCNTL_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_LIMITS_H=1 -DHAVE_ALLOCA_H=1 -DHAVE_SYS_UN_H=1 -DHAVE_STRINGS_H=1 -DHAVE_UNISTD_H=1 -DHAVE_STRING_H=1 -DHAVE_ARPA_INET_H=1 -DHAVE_SYS_SOCKET_H=1 -DHAVE_SYS_MMAN_H=1 -DHAVE_SYS_TIME_H=1 -DHAVE_SYS_UCRED_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_POLL_H=1 -DHAVE_NETDB_H=1 -DHAVE_NET_IF_H=1 -DHAVE_NETINET_IN_H=1 -DHAVE_IFADDRS_H=1 -DHAVE_SYS_SOCKIO_H=1)


# Include search paths
include_directories( "SYSTEM" "${CMAKE_SOURCE_DIR}/serval/general" "${CMAKE_SOURCE_DIR}/serval/sqlite-amalgamation-3100200" "${CMAKE_SOURCE_DIR}/serval/nacl")

file(GLOB SRCS *.c)
file(GLOB_RECURSE HDRS *.h)

ADD_EXECUTABLE(ServalRPC ${SRCS} ${HDRS})
set_target_properties(ServalRPC PROPERTIES OUTPUT_NAME servalrpc)
LINK_DIRECTORIES(${.})
TARGET_LINK_LIBRARIES(ServalRPC ${CMAKE_SOURCE_DIR}/libservalrpc.a curl)