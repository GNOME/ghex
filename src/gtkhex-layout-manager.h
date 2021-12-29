/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex-layout-manager.h - declaration of a HexWidget layout manager

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

   Original GHex Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifndef HEX_WIDGET_LAYOUT_MANAGER_H
#define HEX_WIDGET_LAYOUT_MANAGER_H

#include <gtk/gtk.h>

/* Not a circular dep; this is just for the HEX_WIDGET_GROUP_* enums defined
 * there. */
#include "gtkhex.h"

G_BEGIN_DECLS

#define HEX_TYPE_WIDGET_LAYOUT (hex_widget_layout_get_type ())
G_DECLARE_FINAL_TYPE (HexWidgetLayout, hex_widget_layout, HEX, WIDGET_LAYOUT,
		GtkLayoutManager)

typedef enum {
	NO_COLUMN,
	OFFSETS_COLUMN,
	HEX_COLUMN,
	ASCII_COLUMN,
	SCROLLBAR_COLUMN
} HexWidgetLayoutColumn;

#define HEX_TYPE_WIDGET_LAYOUT_CHILD (hex_widget_layout_child_get_type ())
G_DECLARE_FINAL_TYPE (HexWidgetLayoutChild, hex_widget_layout_child,
		HEX, WIDGET_LAYOUT_CHILD, GtkLayoutChild)

GtkLayoutManager *	hex_widget_layout_new (void);
void				hex_widget_layout_set_char_width (HexWidgetLayout *layout,
						int width);
void				hex_widget_layout_child_set_column (HexWidgetLayoutChild *child,
						HexWidgetLayoutColumn column);
int					hex_widget_layout_get_cpl (HexWidgetLayout *layout);
int 				hex_widget_layout_get_hex_cpl (HexWidgetLayout *layout);
void				hex_widget_layout_set_group_type (HexWidgetLayout *layout,
						HexWidgetGroupType group_type);
void				hex_widget_layout_set_cursor_pos (HexWidgetLayout *layout,
						int x, int y);
void				hex_widget_layout_set_offset_cpl (HexWidgetLayout *layout,
						int offset_cpl);
int					hex_widget_layout_get_offset_cpl (HexWidgetLayout *layout);
int					hex_widget_layout_util_hex_cpl_from_ascii_cpl (int ascii_cpl, 
						HexWidgetGroupType group_type);

G_END_DECLS

#endif	/* HEX_WIDGET_LAYOUT_MANAGER_H */
