## Process this file with automake to create Makefile.in.

SUBDIRS = access

INCLUDES = -I$(includedir) 				\
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\"		\
	-DGNOMELOCALEDIR=\""${prefix}/${DATADIRNAME}/locale"\" 	\
	-DLOCALEDIR=\"${prefix}/${DATADIRNAME}/locale\"		\
	-DGHEX_UI_DIR=\""$(datadir)/ghex2/ui/"\"		\
	-DGHEX_GLADEDIR=\""$(datadir)/ghex2/glade/"\"	\
	-DDATADIR=\""$(datadir)"\"			\
	$(GHEX_CFLAGS)

bin_PROGRAMS = ghex2

uidir = $(datadir)/gnome-2.0/ui/

BUILT_SOURCES = \
	ghex-marshal.c	\
	ghex-marshal.h

ghex2_SOURCES = hex-document.c hex-document.h hex-document-ui.c		\
	preferences.c findreplace.c converter.c config.c ghex.h		\
	gtkhex.c gtkhex.h main.c ui.c chartable.c session.c		\
	print.c ghex-window.c ghex-window.h				\
	gnome-print-font-dialog.c gnome-print-font-dialog.h		\
	gnome-print-font-picker.c gnome-print-font-picker.h		\
	$(BUILT_SOURCES)

ghex-marshal.h: ghex-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=ghex_marshal > $@
ghex-marshal.c: ghex-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"ghex-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=ghex_marshal >> $@

ui_DATA = ghex-ui.xml

ACCESS_LT_LIB = $(top_builddir)/src/access/libgtkhexaccess.la

EXTRA_DIST =	\
	$(ui_DATA)	\
	ghex-marshal.list

LDADD =	\
	$(POPT_LIBS)		\
	$(GHEX_LIBS)		\
	$(ACCESS_LT_LIB)	\
	$(GAILUTIL_LIBS)

CLEAN_FILES = $(BUILT_SOURCES)
