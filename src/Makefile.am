## Process this file with automake to create Makefile.in.

SUBDIRS = help

appicondir = $(datadir)/pixmaps

appicon_DATA = gnome-ghex.png

INCLUDES = -I$(includedir) 				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DLOCALEDIR=\"$(datadir)/locale\"		\
	$(GNOME_INCLUDEDIR)

bin_PROGRAMS = ghex

ghex_SOURCES = hex-document.c hex-document.h             \
	preferences.c findreplace.c converter.c config.c \
	ghex.h gtkhex.c gtkhex.h main.c ui.c session.c
ghex_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS)

EXTRA_DIST = ghex.desktop $(appicon_DATA)

sysdir   = $(datadir)/apps/Applications
sys_DATA = ghex.desktop

