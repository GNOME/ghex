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

#include <config.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include <gtkhex.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

static void hex_document_class_init     (HexDocumentClass *);
static void hex_document_init           (HexDocument *doc);
static void hex_document_finalize       (GObject *obj);
static void hex_document_real_changed   (HexDocument *doc,
										 gpointer change_data,
										 gboolean undoable);
static void hex_document_real_redo      (HexDocument *doc);
static void hex_document_real_undo      (HexDocument *doc);
static void move_gap_to                 (HexDocument *doc,
										 int offset,
								  	     int min_size);
static void free_stack                  (GList *stack);
static gint undo_stack_push             (HexDocument *doc,
									     HexChangeData *change_data);
static void undo_stack_descend          (HexDocument *doc);
static void undo_stack_ascend           (HexDocument *doc);
static void undo_stack_free             (HexDocument *doc);
static gboolean get_document_attributes (HexDocument *doc);

#define DEFAULT_UNDO_DEPTH 1024

enum {
	DOCUMENT_CHANGED,
	UNDO,
	REDO,
	UNDO_STACK_FORGET,
	FILE_NAME_CHANGED,
	FILE_SAVED,
	LAST_SIGNAL
};

static gint hex_signals[LAST_SIGNAL];

static GObjectClass *parent_class = NULL;

static GList *doc_list = NULL;

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

#ifdef ENABLE_DEBUG
	g_message("undo_stack_push");
#endif

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

#ifdef ENABLE_DEBUG
		g_message("depth at: %d", doc->undo_depth);
#endif

		if(doc->undo_depth > doc->undo_max) {
			GList *last;

#ifdef ENABLE_DEBUG
			g_message("forget last undo");
#endif

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
#ifdef ENABLE_DEBUG
	g_message("undo_stack_descend");
#endif

	if(doc->undo_top == NULL)
		return;

	doc->undo_top = doc->undo_top->next;
	doc->undo_depth--;

#ifdef ENABLE_DEBUG
	g_message("undo depth at: %d", doc->undo_depth);
#endif
}

static void
undo_stack_ascend(HexDocument *doc)
{
#ifdef ENABLE_DEBUG
	g_message("undo_stack_ascend");
#endif

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
#ifdef ENABLE_DEBUG
	g_message("undo_stack_free");
#endif

	if(doc->undo_stack == NULL)
		return;

	free_stack(doc->undo_stack);
	doc->undo_stack = NULL;
	doc->undo_top = NULL;
	doc->undo_depth = 0;

	g_signal_emit(G_OBJECT(doc), hex_signals[UNDO_STACK_FORGET], 0);
}

static gboolean
get_document_attributes(HexDocument *doc)
{
	static struct stat stats;

	if(doc->file_name == NULL)
		return FALSE;

	if(!stat(doc->file_name, &stats) &&
	   S_ISREG(stats.st_mode)) {
		doc->file_size = stats.st_size;

		return TRUE;
	}

	return FALSE;
}


static void
move_gap_to(HexDocument *doc, int offset, int min_size)
{
	char *tmp, *buf_ptr, *tmp_ptr;

	if(doc->gap_size < min_size) {
		tmp = g_malloc(doc->file_size);
		buf_ptr = doc->buffer;
		tmp_ptr = tmp;
		while(buf_ptr < doc->gap_pos)
			*tmp_ptr++ = *buf_ptr++;
		buf_ptr += doc->gap_size;
		while(buf_ptr < doc->buffer + doc->buffer_size)
			*tmp_ptr++ = *buf_ptr++;

		doc->gap_size = MAX(min_size, 32);
		doc->buffer_size = doc->file_size + doc->gap_size;
		doc->buffer = g_realloc(doc->buffer, doc->buffer_size);
		doc->gap_pos = doc->buffer + offset;

		buf_ptr = doc->buffer;
		tmp_ptr = tmp;
		
		while(buf_ptr < doc->gap_pos)
			*buf_ptr++ = *tmp_ptr++;
		buf_ptr += doc->gap_size;
		while(buf_ptr < doc->buffer + doc->buffer_size)
			*buf_ptr++ = *tmp_ptr++;

		g_free(tmp);
	}
	else {
		if(doc->buffer + offset < doc->gap_pos) {
			buf_ptr = doc->gap_pos + doc->gap_size - 1;
			while(doc->gap_pos > doc->buffer + offset)
				*buf_ptr-- = *(--doc->gap_pos);
		}
		else if(doc->buffer + offset > doc->gap_pos) {
			buf_ptr = doc->gap_pos + doc->gap_size;
			while(doc->gap_pos < doc->buffer + offset)
				*doc->gap_pos++ = *buf_ptr++;
		}
	}
}

GtkWidget *
hex_document_add_view(HexDocument *doc)
{
	GtkWidget *new_view;
	
	new_view = gtk_hex_new(doc);

	g_object_ref (new_view);

	doc->views = g_list_append(doc->views, new_view);

	return new_view;
}

void
hex_document_remove_view(HexDocument *doc, GtkWidget *view)
{
	if(g_list_index(doc->views, view) == -1)
		return;

	doc->views = g_list_remove(doc->views, view);

	g_object_unref(view);
}

static void
hex_document_finalize(GObject *obj)
{
	HexDocument *hex;
	
	hex = HEX_DOCUMENT(obj);
	
	if(hex->buffer)
		g_free(hex->buffer);
	
	if(hex->file_name)
		g_free(hex->file_name);

	if(hex->path_end)
		g_free(hex->path_end);

	undo_stack_free(hex);

	while(hex->views)
		hex_document_remove_view(hex, (GtkWidget *)hex->views->data);

	doc_list = g_list_remove(doc_list, hex);

	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
hex_document_real_changed(HexDocument *doc, gpointer change_data,
						  gboolean push_undo)
{
	if(push_undo && doc->undo_max > 0)
		undo_stack_push(doc, change_data);
}

static void
hex_document_class_init (HexDocumentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	
	parent_class = g_type_class_peek_parent(klass);
	
	gobject_class->finalize = hex_document_finalize;
	
	klass->document_changed = hex_document_real_changed;
	klass->undo = hex_document_real_undo;
	klass->redo = hex_document_real_redo;
	klass->undo_stack_forget = NULL;
	klass->file_name_changed = NULL;

	hex_signals[DOCUMENT_CHANGED] = 
		g_signal_new ("document-changed",
					  G_TYPE_FROM_CLASS(gobject_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (HexDocumentClass, document_changed),
					  NULL,
					  NULL,
					  NULL,
					  G_TYPE_NONE,
					  2, G_TYPE_POINTER, G_TYPE_BOOLEAN);
	hex_signals[UNDO] = 
		g_signal_new ("undo",
					  G_TYPE_FROM_CLASS(gobject_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (HexDocumentClass, undo),
					  NULL,
					  NULL,
					  NULL,
					  G_TYPE_NONE, 0);
	hex_signals[REDO] = 
		g_signal_new ("redo",
					  G_TYPE_FROM_CLASS(gobject_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (HexDocumentClass, redo),
					  NULL,
					  NULL,
					  NULL,
					  G_TYPE_NONE, 0);
	hex_signals[UNDO_STACK_FORGET] = 
		g_signal_new ("undo_stack_forget",
					  G_TYPE_FROM_CLASS(gobject_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (HexDocumentClass, undo_stack_forget),
					  NULL,
					  NULL,
					  NULL,
					  G_TYPE_NONE, 0);

	hex_signals[FILE_NAME_CHANGED] = 
		g_signal_new ("file-name-changed",
					  G_TYPE_FROM_CLASS(gobject_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (HexDocumentClass, file_name_changed),
					  NULL,
					  NULL,
					  NULL,
					  G_TYPE_NONE, 0);

	hex_signals[FILE_SAVED] =
		g_signal_new ("file-saved",
					  G_TYPE_FROM_CLASS(gobject_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (HexDocumentClass, file_saved),
					  NULL,
					  NULL,
					  NULL,
					  G_TYPE_NONE, 0);
}

static void
hex_document_init (HexDocument *doc)
{
	doc->buffer = NULL;
	doc->buffer_size = 0;
	doc->file_size = 0;
	doc->gap_pos = NULL;
	doc->gap_size = 0;
	doc->changed = FALSE;
	doc->undo_stack = NULL;
	doc->undo_top = NULL;
	doc->undo_depth = 0;
	doc->undo_max = DEFAULT_UNDO_DEPTH;
}

GType
hex_document_get_type (void)
{
	static GType doc_type = 0;
	
	if (!doc_type) {
		static const GTypeInfo doc_info = {
			sizeof (HexDocumentClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) hex_document_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (HexDocument),
			0,
			(GInstanceInitFunc) hex_document_init
		};
	
		doc_type = g_type_register_static (G_TYPE_OBJECT,
				"HexDocument",
				&doc_info,
				0);	
	}

	return doc_type;
}


/*-------- public API starts here --------*/


HexDocument *
hex_document_new()
{
	HexDocument *doc;

	doc = HEX_DOCUMENT (g_object_new (hex_document_get_type(), NULL));
	g_return_val_if_fail (doc != NULL, NULL);

	doc->file_name = NULL;

	doc->gap_size = 100;
	doc->file_size = 0;
	doc->buffer_size = doc->file_size + doc->gap_size;
	doc->gap_pos = doc->buffer = g_malloc(doc->buffer_size);

	doc->path_end = g_strdup(_("New document"));

	doc_list = g_list_append(doc_list, doc);
	return doc;
}

HexDocument *
hex_document_new_from_file(const gchar *name)
{
	HexDocument *doc;
	gchar *path_end;

	doc = HEX_DOCUMENT (g_object_new (hex_document_get_type(), NULL));
	g_return_val_if_fail (doc != NULL, NULL);

	doc->file_name = g_strdup(name);
	if(get_document_attributes(doc)) {
		doc->gap_size = 100;
		doc->buffer_size = doc->file_size + doc->gap_size;
		doc->buffer = g_malloc(doc->buffer_size);

		/* find the start of the filename without path */
		path_end = g_path_get_basename (doc->file_name);
		doc->path_end = g_filename_to_utf8 (path_end, -1, NULL, NULL, NULL);
		g_free (path_end);

		if(hex_document_read(doc)) {
			doc_list = g_list_append(doc_list, doc);
			return doc;
		}
	}
	g_object_unref(G_OBJECT(doc));
	
	return NULL;
}

char
hex_document_get_byte(HexDocument *doc, int offset)
{
	if(offset < doc->file_size) {
		if(doc->gap_pos <= doc->buffer + offset)
			offset += doc->gap_size;
		return doc->buffer[offset];
	}
	else
		return 0;
}

char *
hex_document_get_data(HexDocument *doc, int offset, int len)
{
	char *ptr, *data, *dptr;
	int i;

	ptr = doc->buffer + offset;

	if (ptr >= doc->gap_pos)
		ptr += doc->gap_size;

	dptr = data = g_malloc(len);

	for (i = 0; i < len; ++i) {
		if (ptr >= doc->gap_pos && ptr < doc->gap_pos + doc->gap_size)
			ptr += doc->gap_size;

		*dptr++ = *ptr++;
	}

	return data;
}

void
hex_document_set_nibble(HexDocument *doc, char val, int offset,
						gboolean lower_nibble, gboolean insert,
						gboolean undoable)
{
	static HexChangeData change_data;

	if(offset <= doc->file_size) {
		if(!insert && offset == doc->file_size)
			return;

		doc->changed = TRUE;
		change_data.start = offset;
		change_data.end = offset;
		change_data.v_string = NULL;
		change_data.type = HEX_CHANGE_BYTE;
		change_data.lower_nibble = lower_nibble;
		change_data.insert = insert;
		if(!lower_nibble && insert) {
			move_gap_to(doc, offset, 1);
			doc->gap_size--;
			doc->gap_pos++;
			doc->file_size++;
			change_data.rep_len = 0;
			if(offset == doc->file_size)
				doc->buffer[offset] = 0;
		}
		else {
			if(doc->buffer + offset >= doc->gap_pos)
				offset += doc->gap_size;
			change_data.rep_len = 1;
		}

		change_data.v_byte = doc->buffer[offset];
		doc->buffer[offset] = (doc->buffer[offset] & (lower_nibble?0xF0:0x0F)) | (lower_nibble?val:(val << 4));

	 	hex_document_changed(doc, &change_data, undoable);
	}
}

void
hex_document_set_byte(HexDocument *doc, char val, int offset,
					  gboolean insert, gboolean undoable)
{
	static HexChangeData change_data;

	if(offset <= doc->file_size) {
		if(!insert && offset == doc->file_size)
			return;

		doc->changed = TRUE;
		change_data.start = offset;
		change_data.end = offset;
		change_data.rep_len = (insert?0:1);
		change_data.v_string = NULL;
		change_data.type = HEX_CHANGE_BYTE;
		change_data.lower_nibble = FALSE;
		change_data.insert = insert;
		if(insert) {
			move_gap_to(doc, offset, 1);
			doc->gap_size--;
			doc->gap_pos++;
			doc->file_size++;
		}
		else if(doc->buffer + offset >= doc->gap_pos)
			offset += doc->gap_size;
			
		change_data.v_byte = doc->buffer[offset];
		doc->buffer[offset] = val;

	 	hex_document_changed(doc, &change_data, undoable);
	}
}

void
hex_document_set_data (HexDocument *doc, int offset, int len,
					  int rep_len, char *data, gboolean undoable)
{
	int i;
	char *ptr;
	static HexChangeData change_data;

	if(offset <= doc->file_size) {
		if(doc->file_size - offset < rep_len)
			rep_len -= doc->file_size - offset;

		doc->changed = TRUE;
		
		change_data.v_string = g_realloc(change_data.v_string, rep_len);
		change_data.start = offset;
		change_data.end = change_data.start + len - 1;
		change_data.rep_len = rep_len;
		change_data.type = HEX_CHANGE_STRING;
		change_data.lower_nibble = FALSE;
		
		i = 0;
		ptr = &doc->buffer[offset];
		if(ptr >= doc->gap_pos)
			ptr += doc->gap_size;
		while(offset + i < doc->file_size && i < rep_len) {
			if(ptr >= doc->gap_pos && ptr < doc->gap_pos + doc->gap_size)
				ptr += doc->gap_size;
			change_data.v_string[i] = *ptr++;
			i++;
		}
		
		if(rep_len == len) {
			if(doc->buffer + offset >= doc->gap_pos)
				offset += doc->gap_size;
		}
		else {
			if(rep_len > len) {
				move_gap_to(doc, offset + rep_len, 1);
			}
			else if(rep_len < len) {
				move_gap_to(doc, offset + rep_len, len - rep_len);
			}
			doc->gap_pos -= (gint)rep_len - (gint)len;
			doc->gap_size += (gint)rep_len - (gint)len;
			doc->file_size += (gint)len - (gint)rep_len;
		}
		
		ptr = &doc->buffer[offset];
		i = 0;
		while(offset + i < doc->buffer_size && i < len) {
			*ptr++ = *data++;
			i++;
		}
		
		hex_document_changed(doc, &change_data, undoable);
	}
}

void
hex_document_delete_data(HexDocument *doc, guint offset, guint len, gboolean undoable)
{
	hex_document_set_data(doc, offset, 0, len, NULL, undoable);
}

gint
hex_document_read(HexDocument *doc)
{
	FILE *file;
	static HexChangeData change_data;
	int fread_as_int;

	if(doc->file_name == NULL)
		return FALSE;

	if(!get_document_attributes(doc))
		return FALSE;

	if((file = fopen(doc->file_name, "r")) == NULL)
		return FALSE;

	doc->gap_size = doc->buffer_size - doc->file_size;

	fread_as_int = fread(doc->buffer + doc->gap_size, 1, doc->file_size, file);
	if (fread_as_int != doc->file_size)
	{
		g_return_val_if_reached(FALSE);
	}

	doc->gap_pos = doc->buffer;
	fclose(file);
	undo_stack_free(doc);

	change_data.start = 0;
	change_data.end = doc->file_size - 1;
	doc->changed = FALSE;
	hex_document_changed(doc, &change_data, FALSE);

	return TRUE;
}

gint
hex_document_write_to_file(HexDocument *doc, FILE *file)
{
	int ret = TRUE;
	int exp_len;

	if(doc->gap_pos > doc->buffer) {
		exp_len = MIN(doc->file_size, doc->gap_pos - doc->buffer);
		ret = fwrite(doc->buffer, 1, exp_len, file);
		ret = (ret == exp_len) ? TRUE : FALSE;
	}
	if(doc->gap_pos < doc->buffer + doc->file_size) {
		exp_len = doc->file_size - (size_t)(doc->gap_pos - doc->buffer);
		ret = fwrite(doc->gap_pos + doc->gap_size, 1, exp_len, file);
		ret = (ret == exp_len)?TRUE:FALSE;
	}
	return ret;
}

gint
hex_document_write(HexDocument *doc)
{
	FILE *file;
	gint ret = FALSE;

	if(doc->file_name == NULL)
		return FALSE;

	if((file = fopen(doc->file_name, "wb")) != NULL) {
		ret = hex_document_write_to_file(doc, file);
		fclose(file);
		if(ret) {
			doc->changed = FALSE;
		}
	}
	g_signal_emit (G_OBJECT(doc), hex_signals[FILE_SAVED], 0);
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
hex_document_set_max_undo(HexDocument *doc, int max_undo)
{
	if(doc->undo_max != max_undo) {
		if(doc->undo_max > max_undo)
			undo_stack_free(doc);
		doc->undo_max = max_undo;
	}
}

static gboolean
ignore_dialog_cb(GtkDialog *dialog, gpointer user_data)
{
	/* unused, as this function just ignores user input. */
	(void)dialog, (void)user_data;

	return TRUE;
}

int
hex_document_export_html (HexDocument *doc, char *html_path, char *base_name,
						 int start, int end, int cpl, int lpp,
						 int cpw)
{
	GtkWidget *progress_dialog, *progress_bar;
	FILE *file;
	int page, line, pos, lines, pages, c;
	gchar *page_name, b;
	gint update_pages;
	gchar *progress_str;

	lines = (end - start)/cpl;
	if((end - start)%cpl != 0)
		lines++;
	pages = lines/lpp;
	if(lines%lpp != 0)
		pages++;
	update_pages = pages/1000 + 1;

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
			doc->file_name?doc->file_name:doc->path_end);
	fprintf(file, "<TR>\n<TD COLSPAN=\"3\">&nbsp;</TD>\n</TR>\n");
	for(page = 0; page < pages; page++) {
		fprintf(file, "<TR>\n<TD>\n<A HREF=\"%s%08d.html\"><PRE>", base_name, page);
		fprintf(file, _("Page"));
		fprintf(file, " %d</PRE></A>\n</TD>\n<TD>&nbsp;</TD>\n<TD VALIGN=\"CENTER\"><PRE>%08x -", page+1, page*cpl*lpp);
		fprintf(file, " %08x</PRE></TD>\n</TR>\n", MIN((page+1)*cpl*lpp-1, doc->file_size-1));
	}
	fprintf(file, "</TABLE>\n</CENTER>\n");
	fprintf(file, "<HR WIDTH=\"100%%\">");
	fprintf(file, _("Hex dump generated by"));
	fprintf(file, " <B>"LIBGTKHEX_RELEASE_STRING"</B>\n");
	fprintf(file, "</BODY>\n</HTML>\n");
	fclose(file);

	progress_dialog = gtk_dialog_new();
	gtk_window_set_resizable(GTK_WINDOW(progress_dialog), FALSE);
	gtk_window_set_modal(GTK_WINDOW(progress_dialog), TRUE);

	g_signal_connect(G_OBJECT(progress_dialog), "close",
					 G_CALLBACK(ignore_dialog_cb), NULL);
	gtk_window_set_title(GTK_WINDOW(progress_dialog),
						 _("Saving to HTML..."));

	progress_bar = gtk_progress_bar_new();
	gtk_widget_show(progress_bar);

	gtk_box_append (GTK_BOX (gtk_dialog_get_content_area(GTK_DIALOG (progress_dialog))),
			progress_bar);
	gtk_widget_show(progress_dialog);

	pos = start;
	g_object_ref(G_OBJECT(doc));
	for(page = 0; page < pages; page++) {
		if((page%update_pages) == 0) {
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar),
					(gdouble)page/(gdouble)pages);
			progress_str = g_strdup_printf("%d/%d", page, pages);
			gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar),
									  progress_str);
			g_free(progress_str);

			while (g_main_context_pending (NULL)) {	/* GMainContext - NULL == default */
				g_main_context_iteration (NULL,	/* " " */
						FALSE);		/* gboolean may_block */
			}
		}
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
		fprintf(file, "%s:", doc->path_end);
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
		for(line = 0; line < lpp && pos + line*cpl < doc->file_size; line++) {
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
				fprintf(file, "%02x", hex_document_get_byte(doc, pos + c));
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
				b = hex_document_get_byte(doc, pos + c);
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
	g_object_unref(G_OBJECT(doc));
	gtk_window_destroy(GTK_WINDOW (progress_dialog));

	return TRUE;
}

int
hex_document_compare_data(HexDocument *doc, char *s2, int pos, int len)
{
	char c1;

	for (int i = 0; i < len; i++, s2++)
	{
		c1 = hex_document_get_byte(doc, pos + i);

		if(c1 != (*s2))
			return (c1 - (*s2));
	}
	
	return 0;
}

int
hex_document_find_forward (HexDocument *doc, int start, char *what,
						  int len, int *found)
{
	int pos;
	
	pos = start;
	while (pos < doc->file_size)
	{
		if (hex_document_compare_data(doc, what, pos, len) == 0)
		{
			*found = pos;
			return TRUE;
		}
		pos++;
	}

	return FALSE;
}

gint
hex_document_find_backward(HexDocument *doc, guint start, char *what,
						   gint len, guint *found)
{
	guint pos;
	
	pos = start;

	if(pos == 0)
		return FALSE;

	do {
		pos--;
		if(hex_document_compare_data(doc, what, pos, len) == 0) {
			*found = pos;
			return TRUE;
		}
	} while(pos > 0);

	return FALSE;
}

gboolean
hex_document_undo(HexDocument *doc)
{
	if(doc->undo_top == NULL)
		return FALSE;

	g_signal_emit(G_OBJECT(doc), hex_signals[UNDO], 0);

	return TRUE;
}

static void
hex_document_real_undo(HexDocument *doc)
{
	HexChangeData *cd;
	int len;
	char *rep_data;
	char c_val;

	cd = doc->undo_top->data;

	switch(cd->type) {
	case HEX_CHANGE_BYTE:
		if(cd->start >= 0 && cd->end < doc->file_size) {
			c_val = hex_document_get_byte(doc, cd->start);
			if(cd->rep_len > 0)
				hex_document_set_byte(doc, cd->v_byte, cd->start, FALSE, FALSE);
			else if(cd->rep_len == 0)
				hex_document_delete_data(doc, cd->start, 1, FALSE);
			else
				hex_document_set_byte(doc, cd->v_byte, cd->start, TRUE, FALSE);
			cd->v_byte = c_val;
		}
		break;
	case HEX_CHANGE_STRING:
		len = cd->end - cd->start + 1;
		rep_data = hex_document_get_data(doc, cd->start, len);
		hex_document_set_data(doc, cd->start, cd->rep_len, len, cd->v_string, FALSE);
		g_free(cd->v_string);
		cd->end = cd->start + cd->rep_len - 1;
		cd->rep_len = len;
		cd->v_string = rep_data;
		break;
	}

	hex_document_changed(doc, cd, FALSE);

	undo_stack_descend(doc);
}

gboolean
hex_document_is_writable(HexDocument *doc)
{
	return (doc->file_name != NULL &&
			access(doc->file_name, W_OK) == 0);
}

gboolean 
hex_document_redo(HexDocument *doc)
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
		if(cd->start >= 0 && cd->end <= doc->file_size) {
			c_val = hex_document_get_byte(doc, cd->start);
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
		break;
	case HEX_CHANGE_STRING:
		len = cd->end - cd->start + 1;
		rep_data = hex_document_get_data(doc, cd->start, len);
		hex_document_set_data(doc, cd->start, cd->rep_len, len, cd->v_string, FALSE);
		g_free(cd->v_string);
		cd->end = cd->start + cd->rep_len - 1;
		cd->rep_len = len;
		cd->v_string = rep_data;
		break;
	}

	hex_document_changed(doc, cd, FALSE);
}

const GList *
hex_document_get_list()
{
	return doc_list;
}

gboolean
hex_document_change_file_name (HexDocument *doc, const char *new_file_name)
{
	char *new_path_end = NULL;

	if(doc->file_name)
		g_free(doc->file_name);
	if(doc->path_end)
		g_free(doc->path_end);

	doc->file_name = g_strdup(new_file_name);
	doc->changed = FALSE;

	new_path_end = g_path_get_basename (doc->file_name);
	doc->path_end = g_filename_to_utf8 (new_path_end, -1, NULL, NULL, NULL);

	if (new_path_end)
		g_free (new_path_end);

	if (doc->file_name && doc->path_end) {
		g_signal_emit (G_OBJECT(doc), hex_signals[FILE_NAME_CHANGED], 0);
		return TRUE;
	} else {
		return FALSE;
	}
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
