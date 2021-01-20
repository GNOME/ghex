/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* configuration.c - configuration loading/saving via GSettings

   Copyright (C) 1997 - 2004 Free Software Foundation

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <string.h>

#include "configuration.h"

GSettings *settings;

int def_group_type;
char *def_font_name;
char *header_font_name;
char *data_font_name;
guint shaded_box_size;
gboolean show_offsets_column;

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

void ghex_init_configuration ()
{
    settings = g_settings_new ("org.gnome.GHex");

    g_return_if_fail (settings != NULL);

    g_signal_connect (settings, "changed::" GHEX_PREF_OFFSETS_COLUMN,
                      G_CALLBACK (offsets_column_changed_cb), NULL);
    offsets_column_changed_cb (settings, GHEX_PREF_OFFSETS_COLUMN, NULL);

    g_signal_connect (settings, "changed::" GHEX_PREF_GROUP,
                      G_CALLBACK (group_changed_cb), NULL);
    group_changed_cb (settings, GHEX_PREF_GROUP, NULL);

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
}
