## Process this file with automake to create Makefile.in.

appicondir = $(datadir)/pixmaps

appicon_DATA = gnome-ghex.png

INCLUDES = -I$(includedir) 				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DLOCALEDIR=\"$(datadir)/locale\"		\
	$(GNOME_INCLUDEDIR)

bin_PROGRAMS = ghex

ghex_SOURCES = hex-document.c hex-document.h \
	callbacks.c callbacks.h config.c ghex.h gnome-support.c \
	gnome-support.h gtkhex.c gtkhex.h main.c ui.c
ghex_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS)

EXTRA_DIST = index.html ghex.desktop ghex-topic.dat $(appicon_DATA)

ghex_helpdir = $(datadir)/gnome/help/ghex/C

install-data-local:
	$(mkinstalldirs) $(ghex_helpdir)
	$(INSTALL_DATA)  $(srcdir)/index.html $(ghex_helpdir)
	$(INSTALL_DATA)  $(srcdir)/ghex-topic.dat $(ghex_helpdir)/topic.dat

sysdir   = $(datadir)/apps/Applications
sys_DATA = ghex.desktop

