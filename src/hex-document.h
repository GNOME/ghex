/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* hex-document.h

   Copyright (C) 1998 - 2002 Free Software Foundation

   Copyright © 2003-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

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

   Author: Jaka Mocnik <jaka@gnu.org>
 */

#ifndef HEX_DOCUMENT_H
#define HEX_DOCUMENT_H

#include <stdio.h>

#include <glib-object.h>

#include <hex-buffer-iface.h>

G_BEGIN_DECLS

#define HEX_TYPE_DOCUMENT hex_document_get_type ()
G_DECLARE_FINAL_TYPE (HexDocument, hex_document, HEX, DOCUMENT, GObject)

typedef enum {
	HEX_CHANGE_STRING,
	HEX_CHANGE_BYTE
} HexChangeType;

typedef struct _HexChangeData HexChangeData;
struct _HexChangeData
{
	size_t start, end;
	/* length to replace (overwrite); (0 to insert without overwriting) */
	size_t rep_len;
	gboolean lower_nibble;
	gboolean insert;
	HexChangeType type;
	char *v_string;
	char v_byte;
};


HexDocument *hex_document_new(void);
HexDocument *hex_document_new_from_file (GFile *file);
void        hex_document_set_data(HexDocument *doc, size_t offset, size_t len, size_t rep_len, char *data, gboolean undoable);
void        hex_document_set_byte(HexDocument *doc, char val, size_t offset, gboolean insert, gboolean undoable);
void        hex_document_set_nibble(HexDocument *doc, char val, size_t offset, gboolean lower_nibble, gboolean insert, gboolean undoable);
void        hex_document_delete_data(HexDocument *doc, guint offset, guint len, gboolean undoable);
void		hex_document_read (HexDocument *doc);
gboolean   hex_document_write(HexDocument *doc);

gboolean   hex_document_write_to_file (HexDocument *doc, GFile *file);
gboolean    hex_document_export_html (HexDocument *doc, char *html_path, char *base_name, size_t start, size_t end, guint cpl, guint lpp, guint cpw);
gboolean    hex_document_has_changed(HexDocument *doc);
void        hex_document_changed(HexDocument *doc, gpointer change_data, gboolean push_undo);
void        hex_document_set_max_undo(HexDocument *doc, int max_undo);
gboolean    hex_document_undo(HexDocument *doc);
gboolean    hex_document_redo(HexDocument *doc);
int        hex_document_compare_data(HexDocument *doc, char *s2, int pos, int len);
gboolean   hex_document_find_forward (HexDocument *doc, size_t start, char *what, size_t len, size_t *found);
gboolean   hex_document_find_backward (HexDocument *doc, size_t start, char *what, size_t len, size_t *found);
gboolean    hex_document_can_undo (HexDocument *doc);
gboolean    hex_document_can_redo (HexDocument *doc);
size_t		hex_document_get_file_size (HexDocument *doc);
HexChangeData *hex_document_get_undo_data (HexDocument *doc);

HexBuffer * hex_document_get_buffer (HexDocument *doc);
GFile *     hex_document_get_file (HexDocument *doc);
gboolean    hex_document_set_file (HexDocument *doc, GFile *file);

G_END_DECLS

#endif /* HEX_DOCUMENT_H */
