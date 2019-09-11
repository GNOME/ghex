/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* hex-document.h

   Copyright (C) 1998 - 2002 Free Software Foundation

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

#ifndef __HEX_DOCUMENT_H__
#define __HEX_DOCUMENT_H__

#include <stdio.h>

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define HEX_DOCUMENT_TYPE          (hex_document_get_type())
#define HEX_DOCUMENT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, hex_document_get_type (), HexDocument)
#define HEX_DOCUMENT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, hex_document_get_type (), HexDocumentClass)
#define IS_HEX_DOCUMENT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, hex_document_get_type ())

typedef struct _HexDocument       HexDocument;
typedef struct _HexDocumentClass  HexDocumentClass;
typedef struct _HexChangeData     HexChangeData;

typedef enum {
	HEX_CHANGE_STRING,
	HEX_CHANGE_BYTE
} HexChangeType;

struct _HexChangeData
{
	guint start, end, rep_len;
	gint digit;
	gboolean insert;
	HexChangeType type;
	gchar *v_string;
	gchar v_byte;
};

struct _HexDocument
{
	GObject object;

	GList *views;      /* a list of GtkHex widgets showing this document */
	
	gchar *file_name;
	gchar *path_end;

	guchar *buffer;    /* data buffer */
	guchar *gap_pos;   /* pointer to the start of insertion gap */
	gint gap_size;     /* insertion gap size */
	guint buffer_size; /* buffer size = file size + gap size */
	guint file_size;   /* real file size */

	gboolean changed;

	GList *undo_stack; /* stack base */
	GList *undo_top;   /* top of the stack (for redo) */
	guint undo_depth;  /* number of els on stack */
	guint undo_max;    /* max undo depth */
};

struct _HexDocumentClass
{
	GObjectClass parent_class;

	void (*document_changed)(HexDocument *, gpointer, gboolean);
	void (*undo)(HexDocument *);
	void (*redo)(HexDocument *);
	void (*undo_stack_forget)(HexDocument *);
};

GType       hex_document_get_type(void);
HexDocument *hex_document_new(void);
HexDocument *hex_document_new_from_file(const gchar *name);
void        hex_document_set_data(HexDocument *doc, guint offset,
								  guint len, guint rep_len, guchar *data,
								  gboolean undoable);
void        hex_document_set_byte(HexDocument *doc, guchar val, guint offset,
								  gboolean insert, gboolean undoable);
void        hex_document_set_nibble(HexDocument *doc, guchar val,
									guint offset, gboolean lower_nibble,
									gboolean insert, gboolean undoable);
void        hex_document_set_digit(HexDocument *doc, guchar val,
								   guint offset, gint digit,
								   gboolean insert, gboolean undoable);
guchar      hex_document_get_byte(HexDocument *doc, guint offset);
guchar      *hex_document_get_data(HexDocument *doc, guint offset, guint len);
void        hex_document_delete_data(HexDocument *doc, guint offset,
									 guint len, gboolean undoable);
gint        hex_document_read(HexDocument *doc);
gint        hex_document_write(HexDocument *doc);
gint        hex_document_write_to_file(HexDocument *doc, FILE *file);
gint        hex_document_export_html(HexDocument *doc,
									 gchar *html_path, gchar *base_name,
									 guint start, guint end,
									 guint cpl, guint lpp, guint cpw);
gboolean    hex_document_has_changed(HexDocument *doc);
void        hex_document_changed(HexDocument *doc, gpointer change_data,
								 gboolean push_undo);
void        hex_document_set_max_undo(HexDocument *doc, guint max_undo);
gboolean    hex_document_undo(HexDocument *doc);
gboolean    hex_document_redo(HexDocument *doc);
gint        hex_document_compare_data(HexDocument *doc, guchar *s2,
									  gint pos, gint len);
gint        hex_document_find_forward(HexDocument *doc, guint start,
									  guchar *what, gint len, guint *found);
gint        hex_document_find_backward(HexDocument *doc, guint start,
									   guchar *what, gint len, guint *found);
void        hex_document_remove_view(HexDocument *doc, GtkWidget *view);
GtkWidget   *hex_document_add_view(HexDocument *doc);
const GList *hex_document_get_list(void);
gboolean    hex_document_is_writable(HexDocument *doc);

G_END_DECLS

#endif /* __HEX_DOCUMENT_H__ */
