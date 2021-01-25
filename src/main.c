/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* main.c - genesis of a GHex application

   Copyright © 1998 - 2004 Free Software Foundation
   Copyright © 2021 Logan Rathbone

   GHex is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   GHex is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GHex; see the file COPYING.
   If not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Original Author: Jaka Mocnik <jaka@gnu.org>
*/

#include <locale.h>
#include <config.h>

#include "ghex-application-window.h"

static GtkWindow *window = NULL;

/* FIXME - TEST ON WIN32.
 * This is one of the few functions in this file that has been ripped verbatim
 * from the old main.c. It might work. Maybe.
 */
#ifdef G_OS_WIN32
static char *
ghex_win32_locale_dir (void)
{
    gchar *install_dir;
    gchar *locale_dir = NULL;
    gchar *utf8_locale_dir;

    install_dir = g_win32_get_package_installation_directory_of_module (NULL);

    if (install_dir) {
        utf8_locale_dir = g_build_filename (install_dir,
				"share", "locale", NULL);
        locale_dir = g_win32_locale_filename_from_utf8 (utf8_locale_dir);

        g_free (install_dir);
        g_free (utf8_locale_dir);
    }

    return locale_dir;
}
#endif

static char *
ghex_locale_dir (void)
{
#ifdef G_OS_WIN32
    return ghex_win32_locale_dir ();
#else
    return g_strdup (LOCALEDIR);
#endif
}

static void
do_app_window (GtkApplication *app)
{
	if (! window)
		window = GTK_WINDOW(ghex_application_window_new (app));
	else
		g_return_if_fail (GHEX_IS_APPLICATION_WINDOW
				(GHEX_APPLICATION_WINDOW(window)));
}

static void
activate (GtkApplication *app,
	gpointer user_data)
{
	(void)user_data;	/* unused */

	do_app_window (app);

	gtk_window_set_application (window, app);
	gtk_window_present (window);
}

static void
open (GApplication *application,
		GFile **files,
		int n_files,
		const char *hint,
		gpointer user_data)
{
	GHexApplicationWindow *app_win;

	if (n_files > 1)
		g_warning ("Can only open a single file");

	activate (GTK_APPLICATION(application), NULL);
	app_win = GHEX_APPLICATION_WINDOW(window);
	ghex_application_window_open_file (app_win, files[0]);
}

int
main (int argc, char *argv[])
{
	GtkApplication *app;
	char *locale_dir;
	int status;

	/* boilerplate i18n stuff */
	setlocale (LC_ALL, "");
	locale_dir = ghex_locale_dir ();
	bindtextdomain (GETTEXT_PACKAGE, locale_dir);
	g_free (locale_dir);

	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
	/* </i18n> */

	ghex_init_configuration ();

	/* FIXME - not 100% decided on NON_UNIQUE for this as yet. */
	app = gtk_application_new ("org.gnome.GHex",
			G_APPLICATION_NON_UNIQUE | G_APPLICATION_HANDLES_OPEN);

	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	g_signal_connect (app, "open", G_CALLBACK(open), NULL);

	g_application_register (G_APPLICATION (app), NULL, NULL);

	status = g_application_run (G_APPLICATION(app), argc, argv);

	g_object_unref(app);

	return status;
}
