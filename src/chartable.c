/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* chartable.c - a window with a character table

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "chartable.h"
#include "ghex-window.h"
#include "ui.h"

GtkWidget *char_table = NULL;
static GtkTreeSelection *sel_row = NULL;

static char *ascii_non_printable_label[] = {
	"NUL",
	"SOH",
	"STX",
	"ETX",
	"EOT",
	"ENQ",
	"ACK",
	"BEL",
	"BS",
	"TAB",
	"LF",
	"VT",
	"FF",
	"CR",
	"SO",
	"SI",
	"DLE",
	"DC1",
	"DC2",
	"DC3",
	"DC4",
	"NAK",
	"SYN",
	"ETB",
	"CAN",
	"EM",
	"SUB",
	"ESC",
	"FS",
	"GS",
	"RS",
	"US"
};

static void
insert_char(GtkTreeView *treeview, GtkTreeModel *model)
{
	GHexWindow *win;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GValue value = { 0 };

	selection = gtk_tree_view_get_selection(treeview);
	if(!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;
	gtk_tree_model_get_value(model, &iter, 2, &value);
	if(selection == sel_row) {
		win = ghex_window_get_active();
		if(win->gh) {
			hex_document_set_byte(win->gh->document, (guchar)atoi(g_value_get_string(&value)), win->gh->cursor_pos,
								  win->gh->insert, TRUE);
			gtk_hex_set_cursor(win->gh, win->gh->cursor_pos + 1);
		}
	}
	g_value_unset(&value);
	sel_row = selection;
}

static gboolean select_chartable_row_cb(GtkTreeView *treeview, GdkEventButton *event, gpointer data)
{
	GtkTreeModel *model = GTK_TREE_MODEL(data);

	if(event->type == GDK_2BUTTON_PRESS)
		insert_char(treeview, model);
	return FALSE;
}

static void hide_chartable_cb (GtkWidget *widget, GtkWidget *win)
{
    /* widget may be NULL if called from keypress cb! */
	ghex_window_sync_char_table_item(NULL, FALSE);
	gtk_widget_hide(win);
}

static gint char_table_key_press_cb (GtkWindow *w, GdkEventKey *e, gpointer data)
{
	if (e->keyval == GDK_KEY_Escape) {
		hide_chartable_cb(NULL, GTK_WIDGET(w));
		return TRUE;
	}
	return FALSE;
}

static gint key_press_cb (GtkTreeView *treeview, GdkEventKey *e, gpointer data)
{
	GtkTreeModel *model = GTK_TREE_MODEL(data);

	if (e->keyval == GDK_KEY_Return) {
		insert_char(treeview, model);
		return TRUE;
	}
	return FALSE;
}

static gboolean
char_table_delete_event_cb(GtkWidget *widget, GdkEventAny *e, gpointer user_data)
{
	ghex_window_sync_char_table_item(NULL, FALSE);
	gtk_widget_hide(widget);
	return TRUE;
}

GtkWidget *create_char_table()
{
	static gchar *fmt[] = { NULL, "%02X", "%03d", "%03o" };
	static gchar *titles[] = {  N_("ASCII"), N_("Hex"), N_("Decimal"),
								N_("Octal"), N_("Binary") };
	gchar *real_titles[5];
	GtkWidget *ct, *sw, *ctv, *cbtn, *vbox, *hbox, *lbl;
	GtkListStore *store;
	GtkCellRenderer *cell_renderer;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	int i, col;
	gchar *label, ascii_printable_label[2], bin_label[9], *row[5];

	ct = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(ct), "delete_event",
					 G_CALLBACK(char_table_delete_event_cb), NULL);
	g_signal_connect(G_OBJECT(ct), "key_press_event",
					 G_CALLBACK(char_table_key_press_cb), NULL);
	gtk_window_set_title(GTK_WINDOW(ct), _("Character table"));
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	for(i = 0; i < 5; i++)
		real_titles[i] = _(titles[i]);
	store = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	cell_renderer = gtk_cell_renderer_text_new();
	ctv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	for (i = 0; i < 5; i++) {
		column = gtk_tree_view_column_new_with_attributes (real_titles[i], cell_renderer, "text", i, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(ctv), column);
	}

	bin_label[8] = 0;
	ascii_printable_label[1] = 0;
	for(i = 0; i < 256; i++) {
		if(i < 0x20)
			row[0] = ascii_non_printable_label[i];
		else if(i < 0x7f) {
			ascii_printable_label[0] = i;
			row[0] = ascii_printable_label;
		}
		else
			row[0] = "";
		for(col = 1; col < 4; col++) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
			label = g_strdup_printf(fmt[col], i);
#pragma GCC diagnostic pop
			row[col] = label;
		}
		for(col = 0; col < 8; col++) {
			bin_label[7-col] = (i & (1L << col))?'1':'0';
			row[4] = bin_label;
		}

		gtk_list_store_append(GTK_LIST_STORE(store), &iter);
		gtk_list_store_set(GTK_LIST_STORE(store), &iter,
				   0, row[0],
				   1, row[1],
				   2, row[2],
				   3, row[3],
				   4, row[4],
				   -1);

		for(col = 1; col < 4; col++)
			g_free(row[col]);
	}

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW (ctv));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

	g_signal_connect(G_OBJECT(ct), "delete-event",
					 G_CALLBACK(delete_event_cb), ct);
	g_signal_connect(G_OBJECT(ctv), "button_press_event",
					 G_CALLBACK(select_chartable_row_cb), GTK_TREE_MODEL(store));
	g_signal_connect(G_OBJECT(ctv), "key_press_event",
					 G_CALLBACK(key_press_cb), GTK_TREE_MODEL(store));
	gtk_widget_grab_focus(ctv);

	cbtn = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_widget_show(cbtn);
	g_signal_connect(G_OBJECT (cbtn), "clicked",
					G_CALLBACK(hide_chartable_cb), ct);

	lbl = gtk_label_new ("");
	gtk_widget_show(lbl);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_show(vbox);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_show(hbox);

	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 4);
	gtk_box_pack_start(GTK_BOX(hbox), cbtn, FALSE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(sw), ctv);
	gtk_container_add(GTK_CONTAINER(ct), vbox);
	gtk_widget_show(ctv);
	gtk_widget_show(sw);

	gtk_widget_set_size_request(ct, 320, 256);

	return ct;
}
