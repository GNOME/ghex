/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex-paste-data.h - declaration of paste data for HexWidget

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

#define HEX_TYPE_PASTE_DATA (hex_paste_data_get_type ())
G_DECLARE_FINAL_TYPE (HexPasteData, hex_paste_data, HEX, PASTE_DATA, GObject)

/* Method Declarations */

HexPasteData *	hex_paste_data_new (char *doc_data, int elems);
char *				hex_paste_data_get_string (HexPasteData *self);
char *			hex_paste_data_get_doc_data (HexPasteData *self);
int				hex_paste_data_get_elems (HexPasteData *self);

G_END_DECLS

#endif		/* GTKHEX_PASTE_DATA_H */
