/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* chartable.c - a window with a character table

   Copyright (C) 1998 - 2002 Free Software Foundation

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include "ghex.h"

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

static gboolean select_chartable_row_cb(GtkTreeView *treeview, GdkEventButton *event, GtkTreeModel *model)
{
	GtkWidget *active_view;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GValue value = {0, };

	selection = gtk_tree_view_get_selection(treeview);

	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return FALSE;

	gtk_tree_model_get_value(model, &iter, 2, &value);

	if(selection == sel_row && event->type == GDK_2BUTTON_PRESS) {
		active_view = bonobo_mdi_get_active_view (BONOBO_MDI (mdi));
		if(active_view) {
			GtkHex *gh = GTK_HEX(active_view);
			hex_document_set_byte(gh->document, (guchar)atoi(g_value_get_string(&value)), gh->cursor_pos,
								  gh->insert, TRUE);
			gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
		}
	}
	g_value_unset(&value);
	sel_row = selection;

	return FALSE;
}

GtkWidget *create_char_table()
{
	static gchar *fmt[] = { NULL, "%02X", "%03d", "%03o" };
	static gchar *titles[] = {  N_("ASCII"), N_("Hex"), N_("Decimal"),
								N_("Octal"), N_("Binary") };
	gchar *real_titles[5];
	GtkWidget *ct, *sw, *ctv;
	GtkListStore *store;
	GtkCellRenderer *cell_renderer;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	int i, col;
	gchar *label, ascii_printable_label[2], bin_label[9], *row[5];

	ct = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(ct), _("GHex: Character table"));
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
			label = g_strdup_printf(fmt[col], i);
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

	gtk_signal_connect(GTK_OBJECT(ct), "delete-event",
					   GTK_SIGNAL_FUNC(delete_event_cb), ct);
	g_signal_connect(G_OBJECT(ctv), "button_press_event",
					   G_CALLBACK(select_chartable_row_cb), GTK_TREE_MODEL(store));

	gtk_container_add(GTK_CONTAINER(sw), ctv);
	gtk_container_add(GTK_CONTAINER(ct), sw);
	gtk_widget_show(ctv);
	gtk_widget_show(sw);

	gtk_widget_set_usize(ct, 320, 256);

	return ct;
}
