## Process this file with automake to create Makefile.in.

INCLUDES = -I$(includedir) 				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DLOCALEDIR=\"$(datadir)/locale\"		\
	$(GNOME_INCLUDEDIR)                             \
	$(GNOME_PRINT_CFLAGS)

bin_PROGRAMS = ghex

ghex_SOURCES = hex-document.c hex-document.h hex-document-ui.c  \
	preferences.c findreplace.c converter.c config.c ghex.h \
	gtkhex.c gtkhex.h main.c ui.c session.c chartable.c     \
        print.c

ghex_LDADD = $(GNOME_LIBDIR)        \
             $(GNOMEUI_LIBS)        \
             $(INTLLIBS) 	    \
             $(GNOME_PRINT_LIBS)



