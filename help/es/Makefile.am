## Process this file with automake to produce Makefile.in.

EXTRA_DIST = index.html \
	     topic.dat

ghex_helpdir = $(datadir)/gnome/help/ghex/es

install-data-local:
	$(mkinstalldirs) $(ghex_helpdir)
	$(INSTALL_DATA)  index.html $(ghex_helpdir)
	$(INSTALL_DATA)  topic.dat $(ghex_helpdir)

