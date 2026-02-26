/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* configuration.c - configuration loading/saving via GSettings

   Copyright (C) 1997 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021-2026 Logan Rathbone <poprocks@gmail.com>

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

#include "configuration.h"

#include "config.h"

static GSettings *global_settings;

/* Global CSS provider for our HexWidget widgets */
static GtkCssProvider *global_provider;

static void

dark_mode_changed_cb (GSettings *settings, const gchar *key, gpointer     user_data)
{
	AdwStyleManager *manager = adw_style_manager_get_default ();
    GHexDarkModeOption dark_mode = g_settings_get_enum (settings, key);

	switch (dark_mode)
	{
		case GHEX_DARK_MODE_OFF:
			adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_FORCE_LIGHT);
			break;

		case GHEX_DARK_MODE_ON:
			adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_FORCE_DARK);
			break;

		case GHEX_DARK_MODE_SYSTEM:
			adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_DEFAULT);
			break;

		default:
			g_assert_not_reached ();
			break;
	}
}

static void
init_global_settings (void)
{
	/* GSettings */

    global_settings = g_settings_new (APP_ID);

    g_signal_connect (global_settings, "changed::dark-mode", G_CALLBACK(dark_mode_changed_cb), NULL);

}

static void
init_global_css_provider (void)
{
	/* Global CSS provider */

	global_provider = gtk_css_provider_new ();
}

/* Transfer none */
GSettings *
ghex_get_global_settings (void)
{
	if (! global_settings)
		init_global_settings ();

	g_assert (G_IS_SETTINGS (global_settings));

	return global_settings;
}

/* Transfer none */
GtkCssProvider *
ghex_get_global_css_provider (void)
{
	if (! global_provider)
		init_global_css_provider ();

	g_assert (GTK_IS_CSS_PROVIDER (global_provider));

	return global_provider;
}
