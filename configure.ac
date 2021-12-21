#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([mochi-quintain], [0.1], [],[],[])
AC_CONFIG_MACRO_DIR([m4])
LT_INIT

AC_CANONICAL_TARGET
AC_CANONICAL_SYSTEM
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])

# we should remove this soon, only needed for automake 1.10 and older
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_SRCDIR([README.md])
AC_CONFIG_HEADERS([mochi-quintain-config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_CXXCPP

AC_PROG_MKDIR_P

AC_REQUIRE_CPP

AC_CHECK_SIZEOF([long int])

dnl
dnl Verify pkg-config
dnl
PKG_PROG_PKG_CONFIG
if test "x$PKG_CONFIG" == "x"; then
   AC_MSG_ERROR([Could not find pkg-config utility!])
fi

# coreutils checks for OSX
AC_ARG_VAR([TIMEOUT], timeout program)
AC_ARG_VAR([MKTEMP], mktemp program)

if test -z "$TIMEOUT" ; then
    AC_CHECK_PROGS(TIMEOUT, [timeout gtimeout])
    if test -z "$TIMEOUT" ; then
        AC_MSG_ERROR([Could not find timeout command (can optionally provide via the TIMEOUT variable)])
    fi
else
    AC_SUBST([TIMEOUT], ["$TIMEOUT"])
fi

if test -z "$MKTEMP" ; then
    AC_CHECK_PROGS(MKTEMP, [mktemp gmktemp])
    if test -z "$MKTEMP" ; then
        AC_MSG_ERROR([Could not find mktemp command (can optionally provide via the MKTEMP variable)])
    fi
else
    AC_SUBST([MKTEMP], ["$MKTEMP"])
fi

PKG_CHECK_MODULES([MARGO],[margo],[],
   [AC_MSG_ERROR([Could not find working margo installation!])])
LIBS="$MARGO_LIBS $LIBS"
CPPFLAGS="$MARGO_CFLAGS $CPPFLAGS"
CFLAGS="$MARGO_CFLAGS $CFLAGS"

PKG_CHECK_MODULES([JSONC],[json-c],[],
   [AC_MSG_ERROR([Could not find working json-c installation!])])
LIBS="$JSONC_LIBS $LIBS"
dnl
dnl Note that pkg-config may report an include path that contains a
dnl "/json-c" component.  If so, strip it out.  We prefer to use an explicit
dnl subdir path in the source to to avoid potential header name conflicts
dnl with other json libraries.
dnl
JSONC_CFLAGS=`echo $JSONC_CFLAGS | sed 's/\/include\/json-c/\/include/g'`
CPPFLAGS="$JSONC_CFLAGS $CPPFLAGS"
CFLAGS="$JSONC_CFLAGS $CFLAGS"

PKG_CHECK_MODULES([BEDROCK], [bedrock-server],[],
   [AC_MSG_ERROR([Could not find working bedrock installation!])])
LIBS="$BEDROCK_LIBS $LIBS"
CPPFLAGS="$BEDROCK_CFLAGS $CPPFLAGS"
CFLAGS="$BEDROCK_CFLAGS $CFLAGS"

PKG_CHECK_MODULES([SSG], [ssg],[],
   [AC_MSG_ERROR([Could not find working SSG installation!])])
LIBS="$SSG_LIBS $LIBS"
CPPFLAGS="$SSG_CFLAGS $CPPFLAGS"
CFLAGS="$SSG_CFLAGS $CFLAGS"

# need zlib output file
CHECK_ZLIB

# optional mpi; this will cause the benchmark program to be built as well,
# in addition to the provider piece
AC_MSG_CHECKING([If MPI programs can be compiled])
AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([[#include<mpi.h>]], [[MPI_Init(0,0);]])],
    [have_mpi=1
     AC_MSG_RESULT([yes])],
    [AC_MSG_WARN([Unable to compile MPI programs, try CC=mpicc])])

AM_CONDITIONAL([HAVE_MPI], [test "x${have_mpi}" = x1])

AC_CONFIG_FILES([Makefile maint/mochi-quintain-server.pc])
AC_OUTPUT
