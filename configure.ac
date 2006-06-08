# Process this file with autoconf to produce a configure script.
AC_INIT(libclib,1.0,eliot@sonic.net)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADERS(config/config.h)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([gnu no-dist-gzip dist-bzip2])

# Checks for programs.
AC_PROG_CC
AM_PROG_AS
AM_PROG_LIBTOOL

######################################################################

dnl ------------
dnl Check CFLAGS, CC
dnl ------------
AC_ARG_WITH(cc,
	    AC_HELP_STRING([--with-cc],[Set CC for use as C compiler.]),
	    [CC="$with_cc"])

AC_ARG_WITH(cflags,
	    AC_HELP_STRING([--with-cflags],[Set CFLAGS for use by C compiler.]),
	    [CFLAGS="$with_cflags"])

AC_ARG_WITH(ldflags,
	    AC_HELP_STRING([--with-ldflags],[Set LDFLAGS for linking.]),
	    [LDFLAGS="$with_ldflags"])

######################################################################

AC_ARG_WITH(unix,
            AC_HELP_STRING([--with-unix],[Compile unix version of clib]),
	    [],
	    [case $host_os in
	      darwin* | linux*) with_unix=yes;;
	      *) with_unix=no;;
	      esac])

AM_CONDITIONAL(WITH_UNIX, test "$with_unix" = "yes")

######################################################################
# Kernel version of CLIB

for i in /lib/modules/`uname -r`/build /usr/src/linux; do
    if test -d $i ; then
	LINUX_KERNEL_DIR=$i ;
	break ;
    fi ;
done

## default is without-kernel
AC_ARG_WITH(linux-kernel,
	    AC_HELP_STRING([--with-linux-kernel],[Path of Linux kernel for kernel clib]),
	    [if test "$with_linux_kernel" != "no"; then
		 if test "$with_linux_kernel" != "yes"; then
		     LINUX_KERNEL_DIR="$with_linux_kernel"
		 fi
	     fi])

AC_ARG_WITH(linux-kernel-cflags,
            AC_HELP_STRING([--with-linux-kernel-cflags],[Extra CFLAGS for compiling linux-kernel code]),
	    [LINUX_KERNEL_CFLAGS="$with_linux_kernel_cflags"])

AM_CONDITIONAL(WITH_LINUX_KERNEL, test "$with_linux_kernel" = "yes")

## default is without-standalone
AC_ARG_WITH(standalone,
	    AC_HELP_STRING([--with-standalone], [Compile standalone (non Unix dependent) version of clib]))

AC_ARG_WITH(standalone-cflags,
            AC_HELP_STRING([--with-standalone-cflags],[Extra CFLAGS for compiling standalone code]),
	    [STANDALONE_CFLAGS="$with_standalone_cflags"])

AM_CONDITIONAL(WITH_STANDALONE, test "$with_standalone" = "yes")

AC_SUBST(LINUX_KERNEL_DIR)
AC_SUBST(LINUX_KERNEL_CFLAGS)
AC_SUBST(STANDALONE_CFLAGS)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
