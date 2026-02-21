/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* chartable.c - a window with a character table

   Copyright © 1998 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021-2023 Logan Rathbone <poprocks@gmail.com>

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

   Original Author: Jaka Mocnik <jaka@gnu.org>
*/

#include "chartable.h"

#include <config.h>

/* STATIC GLOBALS */

static GtkTreeSelection *sel_row = NULL;
static HexWidget *gh_glob = NULL;

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

/* HexChartableValue object */

#define HEX_TYPE_CHARTABLE_VALUE (hex_chartable_value_get_type ())
G_DECLARE_FINAL_TYPE (HexChartableValue, hex_chartable_value, HEX, CHARTABLE_VALUE, GObject);

enum {
  PROP_0,
  PROP_VALUE,
  PROP_ASCII,
  PROP_HEX,
  PROP_DECIMAL,
  PROP_OCTAL,
  PROP_BINARY,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

struct _HexChartableValue
{
	GObject parent_instance;

	guchar val;
};

G_DEFINE_TYPE (HexChartableValue, hex_chartable_value, G_TYPE_OBJECT)

/* All of the getters below should be transfer: full */

static char *
hex_chartable_value_get_ascii (HexChartableValue *self)
{
	char *str = NULL;

	if (self->val< 0x20) {
		str = g_strdup (ascii_non_printable_label[self->val]);
	}
	else if (self->val < 0x7f) {
		str = g_strdup_printf ("%c", self->val);
	}

	return str;
}

static char *
hex_chartable_value_get_hex (HexChartableValue *self)
{
	return g_strdup_printf ("%02X", self->val);
}

static char *
hex_chartable_value_get_decimal (HexChartableValue *self)
{
	return g_strdup_printf ("%03d", self->val);
}

static char *
hex_chartable_value_get_octal (HexChartableValue *self)
{
	return g_strdup_printf ("%03o", self->val);
}

static char *
hex_chartable_value_get_binary (HexChartableValue *self)
{
	/* 9 == 8 characters + null terminator. */
	char *bin_label = g_malloc0 (9);

	/* fill in character string with 0's and 1's backwards. */
	for (int col = 0; col < 8; ++col) {
		bin_label[7-col] = (self->val & (1L << col)) ? '1' : '0';
	}

	return bin_label;
}

static void hex_chartable_value_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexChartableValue *self = HEX_CHARTABLE_VALUE(object);

	switch (property_id)
	{
		case PROP_VALUE:
			g_value_set_uchar (value, self->val);
			break;

		case PROP_ASCII:
			g_value_take_string (value, hex_chartable_value_get_ascii (self));
			break;

		case PROP_HEX:
			g_value_take_string (value, hex_chartable_value_get_hex (self));
			break;

		case PROP_DECIMAL:
			g_value_take_string (value, hex_chartable_value_get_decimal (self));
			break;

		case PROP_OCTAL:
			g_value_take_string (value, hex_chartable_value_get_octal (self));
			break;

		case PROP_BINARY:
			g_value_take_string (value, hex_chartable_value_get_binary (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void hex_chartable_value_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexChartableValue *self = HEX_CHARTABLE_VALUE(object);

	switch (property_id)
	{
		case PROP_VALUE:
			self->val = g_value_get_uchar (value);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_chartable_value_class_init (HexChartableValueClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	/* flags for all properties EXCEPT 'value' */
	GParamFlags flags = G_PARAM_READABLE | G_PARAM_STATIC_STRINGS;

	gobject_class->get_property = hex_chartable_value_get_property;
	gobject_class->set_property = hex_chartable_value_set_property;

	properties[PROP_VALUE] = g_param_spec_uchar ("value", NULL, NULL, 0, 255, 0,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
	properties[PROP_ASCII] = g_param_spec_string ("ascii", NULL, NULL, NULL, flags);
	properties[PROP_HEX] = g_param_spec_string ("hex", NULL, NULL, NULL, flags);
	properties[PROP_DECIMAL] = g_param_spec_string ("decimal", NULL, NULL, NULL, flags);
	properties[PROP_OCTAL] = g_param_spec_string ("octal", NULL, NULL, NULL, flags);
	properties[PROP_BINARY] = g_param_spec_string ("binary", NULL, NULL, NULL, flags);

	g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
hex_chartable_value_init (HexChartableValue *self)
{
}

static HexChartableValue *
hex_chartable_value_new (guchar val)
{
	return g_object_new (HEX_TYPE_CHARTABLE_VALUE,
			"value", val,
			NULL);
}

/* --- */

static void
insert_char (guchar byte)
{
	hex_document_set_byte (hex_widget_get_document (gh_glob),
			byte,
			hex_widget_get_cursor (gh_glob),
			hex_widget_get_insert_mode (gh_glob),
			TRUE);	/* undoable */

	hex_widget_set_cursor (gh_glob, hex_widget_get_cursor (gh_glob) + 1);
}


static void
columnview_activate_cb (GtkColumnView *columnview,
		guint position,
		gpointer user_data)
{
	insert_char ((guchar)position);
}

static void
insert_button_clicked_cb (GtkButton *button, GtkColumnView *columnview)
{
	GtkSingleSelection *selection = GTK_SINGLE_SELECTION(gtk_column_view_get_model (columnview));

	insert_char ((guchar)gtk_single_selection_get_selected (selection));
}

static void
setup_columnview (GtkWidget *columnview)
{
	GListStore *store = g_list_store_new (HEX_TYPE_CHARTABLE_VALUE);
	GtkSingleSelection *selection;

	for (int i = 0; i < 256; ++i)
	{
		HexChartableValue *hv = hex_chartable_value_new ((guchar)i);
		g_list_store_append (store, hv);
		g_object_unref (hv);
	}

	selection = gtk_single_selection_new (G_LIST_MODEL(store));
	gtk_column_view_set_model (GTK_COLUMN_VIEW(columnview), GTK_SELECTION_MODEL(selection));

	g_signal_connect (columnview, "activate", G_CALLBACK(columnview_activate_cb), NULL);
}

static gboolean
key_press_cb (GtkEventControllerKey *key,
  guint keyval,
  guint keycode,
  GdkModifierType state,
  GtkWindow *ct)
{
	if (keyval == GDK_KEY_Escape)
	{
		gtk_window_close (ct);
		return GDK_EVENT_STOP;
	}
	return GDK_EVENT_PROPAGATE;
}


GtkWidget *
create_char_table (GtkWindow *parent_win, HexWidget *gh)
{
	GtkBuilder *builder = gtk_builder_new_from_resource (RESOURCE_BASE_PATH "/chartable.ui");
	GtkWidget *window = GTK_WIDGET(gtk_builder_get_object (builder, "window"));
	GtkWidget *columnview = GTK_WIDGET(gtk_builder_get_object (builder, "columnview"));
	GtkWidget *insert_button = GTK_WIDGET(gtk_builder_get_object (builder, "insert_button"));
	GtkWidget *close_button = GTK_WIDGET(gtk_builder_get_object (builder, "close_button"));
	GtkEventController *key;

	/* set global HexWidget widget */
	g_assert (HEX_IS_WIDGET(gh));
	gh_glob = gh;

	/* Make ESC close the dialog */
	key = gtk_event_controller_key_new ();
	gtk_widget_add_controller (window, key);
	g_signal_connect (key, "key-pressed", G_CALLBACK(key_press_cb), window);

	if (parent_win) {
		g_assert (GTK_IS_WINDOW (parent_win));

		gtk_window_set_transient_for (GTK_WINDOW(window), parent_win);
	}

	setup_columnview (columnview);

	g_signal_connect_swapped (close_button, "clicked", G_CALLBACK(gtk_window_close), window);
	g_signal_connect (insert_button, "clicked", G_CALLBACK(insert_button_clicked_cb), columnview);

	g_object_unref (builder);

	return window;
}
