## Process this file with automake to produce Makefile.in.

#Remove macros intl directory from SUBDIRS -- SnM 

#SUBDIRS = po src help omf-install
SUBDIRS = po src help

GHEX_SCHEMA = ghex2.schemas

EXTRA_DIST =			\
	ghex.spec.in		\
	ghex.desktop		\
	app-docs.make   	\
	COPYING-DOCS    	\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	$(GHEX_SCHEMA)
#	$(appicon_DATA)

dist-hook: ghex.spec
	cp ghex.spec $(distdir)

#appicondir = $(datadir)/pixmaps
pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA =

#appicon_DATA = gnome-ghex.png

#sysdir   = $(datadir)/gnome/apps/Applications
#sys_DATA = ghex.desktop
@INTLTOOL_DESKTOP_RULE@

## to automatically rebuild aclocal.m4 if any of the macros in
## `macros/' change
#@MAINT@include macros/macros.dep
#@MAINT@macros/macros.dep: macros/Makefile.am
#@MAINT@	cd macros && $(MAKE) macros.dep

#Install the ghex2 schema file
install-data-local:
	gconftool-2 --install-schema-file $(srcdir)/$(GHEX_SCHEMA)
