/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* hex-document-ui.c - menu definitions and callbacks for hex-document MDI child

   Copyright (C) 1998 - 2004 Free Software Foundation

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
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "hex-document.h"
#include "ghex-window.h"
#include "gtkhex.h"
#include "findreplace.h"
#include "ui.h"

void
set_doc_menu_sensitivity(HexDocument *doc)
{
	GList *view_node;
	GtkWidget *view;
	gboolean sensitive;
	GHexWindow *win;

	view_node = doc->views;

	while (view_node) {
		view = GTK_WIDGET(view_node->data);

		win = GHEX_WINDOW(gtk_widget_get_toplevel(view));

		g_return_if_fail (win != NULL);
 
		sensitive = doc->undo_top != NULL;
		ghex_window_set_action_sensitive (win, "EditUndo", sensitive);
	
		sensitive = doc->undo_stack && doc->undo_top != doc->undo_stack;
		ghex_window_set_action_sensitive (win, "EditRedo", sensitive);

		view_node = view_node->next;
	}

}

void
find_cb (GtkAction *action,
         gpointer   user_data)
{
	if(!find_dialog)
		find_dialog = create_find_dialog();

	if(!gtk_widget_get_visible(find_dialog->window)) {
		gtk_window_set_position (GTK_WINDOW(find_dialog->window), GTK_WIN_POS_MOUSE);
		gtk_window_set_default(GTK_WINDOW(find_dialog->window), find_dialog->f_next);
		gtk_widget_show(find_dialog->window);
	}
	raise_and_focus_widget(find_dialog->window);
}

void
advanced_find_cb (GtkAction *action,
                  gpointer   user_data)
{
	GHexWindow *win = GHEX_WINDOW(user_data);
	if (!win->advanced_find_dialog)
		win->advanced_find_dialog = create_advanced_find_dialog(win);

	if(!gtk_widget_get_visible(win->advanced_find_dialog->window)) {
		gtk_window_set_position (GTK_WINDOW(win->advanced_find_dialog->window), GTK_WIN_POS_MOUSE);
		gtk_window_set_default(GTK_WINDOW(win->advanced_find_dialog->window),
							   win->advanced_find_dialog->f_close);
		gtk_widget_show(win->advanced_find_dialog->window);
	}
	raise_and_focus_widget(win->advanced_find_dialog->window);
}


void
replace_cb (GtkAction *action,
            gpointer   user_data)
{
	if(!replace_dialog)
		replace_dialog = create_replace_dialog();

	if(!gtk_widget_get_visible(replace_dialog->window)) {
		gtk_window_set_position (GTK_WINDOW(replace_dialog->window), GTK_WIN_POS_MOUSE);
		gtk_window_set_default(GTK_WINDOW(replace_dialog->window), replace_dialog->next);
		gtk_widget_show(replace_dialog->window);
	}
	raise_and_focus_widget(replace_dialog->window);
}

void
jump_cb (GtkAction *action,
         gpointer   user_data)
{
	if(!jump_dialog)
		jump_dialog = create_jump_dialog();

	if(!gtk_widget_get_visible(jump_dialog->window)) {
		gtk_window_set_position (GTK_WINDOW(jump_dialog->window), GTK_WIN_POS_MOUSE);
		gtk_window_set_default(GTK_WINDOW(jump_dialog->window), jump_dialog->ok);
		gtk_widget_show(jump_dialog->window);
	}
	raise_and_focus_widget(jump_dialog->window);
}

void
undo_cb (GtkAction *action,
         gpointer   user_data)
{
	GHexWindow *win = GHEX_WINDOW(user_data);
	HexDocument *doc;
	HexChangeData *cd;

	if(win->gh == NULL)
		return;

	doc = win->gh->document;

	if(doc->undo_top) {
		cd = (HexChangeData *)doc->undo_top->data;

		hex_document_undo(doc);

		gtk_hex_set_cursor(win->gh, cd->start);
		gtk_hex_set_nibble(win->gh, cd->lower_nibble);
	}
}

void
redo_cb (GtkAction *action,
         gpointer   user_data)
{
	GHexWindow *win = GHEX_WINDOW(user_data);
	HexDocument *doc;
	HexChangeData *cd;

	if(win->gh == NULL)
		return;

	doc = win->gh->document;

	if(doc->undo_stack && doc->undo_top != doc->undo_stack) {
		hex_document_redo(doc);

		cd = (HexChangeData *)doc->undo_top->data;

		gtk_hex_set_cursor(win->gh, cd->start);
		gtk_hex_set_nibble(win->gh, cd->lower_nibble);
	}
}
