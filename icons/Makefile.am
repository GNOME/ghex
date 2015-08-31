NULL =

public_icons_themes = \
	hicolor \
	$(NULL)

public_icons = \
	hicolor_apps_16x16_ghex.png \
	hicolor_apps_22x22_ghex.png \
	hicolor_apps_24x24_ghex.png \
	hicolor_apps_32x32_ghex.png \
	hicolor_apps_48x48_ghex.png \
	hicolor_apps_256x256_ghex.png \
	hicolor_apps_512x512_ghex.png \
	hicolor_apps_scalable_ghex-symbolic.svg \
	$(NULL)

noinst_DATA = \
	source_ghex.svg \
	$(NULL)

EXTRA_DIST = \
	$(public_icons)		\
	$(noinst_DATA)		\
	$(NULL)

install-icons:
	for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

uninstall-icons:
	-for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

install-data-local: install-icons

uninstall-local: uninstall-icons

-include $(top_srcdir)/git.mk
