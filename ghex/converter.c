/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* converter.c - conversion dialog

   Copyright (C) 1998 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021 Logan Rathbone <poprocks@gmail.com>

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

   Original authors of this file:
   Jaka Mocnik <jaka@gnu.org> (Original GHex author)
   Chema Celorio <chema@gnome.org>
*/

#include "gtkhex.h"
#include "converter.h"

#include <ctype.h>      /* for isdigit */
#include <stdlib.h>     /* for strtoul */
#include <string.h>     /* for strncpy */

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include <config.h>

/* OPAQUE DATATYPES */

typedef struct _Converter {
	GtkWidget *window;
	HexWidget *gh;
	GtkWidget *entry[5];
	GtkWidget *close;
	GtkWidget *get;

	gulong value;
} Converter;

/* FUNCTION DECLARATIONS */

static void conv_entry_cb(GtkEntry *, gint);
static void get_cursor_val_cb(GtkButton *button, Converter *conv);
static void set_values(Converter *conv, gulong val);
static gchar * clean(gchar *ptr);

/* STATIC GLOBALS */

static Converter *converter = NULL;

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

	return ((c < 0) ||(c >(base - 1))) ? FALSE : TRUE;
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

	g_signal_stop_emission_by_name(G_OBJECT(editable), "insert_text");
}

#define BUFFER_LEN 40

static GtkWidget *
create_converter_entry(const gchar *name, GtkWidget *grid, gint pos, gint base)
{
	GtkWidget *label;
    GtkWidget *entry;
    gchar str[BUFFER_LEN + 1];

	/* label */
	label = gtk_label_new_with_mnemonic(name);
	gtk_grid_attach (GTK_GRID (grid), label, 0, pos, 1, 1);

	/* entry */
	entry = gtk_entry_new();
	g_signal_connect(G_OBJECT(entry), "activate",
					 G_CALLBACK(conv_entry_cb), GINT_TO_POINTER(base));
	g_signal_connect(G_OBJECT(entry), "insert-text",
					 G_CALLBACK(entry_filter), GINT_TO_POINTER(base));

	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_widget_set_hexpand (entry, TRUE);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, pos, 1, 1);
       
	return entry;
}

GtkWidget *create_converter (GtkWindow *parent_win, /* can-NULL */
		HexWidget *gh)
{
	Converter *conv;
	GtkWidget *grid;
	GtkWidget *converter_get;
	GtkWidget *close_btn;
	int i;
 
	conv = g_new0(Converter, 1);

	/* set global for usage in other functions. */
	converter = conv;

	/* set struct's HexWidget widget */
	g_assert (HEX_IS_WIDGET(gh));
	conv->gh = gh;

	conv->window = gtk_window_new ();
	gtk_window_set_transient_for (GTK_WINDOW(conv->window), parent_win);
	gtk_window_set_title (GTK_WINDOW(conv->window), _("Base Converter"));

	grid = gtk_grid_new ();
	gtk_widget_set_name (grid, "converter-grid");
	gtk_grid_set_row_spacing (GTK_GRID (grid), 4);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 4);

	gtk_window_set_child (GTK_WINDOW(conv->window), grid);

	/* entries */
	conv->entry[0] = create_converter_entry (_("_Binary:"), grid,
											 0, 2);
	conv->entry[1] = create_converter_entry (_("_Octal:"), grid,
											 1, 8);
	conv->entry[2] = create_converter_entry (_("_Decimal:"), grid,
											 2, 10);
	conv->entry[3] = create_converter_entry (_("_Hex:"), grid,
											 3, 16);
	conv->entry[4] = create_converter_entry (_("_ASCII:"), grid,
											 4, 0);

	/* get cursor button */
	converter_get = gtk_button_new_with_mnemonic (_("_Get cursor value"));
	close_btn = gtk_button_new_with_mnemonic (_("_Close"));

	g_signal_connect (converter_get, "clicked", G_CALLBACK(get_cursor_val_cb), conv);
	g_signal_connect_swapped (close_btn, "clicked", G_CALLBACK(gtk_window_close), conv->window);

	gtk_grid_attach (GTK_GRID (grid), converter_get, 0, 5, 2, 1);
	gtk_grid_attach (GTK_GRID (grid), close_btn, 0, 6, 2, 1);

	gtk_accessible_update_property (GTK_ACCESSIBLE(converter_get),
			GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
			_("Gets the value at cursor in binary, octal, decimal, hex and ASCII"),
			-1);

	return conv->window;
}

static void
get_cursor_val_cb(GtkButton *button, Converter *conv)
{
	guint val, start;
	guint group_type;
	HexDocument *doc;
	size_t payload;

	g_return_if_fail (HEX_IS_WIDGET(conv->gh));

	doc = hex_widget_get_document (conv->gh);
	payload = hex_buffer_get_payload_size (hex_document_get_buffer (doc));
	group_type = hex_widget_get_group_type (conv->gh);
	start = hex_widget_get_cursor (conv->gh);
	start = start - start % group_type;

	val = 0;
	do {
		val <<= 8;
		val |= hex_widget_get_byte(conv->gh, start);
		start++;
	} while((start % group_type != 0) &&
			(start < payload) );

	set_values(conv, val);
}

static gchar *
clean(gchar *ptr)
{
	while(*ptr == '0')
		ptr++;
	return ptr;
}

#define CONV_BUFFER_LEN 32

static void
set_values(Converter *conv, gulong val)
{
	gchar buffer[CONV_BUFFER_LEN + 1];
	gint i, nhex, nbytes;
	gulong tmp = val;

	nhex = 0;
	while(tmp > 0) {
		tmp = tmp >> 4;
		nhex++;
	}
	if(nhex == 0)
		nhex = 1;
	nbytes = nhex/2; 

	conv->value = val;
	
	for(i = 0; i < 32; i++)
		buffer[i] =((val & (1L << (31 - i)))?'1':'0');
	buffer[i] = 0;
	gtk_editable_set_text (GTK_EDITABLE(conv->entry[0]), clean(buffer));

	g_snprintf(buffer, CONV_BUFFER_LEN, "%o",(unsigned int)val);
	gtk_editable_set_text (GTK_EDITABLE(conv->entry[1]), buffer);
	
	g_snprintf(buffer, CONV_BUFFER_LEN, "%lu", val);
	gtk_editable_set_text (GTK_EDITABLE(conv->entry[2]), buffer);

	for(i = 0, tmp = val; i < nhex; i++) {
		buffer[nhex - i - 1] = (tmp & 0x0000000FL);
		if(buffer[nhex - i - 1] < 10)
			buffer[nhex - i - 1] += '0';
		else
			buffer[nhex - i - 1] += 'A' - 10;
		tmp = tmp >> 4;
	}
	buffer[i] = '\0';
	gtk_editable_set_text (GTK_EDITABLE(conv->entry[3]), buffer);
	
	for(i = 0, tmp = val; i < nbytes; i++) {
		buffer[nbytes - i - 1] = tmp & 0x000000FF;
		if(buffer[nbytes - i - 1] < 0x20 || buffer[nbytes - i - 1] >= 0x7F)
			buffer[nbytes - i - 1] = '_';
		tmp = tmp >> 8;
	}
	buffer[i] = 0;
	gtk_editable_set_text (GTK_EDITABLE(conv->entry[4]), buffer);
}

static void
conv_entry_cb(GtkEntry *entry, gint base)
{
	gchar buffer[33];
	const gchar *text;
	gchar *endptr;
	gulong val;
	int i, len;
	
	g_return_if_fail (converter);

	/* shorthand. */
	val = converter->value;
	text = gtk_entry_buffer_get_text (gtk_entry_get_buffer (entry));
	
	switch (base) {
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
				gtk_editable_set_text (GTK_EDITABLE(converter->entry[i]), _("ERROR"));
			gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
			return;
		}
	}

	if(val == converter->value)
		return;
	
	set_values(converter, val);
}
