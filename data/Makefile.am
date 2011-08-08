pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gtkhex-3.pc

@GSETTINGS_RULES@
gsettings_SCHEMAS = org.gnome.GHex.gschema.xml

convertdir = $(datadir)/GConf/gsettings
convert_DATA = ghex.convert

EXTRA_DIST =			\
	ghex.desktop.in		\
	$(gsettings_SCHEMAS)	\
	$(convert_DATA)

desktopdir = $(datadir)/applications
desktop_in_files = ghex.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

CLEANFILES =			\
	$(desktop_DATA)
