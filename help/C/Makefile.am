## Process this file with automake to produce Makefile.in.

ghex_helpdir = $(datadir)/gnome/help/ghex/C

SGML_FILES = \
	ghex.sgml

HTML_FILES = authors.html  ghexediting.html  ghexfindreplace.html  ghexmenues.html  index.html  license.html  ln7.html  prefs.html  usage.html


EXTRA_DIST = \
	topic.dat $(SGML_FILES)

ghex_help_DATA = ${HTML_FILES} topic.dat

${HTML_FILES} : $(SGML_FILES)
	if [ ! -f ghex/index.html ] ; then cd $(srcdir) && db2html ghex.sgml ; fi
	-cd $(srcdir) && cp ghex/$@ .
	if [ -d ghex.junk ] ; then cd $(srcdir) && rm -rf ghex.junk ; fi

dist-hook:
	$(mkinstalldirs) $(distdir)/ghex 
	$(mkinstalldirs) $(distdir)/ghex/stylesheet-images
	-cp $(srcdir)/ghex/*.html ghex/*.css $(distdir)/ghex
	-cp $(srcdir)/ghex/stylesheet-images/*.gif \
	 $(distdir)/ghex/stylesheet-images
	-cp $(srcdir)/*.html $(distdir)
	-cp $(srcdir)/*.png $(distdir)

distclean-generic:
	-rm -f Makefile
	-rm -f config.cache config.log stamp-h stamp-h[0-9]*
	-rm -f $(HTML_FILES)
	-rm -rf ghex

install-data-local: index.html
	$(mkinstalldirs) $(DESTDIR)$(ghex_helpdir)/stylesheet-images
	-for file in $(srcdir)/ghex/*.html $(srcdir)/ghex/*.css; do \
	  basefile=`basename $$file`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(ghex_helpdir)/$$basefile; \
	done
	-for file in $(srcdir)/*.png ; do \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(ghex_helpdir); \
	done
	-for file in $(srcdir)/*.sgml ; do \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(ghex_helpdir); \
	done

	-for file in $(srcdir)/ghex/stylesheet-images/*.gif; do \
	  basefile=`basename $$file`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(ghex_helpdir)/stylesheet-images/$$basefile; \
	done

