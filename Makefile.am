AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS =
bin_SCRIPTS =
noinst_PROGRAMS =
noinst_HEADERS =
TESTS =
XFAIL_TESTS =
check_PROGRAMS =
EXTRA_PROGRAMS =
CLEANFILES = $(bin_SCRIPTS)
MAINTAINERCLEANFILES =
EXTRA_DIST =
BUILT_SOURCES =
include_HEADERS =

TESTS_ENVIRONMENT =

EXTRA_DIST += \
 prepare.sh

AM_CPPFLAGS = -I$(top_srcdir)/include

AM_CFLAGS =

AM_LIBS =

AM_CXXFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES =

lib_LTLIBRARIES += src/libquintain-bedrock.la
src_libquintain_bedrock_la_SOURCES =

LDADD =

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
 maint/mochi-quintain-server.pc

include Make.rules

include $(top_srcdir)/src/Makefile.subdir
include $(top_srcdir)/tests/Makefile.subdir
