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

#include <hex-buffer-iface.h>
#include <stdio.h>
#include <glib-object.h>

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
 * HexSearchFlags:
 * @HEX_SEARCH_NONE: no search flags (byte-for-byte match)
 * @HEX_SEARCH_REGEX: regular expression search
 * @HEX_SEARCH_IGNORE_CASE: case-insensitive search
 *
 * Bitwise flags for search options that can be combined as desired.
 *
 * Since: 4.2
 */
typedef enum
{
	HEX_SEARCH_NONE				= 0,
	HEX_SEARCH_REGEX			= 1 << 0,
	HEX_SEARCH_IGNORE_CASE		= 1 << 1,
} HexSearchFlags;

/**
 * HexDocumentFindData:
 * @found: whether the string was found
 * @start: start offset of the payload, in bytes
 * @what: (array length=len) (element-type gint8): a pointer to the data to
 *   search within the #HexDocument
 * @len: length in bytes of the data to be searched for
 * @flags: [flags@Hex.SearchFlags] search flags (Since: 4.2)
 * @offset: offset of the found string
 * @found_len: length of the found string (may be different from the search
 *   string when dealing with regular expressions, for example) (Since: 4.2)
 * @found_msg: message intended to be displayed by the client if the string
 *   is found
 * @not_found_msg: message intended to be displayed by the client if the string
 *   is not found
 *
 * A structure containing metadata about a find operation in a
 * [class@Hex.Document].
 */
#define HEX_TYPE_DOCUMENT_FIND_DATA (hex_document_find_data_get_type ())
GType hex_document_find_data_get_type (void) G_GNUC_CONST;

typedef struct
{
	gboolean found;
	gint64 start;
	const char *what;
	size_t len;
	HexSearchFlags flags;
	gint64 offset;
	size_t found_len;
	const char *found_msg;
	const char *not_found_msg;

	/*< private >*/
	gpointer padding1[5];
	gint64 padding2[5];
	int padding3[5];
} HexDocumentFindData;

/**
 * HexChangeData:
 * @start: start offset of the payload, in bytes
 * @end: end offset of the payload, in bytes
 * @rep_len: amount of data to replace at @start, or 0 for data to be inserted
 *   without any overwriting
 * @lower_nibble: %TRUE if targetting the lower nibble (2nd hex digit) %FALSE
 *   if targetting the upper nibble (1st hex digit)
 * @insert: %TRUE if the operation should be insert mode, %FALSE if in
 *   overwrite mode
 * @type: [enum@Hex.ChangeType] representing the type of change (ie, a string
 *   or a single byte)
 * @v_string: string of the data representing a change, or %NULL
 * @v_byte: character representing a single byte to be changed, if applicable
 * @external_file_change: whether the change came externally (typically from
 *   another program) (Since: 4.10)
 *
 * A structure containing metadata about a change made to a
 * [class@Hex.Document].
 */
#define HEX_TYPE_CHANGE_DATA (hex_change_data_get_type ())
GType hex_change_data_get_type (void) G_GNUC_CONST;

typedef struct
{
	gint64 start, end;
	/* `replace length`: length to replace (overwrite); (0 to insert without
	 * overwriting) */
	size_t rep_len;
	gboolean lower_nibble;
	gboolean insert;
	HexChangeType type;
	char *v_string;
	char v_byte;

	/*< private >*/
	gpointer padding1[5];
	gint64 padding2[5];
	/*< public >*/
	gboolean external_file_change;
	/*< private >*/
	int padding3[4];
} HexChangeData;


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
void		hex_document_changed (HexDocument *doc,gpointer change_data,
		gboolean push_undo);
void		hex_document_set_max_undo (HexDocument *doc, int max_undo);
gboolean	hex_document_undo (HexDocument *doc);
gboolean	hex_document_redo (HexDocument *doc);
int			hex_document_compare_data (HexDocument *doc, const char *what,
		gint64 pos, size_t len);
int			hex_document_compare_data_full (HexDocument *doc,
		HexDocumentFindData *find_data, gint64 pos);
gboolean	hex_document_find_forward (HexDocument *doc, gint64 start,
		const char *what, size_t len, gint64 *offset);
gboolean hex_document_find_forward_full (HexDocument *doc,
		HexDocumentFindData *find_data);

void	hex_document_find_forward_async (HexDocument *doc, gint64 start,
		const char *what, size_t len, gint64 *offset, const char *found_msg,
		const char *not_found_msg, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data);

void	hex_document_find_forward_full_async (HexDocument *doc,
		HexDocumentFindData *find_data, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data);

gboolean	hex_document_find_backward (HexDocument *doc, gint64 start,
		const char *what, size_t len, gint64 *offset);

gboolean hex_document_find_backward_full (HexDocument *doc,
		HexDocumentFindData *find_data);
void		hex_document_find_backward_async (HexDocument *doc, gint64 start,
		const char *what, size_t len, gint64 *offset, const char *found_msg,
		const char *not_found_msg, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data);
void	hex_document_find_backward_full_async (HexDocument *doc,
		HexDocumentFindData *find_data, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data);

HexDocumentFindData *
hex_document_find_finish (HexDocument *doc, GAsyncResult *result);

gboolean	hex_document_can_undo (HexDocument *doc);
gboolean	hex_document_can_redo (HexDocument *doc);
gint64		hex_document_get_file_size (HexDocument *doc);
GFile *		hex_document_get_file (HexDocument *doc);
gboolean	hex_document_set_file (HexDocument *doc, GFile *file);
HexChangeData *	hex_document_get_undo_data (HexDocument *doc);
HexBuffer * 	hex_document_get_buffer (HexDocument *doc);
gboolean	hex_document_set_buffer (HexDocument *doc, HexBuffer *buf);

/* HexDocumentFindData functions */

HexDocumentFindData *hex_document_find_data_new (void);
HexDocumentFindData *hex_document_find_data_copy (HexDocumentFindData *data);

G_END_DECLS

#endif /* HEX_DOCUMENT_H */
