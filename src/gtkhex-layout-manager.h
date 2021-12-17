/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex-layout-manager.h - declaration of a GtkHex layout manager

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

#ifndef GTK_HEX_LAYOUT_MANAGER_H
#define GTK_HEX_LAYOUT_MANAGER_H

#include <gtk/gtk.h>

/* Not a circular dep; this is just for the GTK_HEX_GROUP_* enums defined
 * there. */
#include "gtkhex.h"

G_BEGIN_DECLS

#define GTK_TYPE_HEX_LAYOUT (gtk_hex_layout_get_type ())
G_DECLARE_FINAL_TYPE (GtkHexLayout, gtk_hex_layout, GTK, HEX_LAYOUT,
		GtkLayoutManager)

typedef enum {
	NO_COLUMN,
	OFFSETS_COLUMN,
	HEX_COLUMN,
	ASCII_COLUMN,
	SCROLLBAR_COLUMN
} GtkHexLayoutColumn;

#define GTK_TYPE_HEX_LAYOUT_CHILD (gtk_hex_layout_child_get_type ())
G_DECLARE_FINAL_TYPE (GtkHexLayoutChild, gtk_hex_layout_child,
		GTK, HEX_LAYOUT_CHILD,
		GtkLayoutChild)

GtkLayoutManager *	gtk_hex_layout_new (void);
void				gtk_hex_layout_set_char_width (GtkHexLayout *layout,
						guint width);
void				gtk_hex_layout_child_set_column (GtkHexLayoutChild *child,
						GtkHexLayoutColumn column);
int					gtk_hex_layout_get_cpl (GtkHexLayout *layout);
int 				gtk_hex_layout_get_hex_cpl (GtkHexLayout *layout);
void				gtk_hex_layout_set_group_type (GtkHexLayout *layout,
						guint group_type);
void				gtk_hex_layout_set_cursor_pos (GtkHexLayout *layout,
						int x, int y);

G_END_DECLS

#endif	/* GTK_HEX_LAYOUT_MANAGER_H */
