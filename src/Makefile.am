## Process this file with automake to create Makefile.in.

INCLUDES = \
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\"			\
	-DGNOMELOCALEDIR=\""${prefix}/${DATADIRNAME}/locale"\" 	\
	-DLOCALEDIR=\"${prefix}/${DATADIRNAME}/locale\"		\
	-DGHEXDATADIR=\""$(pkgdatadir)"\"			\
	-DPREFIX=\""$(prefix)"\"				\
	$(GHEX_CFLAGS) $(GAILUTIL_CFLAGS)			\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED)

ghex-resources.c: ghex.gresource.xml ghex-ui.xml
	$(AM_V_GEN)glib-compile-resources --target=$@ --generate-source --c-name ghex --sourcedir=$(srcdir) $<

ghex-resources.h: ghex.gresource.xml
	$(AM_V_GEN)glib-compile-resources --target=$@ --generate-header --c-name ghex --sourcedir=$(srcdir) $<

BUILT_SOURCES = \
	ghex-resources.c	\
	ghex-resources.h	\
	$(NULL)

bin_PROGRAMS = ghex

lib_LTLIBRARIES = libgtkhex-3.la

gtkhexincludedir = $(includedir)/gtkhex-3/

libgtkhex_3_la_SOURCES = \
	hex-document.c			\
	gtkhex.c			\
	gtkhex-private.h		\
	accessiblegtkhex.c		\
	accessiblegtkhexfactory.c	\
	accessiblegtkhex.h		\
	accessiblegtkhexfactory.h	\
	$(BUILT_SOURCES)

libgtkhex_3_la_LIBADD = \
	$(LIBGTKHEX_LIBS)	\
	$(GAILUTIL_LIBS)

libgtkhex_3_la_CFLAGS = -DG_LOG_DOMAIN=\"libgtkhex-3\"
libgtkhex_3_la_LDFLAGS = -no-undefined

gtkhexinclude_HEADERS = \
	hex-document.h 			\
	gtkhex.h

ghex_SOURCES =			\
	chartable.c		\
	chartable.h		\
	config.c		\
	configuration.h		\
	converter.c		\
	converter.h		\
	factory.c		\
	factory.h		\
	findreplace.c		\
	findreplace.h		\
	ghex-window.c		\
	ghex-window.h		\
	hex-dialog.c		\
	hex-dialog.h		\
	hex-document-ui.c	\
	main.c			\
	preferences.c		\
	preferences.h		\
	print.c			\
	print.h			\
	ui.c			\
	ui.h

EXTRA_DIST = \
	ghex-ui.xml \
	ghex.gresource.xml

LDADD =	\
	libgtkhex-3.la	\
	$(GHEX_LIBS)

CLEAN_FILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
