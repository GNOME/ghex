SUBDIRS = po intl macros src help

EXTRA_DIST = ghex.desktop $(appicon_DATA)

appicondir = $(datadir)/pixmaps

appicon_DATA = gnome-ghex.png

sysdir   = $(datadir)/gnome/apps/Applications
sys_DATA = ghex.desktop

## to automatically rebuild aclocal.m4 if any of the macros in
## `macros/' change
@MAINT@include macros/macros.dep
@MAINT@macros/macros.dep: macros/Makefile.am
@MAINT@	cd macros && $(MAKE) macros.dep
