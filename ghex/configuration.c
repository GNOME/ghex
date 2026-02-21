/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* configuration.c - configuration loading/saving via GSettings

   Copyright (C) 1997 - 2004 Free Software Foundation

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

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#include <config.h>

#include "configuration.h"

GSettings *settings;
/* Global CSS provider for our HexWidget widgets */
GtkCssProvider *global_provider;

int def_group_type;
int def_sb_offset_format;
char *def_font_name;
char *header_font_name;
char *data_font_name;
guint shaded_box_size;
gboolean show_offsets_column;
int def_dark_mode;
gboolean def_display_control_characters;

static void
offsets_column_changed_cb (GSettings   *settings,
                           const gchar *key,
                           gpointer     user_data)
{
    gboolean show_off = g_settings_get_boolean (settings, key);

    show_offsets_column = show_off;
}

static void
group_changed_cb (GSettings   *settings,
                  const gchar *key,
                  gpointer     user_data)
{
    def_group_type = g_settings_get_enum (settings, key);
}

static void
sb_offsetformat_changed_cb (GSettings   *settings,
                  const gchar *key,
                  gpointer     user_data)
{
    def_sb_offset_format = g_settings_get_enum (settings, key);
}

static void
dark_mode_changed_cb (GSettings   *settings,
                  const gchar *key,
                  gpointer     user_data)
{
	AdwStyleManager *manager = adw_style_manager_get_default ();

    def_dark_mode = g_settings_get_enum (settings, key);

	switch (def_dark_mode)
	{
		case DARK_MODE_OFF:
			adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_FORCE_LIGHT);
			break;

		case DARK_MODE_ON:
			adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_FORCE_DARK);
			break;

		case DARK_MODE_SYSTEM:
			adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_DEFAULT);
			break;

		default:
			break;
	}
}

static void
box_size_changed_cb (GSettings   *settings,
                     const gchar *key,
                     gpointer     user_data)
{
    g_settings_get (settings, key, "u", &shaded_box_size);
}

static void
font_changed_cb (GSettings   *settings,
                 const gchar *key,
                 gpointer     user_data)
{
    const gchar *font_name = g_settings_get_string (settings, key);

    g_return_if_fail (font_name != NULL);

	if (def_font_name)
		g_free (def_font_name);

	def_font_name = g_strdup (font_name);
}

static void
data_font_changed_cb (GSettings   *settings,
                      const gchar *key,
                      gpointer     user_data)
{
    if (data_font_name)
        g_free (data_font_name);
    data_font_name = g_strdup (g_settings_get_string (settings, key));
}

static void
header_font_changed_cb (GSettings   *settings,
                        const gchar *key,
                        gpointer     user_data)
{
    if (header_font_name)
        g_free (header_font_name);
    header_font_name = g_strdup (g_settings_get_string (settings, key));
}

static void
control_chars_changed_cb (GSettings   *settings,
                         const gchar *key,
                         gpointer     user_data)
{
    gboolean show = g_settings_get_boolean (settings, key);

    def_display_control_characters = show;
}

void ghex_init_configuration ()
{
	/* GSettings */

    settings = g_settings_new (APP_ID);
    g_return_if_fail (settings);

    g_signal_connect (settings, "changed::" GHEX_PREF_OFFSETS_COLUMN,
                      G_CALLBACK (offsets_column_changed_cb), NULL);
    offsets_column_changed_cb (settings, GHEX_PREF_OFFSETS_COLUMN, NULL);

    g_signal_connect (settings, "changed::" GHEX_PREF_GROUP,
                      G_CALLBACK (group_changed_cb), NULL);
    group_changed_cb (settings, GHEX_PREF_GROUP, NULL);

    g_signal_connect (settings, "changed::" GHEX_PREF_SB_OFFSET_FORMAT,
                      G_CALLBACK (sb_offsetformat_changed_cb), NULL);
    sb_offsetformat_changed_cb (settings, GHEX_PREF_SB_OFFSET_FORMAT, NULL);

    g_signal_connect (settings, "changed::" GHEX_PREF_DARK_MODE,
                      G_CALLBACK (dark_mode_changed_cb), NULL);
    dark_mode_changed_cb (settings, GHEX_PREF_DARK_MODE, NULL);

    g_signal_connect (settings, "changed::" GHEX_PREF_BOX_SIZE,
                      G_CALLBACK (box_size_changed_cb), NULL);
    box_size_changed_cb (settings, GHEX_PREF_BOX_SIZE, NULL);

    g_signal_connect (settings, "changed::" GHEX_PREF_FONT,
                      G_CALLBACK (font_changed_cb), NULL);
    font_changed_cb (settings, GHEX_PREF_FONT, NULL);

    g_signal_connect (settings, "changed::" GHEX_PREF_DATA_FONT,
                      G_CALLBACK (data_font_changed_cb), NULL);

    data_font_changed_cb (settings, GHEX_PREF_DATA_FONT, NULL);

    g_signal_connect (settings, "changed::" GHEX_PREF_HEADER_FONT,
                      G_CALLBACK (header_font_changed_cb), NULL);
    header_font_changed_cb (settings, GHEX_PREF_HEADER_FONT, NULL);

    g_signal_connect (settings, "changed::" GHEX_PREF_CONTROL_CHARS,
                      G_CALLBACK (control_chars_changed_cb), NULL);
    control_chars_changed_cb (settings, GHEX_PREF_CONTROL_CHARS, NULL);

	/* Global CSS provider */

	global_provider = gtk_css_provider_new ();
}
