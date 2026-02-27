/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* converter.c - conversion dialog

   Copyright (C) 1998 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021-2026 Logan Rathbone <poprocks@gmail.com>

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
#include "util.h"

#include <ctype.h>      /* for isdigit */
#include <stdlib.h>     /* for strtoul */
#include <string.h>     /* for strncpy */

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include <config.h>

enum
{
	PROP_0,
	PROP_HEX,
	N_PROPERTIES
};

GParamSpec *properties[N_PROPERTIES];

struct _GHexConverter
{
	GtkWindow parent_instance;

	HexView *hex;
	GtkWidget *entry[5];
	GtkWidget *close;
	GtkWidget *get;
	gulong value;
};

G_DEFINE_FINAL_TYPE (GHexConverter, ghex_converter, GTK_TYPE_WINDOW)

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
			g_signal_stop_emission_by_name(G_OBJECT(editable), "insert-text");
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

	g_signal_stop_emission_by_name(G_OBJECT(editable), "insert-text");
}

static char *
clean (char *ptr)
{
	if (!ptr) return NULL;

	while (*ptr == '0')
		ptr++;

	return ptr;
}

#define CONV_BUFFER_LEN 32
static void
set_values (GHexConverter *self, gulong val)
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

	self->value = val;
	
	for(i = 0; i < CONV_BUFFER_LEN; i++)
		buffer[i] =((val & (1L << (31 - i)))?'1':'0');
	buffer[i] = 0;
	gtk_editable_set_text (GTK_EDITABLE(self->entry[0]), clean(buffer));

	g_snprintf(buffer, CONV_BUFFER_LEN, "%o",(unsigned int)val);
	gtk_editable_set_text (GTK_EDITABLE(self->entry[1]), buffer);
	
	g_snprintf(buffer, CONV_BUFFER_LEN, "%lu", val);
	gtk_editable_set_text (GTK_EDITABLE(self->entry[2]), buffer);

	for(i = 0, tmp = val; i < nhex; i++) {
		buffer[nhex - i - 1] = (tmp & 0x0000000FL);
		if(buffer[nhex - i - 1] < 10)
			buffer[nhex - i - 1] += '0';
		else
			buffer[nhex - i - 1] += 'A' - 10;
		tmp = tmp >> 4;
	}
	buffer[i] = '\0';
	gtk_editable_set_text (GTK_EDITABLE(self->entry[3]), buffer);
	
	for(i = 0, tmp = val; i < nbytes; i++) {
		buffer[nbytes - i - 1] = tmp & 0x000000FF;
		if(buffer[nbytes - i - 1] < 0x20 || buffer[nbytes - i - 1] >= 0x7F)
			buffer[nbytes - i - 1] = '_';
		tmp = tmp >> 8;
	}
	buffer[i] = 0;
	gtk_editable_set_text (GTK_EDITABLE(self->entry[4]), buffer);
}

static void
conv_entry_cb (GHexConverter *self, GtkEntry *entry)
{
	int base = GPOINTER_TO_INT (g_object_get_data (G_OBJECT(entry), "base"));
	gchar buffer[CONV_BUFFER_LEN + 1];
	const gchar *text;
	gchar *endptr;
	gulong val;
	int i, len;
	
	val = self->value;
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
		strncpy(buffer, text, CONV_BUFFER_LEN);
		buffer[CONV_BUFFER_LEN] = 0;
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
			self->value = 0;
			for(i = 0; i < 5; i++)
				gtk_editable_set_text (GTK_EDITABLE(self->entry[i]), _("ERROR"));
			gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
			return;
		}
	}

	if (val == self->value)
		return;
	
	set_values (self, val);
}
#undef CONV_BUFFER_LEN

static GtkWidget *
create_converter_entry (GHexConverter *self, const gchar *name, GtkWidget *grid, gint pos, gint base)
{
	GtkWidget *label;
    GtkWidget *entry;

	/* label */
	label = gtk_label_new_with_mnemonic(name);
	gtk_grid_attach (GTK_GRID (grid), label, 0, pos, 1, 1);

	/* entry */
	entry = gtk_entry_new();

	g_object_set_data (G_OBJECT(entry), "base", GINT_TO_POINTER(base));
	g_signal_connect_object (entry, "activate", G_CALLBACK(conv_entry_cb), self, G_CONNECT_SWAPPED);

	g_signal_connect (G_OBJECT(entry), "insert-text",
					 G_CALLBACK(entry_filter), GINT_TO_POINTER(base));

	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_widget_set_hexpand (entry, TRUE);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, pos, 1, 1);

	return entry;
}

static void
get_cursor_val_cb (GHexConverter *self, GtkButton *button)
{
	guint val, start;
	guint group_type = 0;
	HexDocument *doc;
	HexSelection *selection;
	gint64 payload;
	GtkLayoutManager *layout_manager;

	doc = hex_view_get_document (self->hex);
	payload = hex_buffer_get_payload_size (hex_document_get_buffer (doc));
	layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET(self->hex));
	selection = hex_view_get_selection (self->hex);

	g_object_get (layout_manager, "group-type", &group_type, NULL);
	g_assert (group_type != 0);

	start = hex_selection_get_cursor_pos (selection);
	start = start - start % group_type;

	val = 0;
	do {
		val <<= 8;
		val |= hex_buffer_get_byte (hex_document_get_buffer (doc), start);
		start++;
	} while((start % group_type != 0) &&
			(start < payload) );

	set_values (self, val);
}

/* can-NULL */
void
ghex_converter_set_hex (GHexConverter *self, HexView *hex)
{
	g_return_if_fail (GHEX_IS_CONVERTER (self));
	g_return_if_fail (hex == NULL || HEX_IS_VIEW (hex));

	g_clear_object (&self->hex);

	if (hex)
		self->hex = g_object_ref (hex);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_HEX]);
}

HexView *
ghex_converter_get_hex (GHexConverter *self)
{
	g_return_val_if_fail (GHEX_IS_CONVERTER (self), NULL);

	return self->hex;
}

static void
ghex_converter_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GHexConverter *self = GHEX_CONVERTER(object);

	switch (property_id)
	{
		case PROP_HEX:
			ghex_converter_set_hex (self, g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_converter_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	GHexConverter *self = GHEX_CONVERTER(object);

	switch (property_id)
	{
		case PROP_HEX:
			g_value_set_object (value, ghex_converter_get_hex (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_converter_class_init (GHexConverterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->set_property = ghex_converter_set_property;
	object_class->get_property = ghex_converter_get_property;

	properties[PROP_HEX] = g_param_spec_object ("hex", NULL, NULL,
			HEX_TYPE_VIEW,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
ghex_converter_init (GHexConverter *self)
{
	GtkWidget *grid;
	GtkWidget *converter_get;
	GtkWidget *close_btn;
	int i;

	g_object_bind_property_full (self, "hex", self, "sensitive", G_BINDING_DEFAULT, util_have_object_transform_to, NULL, NULL, NULL);

	gtk_window_set_title (GTK_WINDOW(self), _("Base Converter"));
	gtk_window_set_hide_on_close (GTK_WINDOW(self), TRUE);

	grid = gtk_grid_new ();
	gtk_widget_set_name (grid, "converter-grid");
	gtk_grid_set_row_spacing (GTK_GRID (grid), 4);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 4);

	gtk_window_set_child (GTK_WINDOW(self), grid);

	/* entries */
	self->entry[0] = create_converter_entry (self, _("_Binary:"), grid,
											 0, 2);
	self->entry[1] = create_converter_entry (self, _("_Octal:"), grid,
											 1, 8);
	self->entry[2] = create_converter_entry (self, _("_Decimal:"), grid,
											 2, 10);
	self->entry[3] = create_converter_entry (self, _("_Hex:"), grid,
											 3, 16);
	self->entry[4] = create_converter_entry (self, _("_ASCII:"), grid,
											 4, 0);

	/* get cursor button */
	converter_get = gtk_button_new_with_mnemonic (_("_Get cursor value"));
	close_btn = gtk_button_new_with_mnemonic (_("_Close"));

	g_signal_connect_object (converter_get, "clicked", G_CALLBACK(get_cursor_val_cb), self, G_CONNECT_SWAPPED);
	g_signal_connect_object (close_btn, "clicked", G_CALLBACK(gtk_window_close), self, G_CONNECT_SWAPPED);

	gtk_grid_attach (GTK_GRID (grid), converter_get, 0, 5, 2, 1);
	gtk_grid_attach (GTK_GRID (grid), close_btn, 0, 6, 2, 1);

	gtk_accessible_update_property (GTK_ACCESSIBLE(converter_get),
			GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
			_("Gets the value at cursor in binary, octal, decimal, hex and ASCII"),
			-1);
}

GtkWidget *
ghex_converter_new (GtkWindow *parent_win)
{
	g_return_val_if_fail (GTK_IS_WINDOW (parent_win), NULL);

	return g_object_new (GHEX_TYPE_CONVERTER,
			"transient-for", parent_win,
			NULL);
}
