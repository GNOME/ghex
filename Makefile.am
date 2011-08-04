## Process this file with automake to produce Makefile.in.

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper --disable-schemas-install

SUBDIRS = data po help src icons

EXTRA_DIST =			\
	gnome-doc-utils.make	\
	omf.make		\
	xmldocs.make		\
	COPYING-DOCS    	\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in

DISTCLEANFILES =		\
	intltool-extract	\
	intltool-merge		\
	intltool-update
