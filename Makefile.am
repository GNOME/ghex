SUBDIRS = po intl macros src help omf-install

EXTRA_DIST =		\
	ghex.spec.in	\
	ghex.desktop	\
	app-docs.make   \
	COPYING-DOCS    \
	$(appicon_DATA)

dist-hook: ghex.spec
	cp ghex.spec $(distdir)

appicondir = $(datadir)/pixmaps

appicon_DATA = gnome-ghex.png

sysdir   = $(datadir)/gnome/apps/Applications
sys_DATA = ghex.desktop

## to automatically rebuild aclocal.m4 if any of the macros in
## `macros/' change
@MAINT@include macros/macros.dep
@MAINT@macros/macros.dep: macros/Makefile.am
@MAINT@	cd macros && $(MAKE) macros.dep
