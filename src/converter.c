/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* converter.c - conversion dialog

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

   Authors:
   Jaka Mocnik <jaka@gnu.org>
   Chema Celorio <chema@gnome.org>
*/

#include <config.h>
#include <gnome.h>
#include <ctype.h>      /* for isdigit */
#include <string.h>     /* for strncpy */

#include "ghex.h"

static void conv_entry_cb(GtkEntry *, gint);
static void get_cursor_val_cb(GtkButton *button, Converter *conv);
static void set_values(Converter *conv, gulong val, guint group);

Converter *converter = NULL;

static gboolean
is_char_ok(signed char c, gint base)
{
	/* ASCII which is base 0 is always ok */
	if(base == 0)
		return TRUE;

	/* Normalize A-F to 10-15 */
	if(isalpha(c))
		c = toupper(c) - 'A' + '9' + 1;
	else if(!isdigit(c))
		return FALSE;

	c = c - '0';

	return((c < 0) ||(c >(base - 1))) ? FALSE : TRUE;
}

static void
entry_filter(GtkEditable *editable, const gchar *text, gint length,
			 gint *pos, gpointer data)
{
	int i, l = 0;
	char *s = NULL;
	gint base = GPOINTER_TO_INT(data);

	/* thou shalt optimize for the common case */
	if(length == 1) {
		if(!is_char_ok(*text, base)) {
			gdk_beep();
			g_signal_stop_emission_by_name(G_OBJECT(editable), "insert_text");
		}
		return;
	}

	/* if it is a paste we have to check all of things */
	s = g_new0(gchar, length);
	
	for(i=0; i<length; i++) {
		if(is_char_ok(text[i], base)) {
			s[l++] = text[i];
		}
	}

	if(l == length)
		return;

	gdk_beep();
	g_signal_stop_emission_by_name(G_OBJECT(editable), "insert_text");
#if 0 /* Pasting is not working. gtk is doing weird things. Chema */
	gtk_signal_handler_block_by_func(GTK_OBJECT(editable), entry_filter,
									 data);
	g_print("Inserting -->%s<--- of length %i in pos %i\n", s, l, *pos);
	gtk_editable_insert_text(editable, s, l, pos);
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(editable), entry_filter,
									   data);
#endif 
}

static gint
entry_key_press_event_cb(GtkWidget *widget, GdkEventKey *event,
						 gpointer not_used)
{
	/* For gtk_entries don't let the gtk handle the event if
	 * the alt key was pressed. Otherwise the accelerators do not work cause
	 * gtk takes care of this event
	 */
	if(event->state & GDK_MOD1_MASK)
		g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");

	return FALSE;
}

#define BUFFER_LEN 40

static GtkWidget *
create_converter_entry(const gchar *name, GtkWidget *table,
						GtkAccelGroup *accel_group, gint pos, gint base)
{
	GtkWidget *label;
    GtkWidget *entry;
#if 0
    gint accel_key;
#endif /* 0/1 */
    gchar str[BUFFER_LEN + 1];

	/* label */
	label = gtk_label_new_with_mnemonic(name);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, pos, pos+1);
#if 0
	accel_key = gtk_label_parse_uline(GTK_LABEL(label), name);
#endif /* 0/1 */
	gtk_widget_show(label);

	/* entry */
	entry = gtk_entry_new();
	g_signal_connect(G_OBJECT(entry), "activate",
					 G_CALLBACK(conv_entry_cb), GINT_TO_POINTER(base));
	g_signal_connect(G_OBJECT(entry), "key_press_event",
					 G_CALLBACK(entry_key_press_event_cb), NULL);
	g_signal_connect(G_OBJECT(entry), "insert_text",
					 G_CALLBACK(entry_filter), GINT_TO_POINTER(base));
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
#if 0
	gtk_widget_add_accelerator(entry, "grab_focus", accel_group, accel_key,
							   GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
#endif /* 0/1 */
	gtk_table_attach_defaults(GTK_TABLE(table), entry, 1, 2, pos, pos+1);
	gtk_widget_show(entry);

	if (GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (entry))) {
		g_snprintf (str, BUFFER_LEN, "Displays the value at cursor in %s", name+1);
		add_atk_namedesc (entry, name+1, str);
		add_atk_relation (entry, label, ATK_RELATION_LABELLED_BY);
	}
       
	return entry;
}
       
static GtkWidget *
create_converter_button(const gchar *name, GtkAccelGroup *accel_group)
{
	GtkWidget *button;
    GtkWidget *label;
#if 0
    gint accel_key;
#endif /* 0/1 */

	button = gtk_button_new();
    label = gtk_label_new(name);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
#if 0
	accel_key = gtk_label_parse_uline(GTK_LABEL(label), name);
#endif /* 0/1 */
	gtk_container_add(GTK_CONTAINER(button), label);
#if 0
	gtk_widget_add_accelerator(button, "clicked", accel_group, accel_key,
							   GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
#endif /* 0/1 */
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);

	gtk_widget_show(label);
	gtk_widget_show(button);

	return button;
}    

/* Made global. We need to access this in ui.c */
GtkWidget *converter_get = NULL;

static void
close_converter(GtkWidget *dialog, gint response, gpointer user_data)
{
	ghex_window_sync_converter_item(NULL, FALSE);
	gtk_widget_hide(GTK_WIDGET(dialog));
}

static gboolean
converter_delete_event_cb(GtkWidget *widget, GdkEventAny *e, gpointer user_data)
{
	ghex_window_sync_converter_item(NULL, FALSE);
	gtk_widget_hide(widget);
	return TRUE;
}

static gboolean
conv_key_press_cb (GtkWidget *widget, GdkEventKey *e, gpointer user_data)
{
	if (e->keyval == GDK_Escape) {
		converter_delete_event_cb(widget, (GdkEventAny *)e, user_data);
		return TRUE;
	}
	return FALSE;
}

Converter *
create_converter()
{
	Converter *conv;
	GtkWidget *table;
	GtkAccelGroup *accel_group;
	gint i;
 
	conv = g_new0(Converter, 1);

	conv->window = gtk_dialog_new_with_buttons(_("Base Converter"),
											   NULL, 0,
											   GTK_STOCK_CLOSE,
											   GTK_RESPONSE_OK,
											   NULL);
	g_signal_connect(G_OBJECT(conv->window), "response",
					 G_CALLBACK(close_converter), conv->window);

	table = gtk_table_new(6, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), GNOME_PAD_SMALL);
	gtk_table_set_col_spacings(GTK_TABLE(table), GNOME_PAD_SMALL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(conv->window)->vbox), table,
					   TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(conv->window)->vbox),
								   GNOME_PAD_SMALL);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(conv->window)->vbox), 2);
	gtk_widget_show(table);
	
	accel_group = gtk_accel_group_new();

	/* entries */
	conv->entry[0] = create_converter_entry(_("_Binary:"), table,
											 accel_group, 0, 2);
	conv->entry[1] = create_converter_entry(_("_Octal:"), table,
											 accel_group, 1, 8);
	conv->entry[2] = create_converter_entry(_("_Decimal:"), table,
											 accel_group, 2, 10);
	conv->entry[3] = create_converter_entry(_("_Hex:"), table,
											 accel_group, 3, 16);
	conv->entry[4] = create_converter_entry(_("_ASCII:"), table,
											 accel_group, 4, 0);

	/* get cursor button */
	converter_get = create_converter_button(_("_Get cursor value"), accel_group);

	g_signal_connect(G_OBJECT(conv->window), "delete_event",
					 G_CALLBACK(converter_delete_event_cb), conv);
	g_signal_connect(G_OBJECT(converter_get), "clicked",
					 G_CALLBACK(get_cursor_val_cb), conv);
	g_signal_connect(G_OBJECT(conv->window), "key_press_event",
					 G_CALLBACK(conv_key_press_cb), conv);
 	gtk_table_attach_defaults(GTK_TABLE(table), converter_get, 0, 2, 5, 6);

	/* add the accelerators */
	gtk_window_add_accel_group(GTK_WINDOW(conv->window), accel_group);

	if (GTK_IS_ACCESSIBLE(gtk_widget_get_accessible(converter_get))) {
		add_atk_namedesc (converter_get, _("Get cursor value"), _("Gets the value at cursor in binary, octal, decimal, hex and ASCII"));
		for (i=0; i<5; i++) {
			add_atk_relation (conv->entry[i], converter_get, ATK_RELATION_CONTROLLED_BY);
			add_atk_relation (converter_get, conv->entry[i], ATK_RELATION_CONTROLLER_FOR);
		}
	}

	return conv;
}

static void
get_cursor_val_cb(GtkButton *button, Converter *conv)
{
	GtkWidget *view;
	guint val, start;
	GHexWindow *win = ghex_window_get_active();

	if(win == NULL || win->gh == NULL)
		return;

	view = GTK_WIDGET(win->gh);

	if(view) {
		start = gtk_hex_get_cursor(GTK_HEX(view));
		start = start - start % GTK_HEX(view)->group_type;
		val = 0;
		do {
			val <<= 8;
			val |= gtk_hex_get_byte(GTK_HEX(view), start);
			start++;
		} while((start % GTK_HEX(view)->group_type != 0) &&
				(start < GTK_HEX(view)->document->file_size) );

		set_values(conv, val, GTK_HEX(view)->group_type);
	}
}

static guchar *
clean(guchar *ptr)
{
	while(*ptr == '0')
		ptr++;
	return ptr;
}

#define CONV_BUFFER_LEN 32

static void
set_values(Converter *conv, gulong val, guint group)
{
	guchar buffer[CONV_BUFFER_LEN + 1];
	gint i;

	if(group == 0) {
		if(val > 0xFFFF)
			group = 4;
		else if(val > 0xFF)
			group = 2;
		else
			group = 1;
	}

	conv->value = val;
	
	for(i = 0; i < 32; i++)
		buffer[i] =((val & (1L << (31 - i)))?'1':'0');
	buffer[i] = 0;
	gtk_entry_set_text(GTK_ENTRY(conv->entry[0]), clean(buffer));

	g_snprintf(buffer, CONV_BUFFER_LEN, "%o",(unsigned int)val);
	gtk_entry_set_text(GTK_ENTRY(conv->entry[1]), buffer);
	
	g_snprintf(buffer, CONV_BUFFER_LEN, "%lu", val);
	gtk_entry_set_text(GTK_ENTRY(conv->entry[2]), buffer);
	
	for(i = 0; i < group; i++) {
		g_snprintf(buffer + 2*i, CONV_BUFFER_LEN - 2*i,
				   "%02x", (guint8)((val & (0xFF << ((group - 1 - i)*8)))) >> (group - 1 - i)*8);
	}
	gtk_entry_set_text(GTK_ENTRY(conv->entry[3]), buffer);
	
	for(i = 0; i < group; i++) {
		buffer[i] = ((val & (0xFF << ((group - 1 - i)*8)))) >> (group - 1 - i)*8;
		if(buffer[i] < ' ')
			buffer[i] = '_';
	}
	buffer[i] = 0;
	gtk_entry_set_text(GTK_ENTRY(conv->entry[4]), buffer);
}

static void
conv_entry_cb(GtkEntry *entry, gint base)
{
	guchar buffer[33];
	const gchar *text;
	gchar *endptr;
	gulong val = converter->value;
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
	case 8:
		strncpy(buffer, text, 12);
		buffer[12] = 0;
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
			converter->value = 0;
			for(i = 0; i < 5; i++)
				gtk_entry_set_text(GTK_ENTRY(converter->entry[i]), _("ERROR"));
			gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
			return;
		}
	}

	if(val == converter->value)
		return;
	
	set_values(converter, val, 0);
}
