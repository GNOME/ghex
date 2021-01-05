/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex.h - declaration of a GtkHex widget

   Copyright © 1997 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

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

#ifndef GTKHEX_H
#define GTKHEX_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <hex-document.h>

G_BEGIN_DECLS

/* Declare GtkHex as a *final* type, meaning you cannot derive from it.
 * This is a change from prior versions of libgtkhex <= 3.x, and is in
 * line with changes made to GTK 4 at large.
 */
#define GTK_TYPE_HEX (gtk_hex_get_type ())
G_DECLARE_FINAL_TYPE(GtkHex, gtk_hex, GTK, HEX, GtkWidget)

/* OPAQUE DATATYPES */
typedef struct _GtkHex_AutoHighlight GtkHex_AutoHighlight;

/* PUBLIC METHOD DECLARATIONS */

GtkWidget *gtk_hex_new(HexDocument *);

void gtk_hex_set_cursor(GtkHex *, gint);
void gtk_hex_set_cursor_xy(GtkHex *, gint, gint);
void gtk_hex_set_nibble(GtkHex *, gint);

guint gtk_hex_get_cursor(GtkHex *);
guchar gtk_hex_get_byte(GtkHex *, guint);

void gtk_hex_set_group_type(GtkHex *, guint);

void gtk_hex_set_starting_offset(GtkHex *, gint);
void gtk_hex_show_offsets(GtkHex *, gboolean);
void gtk_hex_set_font(GtkHex *, PangoFontMetrics *,
		const PangoFontDescription *);

void gtk_hex_set_insert_mode(GtkHex *, gboolean);

void gtk_hex_set_geometry(GtkHex *gh, gint cpl, gint vis_lines);

PangoFontMetrics* gtk_hex_load_font (const char *font_name); 

void gtk_hex_copy_to_clipboard(GtkHex *gh);
void gtk_hex_cut_to_clipboard(GtkHex *gh);
void gtk_hex_paste_from_clipboard(GtkHex *gh);

void gtk_hex_set_selection(GtkHex *gh, gint start, gint end);
gboolean gtk_hex_get_selection(GtkHex *gh, gint *start, gint *end);
void gtk_hex_clear_selection(GtkHex *gh);
void gtk_hex_delete_selection(GtkHex *gh);

GtkHex_AutoHighlight *gtk_hex_insert_autohighlight(GtkHex *gh,
		const gchar *search,
		gint len,
		const gchar *colour);
void gtk_hex_delete_autohighlight(GtkHex *gh, GtkHex_AutoHighlight *ahl);

G_END_DECLS

#endif		/* GTKHEX_H */
