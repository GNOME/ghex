schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = ghex2.schemas

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gtkhex-3.pc

EXTRA_DIST =			\
	ghex.desktop.in		\
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
