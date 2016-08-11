# Makefile.in for ServalRPC
prefix=/usr/local
sysconfdir=${prefix}/etc

# Set compiler
CC = gcc

# Set local libs, paths and curl here, everything else programmatically
LDFLAGS = -L. -lservalrpc -lcurl  -lm -lnsl -ldl -Wl,-z,relro 

# Set CFLAGS procramatically
CFLAGS =  -fstack-protector --param=ssp-buffer-size=4
# Include search paths
CFLAGS += -iquote../includes -iquote../includes/serval -iquote../includes/serval/sqlite -iquote../includes/serval/nacl
# sysconfdir definition
CFLAGS += -DSYSCONFDIR="\"$(sysconfdir)\""
# Optimisation, position indipendent code, security check for functions like printf, and make it impossible to compile potential vulnerable code.
# Also add security checks for functions like memcpy, strcpy, etc.
CFLAGS += -O3 -fPIC -Wformat -Werror=format-security -D_FORTIFY_SOURCE=2
# SQLite (disable data functions, disable interactive compiling, no deprecated functions, disable extension loading functions, no virtual tables, no authorization)
CFLAGS += -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_DATETIME_FUNCS -DSQLITE_OMIT_COMPILEOPTION_DIAGS -DSQLITE_OMIT_DEPRECATED -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_VIRTUALTABLE -DSQLITE_OMIT_AUTHORIZATION
# Enable some functions defined in POSIX 600 standard and definitions normally not available or deprecated in macOS (i.e bzero).
CFLAGS += -D_XOPEN_SOURCE=600 -D_DARWIN_C_SOURCE -D_GNU_SOURCE
# Enable all and even more warnings, treat them as errors and show all errors.
CFLAGS += -Wall -Wextra -Werror
# Some additional definitions.
DEFS = -DPACKAGE_NAME=\"servalrpc\" -DPACKAGE_TARNAME=\"servalrpc\" -DPACKAGE_VERSION=\"0.1\" -DPACKAGE_STRING=\"servalrpc\ 0.1\" -DPACKAGE_BUGREPORT=\"\" -DPACKAGE_URL=\"\" -DHAVE_FUNC_ATTRIBUTE_ALIGNED=1 -DHAVE_FUNC_ATTRIBUTE_ALLOC_SIZE=1 -DHAVE_FUNC_ATTRIBUTE_ERROR=1 -DHAVE_FUNC_ATTRIBUTE_FORMAT=1 -DHAVE_FUNC_ATTRIBUTE_MALLOC=1 -DHAVE_FUNC_ATTRIBUTE_UNUSED=1 -DHAVE_FUNC_ATTRIBUTE_USED=1 -DHAVE_VAR_ATTRIBUTE_SECTION=1 -DHAVE_BCOPY=1 -DHAVE_BZERO=1 -DHAVE_BCMP=1 -DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 -DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_UNISTD_H=1 -DSIZEOF_OFF_T=8 -DHAVE_CURL_CURL_H=1 -DHAVE_STDDEF_H=1 -DHAVE_STDINT_H=1 -DHAVE_STDIO_H=1 -DHAVE_ERRNO_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STDARG_H=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_CTYPE_H=1 -DHAVE_FCNTL_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_LIMITS_H=1 -DHAVE_ALLOCA_H=1 -DHAVE_SYS_UN_H=1 -DHAVE_STRINGS_H=1 -DHAVE_UNISTD_H=1 -DHAVE_STRING_H=1 -DHAVE_ARPA_INET_H=1 -DHAVE_SYS_SOCKET_H=1 -DHAVE_SYS_MMAN_H=1 -DHAVE_SYS_TIME_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_SYS_VFS_H=1 -DHAVE_SIGNAL_H=1 -DHAVE_POLL_H=1 -DHAVE_NETDB_H=1 -DHAVE_LINUX_NETLINK_H=1 -DHAVE_LINUX_RTNETLINK_H=1 -DHAVE_NET_IF_H=1 -DHAVE_NETINET_IN_H=1 -DHAVE_IFADDRS_H=1

SRCS = $(wildcard *.c)

all: $(SRCS)
	$(CC) -o ../servalrpc $^ $(CFLAGS) $(DEFS) $(LDFLAGS)
