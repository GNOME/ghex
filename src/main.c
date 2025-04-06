/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* main.c - genesis of a GHex application

   Copyright © 1998 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021 Logan Rathbone <poprocks@gmail.com>

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

#include "ghex-application-window.h"

#include <locale.h>

#include <config.h>

static GOptionEntry entries[] = {
	{	.long_name = 	"new-window",
		.short_name =	'w',
		.flags = 		G_OPTION_FLAG_NONE,
		.arg =			G_OPTION_ARG_NONE,
		.description =	N_("Open a new window"),
	},
	{	.long_name = 	"version",
		.short_name =	'v',
		.flags = 		G_OPTION_FLAG_NONE,
		.arg =			G_OPTION_ARG_NONE,
		.description =	N_("Show the application version"),
	},
	{ NULL }
};

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

static int
handle_local_options (GApplication *application,
		GVariantDict *options,
		gpointer      user_data)
{
	if (g_variant_dict_contains (options, "new-window"))
	{
		g_application_register (application, NULL, NULL);

		if (g_application_get_is_remote (application))
		{
			g_action_group_activate_action (G_ACTION_GROUP(application), "new-window", NULL);
			return 0;
		}
	}
	if (g_variant_dict_contains (options, "version"))
	{
		g_print (_("This is GHex, version %s\n"), PACKAGE_VERSION);

		return 0;	/* exit successfully (see TFM) */
	}
	return -1;		/* let processing continue (see TFM) */
}

static void
startup (AdwApplication *app,
	gpointer user_data)
{
	ghex_init_configuration ();

	/* WORKAROUND https://gitlab.gnome.org/GNOME/gtk/-/issues/4880 */
	g_object_set (gtk_settings_get_default (), "gtk-dialogs-use-header", TRUE, NULL);

}

static void
activate (AdwApplication *app,
	gpointer user_data)
{
	GtkWindow *window;

	window = gtk_application_get_active_window (GTK_APPLICATION(app));
	if (window)
	{
		gtk_window_present (window);
		return;
	}

	window = GTK_WINDOW(ghex_application_window_new (app));
	gtk_window_present (window);
}

static void
open (GApplication *application,
		GFile **files,
		int n_files,
		const char *hint,
		gpointer user_data)
{
	GtkWindow *window;

	activate (ADW_APPLICATION(application), NULL);
	window = gtk_application_get_active_window (GTK_APPLICATION(application));

	for (int i = 0; i < n_files; ++i)
		ghex_application_window_open_file (GHEX_APPLICATION_WINDOW(window), files[i]);
}

static void
new_window (GAction *action,
		GVariant *parameter,
		AdwApplication *app)
{
	GtkWindow *window;

	window = GTK_WINDOW(ghex_application_window_new (app));
	gtk_window_present (window);
}

int
main (int argc, char *argv[])
{
	AdwApplication *app;
	GSimpleAction *new_window_action;
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

	app = adw_application_new (APP_ID, G_APPLICATION_HANDLES_OPEN);

	g_application_add_main_option_entries (G_APPLICATION(app), entries);

	g_application_set_option_context_summary (G_APPLICATION(app),
			_("GHex - A hex editor for the GNOME desktop"));;

	g_signal_connect (app, "startup", G_CALLBACK(startup), NULL);
	g_signal_connect (app, "activate", G_CALLBACK(activate), NULL);
	g_signal_connect (app, "open", G_CALLBACK(open), NULL);
	g_signal_connect (app, "handle-local-options",
			G_CALLBACK(handle_local_options), NULL);

	new_window_action = g_simple_action_new ("new-window", NULL);
	g_signal_connect (new_window_action, "activate", G_CALLBACK(new_window), app);
	g_action_map_add_action (G_ACTION_MAP(app), G_ACTION(new_window_action));

	status = g_application_run (G_APPLICATION(app), argc, argv);

	g_object_unref (app);

	return status;
}
