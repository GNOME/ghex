/* vim: ts=4 sw=4 colorcolumn=80
 */
/* hex-info-bar.h - Declaration of hex info bar widget
 *
 * Copyright © 2025 Logan Rathbone
 *
 * GHex is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GHex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GHex; see the file COPYING.
 * If not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Original GHex Author: Jaka Mocnik
 */

#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gi18n.h>

G_BEGIN_DECLS

#define HEX_TYPE_INFO_BAR (hex_info_bar_get_type ())
G_DECLARE_FINAL_TYPE (HexInfoBar, hex_info_bar, HEX, INFO_BAR, GtkWidget)

GtkWidget *	hex_info_bar_new (void);
gboolean hex_info_bar_get_shown (HexInfoBar *self);
void hex_info_bar_set_shown (HexInfoBar *self, gboolean shown);
void hex_info_bar_set_title (HexInfoBar *self, const char *title);
const char * hex_info_bar_get_description (HexInfoBar *self);
void hex_info_bar_set_description (HexInfoBar *self, const char *description);

G_END_DECLS
