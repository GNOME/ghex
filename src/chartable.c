/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* chartable.c - a window with a character table

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include "ghex.h"

GtkWidget *char_table = NULL;
static gint sel_row = -1;

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

static void select_chartable_row_cb(GtkCList *cl, gint row, gint col,
									GdkEvent *event)
{
	GtkWidget *active_view;

	if(row == sel_row && event->type == GDK_2BUTTON_PRESS) {
		active_view = gnome_mdi_get_active_view(mdi);
		if(active_view) {
			GtkHex *gh = GTK_HEX(active_view);
			hex_document_set_byte(gh->document, (guchar)row, gh->cursor_pos,
								  gh->insert, TRUE);
			gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
		}
	}

	sel_row = row;
}

GtkWidget *create_char_table()
{
	static gchar *fmt[] = { NULL, "%02X", "%03d", "%03o" };
	static gchar *titles[] = {  N_("ASCII"), N_("Hex"), N_("Decimal"),
								N_("Octal"), N_("Binary") };
	gchar *real_titles[5];
	GtkWidget *ct, *sw, *cl;
	int i, col;
	gchar *label, ascii_printable_label[2], bin_label[9], *row[5];

	ct = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(ct), _("GHex: Character table"));
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	for(i = 0; i < 5; i++)
		real_titles[i] = _(titles[i]);
	cl = gtk_clist_new_with_titles(5, real_titles);
	bin_label[8] = 0;
	ascii_printable_label[1] = 0;
	for(i = 0; i < 256; i++) {
		if(i < 0x20)
			row[0] = ascii_non_printable_label[i];
		else {
			ascii_printable_label[0] = i;
			row[0] = ascii_printable_label;
		}
		for(col = 1; col < 4; col++) {
			label = g_strdup_printf(fmt[col], i);
			row[col] = label;
		}
		for(col = 0; col < 8; col++) {
			bin_label[7-col] = (i & (1L << col))?'1':'0';
			row[4] = bin_label;
		}
		gtk_clist_append(GTK_CLIST(cl), row);
		for(col = 1; col < 4; col++)
			g_free(row[col]);
	}

	gtk_signal_connect(GTK_OBJECT(ct), "delete-event",
					   GTK_SIGNAL_FUNC(delete_event_cb), ct);
	gtk_signal_connect(GTK_OBJECT(cl), "select-row",
					   GTK_SIGNAL_FUNC(select_chartable_row_cb), NULL);

	gtk_container_add(GTK_CONTAINER(sw), cl);
	gtk_container_add(GTK_CONTAINER(ct), sw);
	gtk_widget_show(cl);
	gtk_widget_show(sw);

	gtk_widget_set_usize(ct, 320, 256);

	return ct;
}
