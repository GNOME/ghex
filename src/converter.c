/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* converter.c - conversion dialog

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
#include <gnome.h>
#include "ghex.h"

static void conv_entry_cb(GtkEntry *, gint);
static void get_cursor_val_cb(GtkButton *button, Converter *conv);
static void set_values(Converter *conv, gulong val);

Converter converter = { NULL };

void create_converter(Converter *conv) {
	GtkWidget *table, *label, *close, *get;
	
	conv->window = gtk_dialog_new();
	gtk_signal_connect(GTK_OBJECT(conv->window), "delete_event",
					   GTK_SIGNAL_FUNC(delete_event_cb), &conv->window);
	
	gtk_window_set_title(GTK_WINDOW(conv->window), _("GHex: Converter"));
	
	table = gtk_table_new(5, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), GNOME_PAD_SMALL);
	gtk_table_set_col_spacings(GTK_TABLE(table), GNOME_PAD_SMALL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(conv->window)->vbox), table,
					   TRUE, TRUE, 0);
	gtk_widget_show(table);
	
	label = gtk_label_new(_("Binary"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	gtk_widget_show(label);
	conv->entry[0] = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(conv->entry[0]), "activate",
					   GTK_SIGNAL_FUNC(conv_entry_cb), (gpointer)2);
	gtk_table_attach_defaults(GTK_TABLE(table), conv->entry[0], 1, 2, 0, 1);
	gtk_widget_show(conv->entry[0]);
	
	label = gtk_label_new(_("Decimal"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
	gtk_widget_show(label);
	conv->entry[1] = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(conv->entry[1]), "activate",
					   GTK_SIGNAL_FUNC(conv_entry_cb), (gpointer)10);
	gtk_table_attach_defaults(GTK_TABLE(table), conv->entry[1], 1, 2, 1, 2);
	gtk_widget_show(conv->entry[1]);
	
	label = gtk_label_new(_("Hex"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
	gtk_widget_show(label);
	conv->entry[2] = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(conv->entry[2]), "activate",
					   GTK_SIGNAL_FUNC(conv_entry_cb), (gpointer)16);
	gtk_table_attach_defaults(GTK_TABLE(table), conv->entry[2], 1, 2, 2, 3);
	gtk_widget_show(conv->entry[2]);
	
	label = gtk_label_new(_("ASCII"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);
	gtk_widget_show(label);
	conv->entry[3] = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(conv->entry[3]), "activate",
					   GTK_SIGNAL_FUNC(conv_entry_cb), (gpointer)0);
	gtk_table_attach_defaults(GTK_TABLE(table), conv->entry[3], 1, 2, 3, 4);
	gtk_widget_show(conv->entry[3]);
	
	conv->get = get = gtk_button_new_with_label(_("Get cursor value"));
	gtk_signal_connect (GTK_OBJECT (get), "clicked",
						GTK_SIGNAL_FUNC(get_cursor_val_cb), conv);
	gtk_table_attach_defaults(GTK_TABLE(table), get, 0, 2, 4, 5);
	gtk_widget_show(get);

	conv->close = close = gtk_button_new_with_label(_("Close"));
	gtk_signal_connect (GTK_OBJECT (close),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						&conv->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(conv->window)->action_area), close,
					   TRUE, TRUE, 0);
	gtk_widget_show(close);
	
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(conv->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(conv->window)->vbox), 2);
}

static void get_cursor_val_cb(GtkButton *button, Converter *conv) {
	GtkWidget *view = gnome_mdi_get_active_view(mdi);
	guint val, start;

	if(view) {
		start = gtk_hex_get_cursor(GTK_HEX(view));
		start = start - start % GTK_HEX(view)->group_type;
		val = 0;
		do {
			val <<= 8;
			val |= gtk_hex_get_byte(GTK_HEX(view), start);
			start++;
		} while( (start % GTK_HEX(view)->group_type != 0) &&
				 (start < GTK_HEX(view)->document->buffer_size) );

		set_values(conv, val);
	}
}

static void set_values(Converter *conv, gulong val) {
	guchar buffer[33];
	gint i;

	conv->value = val;
	
	for(i = 0; i < 32; i++)
		buffer[i] = ((val & (1L << (31 - i)))?'1':'0');
	buffer[i] = 0;
	gtk_entry_set_text(GTK_ENTRY(conv->entry[0]), buffer);
	
	sprintf(buffer, "%lu", val);
	gtk_entry_set_text(GTK_ENTRY(conv->entry[1]), buffer);
	
	sprintf(buffer, "%08lX", val);
	gtk_entry_set_text(GTK_ENTRY(conv->entry[2]), buffer);
	
	for(i = 0; i < 4; i++) {
		buffer[i] = (val & (0xFF << (3 - i)*8)) >> (3 - i)*8;
		if(buffer[i] < ' ')
			buffer[i] = '_';
	}
	buffer[i] = 0;
	gtk_entry_set_text(GTK_ENTRY(conv->entry[3]), buffer);
}

static void conv_entry_cb(GtkEntry *entry, gint base) {
	guchar buffer[33];
	gchar *text, *endptr;
	gulong val = 0;
	int i, len;
	
	text = gtk_entry_get_text(entry);
	
	switch(base) {
	case 0:
		strncpy(buffer, text, 4);
		buffer[4] = 0;
		for(val = 0, i = 0, len = strlen(buffer); i < len; i++) {
			val <<= 8;
			val |= text[i];
		}
		break;
	case 2:
		strncpy(buffer, text, 32);
		buffer[32] = 0;
		break;
	case 10:
		strncpy(buffer, text, 10);
		buffer[10] = 0;
		break;
	case 16:
		strncpy(buffer, text, 8);
		buffer[8] = 0;
		break;
	}
	
	if(base != 0) {
		val = strtoul(buffer, &endptr, base);
		if(*endptr != 0) {
			converter.value = 0;
			for(i = 0; i < 4; i++)
				gtk_entry_set_text(GTK_ENTRY(converter.entry[i]), _("ERROR"));
			gtk_entry_select_region(entry, 0, -1);
			return;
		}
	}

	if(val == converter.value)
		return;
	
	set_values(&converter, val);
}
