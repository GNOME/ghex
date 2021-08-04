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
#include <gtk/gtk.h>

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
	int start, end;
	/* length to replace (overwrite); (0 to insert without overwriting) */
	int rep_len;
	gboolean lower_nibble;
	gboolean insert;
	HexChangeType type;
	char *v_string;
	char v_byte;
};


HexDocument *hex_document_new(void);
HexDocument *hex_document_new_from_file(const char *name);
void        hex_document_set_data(HexDocument *doc, int offset,
		int len, int rep_len, char *data, gboolean undoable);
void        hex_document_set_byte(HexDocument *doc, char val, int offset,
		gboolean insert, gboolean undoable);
void        hex_document_set_nibble(HexDocument *doc, char val,
		int offset, gboolean lower_nibble, gboolean insert, gboolean undoable);
char        hex_document_get_byte(HexDocument *doc, int offset);
char        *hex_document_get_data(HexDocument *doc, int offset, int len);
void        hex_document_delete_data(HexDocument *doc, guint offset,
		guint len, gboolean undoable);
int        hex_document_read(HexDocument *doc);
int        hex_document_write(HexDocument *doc);
int        hex_document_write_to_file(HexDocument *doc, FILE *file);
int        hex_document_export_html(HexDocument *doc, char *html_path,
		char *base_name, int start, int end, int cpl, int lpp, int cpw);
gboolean    hex_document_has_changed(HexDocument *doc);
void        hex_document_changed(HexDocument *doc, gpointer change_data,
		gboolean push_undo);
void        hex_document_set_max_undo(HexDocument *doc, int max_undo);
gboolean    hex_document_undo(HexDocument *doc);
gboolean    hex_document_redo(HexDocument *doc);
int        hex_document_compare_data(HexDocument *doc, char *s2,
		int pos, int len);
int        hex_document_find_forward(HexDocument *doc, int start,
		char *what, int len, int *found);
int        hex_document_find_backward(HexDocument *doc, guint start,
		char *what, int len, guint *found);
void        hex_document_remove_view(HexDocument *doc, GtkWidget *view);
GtkWidget   *hex_document_add_view(HexDocument *doc);
gboolean    hex_document_is_writable(HexDocument *doc);
gboolean    hex_document_change_file_name (HexDocument *doc,
		const char *new_file_name);
gboolean    hex_document_can_undo (HexDocument *doc);
gboolean    hex_document_can_redo (HexDocument *doc);
const char	*hex_document_get_file_name (HexDocument *doc);
const char	*hex_document_get_basename (HexDocument *doc);
int			hex_document_get_file_size (HexDocument *doc);
HexChangeData *hex_document_get_undo_data (HexDocument *doc);

G_END_DECLS

#endif /* HEX_DOCUMENT_H */
