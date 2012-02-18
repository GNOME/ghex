/* -*- mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* config.c - configuration loading/saving via gnome-config routines

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
#include "gtkhex.h"
#include "ghex-window.h"

#define DEFAULT_FONT "Monospace 12"

GSettings *settings = NULL;

gint def_group_type = GROUP_BYTE;
guint max_undo_depth;
gchar *offset_fmt = NULL;
PangoFontMetrics *def_metrics = NULL; /* Changes for Gnome 2.0 */
PangoFontDescription *def_font_desc = NULL;
gchar *def_font_name = NULL;
gboolean show_offsets_column = TRUE;

static void
offsets_column_changed_cb (GSettings   *settings,
                           const gchar *key,
                           gpointer     user_data)
{
    const GList *winn;
    gboolean show_off = g_settings_get_boolean (settings, key);

    show_offsets_column = show_off;
    winn = ghex_window_get_list ();
    while (winn) {
        if (GHEX_WINDOW (winn->data)->gh)
            gtk_hex_show_offsets (GHEX_WINDOW (winn->data)->gh, show_off);
        winn = g_list_next (winn);
    }
}

static void
group_changed_cb (GSettings   *settings,
                  const gchar *key,
                  gpointer     user_data)
{
    def_group_type = g_settings_get_enum (settings, key);
}

static void
max_undo_depth_changed_cb (GSettings   *settings,
                           const gchar *key,
                           gpointer     user_data)
{
    const GList *docn;

    g_settings_get (settings, key, "u", &max_undo_depth);

    docn = hex_document_get_list ();
    while (docn) {
        hex_document_set_max_undo (HEX_DOCUMENT (docn->data), max_undo_depth);
        docn = g_list_next (docn);
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
offset_format_changed_cb (GSettings   *settings,
                          const gchar *key,
                          gpointer     user_data)
{
    gchar *old_offset_fmt = offset_fmt;
    gint len, i;
    gboolean expect_spec;

    offset_fmt = g_strdup (g_settings_get_string (settings, key));

    /* check for a valid format string */
    len = strlen (offset_fmt);
    expect_spec = FALSE;
    for (i = 0; i < len; i++) {
        if (offset_fmt[i] == '%')
            expect_spec = TRUE;
        if (expect_spec &&
            ((offset_fmt[i] >= 'a' && offset_fmt[i] <= 'z') ||
             (offset_fmt[i] >= 'A' && offset_fmt[i] <= 'Z'))) {
            expect_spec = FALSE;
            if (offset_fmt[i] != 'x' && offset_fmt[i] != 'd' &&
                offset_fmt[i] != 'o' && offset_fmt[i] != 'X' &&
                offset_fmt[i] != 'P' && offset_fmt[i] != 'p') {
                g_free (offset_fmt);
                offset_fmt = old_offset_fmt;
                g_settings_set_string (settings, GHEX_PREF_OFFSET_FORMAT, "%X");
            }
        }
    }
    if (offset_fmt != old_offset_fmt)
        g_free (old_offset_fmt);
}

static void
font_changed_cb (GSettings   *settings,
                 const gchar *key,
                 gpointer     user_data)
{
    const GList *winn;
    const gchar *font_name = g_settings_get_string (settings, key);
    PangoFontMetrics *new_metrics;
    PangoFontDescription *new_desc;

    g_return_if_fail (font_name != NULL);

    if ((new_metrics = gtk_hex_load_font (font_name)) != NULL) {
        new_desc = pango_font_description_from_string (font_name);
        winn = ghex_window_get_list ();
        while (winn) {
            if (GHEX_WINDOW (winn->data)->gh)
                gtk_hex_set_font (GHEX_WINDOW (winn->data)->gh, new_metrics, new_desc);
            winn = g_list_next (winn);
        }

        if (def_metrics)
            pango_font_metrics_unref (def_metrics);

        if (def_font_desc)
            pango_font_description_free (def_font_desc);

        if (def_font_name)
            g_free (def_font_name);

        def_metrics = new_metrics;
        def_font_name = g_strdup (font_name);
        def_font_desc = new_desc;
    }
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

    g_signal_connect (settings, "changed::" GHEX_PREF_MAX_UNDO_DEPTH,
                      G_CALLBACK (max_undo_depth_changed_cb), NULL);
    max_undo_depth_changed_cb (settings, GHEX_PREF_MAX_UNDO_DEPTH, NULL);

    g_signal_connect (settings, "changed::" GHEX_PREF_BOX_SIZE,
                      G_CALLBACK (box_size_changed_cb), NULL);
    box_size_changed_cb (settings, GHEX_PREF_BOX_SIZE, NULL);


    g_signal_connect (settings, "changed::" GHEX_PREF_OFFSET_FORMAT,
                      G_CALLBACK (offset_format_changed_cb), NULL);
    offset_format_changed_cb (settings, GHEX_PREF_OFFSET_FORMAT, NULL);

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
