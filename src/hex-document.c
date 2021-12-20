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

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#include <glib/gi18n.h>

#include <config.h>

/* FIXME / TODO - Allow for swappability. Hardcoding for now for testing
 * purposes. Keep this include below config.h, as it is (un)def'd there.
 */
#ifdef EXPERIMENTAL_MMAP
#  include "hex-buffer-mmap.h"
#else
#  include "hex-buffer-malloc.h"
#endif

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

enum {
	DOCUMENT_CHANGED,
	UNDO,
	REDO,
	UNDO_STACK_FORGET,
	FILE_NAME_CHANGED,
	FILE_SAVED,
	FILE_LOADED,
	LAST_SIGNAL
};

static guint hex_signals[LAST_SIGNAL];


/* GOBJECT DEFINITION */

struct _HexDocument
{
	GObject object;

	GFile *file;
	gboolean changed;
	HexBuffer *buffer;

	GList *undo_stack; /* stack base */
	GList *undo_top;   /* top of the stack (for redo) */
	int undo_depth;  /* number of els on stack */
	int undo_max;    /* max undo depth */
};

G_DEFINE_TYPE (HexDocument, hex_document, G_TYPE_OBJECT)

/* ---- */

static void
free_stack(GList *stack)
{
	HexChangeData *cd;

	while(stack) {
		cd = (HexChangeData *)stack->data;
		if(cd->v_string)
			g_free(cd->v_string);
		stack = g_list_remove(stack, cd);
		g_free(cd);
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
	
	if (doc->file)
		g_object_unref (doc->file);

	g_clear_object (&doc->buffer);

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
	
	gobject_class->finalize = hex_document_finalize;
	gobject_class->dispose= hex_document_dispose;
	
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

	hex_signals[FILE_SAVED] =
		g_signal_new_class_handler ("file-saved",
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
#ifdef EXPERIMENTAL_MMAP
	doc->buffer = HEX_BUFFER(hex_buffer_mmap_new (NULL));
#else
	doc->buffer = HEX_BUFFER(hex_buffer_malloc_new (NULL));
#endif
	doc->undo_max = DEFAULT_UNDO_DEPTH;
}

/*-------- public API starts here --------*/


HexDocument *
hex_document_new (void)
{
	return g_object_new (HEX_TYPE_DOCUMENT, NULL);
}

gboolean
hex_document_set_file (HexDocument *doc, GFile *file)
{
	gboolean had_prev_file = FALSE;

	if (! hex_buffer_set_file (doc->buffer, file)) {
		g_debug ("%s: Invalid file", __func__);
		return FALSE;
	}

	if (G_IS_FILE (doc->file)) {
		had_prev_file = TRUE;
		g_object_unref (doc->file);
	}

	doc->file = g_object_ref (file);

	if (had_prev_file)
		g_signal_emit (G_OBJECT(doc), hex_signals[FILE_NAME_CHANGED], 0);

	hex_document_read (doc);

	return TRUE;
}

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

void
hex_document_delete_data (HexDocument *doc,
		gint64 offset, size_t len, gboolean undoable)
{
	hex_document_set_data (doc, offset, 0, len, NULL, undoable);
}

void
document_ready_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	gboolean success;
	GError *local_error = NULL;
	HexBuffer *buf = HEX_BUFFER(source_object);
	HexDocument *doc = HEX_DOCUMENT(user_data);
	static HexChangeData change_data;
	gint64 payload;

	success = hex_buffer_read_finish (buf, res, &local_error);
	g_debug ("%s: DONE -- result: %d", __func__, success);

	if (local_error)
		g_warning (local_error->message);

	/* Initialize data for new doc */

	undo_stack_free(doc);

	payload = hex_buffer_get_payload_size (hex_document_get_buffer (doc));

	change_data.start = 0;
	change_data.end = payload - 1;

	doc->changed = FALSE;
	hex_document_changed (doc, &change_data, FALSE);
	g_signal_emit (G_OBJECT(doc), hex_signals[FILE_LOADED], 0);
}

void
hex_document_read (HexDocument *doc)
{
	static HexChangeData change_data;
	gint64 payload;

	g_return_if_fail (G_IS_FILE (doc->file));

	/* Read the actual file on disk into the buffer */
	hex_buffer_read_async (doc->buffer, NULL, document_ready_cb, doc);
}

gboolean
hex_document_write_to_file (HexDocument *doc, GFile *file)
{
	return hex_buffer_write_to_file (doc->buffer, file);
}

gboolean
hex_document_write (HexDocument *doc)
{
	gboolean ret = FALSE;
	char *path = NULL;

	g_return_val_if_fail (G_IS_FILE (doc->file), FALSE);

	path = g_file_get_path (doc->file);
	if (! path)
		goto out;

	ret = hex_buffer_write_to_file (doc->buffer, doc->file);
	if (ret)
	{
		doc->changed = FALSE;
		g_signal_emit (G_OBJECT(doc), hex_signals[FILE_SAVED], 0);
	}

out:
	g_free (path);
	return ret;
}

void
hex_document_changed(HexDocument *doc, gpointer change_data,
					 gboolean push_undo)
{
	g_signal_emit(G_OBJECT(doc), hex_signals[DOCUMENT_CHANGED], 0,
				  change_data, push_undo);
}

gboolean
hex_document_has_changed(HexDocument *doc)
{
	return doc->changed;
}

void
hex_document_set_max_undo (HexDocument *doc, int max_undo)
{
	if(doc->undo_max != max_undo) {
		if(doc->undo_max > max_undo)
			undo_stack_free(doc);
		doc->undo_max = max_undo;
	}
}

gboolean
hex_document_export_html (HexDocument *doc, char *html_path, char *base_name,
						 gint64 start, gint64 end, guint cpl, guint lpp,
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

int
hex_document_compare_data (HexDocument *doc,
		char *what, gint64 pos, size_t len)
{
	char c;

	g_return_val_if_fail (what, 0);

	for (size_t i = 0; i < len; i++, what++)
	{
		c = hex_buffer_get_byte (doc->buffer, pos + i);

		if (c != *what)
			return (c - *what);
	}
	
	return 0;
}

gboolean
hex_document_find_forward (HexDocument *doc, gint64 start, char *what,
						  size_t len, gint64 *found)
{
	gint64 pos;
	gint64 payload = hex_buffer_get_payload_size (
			hex_document_get_buffer (doc));

	pos = start;
	while (pos < payload)
	{
		if (hex_document_compare_data (doc, what, pos, len) == 0)
		{
			*found = pos;
			return TRUE;
		}
		pos++;
	}

	return FALSE;
}

gboolean
hex_document_find_backward (HexDocument *doc, gint64 start, char *what,
						   size_t len, gint64 *found)
{
	gint64 pos = start;
	
	if (pos == 0)
		return FALSE;

	do {
		pos--;
		if (hex_document_compare_data (doc, what, pos, len) == 0) {
			*found = pos;
			return TRUE;
		}
	} while (pos > 0);

	return FALSE;
}

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
		g_free(cd->v_string);
		cd->end = cd->start + cd->rep_len - 1;
		cd->rep_len = len;
		cd->v_string = rep_data;
		break;
	}	/* switch */

	hex_document_changed(doc, cd, FALSE);

	undo_stack_descend(doc);
}

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
#if 0
				hex_document_delete_data(doc, cd->start, 1, FALSE);
#endif
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
		g_free(cd->v_string);
		cd->end = cd->start + cd->rep_len - 1;
		cd->rep_len = len;
		cd->v_string = rep_data;
		break;
	}

	hex_document_changed(doc, cd, FALSE);
}

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

HexChangeData *
hex_document_get_undo_data (HexDocument *doc)
{
	return doc->undo_top->data;
}

HexBuffer *
hex_document_get_buffer (HexDocument *doc)
{
	return doc->buffer;
}

GFile *
hex_document_get_file (HexDocument *doc)
{
	return doc->file;
}
