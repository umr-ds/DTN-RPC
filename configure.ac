AC_INIT(servalrpc, 0.1)

AX_GCC_FUNC_ATTRIBUTE(aligned)
AX_GCC_FUNC_ATTRIBUTE(unused)
AX_GCC_FUNC_ATTRIBUTE(used)
AX_GCC_VAR_ATTRIBUTE(section_seg)

AC_CHECK_FUNCS([bcopy bzero bcmp])
AC_CHECK_SIZEOF([off_t])

AC_CHECK_HEADERS(
		curl/curl.h \
		stddef.h \
		stdint.h \
		tdio.h \
		errno.h \
		stdlib.h \
		stdarg.h \
		sys/types.h \
		linux/if.h \
		ctype.h \
		fcntl.h \
		inttypes.h \
		limits.h \
		jni.h \
		alloca.h \
		sys/un.h \
    strings.h \
    unistd.h \
    string.h \
    arpa/inet.h \
    sys/socket.h \
    sys/mman.h \
    sys/time.h \
    sys/ucred.h \
    sys/stat.h \
    sys/vfs.h \
    poll.h \
    netdb.h \
    linux/netlink.h \
    linux/rtnetlink.h \
    net/if.h \
    netinet/in.h \
    ifaddrs.h \
    ucred.h \
    sys/sockio.h
)

AC_CHECK_HEADERS(
    linux/if.h
,,, [
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
])

AS_IF([test "x$ac_cv_header_linux_if_h" = xyes], [AC_DEFINE([USE_ABSTRACT_NAMESPACE])])

AC_CHECK_LIB(curl,curl_easy_init,[LDFLAGS="$LDFLAGS -lcurl"])

AC_CACHE_CHECK([linker -z relro option], libc_cv_z_relro, [dnl
  libc_cv_z_relro=no
  AS_IF([AC_TRY_COMMAND([${CC-cc} -v --help 2>&1 | grep "z relro" 1>&AS_MESSAGE_LOG_FD])], [
    AS_IF([AC_TRY_COMMAND([${CC-cc} -Wl,--verbose 2>&1 | grep DATA_SEGMENT_RELRO_END 1>&AS_MESSAGE_LOG_FD])], [
      libc_cv_z_relro=yes
      LDFLAGS="$LDFLAGS -Wl,-z,relro"
    ])
  ])
])

AC_OUTPUT([
    Makefile
])
