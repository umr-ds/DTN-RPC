dnl Process this file with autoconf to produce a configure script.
AC_INIT(servalrpc, 0.1)
AC_CONFIG_MACRO_DIR([m4])
: ${CFLAGS=""}
AC_PROG_CC

dnl Various GCC function and variable attributes
AX_GCC_FUNC_ATTRIBUTE(aligned)
AX_GCC_FUNC_ATTRIBUTE(unused)
AX_GCC_FUNC_ATTRIBUTE(used)
AX_GCC_VAR_ATTRIBUTE(section_seg)

AC_CHECK_FUNCS([bcopy bzero bcmp])
AC_CHECK_SIZEOF([off_t])

dnl There must be a 64-bit seek(2) system call of some kind
AS_IF([test "x$have_lseek64_t" = "xno" -a "x$ac_cv_sizeof_off_t" != x8 ], [
    AC_MSG_ERROR([Missing lseek64(2) system call])
])

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

dnl Lazy way of checking for Linux
AS_IF([test "x$ac_cv_header_linux_if_h" = xyes], [AC_DEFINE([USE_ABSTRACT_NAMESPACE])])

AC_CACHE_CHECK([linker -z relro option], libc_cv_z_relro, [dnl
  libc_cv_z_relro=no
  AS_IF([AC_TRY_COMMAND([${CC-cc} -v --help 2>&1 | grep "z relro" 1>&AS_MESSAGE_LOG_FD])], [
    AS_IF([AC_TRY_COMMAND([${CC-cc} -Wl,--verbose 2>&1 | grep DATA_SEGMENT_RELRO_END 1>&AS_MESSAGE_LOG_FD])], [
      libc_cv_z_relro=yes
      LDFLAGS="$LDFLAGS -Wl,-z,relro"
    ])
  ])
])

dnl Stack smashing protection is not available on all platforms
AC_MSG_CHECKING([for SSP support])
have_ssp=0
save_cflags="$CFLAGS"
# Security (stack protection)
CFLAGS="$CFLAGS -fstack-protector --param=ssp-buffer-size=4"
AC_LANG([C])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [
    AC_LINK_IFELSE([AC_LANG_PROGRAM([])], [
        have_ssp=1
    ])
])
AS_IF([test x"$have_ssp" = "x1"], [
    AC_MSG_RESULT([yes])
    AC_SUBST([HAVE_SSP],1)
], [
    AC_MSG_RESULT([no])
    CFLAGS="$save_cflags"
])

AC_OUTPUT([
    Makefile
])
