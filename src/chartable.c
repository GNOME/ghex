/* vim: colorcolumn=80 ts=4 sw=4
 */
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

#include <gtkhex.h>
#include "chartable.h"
//#include "ghex-window.h"
//#include "ui.h"

/* STATIC GLOBALS */

static GtkTreeSelection *sel_row = NULL;
static GtkHex *gh_glob = NULL;

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
insert_char (GtkTreeView *treeview, GtkTreeModel *model)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GValue value = { 0 };
	HexDocument *doc;

	g_return_if_fail (GTK_IS_HEX(gh_glob));

	doc = gtk_hex_get_document (gh_glob);

	selection = gtk_tree_view_get_selection(treeview);
	if (! gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get_value(model, &iter, 2, &value);
	if (selection == sel_row) {
		hex_document_set_byte (doc,
				(guchar)atoi(g_value_get_string(&value)),
				gtk_hex_get_cursor (gh_glob),
				gtk_hex_get_insert_mode (gh_glob),
				TRUE);	// undoable

		gtk_hex_set_cursor (gh_glob, gtk_hex_get_cursor (gh_glob) + 1);
	}
	g_value_unset(&value);
	sel_row = selection;
}

static void
chartable_row_activated_cb (GtkTreeView *tree_view,
		GtkTreePath *path,
		GtkTreeViewColumn *column,
		gpointer user_data)
{
	GtkTreeModel *model = GTK_TREE_MODEL(user_data);

	g_debug("%s: start", __func__);

	insert_char (tree_view, model);
}


#if 0
static gboolean select_chartable_row_cb(GtkTreeView *treeview, GdkEventButton *event, gpointer data)
{
	GtkTreeModel *model = GTK_TREE_MODEL(data);

	if(event->type == GDK_2BUTTON_PRESS)
		insert_char(treeview, model);
	return FALSE;
}
#endif

static void hide_chartable_cb (GtkButton *button, gpointer user_data)
{
	GtkWindow *win = GTK_WINDOW(user_data);

	(void)button;	/* unused */

	gtk_window_close (win);
}

GtkWidget *create_char_table(GtkWindow *parent_win, GtkHex *gh)
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

	/* set global GtkHex widget */
	g_assert (GTK_IS_HEX(gh));
	gh_glob = gh;

	/* Create our char table window and set as child of parent window,
	 * if requested.
	 */
	ct = gtk_window_new ();

	if (parent_win) {
		g_assert (GTK_IS_WINDOW (parent_win));

		gtk_window_set_transient_for (GTK_WINDOW(ct), parent_win);
	}

	gtk_window_set_title(GTK_WINDOW(ct), _("Character table"));

	sw = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	for(i = 0; i < 5; i++)
		real_titles[i] = _(titles[i]);

	store = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	cell_renderer = gtk_cell_renderer_text_new();
	ctv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_widget_set_hexpand (ctv, TRUE);
	gtk_widget_set_vexpand (ctv, TRUE);

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
#if defined(__GNUC__) && (__GNUC__ > 4)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
			label = g_strdup_printf(fmt[col], i);
#if defined(__GNUC__) && (__GNUC__ > 4)
#pragma GCC diagnostic pop
#endif
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

	g_signal_connect (ctv, "row-activated",
			G_CALLBACK(chartable_row_activated_cb), GTK_TREE_MODEL(store));

	// REWRITE
#if 0
	g_signal_connect(G_OBJECT(ct), "delete-event",
					 G_CALLBACK(delete_event_cb), ct);
	g_signal_connect(G_OBJECT(ctv), "button_press_event",
					 G_CALLBACK(select_chartable_row_cb), GTK_TREE_MODEL(store));
	g_signal_connect(G_OBJECT(ctv), "key_press_event",
					 G_CALLBACK(key_press_cb), GTK_TREE_MODEL(store));
#endif

	gtk_widget_grab_focus(ctv);

	cbtn = gtk_button_new_with_mnemonic (_("_Close"));
	g_signal_connect(G_OBJECT (cbtn), "clicked",
					G_CALLBACK(hide_chartable_cb), ct);

	lbl = gtk_label_new ("");
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

	gtk_box_append (GTK_BOX(vbox), sw);
	gtk_box_append (GTK_BOX(hbox), lbl);
	gtk_box_append (GTK_BOX(hbox), cbtn);
	gtk_box_append (GTK_BOX(vbox), hbox);

	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW(sw), ctv);
	gtk_window_set_child (GTK_WINDOW (ct), vbox);

	gtk_widget_set_size_request(ct, 320, 320);

	return ct;
}
