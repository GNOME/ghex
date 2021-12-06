/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex-paste-data.h - declaration of paste data for GtkHex

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

#ifndef GTKHEX_PASTE_DATA_H
#define GTKHEX_PASTE_DATA_H

#include <glib-object.h>

#include <hex-document.h>

G_BEGIN_DECLS

#define GTK_TYPE_HEX_PASTE_DATA (gtk_hex_paste_data_get_type ())
G_DECLARE_FINAL_TYPE (GtkHexPasteData, gtk_hex_paste_data, GTK, HEX_PASTE_DATA,
		GObject)

/* Method Declarations */

GtkHexPasteData *	gtk_hex_paste_data_new (char *doc_data, int elems);
char *				gtk_hex_paste_data_get_string (GtkHexPasteData *self);
char *			gtk_hex_paste_data_get_doc_data (GtkHexPasteData *self);
int				gtk_hex_paste_data_get_elems (GtkHexPasteData *self);

G_END_DECLS

#endif		/* GTKHEX_PASTE_DATA_H */
