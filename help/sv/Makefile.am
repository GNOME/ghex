## Process this file with automake to produce Makefile.in.

EXTRA_DIST = index.html \
	     topic.dat

ghex_helpdir = $(datadir)/gnome/help/ghex/sv

install-data-local:
	$(mkinstalldirs) $(ghex_helpdir)
	$(INSTALL_DATA)  $(srcdir)/index.html $(ghex_helpdir)
	$(INSTALL_DATA)  $(srcdir)/topic.dat $(ghex_helpdir)

