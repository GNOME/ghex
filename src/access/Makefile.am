INCLUDES = -I$(includedir)                              \
        -DGNOMEICONDIR=\""$(datadir)/pixmaps"\"         \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
        -DLOCALEDIR=\"$(datadir)/locale\"               \
        -DGHEX_UI_DIR=\""$(datadir)/ghex2/ui/"\"                \
        -DGHEX_GLADEDIR=\""$(datadir)/ghex2/glade/"\"   \
        $(GHEX_CFLAGS)                                  \
        $(GAILUTIL_CFLAGS)                              \
        -I..

noinst_LTLIBRARIES = libgtkhexaccess.la

libgtkhexaccess_la_SOURCES = \
        accessiblegtkhexfactory.h   \
        accessiblegtkhexfactory.c   \
        accessiblegtkhex.h    \
        accessiblegtkhex.c	\
	factory.h		\
	factory.c



