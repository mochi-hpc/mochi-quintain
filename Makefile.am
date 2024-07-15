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
DISTCLEANFILES =
EXTRA_DIST =
BUILT_SOURCES =
include_HEADERS = include/quintain.h \
 include/quintain-server.h \
 include/quintain-client.h \
 include/quintain-macros.h

TESTS_ENVIRONMENT =

EXTRA_DIST += \
 prepare.sh

AM_CPPFLAGS = -I$(top_srcdir)/include

AM_CFLAGS =

AM_LIBS =

AM_CXXFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES =

lib_LTLIBRARIES +=\
 src/libquintain-client.la\
 src/libquintain-server.la\
 src/libquintain-bedrock.la

src_libquintain_bedrock_la_SOURCES =
src_libquintain_client_la_SOURCES =
src_libquintain_server_la_SOURCES =

dist_bin_SCRIPTS =

dist_doc_DATA = README.md

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
 maint/mochi-quintain-server.pc

include Make.rules

include $(top_srcdir)/src/Makefile.subdir
include $(top_srcdir)/tests/Makefile.subdir
