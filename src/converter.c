/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* converter.c - conversion dialog

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

   Authors:
   Jaka Mocnik <jaka@gnu.org>
   Chema Celorio <chema@gnome.org>
*/

#include <config.h>
#include <gnome.h>
#include <ctype.h>      /* for isdigit */
#include "ghex.h"

static void conv_entry_cb(GtkEntry *, gint);
static void get_cursor_val_cb(GtkButton *button, Converter *conv);
static void set_values(Converter *conv, gulong val);

Converter *converter = NULL;

static gboolean
is_char_ok(char c, gint base)
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
			gtk_signal_emit_stop_by_name(GTK_OBJECT(editable), "insert_text");
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
	gtk_signal_emit_stop_by_name(GTK_OBJECT(editable), "insert_text");
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
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");

	return FALSE;
}

static GtkWidget *
create_converter_entry(const gchar *name, GtkWidget *table,
						GtkAccelGroup *accel_group, gint pos, gint base)
{
	GtkWidget *label;
    GtkWidget *entry;
    gint accel_key;

	/* label */
	label = gtk_label_new(name);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, pos, pos+1);
	accel_key = gtk_label_parse_uline(GTK_LABEL(label), name);
	gtk_widget_show(label);

	/* entry */
	entry = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(entry), "activate",
					   GTK_SIGNAL_FUNC(conv_entry_cb), GINT_TO_POINTER(base));
	gtk_signal_connect(GTK_OBJECT(entry), "key_press_event",
						GTK_SIGNAL_FUNC(entry_key_press_event_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(entry), "insert_text",
						GTK_SIGNAL_FUNC(entry_filter), GINT_TO_POINTER(base));
	gtk_widget_add_accelerator(entry, "grab_focus", accel_group, accel_key,
							   GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
	gtk_table_attach_defaults(GTK_TABLE(table), entry, 1, 2, pos, pos+1);
	gtk_widget_show(entry);
       
	return entry;
}
       
static GtkWidget *
create_converter_button(const gchar *name, GtkAccelGroup *accel_group)
{
	GtkWidget *button;
    GtkWidget *label;
    gint accel_key;

	button = gtk_button_new();
    label = gtk_label_new(name);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
	accel_key = gtk_label_parse_uline(GTK_LABEL(label), name);
	gtk_container_add(GTK_CONTAINER(button), label);
	gtk_widget_add_accelerator(button, "clicked", accel_group, accel_key,
							   GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
	gtk_widget_show(label);
	gtk_widget_show(button);

	return button;
}    

static void
close_converter(GtkWidget *button, GnomeDialog *dialog)
{
	gnome_dialog_close(dialog);
}

Converter *
create_converter()
{
	Converter *conv;
	GtkWidget *table, *get;
	GtkAccelGroup *accel_group;
 
	conv = g_new0(Converter, 1);

	conv->window = gnome_dialog_new(_("GHex: Converter"),
									 GNOME_STOCK_BUTTON_CLOSE, NULL);
	gnome_dialog_close_hides(GNOME_DIALOG(conv->window), TRUE);
	gnome_dialog_button_connect(GNOME_DIALOG(conv->window), 0,
								 close_converter, conv->window);

	table = gtk_table_new(6, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), GNOME_PAD_SMALL);
	gtk_table_set_col_spacings(GTK_TABLE(table), GNOME_PAD_SMALL);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(conv->window)->vbox), table,
					   TRUE, TRUE, 0);
	gtk_container_border_width(GTK_CONTAINER(GNOME_DIALOG(conv->window)->vbox),
							   2);
	gtk_box_set_spacing(GTK_BOX(GNOME_DIALOG(conv->window)->vbox), 2);
	gtk_widget_show(table);
	
	accel_group = gtk_accel_group_new();

	/* entries */
	conv->entry[0] = create_converter_entry(_("_Binary"), table,
											 accel_group, 0, 2);
	conv->entry[1] = create_converter_entry(_("_Octal"), table,
											 accel_group, 1, 8);
	conv->entry[2] = create_converter_entry(_("_Decimal"), table,
											 accel_group, 2, 10);
	conv->entry[3] = create_converter_entry(_("_Hex"), table,
											 accel_group, 3, 16);
	conv->entry[4] = create_converter_entry(_("_ASCII"), table,
											 accel_group, 4, 0);

	/* get cursor button */
	get = create_converter_button(_("_Get cursor value"), accel_group);

	gtk_signal_connect(GTK_OBJECT(get), "clicked",
						GTK_SIGNAL_FUNC(get_cursor_val_cb), conv);
	gtk_table_attach_defaults(GTK_TABLE(table), get, 0, 2, 5, 6);

	/* add the accelerators */
	gtk_window_add_accel_group(GTK_WINDOW(conv->window), accel_group);

	return conv;
}

static void
get_cursor_val_cb(GtkButton *button, Converter *conv)
{
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
		} while((start % GTK_HEX(view)->group_type != 0) &&
				(start < GTK_HEX(view)->document->file_size) );

		set_values(conv, val);
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
set_values(Converter *conv, gulong val)
{
	guchar buffer[CONV_BUFFER_LEN + 1];
	gint i;

	conv->value = val;
	
	for(i = 0; i < 32; i++)
		buffer[i] =((val & (1L << (31 - i)))?'1':'0');
	buffer[i] = 0;
	gtk_entry_set_text(GTK_ENTRY(conv->entry[0]), clean(buffer));

	g_snprintf(buffer, CONV_BUFFER_LEN, "%o",(unsigned int)val);
	gtk_entry_set_text(GTK_ENTRY(conv->entry[1]), buffer);
	
	g_snprintf(buffer, CONV_BUFFER_LEN, "%lu", val);
	gtk_entry_set_text(GTK_ENTRY(conv->entry[2]), buffer);
	
	g_snprintf(buffer, CONV_BUFFER_LEN, "%08lX", val);
	gtk_entry_set_text(GTK_ENTRY(conv->entry[3]), buffer);
	
	for(i = 0; i < 4; i++) {
		buffer[i] =(val & (0xFF << (3 - i)*8)) >> (3 - i)*8;
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
	gchar *text, *endptr;
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
			gtk_entry_select_region(entry, 0, -1);
			return;
		}
	}

	if(val == converter->value)
		return;
	
	set_values(converter, val);
}
