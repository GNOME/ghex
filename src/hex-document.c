/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* hex-document.c - implementation of a hex-document MDI child

   Copyright (C) 1997, 1998 Free Software Foundation

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
static void       hex_document_real_changed      (HexDocument *, gpointer);
static gchar     *hex_document_get_config_string (GnomeMDIChild *);

static void find_cb();
static void replace_cb();
static void jump_cb();
static void set_byte_cb();
static void set_word_cb();
static void set_long_cb();

GnomeUIInfo group_radio_items[] = {
	{ GNOME_APP_UI_ITEM, N_("_Bytes"), N_("Group data by 8 bits"), set_byte_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_ITEM, N_("_Words"), N_("Group data by 16 bits"), set_word_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_ITEM, N_("_Longwords"), N_("Group data by 32 bits"), set_long_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_ENDOFINFO },
};

GnomeUIInfo group_type_menu[] = {
	{ GNOME_APP_UI_RADIOITEMS, NULL, NULL, group_radio_items, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_ENDOFINFO },
};
GnomeUIInfo edit_menu[] = {
	{ GNOME_APP_UI_ITEM, N_("_Find..."), N_("Search for a string"), find_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SEARCH, 'F', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_ITEM, N_("_Replace..."), N_("Replace a string"), replace_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SRCHRPL, 'R', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_SEPARATOR, NULL, NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_ITEM, N_("_Goto Byte..."), N_("Jump to a certain position"), jump_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_JUMP_TO, 'J', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_SEPARATOR, NULL, NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_SUBTREE, N_("_Group Data As"), NULL, group_type_menu, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_ENDOFINFO }
};

GnomeUIInfo doc_menu[] = {
	{ GNOME_APP_UI_SUBTREE, N_("_Edit"), NULL, edit_menu, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_ENDOFINFO }
};

enum {
	DOCUMENT_CHANGED,
	LAST_SIGNAL
};

static gint hex_signals[LAST_SIGNAL];

typedef void (*HexDocumentSignal) (GtkObject *, gpointer, gpointer);

static GnomeMDIChildClass *parent_class = NULL;

static void hex_document_marshal (GtkObject	    *object,
                                  GtkSignalFunc     func,
                                  gpointer	    func_data,
                                  GtkArg	    *args) {
	HexDocumentSignal rfunc;
	
	rfunc = (HexDocumentSignal) func;
	
	(* rfunc)(object, GTK_VALUE_POINTER(args[0]), func_data);
}

guint hex_document_get_type () {
	static guint doc_type = 0;
	
	if (!doc_type) {
		GtkTypeInfo doc_info = {
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
		free(hex->buffer);
	
	if(hex->file_name)
		free(hex->file_name);
	
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
													GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	
	gtk_object_class_add_signals (object_class, hex_signals, LAST_SIGNAL);
	
	object_class->destroy = hex_document_destroy;
	
	child_class->create_view = (GnomeMDIChildViewCreator)(hex_document_create_view);
	child_class->get_config_string = (GnomeMDIChildConfigFunc)(hex_document_get_config_string);
	
	class->document_changed = hex_document_real_changed;
	
	parent_class = gtk_type_class (gnome_mdi_child_get_type ());
}

void hex_document_set_byte(HexDocument *document, guchar val, guint offset) {
	document->changed = TRUE;
	
	if((offset >= 0) && (offset < document->buffer_size))
		document->buffer[offset] = val;
	
	document->change_data.start = offset;
	document->change_data.end = offset;
	
	hex_document_changed(document, &document->change_data);
}

void hex_document_set_data(HexDocument *document, guint offset, guint len, guchar *data) {
	guint i;
	
	document->changed = TRUE;
	
	for(i = 0; (offset < document->buffer_size) && (i < len); offset++, i++)
		document->buffer[offset] = data[i];
	
	document->change_data.start = offset;
	document->change_data.end = offset + i - 1;
	
	hex_document_changed(document, &document->change_data);
}

static void hex_document_init (HexDocument *document) {
	document->buffer = NULL;
	document->buffer_size = 0;
	document->changed = FALSE;
	gnome_mdi_child_set_menu_template(GNOME_MDI_CHILD(document), doc_menu);
}

HexDocument *hex_document_new(const gchar *name) {
	HexDocument *document;
	
	struct stat stats;
	int i;
	
	/* hopefully using stat() works for all flavours of UNIX...
	   don't know for sure, though */
	if(!stat(name, &stats)) {
		if(document = gtk_type_new (hex_document_get_type ())) {
			document->buffer_size = stats.st_size;
			
			if((document->buffer = (guchar *)malloc(document->buffer_size)) != NULL) {
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
					free(document->file_name);
				}
				free(document->buffer);
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
		
		doc->change_data.start = 0;
		doc->change_data.end = doc->buffer_size - 1;
		hex_document_changed(doc, &doc->change_data);
		
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
		return 0;
	}
	return 1;
}

static void hex_document_real_changed(HexDocument *doc, gpointer change_data) {
	GList *view;
	GnomeMDIChild *child;
	
	child = GNOME_MDI_CHILD(doc);
	
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

void hex_document_changed(HexDocument *doc, gpointer change_data) {
	gtk_signal_emit(GTK_OBJECT(doc), hex_signals[DOCUMENT_CHANGED], change_data);
}

gboolean hex_document_has_changed(HexDocument *doc) {
	return doc->changed;
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
