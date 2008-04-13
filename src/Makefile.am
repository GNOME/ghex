## Process this file with automake to create Makefile.in.

INCLUDES = \
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\"			\
	-DGNOMELOCALEDIR=\""${prefix}/${DATADIRNAME}/locale"\" 	\
	-DLOCALEDIR=\"${prefix}/${DATADIRNAME}/locale\"		\
	-DDATADIR=\""$(datadir)"\"				\
	-DPREFIX=\""$(prefix)"\"				\
	$(GHEX_CFLAGS) $(GAILUTIL_CFLAGS)			\
	$(GHEX_DEPRECATED_CFLAGS)

BUILT_SOURCES = \
	ghex-marshal.c	\
	ghex-marshal.h

bin_PROGRAMS = ghex2

lib_LTLIBRARIES = libgtkhex.la

libgtkhexdir = $(includedir)/gtkhex/

uidir = $(datadir)/gnome-2.0/ui/

libgtkhex_la_SOURCES = \
	hex-document.c			\
	gtkhex.c			\
	gtkhex-private.h		\
	accessiblegtkhex.c		\
	accessiblegtkhexfactory.c	\
	accessiblegtkhex.h		\
	accessiblegtkhexfactory.h	\
	$(BUILT_SOURCES)

libgtkhex_la_LIBADD = \
	$(LIBGTKHEX_LIBS)	\
	$(GAILUTIL_LIBS)

libgtkhex_la_CFLAGS = -DG_LOG_DOMAIN=\"libgtkhex\"

libgtkhex_HEADERS = \
	hex-document.h 			\
	gtkhex.h

ghex2_SOURCES = \
	hex-document-ui.c preferences.c findreplace.c converter.c	\
	config.c main.c ui.c chartable.c session.c		\
	print.c ghex-window.c ghex-window.h				\
	gnome-print-font-dialog.c gnome-print-font-dialog.h		\
	gnome-print-font-picker.c gnome-print-font-picker.h		\
	factory.c factory.h hex-dialog.c hex-dialog.h			\
	configuration.h preferences.h findreplace.h print.h		\
	converter.h chartable.h ui.h session.h

ghex-marshal.h: ghex-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=ghex_marshal > $@
ghex-marshal.c: ghex-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"ghex-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=ghex_marshal >> $@

ui_DATA = ghex-ui.xml

EXTRA_DIST = \
	$(ui_DATA)		\
	ghex-marshal.list

LDADD =	\
	$(POPT_LIBS)	\
	$(GHEX_LIBS)	\
	libgtkhex.la

CLEAN_FILES = $(BUILT_SOURCES)
