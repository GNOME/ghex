## Process this file with automake to create Makefile.in.

INCLUDES = -I$(includedir) 				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DLOCALEDIR=\"$(datadir)/locale\"		\
	-DGHEX_UI_DIR=\""$(datadir)/ghex2/ui/"\"		\
	-DGHEX_GLADEDIR=\""$(datadir)/ghex2/glade/"\"	\
	$(GHEX_CFLAGS)

bin_PROGRAMS = ghex2

uidir = $(datadir)/ghex2/ui/

BUILT_SOURCES = \
	ghex-marshal.c	\
	ghex-marshal.h

ghex2_SOURCES = hex-document.c hex-document.h hex-document-ui.c		\
	preferences.c findreplace.c converter.c config.c ghex.h		\
	gtkhex.c gtkhex.h main.c ui.c session.c chartable.c		\
	print.c bonobo-mdi.c bonobo-mdi.h bonobo-mdi-child.c		\
	bonobo-mdi-child.h ghex-mdi.c ghex-mdi.h			\
	bonobo-mdi-session.c bonobo-mdi-session.h			\
	$(BUILT_SOURCES)

ghex-marshal.h: ghex-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=ghex_marshal > $@
ghex-marshal.c: ghex-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"ghex-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=ghex_marshal >> $@

ui_DATA = ghex-ui.xml

EXTRA_DIST =	\
	$(ui_DATA)	\
	ghex-marshal.list

#ghex_LDADD = $(GNOME_LIBDIR)        \
#             $(GNOMEUI_LIBS)        \
#             $(INTLLIBS) 	    \
#             $(GNOME_PRINT_LIBS)

LDADD =	\
	$(GHEX_LIBS)

CLEAN_FILES = $(BUILT_SOURCES)
