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
	BonoboUIComponent *uic;

	view_node = bonobo_mdi_child_get_views (BONOBO_MDI_CHILD (doc));

	while (view_node) {
		view = GTK_WIDGET(view_node->data);

		win = bonobo_mdi_get_window_from_view (view);

		g_return_if_fail (win != NULL);
 
		uic = bonobo_mdi_get_ui_component_from_window(win);

		g_return_if_fail (uic != NULL);

		bonobo_ui_component_freeze (uic, NULL);

		sensitive = doc->undo_top != NULL;
		bonobo_ui_component_set_prop (uic, "/commands/EditUndo",
									  "sensitive", sensitive ? "1" : "0",
									  NULL); 
	
		sensitive = doc->undo_stack && doc->undo_top != doc->undo_stack;

		bonobo_ui_component_set_prop (uic, "/commands/EditRedo",
									  "sensitive", sensitive ? "1" : "0",
									  NULL); 

		bonobo_ui_component_thaw (uic, NULL);	
		view_node = view_node->next;
	}

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
		BonoboUIComponent *uic;
		BonoboWindow *active_window;

		active_window = bonobo_mdi_get_active_window (BONOBO_MDI (mdi));

		g_return_if_fail (active_window != NULL);

		uic = bonobo_mdi_get_ui_component_from_window(active_window);

		bonobo_ui_component_freeze (uic, NULL);

		ghex_menus_set_verb_list_sensitive (uic, FALSE);
	
		bonobo_ui_component_thaw (uic, NULL);
	}
}
