/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* hex-document.c - implementation of a hex-document MDI child

   Copyright (C) 1998, 1999 Free Software Foundation

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

   Author: Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */

#include <config.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gnome.h>

#include <hex-document.h>

#include "ghex.h"
#include "gtkhex.h"
#include "callbacks.h"

#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

static void       hex_document_class_init        (HexDocumentClass *);
static void       hex_document_init              (HexDocument *);
static GtkWidget *hex_document_create_view       (GnomeMDIChild *);
static void       hex_document_destroy           (GtkObject *);
static void       hex_document_real_changed      (HexDocument *, gpointer, gboolean);
static gchar     *hex_document_get_config_string (GnomeMDIChild *);

static void find_cb     (GtkWidget *);
static void replace_cb  (GtkWidget *);
static void jump_cb     (GtkWidget *);
static void set_byte_cb (GtkWidget *);
static void set_word_cb (GtkWidget *);
static void set_long_cb (GtkWidget *);
static void undo_cb     (GtkWidget *, gpointer);
static void redo_cb     (GtkWidget *, gpointer);
static void add_view_cb    (GtkWidget *, gpointer);
static void remove_view_cb (GtkWidget *, gpointer);

GnomeUIInfo group_radio_items[] = {
	GNOMEUIINFO_ITEM_NONE(N_("_Bytes"),
			      N_("Group data by 8 bits"), set_byte_cb),
	GNOMEUIINFO_ITEM_NONE(N_("_Words"),
			      N_("Group data by 16 bits"), set_word_cb),
	GNOMEUIINFO_ITEM_NONE(N_("_Longwords"),
			      N_("Group data by 32 bits"), set_long_cb),
	GNOMEUIINFO_END
};

GnomeUIInfo group_type_menu[] = {
	{ GNOME_APP_UI_RADIOITEMS, NULL, NULL, group_radio_items, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	GNOMEUIINFO_END
};
GnomeUIInfo edit_menu[] = {
	GNOMEUIINFO_MENU_UNDO_ITEM(undo_cb,NULL),
	GNOMEUIINFO_MENU_REDO_ITEM(redo_cb,NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_FIND_ITEM(find_cb,NULL),
	GNOMEUIINFO_MENU_REPLACE_ITEM(replace_cb,NULL),
	GNOMEUIINFO_SEPARATOR,
	{ GNOME_APP_UI_ITEM, N_("_Goto Byte..."), N_("Jump to a certain position"), jump_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_JUMP_TO, 'J', GDK_CONTROL_MASK, NULL },
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_SUBTREE(N_("_Group Data As"), group_type_menu),
	GNOMEUIINFO_END
};

GnomeUIInfo view_menu[] = {
	GNOMEUIINFO_ITEM_NONE(N_("_Add view"),
			      N_("Add a new view of the buffer"), add_view_cb),
	GNOMEUIINFO_ITEM_NONE(N_("_Remove view"),
			      N_("Remove the current view of the buffer"),
			      remove_view_cb),
	GNOMEUIINFO_END
};

GnomeUIInfo doc_menu[] = {
	GNOMEUIINFO_MENU_EDIT_TREE(edit_menu),
	GNOMEUIINFO_MENU_VIEW_TREE(view_menu),
	GNOMEUIINFO_END
};

enum {
	DOCUMENT_CHANGED,
	LAST_SIGNAL
};

#define HEX_DOC_UNDO (1L << 0)
#define HEX_DOC_REDO (1L << 1)

static gint hex_signals[LAST_SIGNAL];

typedef void (*HexDocumentSignal) (GtkObject *, gpointer, gboolean, gpointer);

static GnomeMDIChildClass *parent_class = NULL;

static void hex_document_marshal (GtkObject	    *object,
                                  GtkSignalFunc     func,
                                  gpointer	    func_data,
                                  GtkArg	    *args) {
	HexDocumentSignal rfunc;
	
	rfunc = (HexDocumentSignal) func;
	
	(* rfunc)(object, GTK_VALUE_POINTER(args[0]), GTK_VALUE_BOOL(args[1]), func_data);
}

static void set_sensitivity(HexDocument *doc, gint widgets, gboolean sensitive) {
	GList *app;
	GnomeUIInfo *uiinfo;
	GnomeMDI *mdi;
	GnomeMDIChild *child;
	GtkWidget *view;

	child = GNOME_MDI_CHILD(doc);

	if(child->parent == NULL)
		return;

	mdi = GNOME_MDI(child->parent);
	app = mdi->windows;
	while(app) {
		view = gnome_mdi_get_view_from_window(mdi, GNOME_APP(app->data));
		if(child == gnome_mdi_get_child_from_view(view)) {
			uiinfo = gnome_mdi_get_child_menu_info(GNOME_APP(app->data));
			uiinfo = (GnomeUIInfo *)uiinfo[0].moreinfo;
			if(widgets & HEX_DOC_REDO)
				gtk_widget_set_sensitive(uiinfo[1].widget, sensitive);
			if(widgets & HEX_DOC_UNDO)
				gtk_widget_set_sensitive(uiinfo[0].widget, sensitive);
		}
		app = app->next;
	}
}

static void free_stack(GList *stack) {
	HexChangeData *cd;

	while(stack) {
		cd = (HexChangeData *)stack->data;
		if(cd->v_string)
			g_free(cd->v_string);
		stack = g_list_remove(stack, cd);
		g_free(cd);
	}
}

static gint undo_stack_push(HexDocument *doc, HexChangeData *change_data) {
	HexChangeData *cd;
	GList *stack_rest;
#ifdef GNOME_ENABLE_DEBUG
	g_message("undo_stack_push");
#endif

	if(doc->undo_stack != doc->undo_top) {
		set_sensitivity(doc, HEX_DOC_REDO, FALSE);
		stack_rest = doc->undo_stack;
		doc->undo_stack = doc->undo_top;
		if(doc->undo_top) {
			doc->undo_top->prev->next = NULL;
			doc->undo_top->prev = NULL;
		}
		free_stack(stack_rest);
	}

	if(cd = g_new(HexChangeData, 1)) {
		memcpy(cd, change_data, sizeof(HexChangeData));
		if(change_data->v_string) {
			cd->v_string = g_malloc(cd->end - cd->start + 1);
			memcpy(cd->v_string, change_data->v_string, cd->end - cd->start + 1);
		}

		doc->undo_depth++;
#ifdef GNOME_ENABLE_DEBUG
		g_message("depth at: %d", doc->undo_depth);
#endif

		if(doc->undo_depth == 1)
			set_sensitivity(doc, HEX_DOC_UNDO, TRUE);

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

		return TRUE;
	}
	return FALSE;
}

static void undo_stack_descend(HexDocument *doc) {
#ifdef GNOME_ENABLE_DEBUG
	g_message("undo_stack_descend");
#endif

	if(doc->undo_top == NULL)
		return;

	set_sensitivity(doc, HEX_DOC_REDO, TRUE);
	doc->undo_top = doc->undo_top->next;
	doc->undo_depth--;
#ifdef GNOME_ENABLE_DEBUG
	g_message("undo depth at: %d", doc->undo_depth);
#endif
	if(doc->undo_top == NULL)
		set_sensitivity(doc, HEX_DOC_UNDO, FALSE);
}

static void undo_stack_ascend(HexDocument *doc) {
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

	set_sensitivity(doc, HEX_DOC_UNDO, TRUE);

	if(doc->undo_top == doc->undo_stack)
		set_sensitivity(doc, HEX_DOC_REDO, FALSE);
}

static void undo_stack_free(HexDocument *doc) {
#ifdef GNOME_ENABLE_DEBUG
	g_message("undo_stack_free");
#endif

	if(doc->undo_stack == NULL)
		return;

	free_stack(doc->undo_stack);
	doc->undo_stack = NULL;
	doc->undo_top = NULL;
	doc->undo_depth = 0;
	set_sensitivity(doc, HEX_DOC_UNDO|HEX_DOC_REDO, FALSE);
}

GtkType hex_document_get_type () {
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

static GtkWidget *hex_document_create_view(GnomeMDIChild *child) {
	GtkWidget *new_view;
	
	new_view = gtk_hex_new(HEX_DOCUMENT(child));
	
	/* TODO: perhaps it would be nicer to put such stuff in the MDI add_view signal handler */
	gtk_hex_set_group_type(GTK_HEX(new_view), def_group_type);
	gtk_hex_set_font(GTK_HEX(new_view), def_font);
	
	return new_view;
}

static void hex_document_destroy(GtkObject *obj) {
	HexDocument *hex;
	
	hex = HEX_DOCUMENT(obj);
	
	if(hex->buffer)
		g_free(hex->buffer);
	
	if(hex->file_name)
		g_free(hex->file_name);

	if(hex->change_data.v_string)
		g_free(hex->change_data.v_string);

	undo_stack_free(hex);
	
	if(GTK_OBJECT_CLASS(parent_class)->destroy)
		(* GTK_OBJECT_CLASS(parent_class)->destroy)(GTK_OBJECT(hex));
}

static void hex_document_class_init (HexDocumentClass *class) {
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
	
	object_class->destroy = hex_document_destroy;
	
	child_class->create_view = (GnomeMDIChildViewCreator)(hex_document_create_view);
	child_class->get_config_string = (GnomeMDIChildConfigFunc)(hex_document_get_config_string);
	
	class->document_changed = hex_document_real_changed;
	
	parent_class = gtk_type_class (gnome_mdi_child_get_type ());
}

void hex_document_set_nibble(HexDocument *document, guchar val, guint offset, gboolean lower_nibble) {
	if(offset >= 0 && offset < document->buffer_size) {
		document->changed = TRUE;
		document->change_data.start = offset;
		document->change_data.end = offset;
		document->change_data.type = HEX_CHANGE_BYTE;
		document->change_data.lower_nibble = lower_nibble;
		document->change_data.v_byte = document->buffer[offset];
		document->buffer[offset] = (document->buffer[offset] & (lower_nibble?0xF0:0x0F)) | (lower_nibble?val:(val << 4));

	 	hex_document_changed(document, &document->change_data, TRUE);
	}
}

void hex_document_set_byte(HexDocument *document, guchar val, guint offset) {
	if(offset >= 0 && offset < document->buffer_size) {
		document->changed = TRUE;
		document->change_data.start = offset;
		document->change_data.end = offset;
		document->change_data.type = HEX_CHANGE_BYTE;
		document->change_data.lower_nibble = FALSE;
		document->change_data.v_byte = document->buffer[offset];
		document->buffer[offset] = val;

	 	hex_document_changed(document, &document->change_data, TRUE);
	}
}

void hex_document_set_data(HexDocument *document, guint offset, guint len, guchar *data) {
	guint i;
	gchar *old_data;
	
	if(offset >= 0 && offset < document->buffer_size)
		document->changed = TRUE;

	document->change_data.v_string = g_realloc(document->change_data.v_string, len);
	
	document->change_data.start = offset;
	document->change_data.type = HEX_CHANGE_STRING;
	document->change_data.lower_nibble = FALSE;

	for(i = 0; offset < document->buffer_size && i < len; offset++, i++) {
		if(document->change_data.v_string)
			document->change_data.v_string[i] = document->buffer[offset];
		document->buffer[offset] = data[i];
	};

	document->change_data.end = document->change_data.start + i - 1;
	
	hex_document_changed(document, &document->change_data, TRUE);
}

static void hex_document_init (HexDocument *document) {
	document->buffer = NULL;
	document->buffer_size = 0;
	document->changed = FALSE;
	document->change_data.v_string = NULL;
	document->undo_stack = NULL;
	document->undo_top = NULL;
	document->undo_depth = 0;
	document->undo_max = max_undo_depth;
	gnome_mdi_child_set_menu_template(GNOME_MDI_CHILD(document), doc_menu);
}

HexDocument *hex_document_new(const gchar *name) {
	HexDocument *document;
	
	struct stat stats;
	int i;
	
	/* hopefully using stat() works for all flavours of UNIX...
	   don't know for sure, though */
	if(!stat(name, &stats)) {
		if((document = gtk_type_new (hex_document_get_type ()))) {
			document->buffer_size = stats.st_size;
			
			if((document->buffer = (guchar *)g_malloc(document->buffer_size)) != NULL) {
				if((document->file_name = (gchar *)strdup(name)) != NULL) {
					for(i = strlen(document->file_name); (i >= 0) && (document->file_name[i] != '/'); i--)
						;
					
					if(document->file_name[i] == '/')
						document->path_end = &document->file_name[i+1];
					else
						document->path_end = document->file_name;
					
					if((document->file = fopen(name, "r")) != NULL) {
						document->buffer_size = fread(document->buffer, 1, document->buffer_size, document->file);
						gnome_mdi_child_set_name(GNOME_MDI_CHILD(document), document->path_end);
						fclose(document->file);
						document->file = 0;
						return document;
					}
					g_free(document->file_name);
				}
				g_free(document->buffer);
			}
			gtk_object_destroy(GTK_OBJECT(document));
		}
	}
	
	return NULL;
}

gint hex_document_read(HexDocument *doc) {
	if((doc->file = fopen(doc->file_name, "r")) != NULL) {
		doc->buffer_size = fread(doc->buffer, 1, doc->buffer_size, doc->file);
		fclose(doc->file);
		doc->file = 0;
		undo_stack_free(doc);
		
		doc->change_data.start = 0;
		doc->change_data.end = doc->buffer_size - 1;
		if(doc->change_data.v_string) {
			g_free(doc->change_data.v_string);
			doc->change_data.v_string = NULL;
		}
		hex_document_changed(doc, &doc->change_data, FALSE);
		
		doc->changed = FALSE;
		
		return 0;
	}
	
	return 1;
}

gint hex_document_write(HexDocument *doc) {
	if((doc->file = fopen(doc->file_name, "w")) != NULL) {
		fwrite(doc->buffer, 1, doc->buffer_size, doc->file);
		fclose(doc->file);
		doc->file = 0;
		undo_stack_free(doc);
		return 0;
	}
	return 1;
}

static void hex_document_real_changed(HexDocument *doc, gpointer change_data, gboolean push_undo) {
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

static gchar *hex_document_get_config_string(GnomeMDIChild *child) {
	return g_strdup(HEX_DOCUMENT(child)->file_name);
}

GnomeMDIChild *hex_document_new_from_config(const gchar *cfg) {
	HexDocument *doc;
	
	/* our config string is simply a file name */
	doc = hex_document_new(cfg);
	if(doc)
		hex_document_read(doc);
	
	return GNOME_MDI_CHILD(doc);
}

void hex_document_changed(HexDocument *doc, gpointer change_data, gboolean push_undo) {
	gtk_signal_emit(GTK_OBJECT(doc), hex_signals[DOCUMENT_CHANGED], change_data, push_undo);
}

gboolean hex_document_has_changed(HexDocument *doc) {
	return doc->changed;
}

void hex_document_set_max_undo(HexDocument *doc, guint max_undo) {
	if(doc->undo_max != max_undo) {
		if(doc->undo_max > max_undo)
			undo_stack_free(doc);
		doc->undo_max = max_undo;
	}
}

gint compare_data(guchar *s1, guchar *s2, gint len) {
	while(len > 0) {
		if((*s1) != (*s2))
			return ((*s1) - (*s2));
		s1++;
		s2++;
		len--;
	}
	
	return 0;
}

/*
 * search functions return 0 if the string was successfully found and != 0 otherwise;
 * the offset of the first occurence of the searched-for string is returned in *found
 */
gint find_string_forward(HexDocument *doc, guint start, guchar *what, gint len, guint *found) {
	guint pos;
	
	pos = start;
	while(pos < doc->buffer_size) {
		if(compare_data(&doc->buffer[pos], what, len) == 0) {
			*found = pos;
			return 0;
		}
		pos++;
	}
	
	return 1;
}

gint find_string_backward(HexDocument *doc, guint start, guchar *what, gint len, guint *found) {
	guint pos;
	
	pos = start;
	while(pos >= 0) {
		if(compare_data(&doc->buffer[pos], what, len) == 0) {
			*found = pos;
			return 0;
		}
		pos--;
	}
	
	return 1;
}

/*
 * callbacks for document's menus
 */
static void set_byte_cb(GtkWidget *w) {
	if( GTK_CHECK_MENU_ITEM(w)->active && mdi->active_view)
		gtk_hex_set_group_type(GTK_HEX(mdi->active_view), GROUP_BYTE);
}

static void set_word_cb(GtkWidget *w) {
	if( GTK_CHECK_MENU_ITEM(w)->active && mdi->active_view)
		gtk_hex_set_group_type(GTK_HEX(mdi->active_view), GROUP_WORD);
}

static void set_long_cb(GtkWidget *w) {
	if( GTK_CHECK_MENU_ITEM(w)->active && mdi->active_view)
		gtk_hex_set_group_type(GTK_HEX(mdi->active_view), GROUP_LONG);
}

static void find_cb(GtkWidget *w) {
	if(find_dialog.window == NULL)
		create_find_dialog(&find_dialog);
	
	gtk_window_position (GTK_WINDOW(find_dialog.window), GTK_WIN_POS_MOUSE);
	
	gtk_widget_show(find_dialog.window);
}

static void replace_cb(GtkWidget *w) {
	if(replace_dialog.window == NULL)
		create_replace_dialog(&replace_dialog);
	
	gtk_window_position (GTK_WINDOW(replace_dialog.window), GTK_WIN_POS_MOUSE);
	
	gtk_widget_show(replace_dialog.window);
}

static void jump_cb(GtkWidget *w) {
	if(jump_dialog.window == NULL)
		create_jump_dialog(&jump_dialog);
	
	gtk_window_position (GTK_WINDOW(jump_dialog.window), GTK_WIN_POS_MOUSE);
	
	gtk_widget_show(jump_dialog.window);
}

static void undo_cb(GtkWidget *w, gpointer user_data) {
	HexDocument *doc = HEX_DOCUMENT(user_data);
	HexChangeData *cd;
	gint offset;
	gchar c_val;

	if(doc->undo_top == NULL)
		return;

	cd = (HexChangeData *)doc->undo_top->data;

	switch(cd->type) {
	case HEX_CHANGE_BYTE:
		if(cd->start >= 0 && cd->end < doc->buffer_size) {
			c_val = doc->buffer[cd->start];
			doc->buffer[cd->start] = cd->v_byte;
			cd->v_byte = c_val;
		}
		break;
	case HEX_CHANGE_STRING:
		for(offset = cd->start;
			offset < doc->buffer_size && offset <= cd->end;
			offset++) {
			c_val = doc->buffer[offset];
			doc->buffer[offset] = cd->v_string[offset - cd->start];
			cd->v_string[offset - cd->start] = c_val;
		}
		break;
	}

	hex_document_changed(doc, cd, FALSE);

	gtk_hex_set_cursor(GTK_HEX(mdi->active_view), cd->start);
	gtk_hex_set_nibble(GTK_HEX(mdi->active_view), cd->lower_nibble);

	undo_stack_descend(doc);
}

static void redo_cb(GtkWidget *w, gpointer user_data) {
	HexDocument *doc = HEX_DOCUMENT(user_data);
	HexChangeData *cd;
	gint offset;
	gchar c_val;

	if(doc->undo_stack == NULL || doc->undo_top == doc->undo_stack)
		return;

	undo_stack_ascend(doc);

	cd = (HexChangeData *)doc->undo_top->data;

	switch(cd->type) {
	case HEX_CHANGE_BYTE:
		if(cd->start >= 0 && cd->end < doc->buffer_size) {
			c_val = doc->buffer[cd->start];
			doc->buffer[cd->start] = cd->v_byte;
			cd->v_byte = c_val;
		}
		break;
	case HEX_CHANGE_STRING:
		for(offset = cd->start;
			offset < doc->buffer_size && offset <= cd->end;
			offset++) {
			c_val = doc->buffer[offset];
			doc->buffer[offset] = cd->v_string[offset - cd->start];
			cd->v_string[offset - cd->start] = c_val;
		}
		break;
	}

	hex_document_changed(doc, cd, FALSE);

	gtk_hex_set_cursor(GTK_HEX(mdi->active_view), cd->start);
	gtk_hex_set_nibble(GTK_HEX(mdi->active_view), cd->lower_nibble);
}

void add_view_cb(GtkWidget *w, gpointer user_data) {
	GnomeMDIChild *child = GNOME_MDI_CHILD(user_data);

	gnome_mdi_add_view(mdi, child);
}

void remove_view_cb(GtkWidget *w, gpointer user_data) {
	if(mdi->active_view)
		gnome_mdi_remove_view(mdi, mdi->active_view, FALSE);
}
