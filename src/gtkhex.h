/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex.h - declaration of a HexWidget widget

   Copyright © 1997 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021-2025 Logan Rathbone <poprocks@gmail.com>

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

#include <hex-document.h>
#include <gtkhex-paste-data.h>

G_BEGIN_DECLS

/* ENUMS */

/**
 * HexWidgetGroupType:
 * @HEX_WIDGET_GROUP_BYTE: group data by byte (8-bit)
 * @HEX_WIDGET_GROUP_WORD: group data by word (16-bit)
 * @HEX_WIDGET_GROUP_LONG: group data by long (32-bit)
 * @HEX_WIDGET_GROUP_QUAD: group data by quadword (64-bit)
 *
 * Specifies how data is to be grouped by the #HexWidget.
 */
typedef enum
{
	HEX_WIDGET_GROUP_BYTE =		1,
	HEX_WIDGET_GROUP_WORD =		2,
	HEX_WIDGET_GROUP_LONG =		4,
	HEX_WIDGET_GROUP_QUAD =		8
} HexWidgetGroupType;

/* GOBJECT DECLARATION */

#define HEX_TYPE_WIDGET (hex_widget_get_type ())
G_DECLARE_FINAL_TYPE(HexWidget, hex_widget, HEX, WIDGET, GtkWidget)

/* OPAQUE DATATYPES */

#define HEX_TYPE_WIDGET_AUTOHIGHLIGHT (hex_widget_autohighlight_get_type ())
GType hex_widget_autohighlight_get_type (void) G_GNUC_CONST;
typedef struct _HexWidgetAutoHighlight HexWidgetAutoHighlight;

#define HEX_TYPE_WIDGET_MARK (hex_widget_mark_get_type ())
G_DECLARE_FINAL_TYPE (HexWidgetMark, hex_widget_mark, HEX, WIDGET_MARK, GObject)

/* PUBLIC METHOD DECLARATIONS */

GtkWidget *hex_widget_new (HexDocument *owner);

void hex_widget_set_cursor (HexWidget *gh, gint64 index);
void hex_widget_set_cursor_by_row_and_col (HexWidget *gh, int col_x, gint64 line_y);
void hex_widget_set_nibble (HexWidget *gh, gboolean lower_nibble);

gint64 hex_widget_get_cursor (HexWidget *gh);
guchar hex_widget_get_byte (HexWidget *gh, gint64 offset);

void hex_widget_set_group_type (HexWidget *gh, HexWidgetGroupType gt);
HexWidgetGroupType hex_widget_get_group_type (HexWidget *gh);

void hex_widget_show_offsets (HexWidget *gh, gboolean show);
void hex_widget_show_hex_column (HexWidget *self, gboolean show);
void hex_widget_show_ascii_column (HexWidget *self, gboolean show);

gboolean hex_widget_get_insert_mode (HexWidget *gh);
void hex_widget_set_insert_mode (HexWidget *gh, gboolean insert);

void hex_widget_set_geometry (HexWidget *gh, int cpl, int vis_lines);

void hex_widget_copy_to_clipboard (HexWidget *gh);
void hex_widget_cut_to_clipboard (HexWidget *gh);
void hex_widget_paste_from_clipboard (HexWidget *gh);

void hex_widget_set_selection (HexWidget *gh, gint64 start, gint64 end);
gboolean hex_widget_get_selection (HexWidget *gh, gint64 *start, gint64 *end);
void hex_widget_clear_selection (HexWidget *gh);
void hex_widget_delete_selection (HexWidget *gh);
void hex_widget_zero_selection (HexWidget *gh);

void hex_widget_set_fade_zeroes (HexWidget *self, gboolean fade);
gboolean hex_widget_get_fade_zeroes (HexWidget *self);

void hex_widget_set_display_control_characters (HexWidget *self, gboolean display);
gboolean hex_widget_get_display_control_characters (HexWidget *self);

HexWidgetAutoHighlight *
hex_widget_insert_autohighlight (HexWidget *gh, const char *search, int len);
HexWidgetAutoHighlight *
hex_widget_insert_autohighlight_full (HexWidget *self, const char *search,
		int len, HexSearchFlags flags);
void hex_widget_delete_autohighlight (HexWidget *gh, HexWidgetAutoHighlight *ahl);

GtkAdjustment *hex_widget_get_adjustment(HexWidget *gh);
HexDocument *hex_widget_get_document (HexWidget *gh);

HexWidgetMark *hex_widget_add_mark (HexWidget *self, gint64 start, gint64 end,
		GdkRGBA *color);
void hex_widget_delete_mark (HexWidget *self, HexWidgetMark *mark);
void hex_widget_goto_mark (HexWidget *self, HexWidgetMark *mark);
void hex_widget_set_mark_custom_color (HexWidget *self, HexWidgetMark *mark,
		GdkRGBA *color);
void hex_widget_mark_get_custom_color (HexWidgetMark *mark, GdkRGBA *color);
gboolean hex_widget_mark_get_have_custom_color (HexWidgetMark *mark);
gint64 hex_widget_mark_get_start_offset (HexWidgetMark *mark);
gint64 hex_widget_mark_get_end_offset (HexWidgetMark *mark);

G_END_DECLS

#endif		/* GTKHEX_H */
