/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* hex-document-ui.c - menu definitions and callbacks for hex-document MDI child

   Copyright (C) 1998 - 2001 Free Software Foundation

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

void hex_document_set_menu_sensitivity(HexDocument *doc);

#ifdef SNM
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
static void insert_cb      (GtkWidget *w);
#endif

static GnomeUIInfo group_radio_items[] =
{
	GNOMEUIINFO_ITEM_NONE(N_("_Bytes"),
						  N_("Group data by 8 bits"), set_byte_cb),
	GNOMEUIINFO_ITEM_NONE(N_("_Words"),
						  N_("Group data by 16 bits"), set_word_cb),
	GNOMEUIINFO_ITEM_NONE(N_("_Longwords"),
						  N_("Group data by 32 bits"), set_long_cb),
	GNOMEUIINFO_END
};

static GnomeUIInfo group_type_menu[] =
{
	{ GNOME_APP_UI_RADIOITEMS, NULL, NULL, group_radio_items, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	GNOMEUIINFO_END
};

static GnomeUIInfo edit_menu[] =
{
	GNOMEUIINFO_MENU_UNDO_ITEM(undo_cb,NULL),
	GNOMEUIINFO_MENU_REDO_ITEM(redo_cb,NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_FIND_ITEM(find_cb,NULL),
	GNOMEUIINFO_MENU_REPLACE_ITEM(replace_cb,NULL),
	GNOMEUIINFO_SEPARATOR,
	{ GNOME_APP_UI_ITEM, N_("_Goto Byte..."), N_("Jump to a certain position"),
	  jump_cb, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_JUMP_TO,
	  'J', GDK_CONTROL_MASK, NULL },
	GNOMEUIINFO_SEPARATOR,
	{ GNOME_APP_UI_TOGGLEITEM, N_("_Insert mode"), N_("Insert/overwrite data"),
	  insert_cb, NULL, NULL, GNOME_APP_PIXMAP_NONE, NULL, GDK_Insert, 0, NULL },
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_SUBTREE(N_("_Group Data As"), group_type_menu),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_menu[] =
{
	GNOMEUIINFO_ITEM_NONE(N_("_Add view"),
			      N_("Add a new view of the buffer"), add_view_cb),
	GNOMEUIINFO_ITEM_NONE(N_("_Remove view"),
			      N_("Remove the current view of the buffer"),
			      remove_view_cb),
	GNOMEUIINFO_END
};

GnomeUIInfo hex_document_menu[] =
{
	GNOMEUIINFO_MENU_EDIT_TREE(edit_menu),
	GNOMEUIINFO_MENU_VIEW_TREE(view_menu),
	GNOMEUIINFO_END
};

#ifdef SNM /* No longer required for Gnome 2.0 -- SnM */
void hex_document_set_menu_sensitivity(HexDocument *doc)
{
	GList *view_node;
	GnomeApp *app;
	GnomeUIInfo *uiinfo;
	GtkWidget *view;
	gboolean sensitive;

	view_node = BONOBO_MDI_CHILD(doc)->views;
	while(view_node) {
		view = GTK_WIDGET(view_node->data);
		app = gnome_mdi_get_app_from_view(view);
		if(view == gnome_mdi_get_view_from_window(mdi, app)) { 
			uiinfo = gnome_mdi_get_child_menu_info(app);
			uiinfo = (GnomeUIInfo *)uiinfo[0].moreinfo;
			sensitive = doc->undo_top != NULL;
			gtk_widget_set_sensitive(uiinfo[0].widget, sensitive);
			sensitive = doc->undo_stack && doc->undo_top != doc->undo_stack;
			gtk_widget_set_sensitive(uiinfo[1].widget, sensitive);
			view_node = view_node->next;
		}
	}
}
#endif


void hex_document_set_menu_sensitivity(HexDocument *doc)
{
	GList *view_node;
	GtkWidget *view;
	gboolean sensitive;
	BonoboWindow *win;

	BonoboUIEngine *ui_engine;

	view_node = bonobo_mdi_child_get_views (BONOBO_MDI_CHILD (doc));

	while (view_node) {
		view = GTK_WIDGET(view_node->data);

		win = bonobo_mdi_get_window_from_view (view);

		g_return_if_fail (win != NULL);
 
		ui_engine = bonobo_window_get_ui_engine (win);

		g_return_if_fail (ui_engine != NULL);

		bonobo_ui_engine_freeze (ui_engine);

		sensitive = doc->undo_top != NULL;
		bonobo_ui_engine_xml_set_prop (ui_engine, "/commands/EditUndo",
						"sensitive", sensitive ? "1" : "0", "EditUndo"); 
	
		sensitive = doc->undo_stack && doc->undo_top != doc->undo_stack;

		bonobo_ui_engine_xml_set_prop (ui_engine, "/commands/EditRedo",
						"sensitive", sensitive ? "1" : "0", "EditRedo"); 

		bonobo_ui_engine_thaw (ui_engine);	
		view_node = view_node->next;
	}

}



/*
 * callbacks for document's menus
 */

/* Changed the function parameters -- SnM */
void set_byte_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
#ifdef SNM
	if(GTK_CHECK_MENU_ITEM(w)->active && bonobo_mdi_get_active_view (BONOBO_MDI (mdi)))
#endif
	if(bonobo_mdi_get_active_view (BONOBO_MDI (mdi)))
		gtk_hex_set_group_type(GTK_HEX(bonobo_mdi_get_active_view (BONOBO_MDI (mdi))), GROUP_BYTE);
}

/* Changed the function parameters -- SnM */
void set_word_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
#ifdef SNM
	if(GTK_CHECK_MENU_ITEM(w)->active && bonobo_mdi_get_active_view (BONOBO_MDI (mdi)))
#endif
	if(bonobo_mdi_get_active_view (BONOBO_MDI (mdi)))
		gtk_hex_set_group_type(GTK_HEX(bonobo_mdi_get_active_view (BONOBO_MDI (mdi))), GROUP_WORD);
}

/* Changed the function parameters -- SnM */
void set_long_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
#ifdef SNM
	if(GTK_CHECK_MENU_ITEM(w)->active && bonobo_mdi_get_active_view (BONOBO_MDI (mdi)))
#endif
	if(bonobo_mdi_get_active_view (BONOBO_MDI (mdi)))
		gtk_hex_set_group_type(GTK_HEX(bonobo_mdi_get_active_view (BONOBO_MDI (mdi))), GROUP_LONG);
}

/* Changed the function parameters -- SnM */
void insert_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
#ifdef REQUIRED
	if(bonobo_mdi_get_active_view (BONOBO_MDI (mdi)))
		gtk_hex_set_insert_mode(GTK_HEX(bonobo_mdi_get_active_view (BONOBO_MDI (mdi))),
								GTK_CHECK_MENU_ITEM(w)->active);
#endif
	if(bonobo_mdi_get_active_view (BONOBO_MDI (mdi)))
		gtk_hex_set_insert_mode(GTK_HEX(bonobo_mdi_get_active_view (BONOBO_MDI (mdi))), TRUE ); /* Have to fix this -- SnM */

}

/* Changed the function parameters -- SnM */
void find_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	if(!find_dialog)
		find_dialog = create_find_dialog();

	if(!GTK_WIDGET_VISIBLE(find_dialog->window)) {
		gtk_window_position (GTK_WINDOW(find_dialog->window), GTK_WIN_POS_MOUSE);
		gtk_window_set_default(GTK_WINDOW(find_dialog->window), find_dialog->f_next);
		gtk_widget_show(find_dialog->window);
	}
	gdk_window_raise(find_dialog->window->window);
}

/* Changed the function parameters -- SnM */
void replace_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	if(!replace_dialog)
		replace_dialog = create_replace_dialog();

	if(!GTK_WIDGET_VISIBLE(replace_dialog->window)) {
		gtk_window_position (GTK_WINDOW(replace_dialog->window), GTK_WIN_POS_MOUSE);
		gtk_window_set_default(GTK_WINDOW(replace_dialog->window), replace_dialog->next);
		gtk_widget_show(replace_dialog->window);
	}
	gdk_window_raise(replace_dialog->window->window);
}

/* Changed the function parameters -- SnM */
void jump_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	if(!jump_dialog)
		jump_dialog = create_jump_dialog();

	if(!GTK_WIDGET_VISIBLE(jump_dialog->window)) {
		gtk_window_position (GTK_WINDOW(jump_dialog->window), GTK_WIN_POS_MOUSE);
		gtk_window_set_default(GTK_WINDOW(jump_dialog->window), jump_dialog->ok);
		gtk_widget_show(jump_dialog->window);
	}
	gdk_window_raise(jump_dialog->window->window);
}

/* Changed the function parameters -- SnM */
void undo_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	HexDocument *doc = HEX_DOCUMENT(bonobo_mdi_get_active_child (BONOBO_MDI (mdi)));
	HexChangeData *cd;

	if(doc->undo_top) {
		cd = (HexChangeData *)doc->undo_top->data;

		hex_document_undo(doc);

		gtk_hex_set_cursor(GTK_HEX(bonobo_mdi_get_active_view (BONOBO_MDI (mdi))), cd->start);
		gtk_hex_set_nibble(GTK_HEX(bonobo_mdi_get_active_view (BONOBO_MDI (mdi))), cd->lower_nibble);
	}
}

/* Changed the function parameters -- SnM */
void redo_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	HexDocument *doc = HEX_DOCUMENT(bonobo_mdi_get_active_child (BONOBO_MDI (mdi)));
	HexChangeData *cd;

	if(doc->undo_stack && doc->undo_top != doc->undo_stack) {
		hex_document_redo(doc);

		cd = (HexChangeData *)doc->undo_top->data;

		gtk_hex_set_cursor(GTK_HEX(bonobo_mdi_get_active_view (BONOBO_MDI (mdi))), cd->start);
		gtk_hex_set_nibble(GTK_HEX(bonobo_mdi_get_active_view (BONOBO_MDI (mdi))), cd->lower_nibble);
	}
}

/* Changed the function parameters -- SnM */
void add_view_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
#ifdef SNM
	BonoboMDIChild *child = BONOBO_MDI_CHILD(user_data);
#endif

	bonobo_mdi_add_view( BONOBO_MDI (mdi), bonobo_mdi_get_active_child (BONOBO_MDI (mdi)));
}

/* Changed the function parameters -- SnM */
void remove_view_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	if(bonobo_mdi_get_active_view (BONOBO_MDI (mdi)))
		bonobo_mdi_remove_view( BONOBO_MDI (mdi), bonobo_mdi_get_active_view (BONOBO_MDI (mdi)), FALSE);
	
	if ( NULL == bonobo_mdi_get_active_child (BONOBO_MDI (mdi))) {
		BonoboUIEngine *ui_engine;
		BonoboWindow *active_window;

		active_window = bonobo_mdi_get_active_window (BONOBO_MDI (mdi));

		g_return_if_fail (active_window != NULL);

		ui_engine = bonobo_window_get_ui_engine (active_window);

		bonobo_ui_engine_freeze (ui_engine);

		ghex_menus_set_verb_list_sensitive (ui_engine, FALSE);
	
		bonobo_ui_engine_thaw (ui_engine);
	}
}
