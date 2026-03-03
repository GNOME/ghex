/* ghex-application.c
 *
 * Copyright © 2026 Logan Rathbone
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include <glib/gi18n.h>

#include "ghex-application.h"

#include "ghex-application-window.h"
#include "configuration.h"
#include "util.h"

#include "config.h"

struct _GHexApplication
{
	AdwApplication parent_instance;

	gboolean activated;
};

G_DEFINE_FINAL_TYPE (GHexApplication, ghex_application, ADW_TYPE_APPLICATION)

GHexApplication *
ghex_application_new (const char        *application_id,
                      GApplicationFlags  flags)
{
	g_return_val_if_fail (application_id != NULL, NULL);

	return g_object_new (GHEX_TYPE_APPLICATION,
	                     "application-id", application_id,
	                     "flags", flags,
	                     "resource-base-path", "/org/gnome/GHex",
	                     NULL);
}

#define LOCAL_OPTIONS_LET_PROCESSING_CONTINUE -1
#define LOCAL_OPTIONS_EXIT_SUCCESSFULLY 0
static int
ghex_application_handle_local_options (GApplication *app, GVariantDict *options)
{
	if (g_variant_dict_contains (options, "new-window"))
	{
		g_application_register (app, NULL, NULL);

		if (g_application_get_is_remote (app))
		{
			g_action_group_activate_action (G_ACTION_GROUP(app), "new-window", NULL);

			return LOCAL_OPTIONS_EXIT_SUCCESSFULLY;
		}
	}
	if (g_variant_dict_contains (options, "version"))
	{
		g_print (_("This is GHex, version %s\n"), PACKAGE_VERSION);

		return LOCAL_OPTIONS_EXIT_SUCCESSFULLY;
	}
	return LOCAL_OPTIONS_LET_PROCESSING_CONTINUE;
}
#undef LOCAL_OPTIONS_EXIT_SUCCESSFULLY
#undef LOCAL_OPTIONS_LET_PROCESSING_CONTINUE

static void
ghex_application_startup (GApplication *app)
{
	g_assert (GHEX_IS_APPLICATION (app));

	ghex_init_configuration ();

	G_APPLICATION_CLASS(ghex_application_parent_class)->startup (app);
}

static void
ghex_application_activate (GApplication *app)
{
	GHexApplication *self = GHEX_APPLICATION(app);
	GtkWindow *window;

	g_assert (GHEX_IS_APPLICATION (app));

	if (self->activated)
		return;

	self->activated = TRUE;

	window = gtk_application_get_active_window (GTK_APPLICATION(app));

	if (!window)
		window = GTK_WINDOW(ghex_application_window_new (ADW_APPLICATION(app)));

	gtk_window_present (window);
}

static gboolean
open_source_func (gpointer user_data)
{
	UtilTuple *tuple = user_data;
	GHexApplication *self = tuple->data[0];
	GFile *file = tuple->data[1];
	int attempt = GPOINTER_TO_INT (tuple->data[2]);
	const int max_attempts = 100;
	GHexApplicationWindow *window = NULL;

	g_assert (GHEX_IS_APPLICATION (self));
	g_assert (G_IS_FILE (file));

	g_debug ("%s: attempt: %d to open file (max_attempts: %d)",
			__func__, attempt, max_attempts);

	window = GHEX_APPLICATION_WINDOW (gtk_application_get_active_window (GTK_APPLICATION(self)));

	if G_UNLIKELY (!window)
	{
		if (attempt < max_attempts)
		{
			++attempt;
			tuple->data[2] = GINT_TO_POINTER (attempt);

			return G_SOURCE_CONTINUE;
		}
		else
		{
			g_printerr (_("GHex failed to open an application window to open the requested file: %s\n"), g_file_peek_path (file));

			return G_SOURCE_REMOVE;
		}
	}

	g_assert (GHEX_IS_APPLICATION_WINDOW (window));

	ghex_application_window_open_file (window, file);

	return G_SOURCE_REMOVE;
}

static void
ghex_application_open_file (GHexApplication *self, GFile *file)
{
	g_autoptr(UtilTuple) tuple = NULL;
	g_assert (GHEX_IS_APPLICATION (self));
	g_assert (G_IS_FILE (file));

	tuple = util_tuple_new ();
	tuple->data[0] =    g_object_ref (self);
	tuple->destroy[0] = g_object_unref;
	tuple->data[1] =    g_object_ref (file);
	tuple->destroy[1] = g_object_unref;
	tuple->data[2] =    GINT_TO_POINTER (0);

	g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, open_source_func, g_steal_pointer (&tuple), (GDestroyNotify) util_tuple_destroy);
}

static void
ghex_application_open__gapp_method (GApplication *app, GFile *files[], int n_files, const char *hint)
{
	GHexApplication *self = GHEX_APPLICATION(app);

	g_assert (GHEX_IS_APPLICATION (self));

	for (int i = 0; i < n_files; ++i)
		ghex_application_open_file (self, files[i]);

	ghex_application_activate (app);
}

static void
ghex_application_class_init (GHexApplicationClass *klass)
{
	GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

	app_class->handle_local_options = ghex_application_handle_local_options;
	app_class->startup = ghex_application_startup;
	app_class->activate = ghex_application_activate;
	app_class->open = ghex_application_open__gapp_method;
}

static void
ghex_application_about_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GHexApplication *self = user_data;
	GtkWindow *window = NULL;
	g_autofree char *copyright = NULL;
	g_autofree char *version = NULL;

	const char *authors[] = {
		"Jaka Mo\304\215nik",
		"Chema Celorio",
		"Shivram Upadhyayula",
		"Rodney Dawes",
		"Jonathon Jongsma",
		"Kalev Lember",
		"Logan Rathbone",
		NULL
	};

	const char *documentation_credits[] = {
		"Jaka Mo\304\215nik",
		"Sun GNOME Documentation Team",
		"Logan Rathbone",
		NULL
	};

	g_assert (GHEX_IS_APPLICATION (self));

	window = gtk_application_get_active_window (GTK_APPLICATION (self));

	/* Translators: these two strings here indicate the copyright time span,
	   e.g. 1998-2018. */
	copyright = g_strdup_printf (_("Copyright © %d–%d The GHex authors"),
			1998, 2023);

	if (strstr (APP_ID, "Devel"))
		version = g_strdup_printf ("%s (Running against GTK %d.%d.%d)",
				PACKAGE_VERSION,
				gtk_get_major_version (),
				gtk_get_minor_version (),
				gtk_get_micro_version ());
	else
		version = g_strdup (PACKAGE_VERSION);

	adw_show_about_dialog (GTK_WIDGET (window),
	                       "application-icon", APP_ID,
	                       "application-name", strstr (APP_ID, "Devel") ? "GHex (Development)" : "GHex",
	                       "developer-name", "Logan Rathbone",
	                       "version", version,
	                       "issue-url", "https://gitlab.gnome.org/GNOME/ghex/-/issues",
	                       "developers", authors,
	                       "documenters", documentation_credits,
	                       "translator-credits", _("translator-credits"),
	                       "copyright", copyright,
	                       "license-type", GTK_LICENSE_GPL_2_0,
	                       NULL);
}

static void
ghex_application_help_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GHexApplication *self = user_data;
	GdkAppLaunchContext *context = NULL;
	g_autoptr(GError) error = NULL;
	gboolean launched = FALSE;

	g_assert (GHEX_IS_APPLICATION (self));

	context = gdk_display_get_app_launch_context (gdk_display_get_default ());

	launched = g_app_info_launch_default_for_uri ("help:ghex", G_APP_LAUNCH_CONTEXT (context), &error);

	if (!launched)
	{
		/* Translators: '%s' is the error message that will be generated
		 * pre-translated by the GError.
		 */
		g_autofree char *err_str = g_strdup_printf (_("Failed to launch help: %s"), error->message);
#if 0
		ghex_display_dialog (gtk_application_get_active_window (GTK_APPLICATION(self)),
				err_str);
#endif
	}
}

static void
ghex_application_quit_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GHexApplication *self = user_data;

	g_assert (GHEX_IS_APPLICATION (self));

	g_application_quit (G_APPLICATION (self));
}

static void
ghex_application_new_window_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GHexApplication *self = user_data;
	GtkWindow *window;

	g_assert (GHEX_IS_APPLICATION (self));

	window = GTK_WINDOW(ghex_application_window_new (ADW_APPLICATION(self)));
	gtk_window_present (window);
}

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

static const GActionEntry app_actions[] = {
	{ "quit", ghex_application_quit_action },
	{ "about", ghex_application_about_action },
	{ "help", ghex_application_help_action },
	{ "new-window", ghex_application_new_window_action },
};

static void
ghex_application_init (GHexApplication *self)
{
	g_application_add_main_option_entries (G_APPLICATION(self), entries);

	g_action_map_add_action_entries (G_ACTION_MAP(self),
			app_actions,
			G_N_ELEMENTS (app_actions),
			self);

	gtk_application_set_accels_for_action (GTK_APPLICATION(self),
			"app.quit",
			(const char *[]) { "<control>q", NULL });

	gtk_application_set_accels_for_action (GTK_APPLICATION(self),
			"app.help",
			(const char *[]) { "F1", NULL });

	gtk_application_set_accels_for_action (GTK_APPLICATION(self),
			"app.new-window",
			(const char *[]) { "<control>n", NULL });
}
