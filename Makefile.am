## Process this file with automake to produce Makefile.in.

#Remove macros intl directory from SUBDIRS -- SnM 

SUBDIRS = po src help omf-install

schemadir = $(sysconfdir)/gconf/schemas
schema_DATA = ghex2.schemas

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gtkhex.pc

EXTRA_DIST =			\
	ghex.spec.in		\
	ghex.desktop.in		\
	app-docs.make   	\
	xmldocs.make		\
	COPYING-DOCS    	\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	$(schema_DATA)		\
	$(appicon_DATA)

dist-hook: ghex.spec
	cp ghex.spec $(distdir)

desktopdir = $(datadir)/applications
desktop_in_files = ghex.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

appicondir = $(datadir)/pixmaps
appicon_DATA = gnome-ghex.png

@INTLTOOL_DESKTOP_RULE@

#Install the ghex2 schema file
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)
