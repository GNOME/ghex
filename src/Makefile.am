## Process this file with automake to create Makefile.in.

INCLUDES = -I$(includedir) 					\
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\"			\
	-DGNOMELOCALEDIR=\""${prefix}/${DATADIRNAME}/locale"\" 	\
	-DLOCALEDIR=\"${prefix}/${DATADIRNAME}/locale\"		\
	-DGHEX_UI_DIR=\""$(datadir)/ghex2/ui/"\"		\
	-DGHEX_GLADEDIR=\""$(datadir)/ghex2/glade/"\"		\
	-DDATADIR=\""$(datadir)"\"				\
	$(GHEX_CFLAGS) $(GAILUTIL_CFLAGS)

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
	accessiblegtkhex.c		\
	accessiblegtkhexfactory.c	\
	$(BUILT_SOURCES)

libgtkhex_la_LIBADD = \
	$(LIBGTKHEX_LIBS)	\
	$(GAILUTIL_LIBS)

libgtkhex_HEADERS = \
	hex-document.h 			\
	gtkhex.h			\
	accessiblegtkhex.h		\
	accessiblegtkhexfactory.h

ghex2_SOURCES = \
	hex-document-ui.c preferences.c findreplace.c converter.c	\
	config.c ghex.h	main.c ui.c chartable.c session.c		\
	print.c ghex-window.c ghex-window.h				\
	gnome-print-font-dialog.c gnome-print-font-dialog.h		\
	gnome-print-font-picker.c gnome-print-font-picker.h		\
	factory.c factory.h

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
	$(POPT_LIBS)			\
	$(GHEX_LIBS)			\
	$(top_builddir)/src/libgtkhex.la

CLEAN_FILES = $(BUILT_SOURCES)
