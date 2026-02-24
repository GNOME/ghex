// vim: linebreak breakindent breakindentopt=shift\:4

/* ghex-info-bar.h - Declaration of info bar widget
 *
 * Copyright © 2025-2026 Logan Rathbone
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

#define GHEX_TYPE_INFO_BAR (ghex_info_bar_get_type ())
G_DECLARE_FINAL_TYPE (GHexInfoBar, ghex_info_bar, GHEX, INFO_BAR, GtkWidget)

GtkWidget *	ghex_info_bar_new (void);
gboolean ghex_info_bar_get_shown (GHexInfoBar *self);
void ghex_info_bar_set_shown (GHexInfoBar *self, gboolean shown);
void ghex_info_bar_set_title (GHexInfoBar *self, const char *title);
const char * ghex_info_bar_get_description (GHexInfoBar *self);
void ghex_info_bar_set_description (GHexInfoBar *self, const char *description);

G_END_DECLS
