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
	glib-compile-resources --target=$@ --generate-source --c-name ghex --sourcedir=$(srcdir) $<

ghex-resources.h: ghex.gresource.xml
	glib-compile-resources --target=$@ --generate-header --c-name ghex --sourcedir=$(srcdir) $<

BUILT_SOURCES = \
	ghex-marshal.c	\
	ghex-marshal.h	\
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

gtkhexinclude_HEADERS = \
	hex-document.h 			\
	gtkhex.h

ghex_SOURCES = \
	hex-document-ui.c preferences.c findreplace.c converter.c	\
	config.c main.c ui.c chartable.c				\
	print.c ghex-window.c ghex-window.h				\
	factory.c factory.h hex-dialog.c hex-dialog.h			\
	configuration.h preferences.h findreplace.h print.h		\
	converter.h chartable.h ui.h

ghex-marshal.h: ghex-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=ghex_marshal > $@
ghex-marshal.c: ghex-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"ghex-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=ghex_marshal >> $@

EXTRA_DIST = \
	ghex-marshal.list \
	ghex-ui.xml \
	ghex.gresource.xml

LDADD =	\
	libgtkhex-3.la	\
	$(GHEX_LIBS)

CLEAN_FILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
