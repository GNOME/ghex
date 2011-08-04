## Process this file with automake to produce Makefile.in.

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper --disable-schemas-install

SUBDIRS = po help src icons

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = ghex2.schemas

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gtkhex.pc

EXTRA_DIST =			\
	ghex.desktop.in		\
	gnome-doc-utils.make	\
	omf.make		\
	xmldocs.make		\
	COPYING-DOCS    	\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	$(schema_DATA)

desktopdir = $(datadir)/applications
desktop_in_files = ghex.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

#Install the ghex2 schema file
if GCONF_SCHEMAS_INSTALL
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)
else
install-data-local:
endif

CLEANFILES =			\
	$(desktop_DATA)

DISTCLEANFILES =		\
	intltool-extract	\
	intltool-merge		\
	intltool-update
