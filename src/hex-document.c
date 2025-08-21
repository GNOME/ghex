/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* hex-document.c - implementation of a hex document

   Copyright (C) 1998 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
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

#include "hex-document.h"
#include "hex-file-monitor.h"

#include <stdio.h>
#include <string.h>

#include <glib/gi18n.h>

#include <config.h>

static void hex_document_real_changed   (HexDocument *doc,
										 gpointer change_data,
										 gboolean undoable);
static void hex_document_real_redo      (HexDocument *doc);
static void hex_document_real_undo      (HexDocument *doc);
static void free_stack                  (GList *stack);
static gint undo_stack_push             (HexDocument *doc,
									     HexChangeData *change_data);
static void undo_stack_descend          (HexDocument *doc);
static void undo_stack_ascend           (HexDocument *doc);
static void undo_stack_free             (HexDocument *doc);

#define DEFAULT_UNDO_DEPTH 1024
#define REGEX_SEARCH_LEN 1024

/* SIGNALS */

enum {
	DOCUMENT_CHANGED,
	UNDO,
	REDO,
	UNDO_STACK_FORGET,
	FILE_NAME_CHANGED,
	FILE_SAVE_STARTED,
	FILE_SAVED,
	FILE_READ_STARTED,
	FILE_LOADED,
	LAST_SIGNAL
};

static guint hex_signals[LAST_SIGNAL];

/* PROPERTIES */

enum
{
	GFILE = 1,
	BUFFER,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];


/* HexDocumentFindData GType Definitions */

/**
 * hex_document_find_data_new:
 *
 * Create a new empty [struct@Hex.DocumentFindData] structure.
 *
 * Returns: a new #HexDocumentFindData structure. Can be freed with
 *   `g_free ()`.
 *
 * Since: 4.2
 */
HexDocumentFindData *
hex_document_find_data_new (void)
{
	return g_new0 (HexDocumentFindData, 1);
}

/**
 * hex_document_find_data_copy:
 *
 * Copy a [struct@Hex.DocumentFindData] structure. This function is likely
 * only useful for language bindings.
 *
 * Returns: a newly allocated #HexDocumentFindData structure. Can be freed with
 *   `g_free ()`.
 *
 * Since: 4.2
 */
HexDocumentFindData *
hex_document_find_data_copy (HexDocumentFindData *data)
{
	return g_memdup2 (data, sizeof *data);
}

G_DEFINE_BOXED_TYPE (HexDocumentFindData, hex_document_find_data,
		hex_document_find_data_copy, g_free)

/* HexChangeData GType Definitions */

static HexChangeData *
hex_change_data_copy (HexChangeData *data)
{
	HexChangeData *new = NULL;

	g_return_val_if_fail (data != NULL, NULL);

	new = g_new0 (HexChangeData, 1);

	new->start = data->start;
	new->end = data->end;
	new->rep_len = data->rep_len;
	new->lower_nibble = data->lower_nibble;
	new->insert = data->insert;
	new->type = data->type;
	new->v_string = g_strdup (data->v_string);
	new->v_byte = data->v_byte;

	return new;
}

G_DEFINE_BOXED_TYPE (HexChangeData, hex_change_data,
		hex_change_data_copy, g_free)


/* GOBJECT DEFINITION */

/**
 * HexDocument:
 *
 * `HexDocument` is an object which allows raw data to be loaded,
 * saved and manipulated, intended primarily to be used with the `HexWidget`
 * widget.
 */
struct _HexDocument
{
	GObject object;

	GFile *file;
	gboolean changed;
	HexBuffer *buffer;
	HexFileMonitor *monitor;

	GList *undo_stack; /* stack base */
	GList *undo_top;   /* top of the stack (for redo) */
	int undo_depth;  /* number of els on stack */
	int undo_max;    /* max undo depth */
};

G_DEFINE_TYPE (HexDocument, hex_document, G_TYPE_OBJECT)


/* PROPERTIES - GETTERS AND SETTERS */

static void
hex_document_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexDocument *doc = HEX_DOCUMENT(object);

	switch (property_id)
	{
		case GFILE:
			hex_document_set_file (doc, g_value_get_object (value));
			break;

		case BUFFER:
			hex_document_set_buffer (doc, g_value_get_object (value));
			break;

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_document_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexDocument *doc = HEX_DOCUMENT(object);

	switch (property_id)
	{
		case GFILE:
			g_value_set_object (value, hex_document_get_file (doc));
			break;

		case BUFFER:
			g_value_set_object (value, hex_document_get_buffer (doc));
			break;

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/* ---- */

static void
free_stack(GList *stack)
{
	HexChangeData *cd;

	while(stack) {
		cd = (HexChangeData *)stack->data;
		if(cd->v_string)
			g_free (cd->v_string);
		stack = g_list_remove(stack, cd);
		g_free (cd);
	}
}

static gint
undo_stack_push(HexDocument *doc, HexChangeData *change_data)
{
	HexChangeData *cd;
	GList *stack_rest;

	if(doc->undo_stack != doc->undo_top) {
		stack_rest = doc->undo_stack;
		doc->undo_stack = doc->undo_top;
		if(doc->undo_top) {
			doc->undo_top->prev->next = NULL;
			doc->undo_top->prev = NULL;
		}
		free_stack(stack_rest);
	}

	if((cd = g_new(HexChangeData, 1)) != NULL) {
		memcpy(cd, change_data, sizeof(HexChangeData));
		if(change_data->v_string) {
			cd->v_string = g_malloc(cd->rep_len);
			memcpy(cd->v_string, change_data->v_string, cd->rep_len);
		}

		doc->undo_depth++;

		if(doc->undo_depth > doc->undo_max) {
			GList *last;

			last = g_list_last(doc->undo_stack);
			doc->undo_stack = g_list_remove_link(doc->undo_stack, last);
			doc->undo_depth--;
			free_stack(last);
		}

		doc->undo_stack = g_list_prepend(doc->undo_stack, cd);
		doc->undo_top = doc->undo_stack;

		return TRUE;
	}

	return FALSE;
}

static void
undo_stack_descend(HexDocument *doc)
{
	if(doc->undo_top == NULL)
		return;

	doc->undo_top = doc->undo_top->next;
	doc->undo_depth--;
}

static void
undo_stack_ascend(HexDocument *doc)
{
	if(doc->undo_stack == NULL || doc->undo_top == doc->undo_stack)
		return;

	if(doc->undo_top == NULL)
		doc->undo_top = g_list_last(doc->undo_stack);
	else
		doc->undo_top = doc->undo_top->prev;
	doc->undo_depth++;
}

static void
undo_stack_free(HexDocument *doc)
{
	if(doc->undo_stack == NULL)
		return;

	free_stack(doc->undo_stack);
	doc->undo_stack = NULL;
	doc->undo_top = NULL;
	doc->undo_depth = 0;

	g_signal_emit(G_OBJECT(doc), hex_signals[UNDO_STACK_FORGET], 0);
}

static void
hex_document_dispose (GObject *obj)
{
	HexDocument *doc = HEX_DOCUMENT(obj);
	
	g_clear_object (&doc->file);
	g_clear_object (&doc->buffer);
	g_clear_object (&doc->monitor);

	G_OBJECT_CLASS(hex_document_parent_class)->dispose (obj);
}

static void
hex_document_finalize (GObject *obj)
{
	HexDocument *doc = HEX_DOCUMENT (obj);
	
	undo_stack_free (doc);

	G_OBJECT_CLASS(hex_document_parent_class)->finalize (obj);
}

static void
hex_document_real_changed (HexDocument *doc, gpointer change_data,
						  gboolean push_undo)
{
	if(push_undo && doc->undo_max > 0)
		undo_stack_push(doc, change_data);
}

static void
hex_document_class_init (HexDocumentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GParamFlags default_flags = G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY;
	
	gobject_class->finalize = hex_document_finalize;
	gobject_class->dispose = hex_document_dispose;
	gobject_class->set_property = hex_document_set_property;
	gobject_class->get_property = hex_document_get_property;

	/* PROPERTIES */

	properties[GFILE] = g_param_spec_object ("file", NULL, NULL, 
			G_TYPE_FILE,
			default_flags);

	properties[BUFFER] = g_param_spec_object ("buffer", NULL, NULL, 
			HEX_TYPE_BUFFER,
			default_flags);

	g_object_class_install_properties (gobject_class, N_PROPERTIES, properties);

	/* SIGNALS */
	
	hex_signals[DOCUMENT_CHANGED] =
		g_signal_new_class_handler ("document-changed",
				G_OBJECT_CLASS_TYPE (gobject_class),
				G_SIGNAL_RUN_FIRST,
				G_CALLBACK(hex_document_real_changed),
				NULL, NULL, NULL,
				G_TYPE_NONE,
				2, G_TYPE_POINTER, G_TYPE_BOOLEAN);

	hex_signals[UNDO] = 
		g_signal_new_class_handler ("undo",
				G_OBJECT_CLASS_TYPE (gobject_class),
				G_SIGNAL_RUN_FIRST,
				G_CALLBACK(hex_document_real_undo),
				NULL, NULL, NULL,
				G_TYPE_NONE,
				0);

	hex_signals[REDO] = 
		g_signal_new_class_handler ("redo",
				G_OBJECT_CLASS_TYPE (gobject_class),
				G_SIGNAL_RUN_FIRST,
				G_CALLBACK(hex_document_real_redo),
				NULL, NULL, NULL,
				G_TYPE_NONE,
				0);

	hex_signals[UNDO_STACK_FORGET] = 
		g_signal_new_class_handler ("undo_stack_forget",
				G_OBJECT_CLASS_TYPE (gobject_class),
				G_SIGNAL_RUN_FIRST,
				NULL,
				NULL, NULL, NULL,
				G_TYPE_NONE,
				0);

	hex_signals[FILE_NAME_CHANGED] = 
		g_signal_new_class_handler ("file-name-changed",
				G_OBJECT_CLASS_TYPE (gobject_class),
				G_SIGNAL_RUN_FIRST,
				NULL,
				NULL, NULL, NULL,
				G_TYPE_NONE,
				0);

	/**
	 * HexDocument::file-save-started:
	 *
	 * Since: 4.6.1
	 */
	hex_signals[FILE_SAVE_STARTED] =
		g_signal_new_class_handler ("file-save-started",
				G_OBJECT_CLASS_TYPE (gobject_class),
				G_SIGNAL_RUN_FIRST,
				NULL,
				NULL, NULL, NULL,
				G_TYPE_NONE,
				0);

	hex_signals[FILE_SAVED] =
		g_signal_new_class_handler ("file-saved",
				G_OBJECT_CLASS_TYPE (gobject_class),
				G_SIGNAL_RUN_FIRST,
				NULL,
				NULL, NULL, NULL,
				G_TYPE_NONE,
				0);

	hex_signals[FILE_READ_STARTED] =
		g_signal_new_class_handler ("file-read-started",
				G_OBJECT_CLASS_TYPE (gobject_class),
				G_SIGNAL_RUN_FIRST,
				NULL,
				NULL, NULL, NULL,
				G_TYPE_NONE,
				0);

	hex_signals[FILE_LOADED] =
		g_signal_new_class_handler ("file-loaded",
				G_OBJECT_CLASS_TYPE (gobject_class),
				G_SIGNAL_RUN_FIRST,
				NULL,
				NULL, NULL, NULL,
				G_TYPE_NONE,
				0);
}

static void
hex_document_init (HexDocument *doc)
{
	HexBuffer *try_buf = NULL;
	const char *default_buf;

	default_buf = g_getenv ("HEX_BUFFER");
	if (! default_buf)
		default_buf = "mmap";

	try_buf = hex_buffer_util_new (default_buf, NULL);
	if (! try_buf)
		try_buf = hex_buffer_util_new (NULL, NULL);

	g_assert (try_buf != NULL);
	doc->buffer = try_buf;

	doc->undo_max = DEFAULT_UNDO_DEPTH;
}

/*-------- public API starts here --------*/


/**
 * hex_document_new:
 *
 * Create a new empty [class@Hex.Document] object.
 *
 * Since 4.6, the HEX_BUFFER environment variable can optionally be set to
 * specify which backend should be tried first as the default. Otherwise, the
 * `mmap` buffer will be attempted to be loaded if available, and if it is not,
 * it will fall back to the `malloc` buffer backend.
 *
 * Returns: a new [class@Hex.Document] object.
 */
HexDocument *
hex_document_new (void)
{
	return g_object_new (HEX_TYPE_DOCUMENT, NULL);
}

static void
hex_document_file_changed_cb (HexDocument *doc,
                              GParamSpec *pspec,
                              HexFileMonitor *monitor)
{
	if (hex_file_monitor_get_changed(monitor))
	{
		HexChangeData *change_data;

		doc->changed = TRUE;

		change_data = g_new0 (HexChangeData, 1);
		change_data->external_file_change = TRUE;

		hex_document_changed (doc, &change_data, FALSE);
	}
}

/**
 * hex_document_set_file:
 * @doc: a [class@Hex.Document] object
 * @file: a #GFile pointing to a valid file on the system
 *
 * Set the file of a [class@Hex.Document] object by #GFile.
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
gboolean
hex_document_set_file (HexDocument *doc, GFile *file)
{
	g_return_val_if_fail (HEX_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	/* Only the malloc backend can open 0-length files */
	if (hex_buffer_util_get_file_size (file) == 0)
	{
		HexBuffer *buf;

		g_debug ("%s: Zero-length file detected. Attempting to set `malloc` buffer.", __func__);
		buf = hex_buffer_util_new ("malloc", file);
		hex_document_set_buffer (doc, buf);
	}

	if (! hex_buffer_set_file (doc->buffer, file)) {
		g_debug ("%s: Invalid file", __func__);
		return FALSE;
	}

	g_clear_object (&doc->file);
	doc->file = g_object_ref (file);

	g_signal_emit (G_OBJECT(doc), hex_signals[FILE_NAME_CHANGED], 0);
	g_object_notify_by_pspec (G_OBJECT(doc), properties[BUFFER]);

	g_clear_object (&doc->monitor);
	doc->monitor = hex_file_monitor_new (file);
	g_signal_connect_object(doc->monitor,
	                        "notify::changed",
	                        G_CALLBACK (hex_document_file_changed_cb),
	                        doc,
	                        G_CONNECT_SWAPPED);

	return TRUE;
}

/**
 * hex_document_new_from_file:
 * @file: a #GFile pointing to a valid file on the system
 *
 * A convenience method to create a new [class@Hex.Document] from file.
 *
 * Returns: a [class@Hex.Document] pre-loaded with a #GFile ready for a
 * `read` operation, or %NULL if the operation failed.
 */
HexDocument *
hex_document_new_from_file (GFile *file)
{
	HexDocument *doc;

	g_return_val_if_fail (G_IS_FILE (file), NULL);
	
	doc = hex_document_new ();
	g_return_val_if_fail (doc, NULL);

	if (! hex_document_set_file (doc, file))
	{
		g_clear_object (&doc);
	}

	return doc;
}

/**
 * hex_document_set_nibble:
 * @doc: a [class@Hex.Document] object
 * @val: a character to set the nibble as
 * @offset: offset in bytes within the payload
 * @lower_nibble: %TRUE if targetting the lower nibble (2nd hex digit) %FALSE
 *   if targetting the upper nibble (1st hex digit)
 * @insert: %TRUE if the operation should be insert mode, %FALSE if in
 *   overwrite mode
 * @undoable: whether the operation should be undoable
 *
 * Set a particular nibble of a #HexDocument. 
 */
void
hex_document_set_nibble (HexDocument *doc, char val, gint64 offset,
						gboolean lower_nibble, gboolean insert,
						gboolean undoable)
{
	static HexChangeData tmp_change_data;
	static HexChangeData change_data;
	char tmp_data[2] = {0};		/* 1 char + NUL */

	doc->changed = TRUE;
	tmp_change_data.start = offset;
	tmp_change_data.end = offset;
	tmp_change_data.v_string = NULL;
	tmp_change_data.type = HEX_CHANGE_BYTE;
	tmp_change_data.lower_nibble = lower_nibble;
	tmp_change_data.insert = insert;

	tmp_change_data.v_byte = hex_buffer_get_byte (doc->buffer, offset);

	/* If in insert mode and on lower nibble, let the user enter the 2nd
	 * nibble on the selected byte, and don't insert a new byte until the
	 * next keystroke. nb: This has the side effect of only letting you
	 * insert a new byte if you're on the upper nibble...
	 */
	if (!lower_nibble && insert)
		tmp_change_data.rep_len = 0;
	else
		tmp_change_data.rep_len = 1;

	/* some 80s C magic right here, folks */
	snprintf (tmp_data, 2, "%c",
			(tmp_change_data.v_byte & (lower_nibble ? 0xF0 : 0x0F)) |
			(lower_nibble ? val : (val << 4)));

	if (hex_buffer_set_data (doc->buffer, offset, 1, tmp_change_data.rep_len,
				tmp_data))
	{
		change_data = tmp_change_data;
		hex_document_changed (doc, &change_data, undoable);
	}
}

/**
 * hex_document_set_byte:
 * @doc: a [class@Hex.Document] object
 * @val: a character to set the byte as
 * @offset: offset in bytes within the payload
 * @insert: %TRUE if the operation should be insert mode, %FALSE if in
 *   overwrite mode
 * @undoable: whether the operation should be undoable
 *
 * Set a particular byte of a #HexDocument at position `offset` within 
 * the payload.
 */
void
hex_document_set_byte (HexDocument *doc, char val, gint64 offset,
					  gboolean insert, gboolean undoable)
{
	static HexChangeData tmp_change_data;
	static HexChangeData change_data;
	char tmp_data[2] = {0};		/* 1 char + NUL */

	doc->changed = TRUE;
	tmp_change_data.start = offset;
	tmp_change_data.end = offset;
	tmp_change_data.rep_len = (insert ? 0 : 1);
	tmp_change_data.v_string = NULL;
	tmp_change_data.type = HEX_CHANGE_BYTE;
	tmp_change_data.lower_nibble = FALSE;
	tmp_change_data.insert = insert;

	tmp_change_data.v_byte = hex_buffer_get_byte (doc->buffer, offset);

	snprintf (tmp_data, 2, "%c", val);

	if (hex_buffer_set_data (doc->buffer, offset, 1, tmp_change_data.rep_len,
				tmp_data))
	{
		change_data = tmp_change_data;
		hex_document_changed (doc, &change_data, undoable);
	}
}

/**
 * hex_document_set_data:
 * @doc: a [class@Hex.Document] object
 * @offset: offset in bytes within the payload
 * @len: length in bytes of the data to be set
 * @rep_len: amount of bytes to replace/overwrite (if any)
 * @data: (array length=len) (element-type gint8): a pointer to the data being
 *   provided
 * @undoable: whether the operation should be undoable
 *
 * A convenience wrapper for [method@Hex.Buffer.set_data]. See the
 * description of that method for details.
 */
void
hex_document_set_data (HexDocument *doc, gint64 offset, size_t len,
					  size_t rep_len, char *data, gboolean undoable)
{
	int i;
	char *ptr;
	static HexChangeData tmp_change_data;
	static HexChangeData change_data;

	doc->changed = TRUE;

	tmp_change_data.start = offset;
	tmp_change_data.end = tmp_change_data.start + len - 1;
	tmp_change_data.rep_len = rep_len;
	tmp_change_data.type = HEX_CHANGE_STRING;
	tmp_change_data.lower_nibble = FALSE;

	g_clear_pointer (&tmp_change_data.v_string, g_free);

	tmp_change_data.v_string = hex_buffer_get_data (doc->buffer,
			tmp_change_data.start, tmp_change_data.rep_len);

	if (hex_buffer_set_data (doc->buffer, offset, len, rep_len, data))
	{
		change_data = tmp_change_data;
		hex_document_changed (doc, &change_data, undoable);
	}
}

/**
 * hex_document_delete_data:
 * @doc: a [class@Hex.Document] object
 * @offset: offset in bytes within the payload
 * @len: length in bytes of the data to be set
 * @undoable: whether the operation should be undoable
 *
 * Delete data at `offset` of `length` within the buffer.
 */
void
hex_document_delete_data (HexDocument *doc,
		gint64 offset, size_t len, gboolean undoable)
{
	hex_document_set_data (doc, offset, 0, len, NULL, undoable);
}

/**
 * hex_document_read_finish:
 * @doc: a [class@Hex.Document] object
 * @result: result of the task
 * @error: (nullable): optional pointer to a #GError object to populate with
 *   any error returned by the task
 *
 * Obtain the result of a completed file read operation.
 *
 * This method is mostly a wrapper around [method@Hex.Buffer.read_finish]
 * but takes some additional steps and emits the appropriate signals
 * applicable to the document object above and beyond the buffer.
 *
 * This method is typically called from the #GAsyncReadyCallback function
 * passed to [method@Hex.Document.read_async] to obtain the result of the
 * operation.
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
gboolean
hex_document_read_finish (HexDocument *doc,
		GAsyncResult   *result,
		GError        **error)
{
  g_return_val_if_fail (HEX_IS_DOCUMENT (doc), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
document_ready_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	gboolean success;
	GError *local_error = NULL;
	HexBuffer *buf = HEX_BUFFER(source_object);
	GTask *task = G_TASK (user_data);
	HexDocument *doc;

	doc = HEX_DOCUMENT(g_task_get_task_data (task));
	success = hex_buffer_read_finish (buf, res, &local_error);
	g_debug ("%s: DONE -- result: %d", __func__, success);

	if (! success)
	{
		if (local_error)
			g_task_return_error (task, local_error);
		else
			g_task_return_boolean (task, FALSE);

		goto cleanup;
	}

	/* Initialize data for new doc */

	undo_stack_free(doc);
	doc->changed = FALSE;

	if (doc->monitor)
		hex_file_monitor_reset (doc->monitor);

	g_signal_emit (G_OBJECT(doc), hex_signals[FILE_LOADED], 0);
	g_task_return_boolean (task, TRUE);

cleanup:
	g_object_unref (task);
}

/**
 * hex_document_read_async:
 * @doc: a [class@Hex.Document] object
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): function to be called when the operation is
 *   complete
 *
 * Read the #GFile into the buffer connected to the #HexDocument object.
 *
 * This method is mostly a wrapper around [method@Hex.Buffer.read_async]
 * but will allow additional steps and appropriate signals to be emitted
 * applicable to the document object above and beyond the buffer, when
 * the operation completes.
 */
void
hex_document_read_async (HexDocument *doc,
		GCancellable *cancellable,
		GAsyncReadyCallback callback,
		gpointer user_data)
{
	GTask *task;
	gint64 payload;

	g_return_if_fail (G_IS_FILE (doc->file));

	task = g_task_new (doc, cancellable, callback, user_data);
	g_task_set_task_data (task, doc, NULL);

	/* Read the actual file on disk into the buffer */
	hex_buffer_read_async (doc->buffer, NULL, document_ready_cb, task);
	g_signal_emit (G_OBJECT(doc), hex_signals[FILE_READ_STARTED], 0);
}

/**
 * hex_document_write_to_file:
 * @doc: a [class@Hex.Document] object
 * @file: #GFile to be written to
 *
 * Write the buffer to `file`. This can be used for a 'Save As' operation.
 *
 * This operation will block.
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
gboolean
hex_document_write_to_file (HexDocument *doc, GFile *file)
{
	return hex_buffer_write_to_file (doc->buffer, file);
}

/**
 * hex_document_write:
 * @doc: a [class@Hex.Document] object
 *
 * Write the buffer to the pre-existing #GFile connected to the #HexDocument
 * object. This can be used for a 'Save (in place)' operation.
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
gboolean
hex_document_write (HexDocument *doc)
{
	gboolean ret = FALSE;
	char *path = NULL;

	g_return_val_if_fail (HEX_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (G_IS_FILE (doc->file), FALSE);
	g_return_val_if_fail (HEX_IS_FILE_MONITOR (doc->monitor), FALSE);

	g_signal_emit (doc, hex_signals[FILE_SAVE_STARTED], 0);

	path = g_file_get_path (doc->file);
	if (! path)
		goto out;

	ret = hex_buffer_write_to_file (doc->buffer, doc->file);
	if (ret)
	{
		doc->changed = FALSE;
		hex_file_monitor_reset (doc->monitor);
		g_signal_emit (G_OBJECT(doc), hex_signals[FILE_SAVED], 0);
	}

out:
	g_free (path);
	return ret;
}

/**
 * hex_document_write_finish:
 * @doc: a [class@Hex.Document] object
 * @result: result of the task
 * @error: (nullable): optional pointer to a #GError object to populate with
 *   any error returned by the task
 *
 * Obtain the result of a completed write-to-file operation.
 *
 * Currently, this method is mostly a wrapper around
 * [method@Hex.Buffer.write_to_file_finish].
 *
 * This method is typically called from the #GAsyncReadyCallback function
 * passed to [method@Hex.Document.write_async] or
 * [method@Hex.Document.write_to_file_async] to obtain the result of the
 * operation.
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
gboolean
hex_document_write_finish (HexDocument *doc,
		GAsyncResult *result,
		GError **error)
{
  g_return_val_if_fail (HEX_IS_DOCUMENT (doc), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK(result), error);
}

static void
write_ready_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	HexBuffer *buf = HEX_BUFFER(source_object);
	GTask *doc_task = G_TASK(user_data);
	HexDocument *doc = g_task_get_source_object (doc_task);
	gboolean success;
	GError *local_error = NULL;

	success = hex_buffer_write_to_file_finish (buf, res, &local_error);
	g_debug ("%s: DONE -- result: %d", __func__, success);

	if (success)
	{
		doc->changed = FALSE;

		/* If going from untitled to a saved-as document, we might not have a
		 * file monitor yet.
		 */
		if (doc->monitor)
			hex_file_monitor_reset (doc->monitor);

		g_signal_emit (doc, hex_signals[FILE_SAVED], 0);
		g_task_return_boolean (doc_task, TRUE);
	}
	else
	{
		if (local_error)
			g_task_return_error (doc_task, local_error);
		else
			g_task_return_boolean (doc_task, FALSE);
	}

	g_object_unref (doc_task);	/* g_task_return_* takes a ref. */
}

/**
 * hex_document_write_async:
 * @doc: a [class@Hex.Document] object
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): function to be called when the operation is
 *   complete
 *
 * Write the buffer to the pre-existing #GFile connected to the #HexDocument
 * object. This can be used for a 'Save (in place)' operation. This is the
 * non-blocking version of [method@Hex.Document.write].
 *
 * Note that for both this method and
 * [method@Hex.Document.write_to_file_async],
 * [method@Hex.Document.write_finish] is the method to retrieve the return
 * value in your #GAsyncReadyCallback function.
 */
void
hex_document_write_async (HexDocument *doc,
		GCancellable *cancellable,
		GAsyncReadyCallback callback,
		gpointer user_data)
{
	GTask *doc_task;
	char *path = NULL;

	g_return_if_fail (G_IS_FILE (doc->file));

	path = g_file_get_path (doc->file);
	if (! path)
		goto out;

	g_signal_emit (doc, hex_signals[FILE_SAVE_STARTED], 0);

	doc_task = g_task_new (doc, cancellable, callback, user_data);

	hex_buffer_write_to_file_async (doc->buffer,
			doc->file,
			NULL,	/* cancellable */
			write_ready_cb,
			doc_task);

out:
	g_free (path);
}

/**
 * hex_document_write_to_file_async:
 * @doc: a [class@Hex.Document] object
 * @file: #GFile to be written to
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): function to be called when the operation is
 *   complete
 *
 * Write the buffer to `file` asynchronously. This can be used for a 'Save As'
 * operation.  This is the non-blocking version of
 * [method@Hex.Document.write_to_file].
 *
 * Note that for both this method and [method@Hex.Document.write_async],
 * [method@Hex.Document.write_finish] is the method to retrieve the return
 * value in your #GAsyncReadyCallback function.
 */
void
hex_document_write_to_file_async (HexDocument *doc,
		GFile *file,
		GCancellable *cancellable,
		GAsyncReadyCallback callback,
		gpointer user_data)
{
	GTask *doc_task;

	g_return_if_fail (G_IS_FILE (file));

	doc_task = g_task_new (doc, cancellable, callback, user_data);

	hex_buffer_write_to_file_async (doc->buffer,
			file,
			NULL,	/* cancellable */
			write_ready_cb,
			doc_task);
}

/**
 * hex_document_changed:
 * @doc: a [class@Hex.Document] object
 * @change_data: pointer to a [struct@Hex.ChangeData] structure
 * @push_undo: whether the undo stack should be pushed to
 *
 * Convenience method to emit the [signal@Hex.Document::document-changed]
 * signal. This method is mostly only useful for widgets utilizing
 * #HexDocument.
 */
void
hex_document_changed (HexDocument *doc, gpointer change_data,
					 gboolean push_undo)
{
	g_signal_emit(G_OBJECT(doc), hex_signals[DOCUMENT_CHANGED], 0,
				  change_data, push_undo);
}

/**
 * hex_document_has_changed:
 * @doc: a [class@Hex.Document] object
 * 
 * Method to check whether the #HexDocument has changed.
 *
 * Returns: %TRUE if the document has changed, %FALSE otherwise
 */
gboolean
hex_document_has_changed (HexDocument *doc)
{
	return doc->changed;
}

/**
 * hex_document_set_max_undo:
 * @doc: a [class@Hex.Document] object
 * @max_undo: the new maximum size of the undo stack
 *
 * Set the maximum size of the #HexDocument undo stack.
 */
void
hex_document_set_max_undo (HexDocument *doc, int max_undo)
{
	if(doc->undo_max != max_undo) {
		if(doc->undo_max > max_undo)
			undo_stack_free(doc);
		doc->undo_max = max_undo;
	}
}

/**
 * hex_document_export_html:
 * @doc: a [class@Hex.Document] object
 * @html_path: path to the directory in which the HTML file will be saved
 * @base_name: the base name of the filename to be saved, without the .html
 *   extension.
 * @start: starting offset byte of the payload in the range to save
 * @end: ending offset byte of the payload in the range to save
 * @cpl: columns per line
 * @lpp: lines per page
 * @cpw: characters per word (for grouping of nibbles)
 *
 * Export the #HexDocument to HTML.
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
gboolean
hex_document_export_html (HexDocument *doc,
		const char *html_path,
		const char *base_name,
		gint64 start,
		gint64 end,
		guint cpl,
		guint lpp,
		guint cpw)
{
	FILE *file;
	guint page, line, pos, lines, pages, c;
	gchar *page_name, b;
	gchar *progress_str;
	gint64 payload = hex_buffer_get_payload_size (hex_document_get_buffer (doc));
	char *basename;

	basename = g_file_get_basename (doc->file);
	if (! basename)
		basename = g_strdup (_("Untitled"));

	lines = (end - start)/cpl;
	if((end - start)%cpl != 0)
		lines++;
	pages = lines/lpp;
	if(lines%lpp != 0)
		pages++;

	/* top page */
	page_name = g_strdup_printf("%s/%s.html", html_path, base_name);
	file = fopen(page_name, "w");
	g_free(page_name);
	if(!file)
		return FALSE;
	fprintf(file, "<HTML>\n<HEAD>\n");
	fprintf(file, "<META HTTP-EQUIV=\"Content-Type\" "
			"CONTENT=\"text/html; charset=UTF-8\">\n");
	fprintf(file, "<META NAME=\"hexdata\" CONTENT=\"GHex export to HTML\">\n");
	fprintf(file, "</HEAD>\n<BODY>\n");

	fprintf(file, "<CENTER>");
	fprintf(file, "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\">\n");
	fprintf(file, "<TR>\n<TD COLSPAN=\"3\"><B>%s</B></TD>\n</TR>\n",
			basename);
	fprintf(file, "<TR>\n<TD COLSPAN=\"3\">&nbsp;</TD>\n</TR>\n");
	for(page = 0; page < pages; page++) {
		fprintf(file, "<TR>\n<TD>\n<A HREF=\"%s%08d.html\"><PRE>", base_name, page);
		fprintf(file, _("Page"));
		fprintf(file, " %d</PRE></A>\n</TD>\n<TD>&nbsp;</TD>\n<TD VALIGN=\"CENTER\"><PRE>%08x -", page+1, page*cpl*lpp);
		fprintf(file, " %08lx</PRE></TD>\n</TR>\n", MIN((page+1)*cpl*lpp-1, payload-1));
	}
	fprintf(file, "</TABLE>\n</CENTER>\n");
	fprintf(file, "<HR WIDTH=\"100%%\">");
	fprintf(file, _("Hex dump generated by"));
	fprintf(file, " <B>"LIBGTKHEX_RELEASE_STRING"</B>\n");
	fprintf(file, "</BODY>\n</HTML>\n");
	fclose(file);

	pos = start;
	g_object_ref(G_OBJECT(doc));
	for(page = 0; page < pages; page++)
	{
		/* write page header */
		page_name = g_strdup_printf("%s/%s%08d.html",
									html_path, base_name, page);
		file = fopen(page_name, "w");
		g_free(page_name);
		if(!file)
			break;
		/* write header */
		fprintf(file, "<HTML>\n<HEAD>\n");
		fprintf(file, "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=iso-8859-1\">\n");
		fprintf(file, "<META NAME=\"hexdata\" CONTENT=\"GHex export to HTML\">\n");
		fprintf(file, "</HEAD>\n<BODY>\n");
		/* write top table |previous|filename: page/pages|next| */
		fprintf(file, "<TABLE BORDER=\"0\" CELLSPACING=\"0\" WIDTH=\"100%%\">\n");
		fprintf(file, "<TR>\n<TD WIDTH=\"33%%\">\n");
		if(page > 0) {
			fprintf(file, "<A HREF=\"%s%08d.html\">", base_name, page-1);
			fprintf(file, _("Previous page"));
			fprintf(file, "</A>");
		}
		else
			fprintf(file, "&nbsp;");
		fprintf(file, "\n</TD>\n");
		fprintf(file, "<TD WIDTH=\"33%%\" ALIGN=\"CENTER\">\n");
		fprintf(file, "<A HREF=\"%s.html\">", base_name);
		fprintf(file, "%s:", basename);
		fprintf(file, "</A>");
		fprintf(file, " %d/%d", page+1, pages);
		fprintf(file, "\n</TD>\n");
		fprintf(file, "<TD WIDTH=\"33%%\" ALIGN=\"RIGHT\">\n");
		if(page < pages - 1) {
			fprintf(file, "<A HREF=\"%s%08d.html\">", base_name, page+1);
			fprintf(file, _("Next page"));
			fprintf(file, "</A>");
		}
		else
			fprintf(file, "&nbsp;");
		fprintf(file, "\n</TD>\n");
		fprintf(file, "</TR>\n</TABLE>\n");
		
		/* now the actual data */
		fprintf(file, "<CENTER>\n");
		fprintf(file, "<TABLE BORDER=\"1\" CELLSPACING=\"2\" CELLPADDING=\"2\">\n");
		fprintf(file, "<TR>\n<TD>\n");
		fprintf(file, "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\">\n");
		for(line = 0; line < lpp && pos + line*cpl < payload; line++) {
		/* offset of line*/
			fprintf(file, "<TR>\n<TD>\n");
			fprintf(file, "<PRE>%08x</PRE>\n", pos + line*cpl);
			fprintf(file, "</TD>\n</TR>\n");
		}
		fprintf(file, "</TABLE>\n");
		fprintf(file, "</TD>\n<TD>\n");
		fprintf(file, "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\">\n");
		c = 0;
		for(line = 0; line < lpp; line++) {
			/* hex data */
			fprintf(file, "<TR>\n<TD>\n<PRE>");
			while(pos + c < end) {
				fprintf(file, "%02x", hex_buffer_get_byte (doc->buffer, pos + c));
				c++;
				if(c%cpl == 0)
					break;
				if(c%cpw == 0)
					fprintf(file, " ");
			}
			fprintf(file, "</PRE>\n</TD>\n</TR>\n");
		}
		fprintf(file, "</TABLE>\n");
		fprintf(file, "</TD>\n<TD>\n");
		fprintf(file, "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\">\n");
		c = 0;
		for(line = 0; line < lpp; line++) {
			/* ascii data */
			fprintf(file, "<TR>\n<TD>\n<PRE>");
			while(pos + c < end) {
				b = hex_buffer_get_byte (doc->buffer, pos + c);
				if(b >= 0x20)
					fprintf(file, "%c", b);
				else
					fprintf(file, ".");
				c++;
				if(c%cpl == 0)
					break;
			}
			fprintf(file, "</PRE></TD>\n</TR>\n");
			if(pos >= end)
				line = lpp;
		}
		pos += c;
		fprintf(file, "</TD>\n</TR>\n");
		fprintf(file, "</TABLE>\n");
		fprintf(file, "</TABLE>\n</CENTER>\n");
		fprintf(file, "<HR WIDTH=\"100%%\">");
		fprintf(file, _("Hex dump generated by"));
		fprintf(file, " <B>" LIBGTKHEX_RELEASE_STRING "</B>\n");
		fprintf(file, "</BODY>\n</HTML>\n");
		fclose(file);
	}
	g_free (basename);
	g_object_unref(G_OBJECT(doc));

	return TRUE;
}

/**
 * hex_document_compare_data_full:
 * @find_data: a #HexDocumentFindData structure
 * @pos: offset position of the #HexDocument data to compare with the
 *   string contained in the `find_data` structure
 *
 * Full version of [method@Hex.Document.compare_data] to allow data
 * comparisons broader than byte-for-byte matches only. However, it is
 * less convenient than the above since it requires the caller to allocate
 * and free a #HexDocumentFindData structure.
 *
 * Returns: 0 if the comparison is an exact match; otherwise, a non-zero
 *   value is returned.
 *
 * Since: 4.2
 */
int
hex_document_compare_data_full (HexDocument *doc,
		HexDocumentFindData *find_data,
		gint64 pos)
{
	char *cp = 0;
	GError *local_error = NULL;
	int retval = 1;		/* match will make it zero, so set it non-zero now. */

	g_return_val_if_fail (find_data, 0);
	g_return_val_if_fail (find_data->what, 0);

	if (find_data->flags & HEX_SEARCH_REGEX)
	{
		GRegex *regex;
		GMatchInfo *match_info;
		char *regex_search_str;
		GRegexCompileFlags regex_compile_flags;

		/* GRegex doesn't let you specify the length of the search string, so
		 * it needs to be NULL-terminated.
		 */
		regex_search_str = g_malloc (find_data->len+1);
		memcpy (regex_search_str, find_data->what, find_data->len);
		regex_search_str[find_data->len] = 0;

		/* match string doesn't have to be UTF-8 */
		regex_compile_flags = G_REGEX_RAW;

		if (find_data->flags & HEX_SEARCH_IGNORE_CASE)
			regex_compile_flags |= G_REGEX_CASELESS;

		regex = g_regex_new (regex_search_str,
				regex_compile_flags,
				G_REGEX_MATCH_ANCHORED,
				&local_error);

		g_free (regex_search_str);

		/* sanity check */
		if (!regex || local_error)
		{
			g_debug ("%s: error: %s", __func__, local_error->message);
			goto out;
		}

		cp = hex_buffer_get_data (doc->buffer, pos, REGEX_SEARCH_LEN);

		if (g_regex_match_full (regex, cp,
					REGEX_SEARCH_LEN,	/* length of string being searched */
					0,					/* start pos */
					0,					/* addl match_options */
					&match_info,
					&local_error))
		{
			char *word = g_match_info_fetch (match_info, 0);

			find_data->found_len = strlen (word);
			g_free (word);
			retval = 0;
		}
		else
		{
			if (local_error)
			{
				g_debug ("%s: error: %s",
						__func__,
						local_error ? local_error->message : NULL);
			}
			retval = 1;
		}
	}
	else	/* non regex */
	{
		cp = hex_buffer_get_data (doc->buffer, pos, find_data->len);

		if (find_data->flags & HEX_SEARCH_IGNORE_CASE)
		{
			retval = g_ascii_strncasecmp (cp, find_data->what, find_data->len);
		}
		else
		{
			retval = memcmp (cp, find_data->what, find_data->len);
		}

		if (retval == 0)
			find_data->found_len = find_data->len;
	}
out:
	g_clear_error (&local_error);
	g_free (cp);
	return retval;
}

/**
 * hex_document_compare_data:
 * @doc: a [class@Hex.Document] object
 * @what: (array length=len) (element-type gint8): a pointer to the data to
 *   compare to data within the #HexDocument
 * @pos: offset position of the #HexDocument data to compare with @what
 * @len: size of the #HexDocument data to compare with @what, in bytes
 *
 * Returns: 0 if the comparison is an exact match; otherwise, a non-zero
 *   value comparable to strcmp().
 */
int
hex_document_compare_data (HexDocument *doc,
		const char *what, gint64 pos, size_t len)
{
	int retval;
	HexDocumentFindData *find_data = hex_document_find_data_new ();

	find_data->what = what;
	find_data->len = len;
	find_data->flags = HEX_SEARCH_NONE;

	retval = hex_document_compare_data_full (doc, find_data, pos);
	g_free (find_data);

	return retval;
}

/**
 * hex_document_find_forward_full:
 * @find_data: a #HexDocumentFindData structure
 *
 * Full version of [method@Hex.Document.find_forward] which allows for
 * more flexibility than the above, which is only for a byte-by-byte exact
 * match. However, it is less convenient to call since the caller must
 * create and and free a #HexDocumentFindData structure manually.
 *
 * This method will block. For a non-blocking version, use
 * [method@Hex.Document.find_forward_async].
 *
 * Returns: %TRUE if the search string contained in `find_data` was found by
 *   the requested operation; %FALSE otherwise.
 *
 * Since: 4.2
 */
gboolean
hex_document_find_forward_full (HexDocument *doc,
		HexDocumentFindData *find_data)
{
	gint64 pos;
	gint64 payload = hex_buffer_get_payload_size (
			hex_document_get_buffer (doc));

	g_return_val_if_fail (find_data != NULL, FALSE);

	pos = find_data->start;
	while (pos < payload)
	{
		if (hex_document_compare_data_full (doc, find_data, pos) == 0)
		{
			find_data->offset = pos;
			return TRUE;
		}
		pos++;
	}

	return FALSE;
}

/**
 * hex_document_find_forward:
 * @doc: a [class@Hex.Document] object
 * @start: starting offset byte of the payload to commence the search
 * @what: (array length=len) (element-type gint8): a pointer to the data to
 *   search within the #HexDocument
 * @len: length in bytes of the data to be searched for
 * @offset: (out): offset of the found string, if the method returns %TRUE
 * 
 * Find a string forwards in a #HexDocument.
 *
 * This method will block. For a non-blocking version, use
 * [method@Hex.Document.find_forward_async], which is also recommended
 * for GUI operations, as it, unlike this method, allows for easy passing-in
 * of found/not-found strings to be passed back to the interface.
 *
 * Returns: %TRUE if @what was found by the requested operation; %FALSE
 *   otherwise.
 */
gboolean
hex_document_find_forward (HexDocument *doc, gint64 start, const char *what,
						  size_t len, gint64 *offset)
{
	gboolean retval;
	HexDocumentFindData *find_data = hex_document_find_data_new ();

	find_data->start = start;
	find_data->what = what;
	find_data->len = len;
	find_data->flags = HEX_SEARCH_NONE;

	retval = hex_document_find_forward_full (doc, find_data);
	*offset = find_data->offset;

	g_free (find_data);

	return retval;
}

/**
 * hex_document_find_finish:
 * @doc: a [class@Hex.Document] object
 * @result: result of the task
 * 
 * Obtain the result of a completed asynchronous find operation (forwards or
 * backwards).
 *
 * Returns: a pointer to a [struct@Hex.DocumentFindData] structure, or %NULL
 */
HexDocumentFindData *
hex_document_find_finish (HexDocument *doc,
		GAsyncResult *result)
{
	g_return_val_if_fail (g_task_is_valid (result, G_OBJECT(doc)), FALSE);

	return g_task_propagate_pointer (G_TASK(result), NULL);
}

#define FIND_FULL_THREAD_TEMPLATE(FUNC_NAME, FUNC_TO_CALL) \
static void \
FUNC_NAME (GTask *task, \
		gpointer source_object, \
		gpointer task_data, \
		GCancellable *cancellable) \
{ \
	HexDocument *doc = HEX_DOCUMENT (source_object); \
	HexDocumentFindData *find_data = task_data; \
 \
	g_return_if_fail (find_data); \
 \
	find_data->found = FUNC_TO_CALL (doc, find_data); \
 \
	g_task_return_pointer (task, find_data, g_free); \
}

FIND_FULL_THREAD_TEMPLATE(hex_document_find_forward_full_thread,
		hex_document_find_forward_full)

static void
hex_document_find_forward_thread (GTask *task,
		gpointer source_object,
		gpointer task_data,
		GCancellable *cancellable)
{
	HexDocument *doc = HEX_DOCUMENT (source_object);
	HexDocumentFindData *find_data = task_data;

	find_data->found = hex_document_find_forward (doc,
			find_data->start, find_data->what,
			find_data->len, &find_data->offset);

	g_task_return_pointer (task, find_data, g_free);
}

#define FIND_FULL_ASYNC_TEMPLATE(FUNC_NAME, FUNC_TO_CALL) \
void \
FUNC_NAME (HexDocument *doc, \
		HexDocumentFindData *find_data, \
		GCancellable *cancellable, \
		GAsyncReadyCallback callback, \
		gpointer user_data) \
{ \
	GTask *task; \
 \
	task = g_task_new (doc, cancellable, callback, user_data); \
	g_task_set_return_on_cancel (task, TRUE); \
	g_task_set_task_data (task, find_data, g_free); \
	g_task_run_in_thread (task, FUNC_TO_CALL); \
}

/**
 * hex_document_find_forward_full_async:
 * @find_data: a #HexDocumentFindData structure
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): function to be called when the operation is
 *   complete
 *
 * Non-blocking version of [method@Hex.Document.find_forward_full].
 *
 * Since: 4.2
 */
FIND_FULL_ASYNC_TEMPLATE(hex_document_find_forward_full_async, 
		hex_document_find_forward_full_thread)

/**
 * hex_document_find_forward_async:
 * @doc: a [class@Hex.Document] object
 * @start: starting offset byte of the payload to commence the search
 * @what: (array length=len) (element-type gint8): a pointer to the data to
 *   search within the #HexDocument
 * @len: length in bytes of the data to be searched for
 * @offset: (out): offset of the found string, if the method returns %TRUE
 * @found_msg: message intended to be displayed by the client if the string
 *   is found
 * @not_found_msg: message intended to be displayed by the client if the string
 *   is not found
 * @callback: (scope async): function to be called when the operation is
 *   complete
 * 
 * Non-blocking version of [method@Hex.Document.find_forward]. This is the
 * function that should generally be used by a GUI client to find a string
 * forwards in a #HexDocument.
 */

#define FIND_ASYNC_TEMPLATE(FUNC_NAME, FUNC_TO_CALL) \
void \
FUNC_NAME (HexDocument *doc, \
		gint64 start, \
		const char *what, \
		size_t len, \
		gint64 *offset, \
		const char *found_msg, \
		const char *not_found_msg, \
		GCancellable *cancellable, \
		GAsyncReadyCallback callback, \
		gpointer user_data) \
{ \
	GTask *task; \
	HexDocumentFindData *find_data = hex_document_find_data_new (); \
 \
	find_data->start = start; \
	find_data->what = what; \
	find_data->len = len; \
	find_data->found_msg = found_msg; \
	find_data->not_found_msg = not_found_msg; \
 \
	task = g_task_new (doc, cancellable, callback, user_data); \
	g_task_set_return_on_cancel (task, TRUE); \
	g_task_set_task_data (task, find_data, g_free); \
	g_task_run_in_thread (task, FUNC_TO_CALL); \
	g_object_unref (task);	/* _run_in_thread takes a ref */ \
}

FIND_ASYNC_TEMPLATE(hex_document_find_forward_async,
		hex_document_find_forward_thread)

/**
 * hex_document_find_backward_full:
 * @find_data: a #HexDocumentFindData structure
 *
 * Full version of [method@Hex.Document.find_backward] which allows for
 * more flexibility than the above, which is only for a byte-by-byte exact
 * match. However, it is less convenient to call since the caller must
 * create and and free a #HexDocumentFindData structure manually.
 *
 * This method will block. For a non-blocking version, use
 * [method@Hex.Document.find_backward_full_async].
 *
 * Returns: %TRUE if the search string contained in `find_data` was found by
 *   the requested operation; %FALSE otherwise.
 *
 * Since: 4.2
 */
gboolean
hex_document_find_backward_full (HexDocument *doc,
		HexDocumentFindData *find_data)
{
	gint64 pos = find_data->start;
	
	if (pos == 0)
		return FALSE;

	do {
		pos--;
		if (hex_document_compare_data_full (doc, find_data, pos) == 0) {
			find_data->offset = pos;
			return TRUE;
		}
	} while (pos > 0);

	return FALSE;
}

/**
 * hex_document_find_backward:
 * @doc: a [class@Hex.Document] object
 * @start: starting offset byte of the payload to commence the search
 * @what: (array length=len) (element-type gint8): a pointer to the data to
 *   search within the #HexDocument
 * @len: length in bytes of the data to be searched for
 * @offset: (out): offset of the found string, if the method returns %TRUE
 * 
 * Find a string backwards in a #HexDocument.
 *
 * This method will block. For a non-blocking version, use
 * [method@Hex.Document.find_backward_async], which is also recommended
 * for GUI operations, as it, unlike this method, allows for easy passing-in
 * of found/not-found strings to be passed back to the interface.
 *
 * Returns: %TRUE if @what was found by the requested operation; %FALSE
 *   otherwise.
 */
gboolean
hex_document_find_backward (HexDocument *doc, gint64 start, const char *what,
						   size_t len, gint64 *offset)
{
	gboolean retval;
	HexDocumentFindData *find_data = hex_document_find_data_new ();

	find_data->start = start;
	find_data->what = what;
	find_data->len = len;
	find_data->flags = HEX_SEARCH_NONE;

	retval = hex_document_find_backward_full (doc, find_data);
	*offset = find_data->offset;

	g_free (find_data);

	return retval;
}

FIND_FULL_THREAD_TEMPLATE(hex_document_find_backward_full_thread,
		hex_document_find_backward_full)

static void
hex_document_find_backward_thread (GTask *task,
		gpointer source_object,
		gpointer task_data,
		GCancellable *cancellable)
{
	HexDocument *doc = HEX_DOCUMENT (source_object);
	HexDocumentFindData *find_data = task_data;

	find_data->found = hex_document_find_backward (doc,
			find_data->start, find_data->what,
			find_data->len, &find_data->offset);

	g_task_return_pointer (task, find_data, g_free);
}

/**
 * hex_document_find_backward_full_async:
 * @find_data: a #HexDocumentFindData structure
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): function to be called when the operation is
 *   complete
 *
 * Non-blocking version of [method@Hex.Document.find_backward_full].
 *
 * Since: 4.2
 */
FIND_FULL_ASYNC_TEMPLATE(hex_document_find_backward_full_async,
		hex_document_find_backward_full_thread)

/**
 * hex_document_find_backward_async:
 * @doc: a [class@Hex.Document] object
 * @start: starting offset byte of the payload to commence the search
 * @what: (array length=len) (element-type gint8): a pointer to the data to
 *   search within the #HexDocument
 * @len: length in bytes of the data to be searched for
 * @offset: (out): offset of the found string, if the method returns %TRUE
 * @found_msg: message intended to be displayed by the client if the string
 *   is found
 * @not_found_msg: message intended to be displayed by the client if the string
 *   is not found
 * @callback: (scope async): function to be called when the operation is
 *   complete
 *
 * Non-blocking version of [method@Hex.Document.find_backward]. This is the
 * function that should generally be used by a GUI client to find a string
 * backwards in a #HexDocument.
 */
FIND_ASYNC_TEMPLATE(hex_document_find_backward_async,
		hex_document_find_backward_thread)

/**
 * hex_document_undo:
 * @doc: a [class@Hex.Document] object
 *
 * Perform an undo operation.
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
gboolean
hex_document_undo (HexDocument *doc)
{
	if(doc->undo_top == NULL)
		return FALSE;

	g_signal_emit(G_OBJECT(doc), hex_signals[UNDO], 0);

	return TRUE;
}

static void
hex_document_real_undo (HexDocument *doc)
{
	HexChangeData *cd;
	size_t len;
	char *rep_data;
	char c_val;

	cd = doc->undo_top->data;

	switch(cd->type) {

	case HEX_CHANGE_BYTE:
	{
		gint64 payload = hex_buffer_get_payload_size (
				hex_document_get_buffer (doc));

		if (cd->end < payload)
		{
			c_val = hex_buffer_get_byte (doc->buffer, cd->start);
			if(cd->rep_len > 0)
				hex_document_set_byte(doc, cd->v_byte, cd->start, FALSE, FALSE);
			else if(cd->rep_len == 0)
				hex_document_delete_data(doc, cd->start, 1, FALSE);
			else
				hex_document_set_byte(doc, cd->v_byte, cd->start, TRUE, FALSE);
			cd->v_byte = c_val;
		}
	}
		break;

	case HEX_CHANGE_STRING:
		len = cd->end - cd->start + 1;
		rep_data = hex_buffer_get_data (doc->buffer, cd->start, len);
		hex_document_set_data (doc, cd->start, cd->rep_len, len, cd->v_string, FALSE);
		g_free (cd->v_string);
		cd->end = cd->start + cd->rep_len - 1;
		cd->rep_len = len;
		cd->v_string = rep_data;
		break;
	}	/* switch */

	hex_document_changed(doc, cd, FALSE);

	undo_stack_descend(doc);
}

/**
 * hex_document_redo:
 * @doc: a [class@Hex.Document] object
 *
 * Perform a redo operation.
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
gboolean 
hex_document_redo (HexDocument *doc)
{
	if(doc->undo_stack == NULL || doc->undo_top == doc->undo_stack)
		return FALSE;

	g_signal_emit(G_OBJECT(doc), hex_signals[REDO], 0);

	return TRUE;
}

static void
hex_document_real_redo(HexDocument *doc)
{
	HexChangeData *cd;
	int len;
	char *rep_data;
	char c_val;

	undo_stack_ascend(doc);

	cd = (HexChangeData *)doc->undo_top->data;

	switch(cd->type) {

	case HEX_CHANGE_BYTE:
	{
		gint64 payload = hex_buffer_get_payload_size (
				hex_document_get_buffer (doc));

		if (cd->end <= payload)
		{
			c_val = hex_buffer_get_byte (doc->buffer, cd->start);
			if(cd->rep_len > 0)
				hex_document_set_byte(doc, cd->v_byte, cd->start, FALSE, FALSE);
			else if(cd->rep_len == 0)
				hex_document_set_byte(doc, cd->v_byte, cd->start, cd->insert, FALSE);
			else
				hex_document_set_byte(doc, cd->v_byte, cd->start, TRUE, FALSE);
			cd->v_byte = c_val;
		}
	}
		break;

	case HEX_CHANGE_STRING:
		len = cd->end - cd->start + 1;
		rep_data = hex_buffer_get_data (doc->buffer, cd->start, len);
		hex_document_set_data (doc, cd->start, cd->rep_len, len, cd->v_string, FALSE);
		g_free (cd->v_string);
		cd->end = cd->start + cd->rep_len - 1;
		cd->rep_len = len;
		cd->v_string = rep_data;
		break;
	}

	hex_document_changed(doc, cd, FALSE);
}

/**
 * hex_document_can_undo:
 * @doc: a [class@Hex.Document] object
 *
 * Determine whether an undo operation is possible.
 *
 * Returns: %TRUE if an undo operation is possible; %FALSE otherwise
 */
gboolean
hex_document_can_undo (HexDocument *doc)
{
	if (! doc->undo_max)
		return FALSE;
	else if (doc->undo_top)
		return TRUE;
	else
		return FALSE;
}

/**
 * hex_document_can_redo:
 * @doc: a [class@Hex.Document] object
 *
 * Determine whether a redo operation is possible.
 *
 * Returns: %TRUE if a redo operation is possible; %FALSE otherwise
 */
gboolean
hex_document_can_redo (HexDocument *doc)
{
	if (! doc->undo_stack)
		return FALSE;
	else if (doc->undo_stack != doc->undo_top)
		return TRUE;
	else
		return FALSE;
}

/**
 * hex_document_get_undo_data:
 * @doc: a [class@Hex.Document] object
 *
 * Get the undo data at the top of the undo stack of a #HexDocument, if any.
 *
 * Returns: (transfer none): a pointer to the [struct@Hex.ChangeData]
 *   structure at the top of the undo stack, or %NULL
 */
HexChangeData *
hex_document_get_undo_data (HexDocument *doc)
{
	return doc->undo_top->data;
}

/**
 * hex_document_get_buffer:
 * @doc: a [class@Hex.Document] object
 *
 * Get the [iface@Hex.Buffer] connected with the #HexDocument.
 *
 * Returns: (transfer none): a pointer to the [iface@Hex.Buffer] connected
 * with the #HexDocument, or %NULL if no such interface is so connected.
 */
HexBuffer *
hex_document_get_buffer (HexDocument *doc)
{
	g_return_val_if_fail (HEX_IS_DOCUMENT (doc), NULL);

	return doc->buffer;
}

/**
 * hex_document_get_file:
 * @doc: a [class@Hex.Document] object
 *
 * Get the #GFile connected with the #HexDocument.
 *
 * Returns: (transfer none): the #GFile connected with the #HexDocument,
 * or %NULL if no such object is so connected.
 */
GFile *
hex_document_get_file (HexDocument *doc)
{
	g_return_val_if_fail (HEX_IS_DOCUMENT (doc), NULL);

	return doc->file;
}

/**
 * hex_document_set_buffer:
 * @doc: a [class@Hex.Document] object
 * @buf: (transfer full): [iface@Hex.Buffer]
 *
 * Set the [iface@Hex.Buffer] connected with the #HexDocument.
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 *
 * Since: 4.2
 */
gboolean
hex_document_set_buffer (HexDocument *doc, HexBuffer *buf)
{
	g_return_val_if_fail (HEX_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (HEX_IS_BUFFER (buf), FALSE);

	g_clear_object (&doc->buffer);
	doc->buffer = buf;

	g_object_notify_by_pspec (G_OBJECT(doc), properties[BUFFER]);
	return TRUE;
}
