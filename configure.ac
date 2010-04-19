AC_PREREQ(2.61)
AC_INIT([ghex],[2.24.0],[http://bugzilla.gnome.org/enter_bug.cgi?product=ghex],[ghex])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/ghex-window.c])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.10])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE

# Check for programs
AC_PROG_CC
AC_PROG_INSTALL

# Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

# Check for header files.
AC_CHECK_HEADERS([string.h unistd.h])

# Check for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Check for library functions.
AC_CHECK_FUNCS([pow strstr strtoul])

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

LIBGTKHEX_RELEASE=1.0
AC_SUBST(LIBGTKHEX_RELEASE)
LIBGTKHEX_RELEASE_STRING=gtkhex-${LIBGTKHEX_RELEASE}
AC_DEFINE_UNQUOTED(LIBGTKHEX_RELEASE_STRING, "$LIBGTKHEX_RELEASE_STRING",
                   [GtkHex library release string.])

PKG_CHECK_MODULES(LIBGTKHEX,
		  gtk+-2.0 >= 2.20.0	\
		  libbonobo-2.0		\
		  atk >= 1.0.0)
AC_SUBST(LIBGTKHEX_LIBS)
AC_SUBST(LIBGTKHEX_CFLAGS)

PKG_CHECK_MODULES(GHEX,
		  gio-2.0                       \
		  gtk+-2.0 >= 2.20.0 		\
		  gconf-2.0			\
		  libbonoboui-2.0		\
		  libgnomeprintui-2.2)
AC_SUBST(GHEX_CFLAGS)
AC_SUBST(GHEX_LIBS)

PKG_CHECK_MODULES(GAILUTIL, gail >= 0.17)
AC_SUBST(GAILUTIL_CFLAGS)
AC_SUBST(GAILUTIL_LIBS)

dnl =================================================================
dnl Gettext stuff
dnl =================================================================
IT_PROG_INTLTOOL([0.40.0])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])

GETTEXT_PACKAGE=ghex-2.0
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext package name.])
dnl =================================================================

# Uncomment this when we get rid of bonobo code: GB #590996
#GNOME_MAINTAINER_MODE_DEFINES
GNOME_COMPILE_WARNINGS([maximum])

AC_ARG_ENABLE(debug,
              [  --enable-debug      Enables extra debugging output], AC_DEFINE(ENABLE_DEBUG, 1, [Should verbose debugging output be enabled.]))

dnl Checking for gconftool-2
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)

if test "x$GCONFTOOL" = xno; then
	AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi

AM_GCONF_SOURCE_2

GNOME_DOC_INIT([0.9.0],,
  [AC_MSG_WARN([[gnome-doc-utils not found: documentation will not be built.]])])

AC_CONFIG_FILES([
Makefile
po/Makefile.in
src/Makefile
help/Makefile
icons/Makefile
icons/16x16/Makefile
icons/22x22/Makefile
icons/24x24/Makefile
icons/32x32/Makefile
icons/48x48/Makefile
ghex.spec
gtkhex.pc])
AC_OUTPUT
