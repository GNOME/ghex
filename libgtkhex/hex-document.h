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

#pragma once

#include <glib-object.h>
#include <hex-buffer-iface.h>
#include "hex-search-info.h"

G_BEGIN_DECLS

#define HEX_TYPE_DOCUMENT hex_document_get_type ()
G_DECLARE_FINAL_TYPE (HexDocument, hex_document, HEX, DOCUMENT, GObject)

/**
 * HexChangeType:
 * @HEX_CHANGE_STRING: the change is a string
 * @HEX_CHANGE_BYTE: the change is a single byte/character
 *
 * Type of change operation.
 */
typedef enum
{
	HEX_CHANGE_STRING,
	HEX_CHANGE_BYTE
} HexChangeType;

/**
 * HexChangeData:
 *
 * A opaque structure containing metadata about a change made
 * to a [class@Hex.Document].
 *
 * The data is modified internally as the `HexDocument`
 * changes, but when accessed it is read-only.
 */
#define HEX_TYPE_CHANGE_DATA (hex_change_data_get_type ())
GType hex_change_data_get_type (void) G_GNUC_CONST;

typedef struct _HexChangeData HexChangeData;

gint64 hex_change_data_get_start_offset (HexChangeData *data);
gint64 hex_change_data_get_end_offset (HexChangeData *data);

HexDocument	*hex_document_new (void);
HexDocument	*hex_document_new_from_file (GFile *file);
void		hex_document_set_data (HexDocument *doc, gint64 offset, size_t len,
		size_t rep_len, char *data, gboolean undoable);
void		hex_document_set_byte (HexDocument *doc, char val, gint64 offset,
		gboolean insert, gboolean undoable);
void		hex_document_set_nibble (HexDocument *doc, char val, gint64 offset,
		gboolean lower_nibble, gboolean insert, gboolean undoable);
void		hex_document_delete_data (HexDocument *doc, gint64 offset, 
		size_t len, gboolean undoable);
/* TODO - Reimplement  void		hex_document_read (HexDocument *doc); */

void	hex_document_read_async (HexDocument *doc, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data);
gboolean hex_document_read_finish (HexDocument *doc, GAsyncResult   *result,
		GError        **error);

gboolean	hex_document_write (HexDocument *doc);
gboolean	hex_document_write_to_file (HexDocument *doc, GFile *file);
void		hex_document_write_to_file_async (HexDocument *doc, GFile *file,
		GCancellable *cancellable, GAsyncReadyCallback callback,
		gpointer user_data);
void		hex_document_write_async (HexDocument *doc,
		GCancellable *cancellable, GAsyncReadyCallback callback,
		gpointer user_data);
gboolean	hex_document_write_finish (HexDocument *doc, GAsyncResult *result,
		GError **error);

gboolean	hex_document_export_html (HexDocument *doc, const char *html_path,
		const char *base_name, gint64 start, gint64 end, guint cpl, guint lpp,
		guint cpw);
gboolean	hex_document_has_changed (HexDocument *doc);
void		hex_document_changed (HexDocument *doc, HexChangeData *change_data,
		gboolean push_undo);
void		hex_document_set_max_undo (HexDocument *doc, int max_undo);
gboolean	hex_document_undo (HexDocument *doc);
gboolean	hex_document_redo (HexDocument *doc);
int			hex_document_compare_data (HexDocument *doc, const char *what,
		gint64 pos, size_t len);
int			hex_document_compare_data_full (HexDocument *doc,
		HexSearchInfo *search_info);
gboolean	hex_document_find_forward (HexDocument *doc, gint64 start,
		const char *what, size_t len, gint64 *offset);
gboolean hex_document_find_forward_full (HexDocument *doc,
		HexSearchInfo *search_info);

void	hex_document_find_forward_async (HexDocument *doc, gint64 start,
		const char *what, size_t len, gint64 *offset, const char *found_msg,
		const char *not_found_msg, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data);

void	hex_document_find_forward_full_async (HexDocument *doc,
		HexSearchInfo *search_info, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data);

gboolean	hex_document_find_backward (HexDocument *doc, gint64 start,
		const char *what, size_t len, gint64 *offset);

gboolean hex_document_find_backward_full (HexDocument *doc,
		HexSearchInfo *search_info);
void		hex_document_find_backward_async (HexDocument *doc, gint64 start,
		const char *what, size_t len, gint64 *offset, const char *found_msg,
		const char *not_found_msg, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data);
void	hex_document_find_backward_full_async (HexDocument *doc,
		HexSearchInfo *search_info, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data);

HexSearchInfo *
hex_document_find_finish (HexDocument *doc, GAsyncResult *result);

gboolean	hex_document_get_can_undo (HexDocument *doc);
gboolean	hex_document_get_can_redo (HexDocument *doc);
gint64		hex_document_get_file_size (HexDocument *doc);
GFile *		hex_document_get_file (HexDocument *doc);
gboolean	hex_document_set_file (HexDocument *doc, GFile *file);
HexChangeData *	hex_document_get_undo_data (HexDocument *doc);
HexBuffer * 	hex_document_get_buffer (HexDocument *doc);
gboolean	hex_document_set_buffer (HexDocument *doc, HexBuffer *buf);

G_END_DECLS
