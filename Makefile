prefix = /usr/local
sysconfdir = ${prefix}/etc
localstatedir = ${prefix}/var

CC = gcc

# Include search paths
CFLAGS = -Iserval/general -Iserval/sqlite-amalgamation-3100200 -Iserval/nacl
# sysconfdir definition
CFLAGS += -DSYSCONFDIR="\"$(sysconfdir)\"" -DLOCALSTATEDIR="\"$(localstatedir)\""
# Optimisation, position indipendent code, security check for functions like printf, and make it impossible to compile potential vulnerable code.
# Also add security checks for functions like memcpy, strcpy, etc.
CFLAGS += -O3 -fPIC -Wformat -Werror=format-security -D_FORTIFY_SOURCE=2
# Security (stack protection)
CFLAGS += -fstack-protector-all --param=ssp-buffer-size=4
# SQLite (disable data functions, disable interactive compiling, no deprecated functions, disable extension loading functions, no virtual tables, no authorization)
CFLAGS += -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_DATETIME_FUNCS -DSQLITE_OMIT_COMPILEOPTION_DIAGS -DSQLITE_OMIT_DEPRECATED -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_VIRTUALTABLE -DSQLITE_OMIT_AUTHORIZATION
# Enable some functions defined in POSIX 600 standard and definitions normally not available or deprecated in macOS (i.e bzero).
CFLAGS += -D_XOPEN_SOURCE=600 -D_DARWIN_C_SOURCE
# Enable all and even more warnings, treat them as errors and show all errors.
CFLAGS += -Wall -Wextra -Werror -ferror-limit=0
# Some additional definitions, to not include headers for things like bzero or poll, ...
CFLAGS += -DPACKAGE_NAME=\"ServalRPC\" -DPACKAGE_TARNAME=\"ServalRPC\" -DPACKAGE_VERSION=\"0.1\" -DPACKAGE_STRING=\"ServalRPC\ 0.1\" -DHAVE_FUNC_ATTRIBUTE_ALIGNED=1 -DHAVE_FUNC_ATTRIBUTE_UNUSED=1 -DHAVE_FUNC_ATTRIBUTE_USED=1 -DHAVE_VAR_ATTRIBUTE_SECTION_SEG=1 -DHAVE_BCOPY=1 -DHAVE_BZERO=1 -DHAVE_BCMP=1 -DSIZEOF_OFF_T=8 -DHAVE_ARPA_INET_H=1 -DHAVE_POLL_H=1 -DHAVE_NET_IF_H=1 -DHAVE_NETINET_IN_H=1 -DHAVE_SYS_STATVFS_H=1 -DHAVE_SYS_STAT_H=1

# Use the static serval library we compile.
LDFLAGS= -L. -lservalrpc -lcurl

# Use all .c files in this folder and all .o files specified in the obj_files file (stored in $OBJS).
RPC_SRC=$(wildcard *.c)
include obj_files

# If we just run make, only servalrpc will be build. Make sure to run make libservalrpc.a once.
all: servalrpc

# Build servalrpc
servalrpc: $(RPC_SRC)
	@$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

# Remove only the servalrpc binary
clean:
	@rm -f servalrpc
