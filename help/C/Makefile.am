## Process this file with automake to produce Makefile.in.

EXTRA_DIST = index.html \
	     ghex-topic.dat

ghex_helpdir = $(datadir)/gnome/help/ghex/C

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(ghex_helpdir)
	$(INSTALL_DATA)  $(srcdir)/index.html \
			 $(DESTDIR)$(ghex_helpdir)/index.html
	$(INSTALL_DATA)  $(srcdir)/ghex-topic.dat \ 
			 $(DESTDIR)$(ghex_helpdir)/topic.dat

