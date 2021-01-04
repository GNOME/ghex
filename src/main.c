/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* main.c - genesis of a GHex application

   Copyright (C) 1998 - 2004 Free Software Foundation

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

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#include <config.h>
#include <glib/gi18n.h>

#include "configuration.h"
#include "ghex-window.h"

/* Command line options */
static gchar *geometry = NULL;
static gchar **args_remaining = NULL;

static GOptionEntry options[] = {
        { "geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry, N_("X geometry specification (see \"X\" man page)."), N_("GEOMETRY") },
        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &args_remaining, NULL, N_("FILES") },
        { NULL }
};

#ifdef G_OS_WIN32
static gchar *
_ghex_win32_locale_dir (void)
{
    gchar *install_dir;
    gchar *locale_dir = NULL;
    gchar *utf8_locale_dir;

    install_dir = g_win32_get_package_installation_directory_of_module (NULL);

    if (install_dir) {
        utf8_locale_dir = g_build_filename (install_dir, "share", "locale", NULL);
        locale_dir = g_win32_locale_filename_from_utf8 (utf8_locale_dir);

        g_free (install_dir);
        g_free (utf8_locale_dir);
    }

    return locale_dir;
}
#endif

static gchar *
ghex_locale_dir (void)
{
#ifdef G_OS_WIN32
    return _ghex_win32_locale_dir ();
#else
    return g_strdup (LOCALEDIR);
#endif
}

static void
ghex_activate (GApplication *application,
               gpointer      unused)
{
    GList *windows = gtk_application_get_windows (GTK_APPLICATION (application));
    gtk_window_present (GTK_WINDOW (windows->data));
}

int
main(int argc, char **argv)
{
	GtkWidget *win;
	GError *error = NULL;
	GtkApplication *application;
	gchar *locale_dir;
	gint retval;

	locale_dir = ghex_locale_dir ();
	bindtextdomain (GETTEXT_PACKAGE, locale_dir);
	g_free (locale_dir);

	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Initialize GTK+ program */
	if (!gtk_init_with_args (&argc, &argv,
	                         _("- GTK+ binary editor"),
	                         options,
	                         GETTEXT_PACKAGE,
	                         &error)) {
		g_printerr (_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
		            error->message, argv[0]);
		g_error_free (error);
		return 1;
	}

	/* Set default window icon */
	gtk_window_set_default_icon_name ("org.gnome.GHex");

	/* load preferences */
	ghex_init_configuration();

	/* accessibility setup */
	setup_factory();

	application = gtk_application_new ("org.gnome.GHex",
	                                   G_APPLICATION_NON_UNIQUE);
	g_signal_connect (application, "activate",
	                  G_CALLBACK (ghex_activate), NULL);

	g_application_register (G_APPLICATION (application), NULL, NULL);

	if (args_remaining != NULL) {
		gchar **filename;
		for (filename = args_remaining; *filename != NULL; filename++) {
			if (g_file_test (*filename, G_FILE_TEST_EXISTS)) {
				win = ghex_window_new_from_file (application, *filename);
				if(win != NULL) {
					if(geometry) {
						if(!gtk_window_parse_geometry(GTK_WINDOW(win), geometry))
							g_warning(_("Invalid geometry string \"%s\"\n"), geometry);
						geometry = NULL;
					}
					gtk_widget_show(win);
				}
			}
		}
	}

	if(ghex_window_get_list() == NULL) {
		win = ghex_window_new (application);
		if(geometry) {
			if(!gtk_window_parse_geometry(GTK_WINDOW(win), geometry))
				g_warning(_("Invalid geometry string \"%s\"\n"), geometry);
			geometry = NULL;
		}
		gtk_widget_show(win);
	}
	else win = GTK_WIDGET(ghex_window_get_list()->data);

	retval = g_application_run (G_APPLICATION (application), argc, argv);
	g_object_unref (application);

	return retval;
}
