/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* hex-document.c - implementation of a hex-document MDI child

   Copyright (C) 1998, 1999, 2000 Free Software Foundation

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
   If not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Jaka Mocnik <jaka@gnu.org>
 */

#include <config.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gnome.h>

#include <hex-document.h>

#include "ghex.h"
#include "gtkhex.h"

#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

static void       hex_document_class_init        (HexDocumentClass *);
static void       hex_document_init              (HexDocument *doc);
static GtkWidget *hex_document_create_view       (GnomeMDIChild *child);
static void       hex_document_finalize          (GtkObject *obj);
static void       hex_document_real_changed      (HexDocument *doc, gpointer change_data, gboolean undoable);
static gchar     *hex_document_get_config_string (GnomeMDIChild *child);

static void move_gap_to (HexDocument *doc, guint offset, gint min_size);
static void free_stack(GList *stack);
static gint undo_stack_push(HexDocument *doc, HexChangeData *change_data);
static void undo_stack_descend(HexDocument *doc);
static void undo_stack_ascend(HexDocument *doc);
static void undo_stack_free(HexDocument *doc);
static gboolean get_document_attributes(HexDocument *doc);

extern GnomeUIInfo hex_document_menu[];
extern void hex_document_set_menu_sensitivity(HexDocument *doc);

enum {
	DOCUMENT_CHANGED,
	LAST_SIGNAL
};

static gint hex_signals[LAST_SIGNAL];

typedef void (*HexDocumentSignal) (GtkObject *, gpointer, gboolean, gpointer);

static GnomeMDIChildClass *parent_class = NULL;

static void hex_document_marshal (GtkObject	    *object,
                                  GtkSignalFunc     func,
                                  gpointer	    func_data,
                                  GtkArg	    *args)
{
	HexDocumentSignal rfunc;
	
	rfunc = (HexDocumentSignal) func;
	
	(* rfunc)(object, GTK_VALUE_POINTER(args[0]), GTK_VALUE_BOOL(args[1]), func_data);
}

static void free_stack(GList *stack)
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

static gint undo_stack_push(HexDocument *doc, HexChangeData *change_data)
{
	HexChangeData *cd;
	GList *stack_rest;
#ifdef GNOME_ENABLE_DEBUG
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
#ifdef GNOME_ENABLE_DEBUG
		g_message("depth at: %d", doc->undo_depth);
#endif

		if(doc->undo_depth > doc->undo_max) {
			GList *last;

#ifdef GNOME_ENABLE_DEBUG
			g_message("forget last undo");
#endif

			last = g_list_last(doc->undo_stack);
			doc->undo_stack = g_list_remove_link(doc->undo_stack, last);
			doc->undo_depth--;
			free_stack(last);
		}

		doc->undo_stack = g_list_prepend(doc->undo_stack, cd);
		doc->undo_top = doc->undo_stack;

		hex_document_set_menu_sensitivity(doc);

		return TRUE;
	}

	hex_document_set_menu_sensitivity(doc);

	return FALSE;
}

static void undo_stack_descend(HexDocument *doc)
{
#ifdef GNOME_ENABLE_DEBUG
	g_message("undo_stack_descend");
#endif

	if(doc->undo_top == NULL)
		return;

	doc->undo_top = doc->undo_top->next;
	doc->undo_depth--;
#ifdef GNOME_ENABLE_DEBUG
	g_message("undo depth at: %d", doc->undo_depth);
#endif

	hex_document_set_menu_sensitivity(doc);
}

static void undo_stack_ascend(HexDocument *doc)
{
#ifdef GNOME_ENABLE_DEBUG
	g_message("undo_stack_ascend");
#endif

	if(doc->undo_stack == NULL || doc->undo_top == doc->undo_stack)
		return;

	if(doc->undo_top == NULL)
		doc->undo_top = g_list_last(doc->undo_stack);
	else
		doc->undo_top = doc->undo_top->prev;
	doc->undo_depth++;

	hex_document_set_menu_sensitivity(doc);
}

static void undo_stack_free(HexDocument *doc)
{
#ifdef GNOME_ENABLE_DEBUG
	g_message("undo_stack_free");
#endif

	if(doc->undo_stack == NULL)
		return;

	free_stack(doc->undo_stack);
	doc->undo_stack = NULL;
	doc->undo_top = NULL;
	doc->undo_depth = 0;
	hex_document_set_menu_sensitivity(doc);
}

static gboolean get_document_attributes(HexDocument *doc)
{
	static struct stat stats;

	if(!stat(doc->file_name, &stats) &&
	   S_ISREG(stats.st_mode) &&
	   stats.st_size > 0) {
		doc->file_size = stats.st_size;

		return TRUE;
	}

	return FALSE;
}


static void move_gap_to(HexDocument *doc, guint offset, gint min_size)
{
	guchar *tmp, *buf_ptr, *tmp_ptr;

	if(doc->gap_size < min_size) {
		tmp = g_malloc(sizeof(guchar)*doc->file_size);
		buf_ptr = doc->buffer;
		tmp_ptr = tmp;
		while(buf_ptr < doc->gap_pos)
			*tmp_ptr++ = *buf_ptr++;
		buf_ptr += doc->gap_size;
		while(buf_ptr < doc->buffer + doc->buffer_size)
			*tmp_ptr++ = *buf_ptr++;

		doc->gap_size = MAX(min_size, 32);
		doc->buffer_size = doc->file_size + doc->gap_size;
		doc->buffer = g_realloc(doc->buffer, sizeof(guchar)*doc->buffer_size);
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

static GtkWidget *hex_document_create_view(GnomeMDIChild *child)
{
	GtkWidget *new_view;
	
	new_view = gtk_hex_new(HEX_DOCUMENT(child));
	
	/* TODO: perhaps it would be nicer to put such stuff in the MDI add_view signal handler */
	gtk_hex_set_group_type(GTK_HEX(new_view), def_group_type);
	gtk_hex_set_font(GTK_HEX(new_view), def_font);

	return new_view;
}

static void hex_document_finalize(GtkObject *obj)
{
	HexDocument *hex;
	
	hex = HEX_DOCUMENT(obj);
	
	if(hex->buffer)
		g_free(hex->buffer);
	
	if(hex->file_name)
		g_free(hex->file_name);

	undo_stack_free(hex);
	
	if(GTK_OBJECT_CLASS(parent_class)->finalize)
		(* GTK_OBJECT_CLASS(parent_class)->finalize)(GTK_OBJECT(hex));
}

static void hex_document_real_changed(HexDocument *doc, gpointer change_data, gboolean push_undo)
{
	GList *view;
	GnomeMDIChild *child;
	
	child = GNOME_MDI_CHILD(doc);

	if(push_undo)
		undo_stack_push(doc, change_data);

	view = child->views;
	while(view) {
		gtk_signal_emit_by_name(GTK_OBJECT(view->data), "data_changed", change_data);
		view = g_list_next(view);
	}
}

static gchar *hex_document_get_config_string(GnomeMDIChild *child)
{
	return g_strdup(HEX_DOCUMENT(child)->file_name);
}

static void hex_document_class_init (HexDocumentClass *class)
{
	GtkObjectClass *object_class;
	GnomeMDIChildClass *child_class;
	
	object_class = (GtkObjectClass*)class;
	child_class = GNOME_MDI_CHILD_CLASS(class);
	
	hex_signals[DOCUMENT_CHANGED] = gtk_signal_new ("document_changed",
													GTK_RUN_LAST,
													object_class->type,
													GTK_SIGNAL_OFFSET (HexDocumentClass, document_changed),
													hex_document_marshal,
													GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_BOOL);
	
	gtk_object_class_add_signals (object_class, hex_signals, LAST_SIGNAL);
	
	object_class->finalize = hex_document_finalize;
	
	child_class->create_view = (GnomeMDIChildViewCreator)(hex_document_create_view);
	child_class->get_config_string = (GnomeMDIChildConfigFunc)(hex_document_get_config_string);
	
	class->document_changed = hex_document_real_changed;
	
	parent_class = gtk_type_class (gnome_mdi_child_get_type ());
}

static void hex_document_init (HexDocument *doc)
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
	doc->undo_max = max_undo_depth;
	gnome_mdi_child_set_menu_template(GNOME_MDI_CHILD(doc), hex_document_menu);
}

GtkType hex_document_get_type ()
{
	static GtkType doc_type = 0;
	
	if (!doc_type) {
		static const GtkTypeInfo doc_info = {
			"HexDocument",
			sizeof (HexDocument),
			sizeof (HexDocumentClass),
			(GtkClassInitFunc) hex_document_class_init,
			(GtkObjectInitFunc) hex_document_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};
		
		doc_type = gtk_type_unique (gnome_mdi_child_get_type (), &doc_info);
	}

	return doc_type;
}


/*-------- public API starts here --------*/


HexDocument *hex_document_new(const gchar *name)
{
	HexDocument *doc;
	
	int i;
	
	if((doc = gtk_type_new(hex_document_get_type()))) {
		doc->file_name = (gchar *)g_strdup(name);
		if(get_document_attributes(doc)) {
			doc->gap_size = 100;
			doc->buffer_size = doc->file_size + doc->gap_size;
			doc->buffer = (guchar *)g_malloc(doc->buffer_size);

			/* find the start of the filename without path */
			for(i = strlen(doc->file_name); (i >= 0) && (doc->file_name[i] != '/'); i--)
				;					
			if(doc->file_name[i] == '/')
				doc->path_end = &doc->file_name[i+1];
			else
				doc->path_end = doc->file_name;
					
			gnome_mdi_child_set_name(GNOME_MDI_CHILD(doc), doc->path_end);
			if(hex_document_read(doc))
				return doc;
		}
		gtk_object_destroy(GTK_OBJECT(doc));
	}
	
	return NULL;
}

guchar hex_document_get_byte(HexDocument *doc, guint offset)
{
	if(offset < doc->file_size) {
		if(doc->gap_pos <= doc->buffer + offset)
			offset += doc->gap_size;
		return doc->buffer[offset];
	}
	else
		return 0;
}

guchar *hex_document_get_data(HexDocument *doc, guint offset, guint len)
{
	guchar *ptr, *data, *dptr;
	guint i;

	ptr = doc->buffer + offset;
	if(ptr >= doc->gap_pos)
		ptr += doc->gap_size;
	dptr = data = g_malloc(sizeof(guchar)*len);
	i = 0;
	while(i < len) {
		if(ptr >= doc->gap_pos && ptr < doc->gap_pos + doc->gap_size)
			ptr += doc->gap_size;
		*dptr++ = *ptr++;
		i++;
	}

	return data;
}

void hex_document_set_nibble(HexDocument *doc, guchar val, guint offset,
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

void hex_document_set_byte(HexDocument *doc, guchar val, guint offset,
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

void hex_document_set_data(HexDocument *doc, guint offset, guint len,
						   guint rep_len, guchar *data, gboolean undoable)
{
	guint i;
	guchar *ptr;
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

void hex_document_delete_data(HexDocument *doc, guint offset, guint len, gboolean undoable)
{
	hex_document_set_data(doc, offset, 0, len, NULL, undoable);
#if 0
	/* TODO: add undo capabilities */

	move_gap_to(doc, MIN(offset + len, doc->file_size), 1);
	doc->gap_pos -= len;
	doc->gap_size += len;
	doc->file_size -= len;
#endif
}

gint hex_document_read(HexDocument *doc)
{
	FILE *file;
	static HexChangeData change_data;

	if(!get_document_attributes(doc))
		return FALSE;

	if((file = fopen(doc->file_name, "r")) == NULL)
		return FALSE;

	doc->gap_size = doc->buffer_size - doc->file_size;
	fread(doc->buffer + doc->gap_size, 1, doc->file_size, file);
	doc->gap_pos = doc->buffer;
	fclose(file);
	undo_stack_free(doc);

	change_data.start = 0;
	change_data.end = doc->file_size - 1;
	hex_document_changed(doc, &change_data, FALSE);

	doc->changed = FALSE;

	return TRUE;
}

gint hex_document_write_to_file(HexDocument *doc, FILE *file)
{
	gint ret = TRUE;
	size_t exp_len;

	if(doc->gap_pos > doc->buffer) {
		exp_len = MIN(doc->file_size, doc->gap_pos - doc->buffer);
		ret = fwrite(doc->buffer, 1, exp_len, file);
		ret = (ret == exp_len)?TRUE:FALSE;
	}
	if(doc->gap_pos < doc->buffer + doc->file_size) {
		exp_len = doc->file_size - (size_t)(doc->gap_pos - doc->buffer);
		ret = fwrite(doc->gap_pos + doc->gap_size, 1, exp_len, file);
		ret = (ret == exp_len)?TRUE:FALSE;
	}

	return ret;
}

gint hex_document_write(HexDocument *doc)
{
	FILE *file;
	gint ret = FALSE;

	if((file = fopen(doc->file_name, "w")) != NULL) {
		ret = hex_document_write_to_file(doc, file);
		fclose(file);
		if(ret) {
			doc->changed = FALSE;
			undo_stack_free(doc);
		}
	}

	return ret;
}

GnomeMDIChild *hex_document_new_from_config(const gchar *cfg)
{
	HexDocument *doc;

	/* our config string is simply a file name */
	doc = hex_document_new(cfg);
	if(doc)
		hex_document_read(doc);
	
	return GNOME_MDI_CHILD(doc);
}

void hex_document_changed(HexDocument *doc, gpointer change_data,
						  gboolean push_undo)
{
	gtk_signal_emit(GTK_OBJECT(doc), hex_signals[DOCUMENT_CHANGED], change_data, push_undo);
}

gboolean hex_document_has_changed(HexDocument *doc)
{
	return doc->changed;
}

void hex_document_set_max_undo(HexDocument *doc, guint max_undo)
{
	if(doc->undo_max != max_undo) {
		if(doc->undo_max > max_undo)
			undo_stack_free(doc);
		doc->undo_max = max_undo;
	}
}

gint hex_document_export_html(HexDocument *doc, gchar *html_path, gchar *base_name, guint start, guint end, guint cpl, guint lpp, guint cpw)
{
	FILE *file;
	int page, line, pos, lines, pages, c;
	gchar *page_name, b;

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
	fprintf(file, "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=iso-8859-1\">\n");
	fprintf(file, "<META NAME=\"hexdata\" CONTENT=\"GHex export to HTML\">\n");
	fprintf(file, "</HEAD>\n<BODY>\n");

	fprintf(file, "<CENTER>");
	fprintf(file, "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\">\n");
	fprintf(file, "<TR>\n<TD COLSPAN=\"3\"><B>%s</B></TD>\n</TR>\n", doc->file_name);
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
	fprintf(file, " <A HREF=\""GHEX_URL"\">GHex</A> "VERSION"\n");
	fprintf(file, "</BODY>\n</HTML>\n");
	fclose(file);

	pos = start;
	for(page = 0; page < pages; page++) {
		/* write page header */
		page_name = g_strdup_printf("%s/%s%08d.html",
									html_path, base_name, page);
		file = fopen(page_name, "w");
		g_free(page_name);
		if(!file)
			return FALSE;
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
		fprintf(file, " <A HREF=\""GHEX_URL"\">GHex</A> "VERSION"\n");
		fprintf(file, "</BODY>\n</HTML>\n");
		fclose(file);
	}
	return TRUE;
}

gint hex_document_compare_data(HexDocument *doc, guchar *s2, gint pos, gint len)
{
	guchar c1;
	guint i;

	for(i = 0; i < len; i++, s2++) {
		c1 = hex_document_get_byte(doc, pos + i);
		if(c1 != (*s2))
			return (c1 - (*s2));
	}
	
	return 0;
}

gint hex_document_find_forward(HexDocument *doc, guint start, guchar *what,
							   gint len, guint *found)
{
	guint pos;
	
	pos = start;
	while(pos < doc->file_size) {
		if(hex_document_compare_data(doc, what, pos, len) == 0) {
			*found = pos;
			return TRUE;
		}
		pos++;
	}

	return FALSE;
}

gint hex_document_find_backward(HexDocument *doc, guint start, guchar *what,
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

gboolean hex_document_undo(HexDocument *doc)
{
	HexChangeData *cd;
	gint len;
	guchar *rep_data;
	gchar c_val;

	if(doc->undo_top == NULL)
		return FALSE;

	cd = (HexChangeData *)doc->undo_top->data;

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

	return TRUE;
}

gboolean hex_document_redo(HexDocument *doc)
{
	HexChangeData *cd;
	gint len;
	guchar *rep_data;
	gchar c_val;

	if(doc->undo_stack == NULL || doc->undo_top == doc->undo_stack)
		return FALSE;

	undo_stack_ascend(doc);

	cd = (HexChangeData *)doc->undo_top->data;

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

	return TRUE;
}
