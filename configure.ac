AC_PREREQ(2.61)
AC_INIT([ghex],[3.10.0],[http://bugzilla.gnome.org/enter_bug.cgi?product=ghex],[ghex])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/ghex-window.c])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 foreign dist-xz no-dist-gzip])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PATH_PROG(DESKTOP_FILE_VALIDATE, desktop-file-validate)

# Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

# Check for header files.
AC_CHECK_HEADERS([string.h unistd.h])

# Check for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Check for library functions.
AC_CHECK_FUNCS([pow strstr strtoul])

GLIB_GSETTINGS

LIBGTKHEX_RELEASE=3.0
AC_SUBST(LIBGTKHEX_RELEASE)
LIBGTKHEX_RELEASE_STRING=gtkhex-${LIBGTKHEX_RELEASE}
AC_DEFINE_UNQUOTED(LIBGTKHEX_RELEASE_STRING, "$LIBGTKHEX_RELEASE_STRING",
                   [GtkHex library release string.])

ATK_MIN_VERSION=1.0.0
GLIB_MIN_VERSION=2.31.10
GTK_MIN_VERSION=3.3.8

PKG_CHECK_MODULES(LIBGTKHEX, [
	atk >= $ATK_MIN_VERSION
	gtk+-3.0 >= $GTK_MIN_VERSION
])
AC_SUBST(LIBGTKHEX_LIBS)
AC_SUBST(LIBGTKHEX_CFLAGS)

PKG_CHECK_MODULES(GHEX, [
	gio-2.0 >= $GLIB_MIN_VERSION
	gtk+-3.0 >= $GTK_MIN_VERSION
])
AC_SUBST(GHEX_CFLAGS)
AC_SUBST(GHEX_LIBS)

PKG_CHECK_MODULES(GAILUTIL, gail-3.0)
AC_SUBST(GAILUTIL_CFLAGS)
AC_SUBST(GAILUTIL_LIBS)

dnl =================================================================
dnl Gettext stuff
dnl =================================================================
IT_PROG_INTLTOOL([0.41.1])

GETTEXT_PACKAGE=ghex-3.0
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext package name.])
dnl =================================================================

GNOME_COMPILE_WARNINGS([maximum])

AC_ARG_ENABLE(debug,
              [  --enable-debug      Enables extra debugging output], AC_DEFINE(ENABLE_DEBUG, 1, [Should verbose debugging output be enabled.]))

YELP_HELP_INIT

AC_CONFIG_FILES([
Makefile
data/Makefile
po/Makefile.in
src/Makefile
help/Makefile
icons/Makefile
data/gtkhex-3.pc])
AC_OUTPUT
