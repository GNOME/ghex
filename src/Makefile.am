## Process this file with automake to create Makefile.in.

INCLUDES = -I$(includedir) 				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DLOCALEDIR=\"$(datadir)/locale\"		\
	$(GNOME_INCLUDEDIR)

bin_PROGRAMS = ghex

ghex_SOURCES = callbacks.c callbacks.h config.c ghex.h gnome-support.c \
	gnome-support.h gtkhex.c gtkhex.h io.c main.c ui.c
ghex_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS)
