/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* chartable.c - a window with a character table

   Copyright © 1998 - 2004 Free Software Foundation

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

   Original Author: Jaka Mocnik <jaka@gnu.org>
*/

#include "chartable.h"

#include "util.h"

#include "config.h"

static const char *ascii_non_printable_label[] = {
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
  CHARTABLE_VALUE_PROP_0,
  PROP_VALUE,
  PROP_ASCII,
  PROP_HEX,
  PROP_DECIMAL,
  PROP_OCTAL,
  PROP_BINARY,
  CHARTABLE_VALUE_N_PROPS
};

static GParamSpec *chartable_value_properties[CHARTABLE_VALUE_N_PROPS];

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

	chartable_value_properties[PROP_VALUE] = g_param_spec_uchar ("value", NULL, NULL, 0, 255, 0,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
	chartable_value_properties[PROP_ASCII] = g_param_spec_string ("ascii", NULL, NULL, NULL, flags);
	chartable_value_properties[PROP_HEX] = g_param_spec_string ("hex", NULL, NULL, NULL, flags);
	chartable_value_properties[PROP_DECIMAL] = g_param_spec_string ("decimal", NULL, NULL, NULL, flags);
	chartable_value_properties[PROP_OCTAL] = g_param_spec_string ("octal", NULL, NULL, NULL, flags);
	chartable_value_properties[PROP_BINARY] = g_param_spec_string ("binary", NULL, NULL, NULL, flags);

	g_object_class_install_properties (gobject_class, CHARTABLE_VALUE_N_PROPS, chartable_value_properties);
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

enum
{
	PROP_0,
	PROP_HEX_VIEW,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _GHexCharTable
{
	GtkWindow parent_instance;

	HexView *hex;
	
	/* From template */
	GtkWidget *main_box;
	GtkWidget *columnview;
	GtkWidget *insert_button;
	GtkWidget *close_button;
};

G_DEFINE_FINAL_TYPE (GHexCharTable, ghex_char_table, GTK_TYPE_WINDOW)

/* can-NULL */
void
ghex_char_table_set_hex (GHexCharTable *self, HexView *hex)
{
	g_return_if_fail (GHEX_IS_CHAR_TABLE (self));
	g_return_if_fail (hex == NULL || HEX_IS_VIEW (hex));

	g_clear_object (&self->hex);

	if (hex)
		self->hex = g_object_ref (hex);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_HEX_VIEW]);
}

HexView *
ghex_char_table_get_hex (GHexCharTable *self)
{
	g_return_val_if_fail (GHEX_IS_CHAR_TABLE (self), NULL);

	return self->hex;
}

static void
ghex_char_table_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GHexCharTable *self = GHEX_CHAR_TABLE(object);

	switch (property_id)
	{
		case PROP_HEX_VIEW:
			ghex_char_table_set_hex (self, g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_char_table_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	GHexCharTable *self = GHEX_CHAR_TABLE(object);

	switch (property_id)
	{
		case PROP_HEX_VIEW:
			g_value_set_object (value, ghex_char_table_get_hex (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
insert_char (GHexCharTable *self, guchar byte)
{
	if (self->hex)
	{
		g_assert (HEX_IS_VIEW (self->hex));

		HexDocument *doc = hex_view_get_document (self->hex);
		HexSelection *selection = hex_view_get_selection (self->hex);
		const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
		const gboolean insert_mode = hex_view_get_insert_mode (self->hex);

		hex_document_set_byte (doc, byte, cursor_pos, insert_mode, TRUE);
		hex_selection_collapse (selection, cursor_pos + 1);
	}
}

static void
columnview_activate_cb (GHexCharTable *self,
		guint position,
		GtkColumnView *columnview)
{
	g_assert (GHEX_IS_CHAR_TABLE (self));

	insert_char (self, (guchar)position);
}

static void
insert_button_clicked_cb (GHexCharTable *self)
{
	g_assert (GHEX_IS_CHAR_TABLE (self));

	GtkSingleSelection *selection = GTK_SINGLE_SELECTION(gtk_column_view_get_model (GTK_COLUMN_VIEW (self->columnview)));

	insert_char (self, (guchar)gtk_single_selection_get_selected (selection));
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
}

static gboolean
esc_key_press_cb (GtkEventControllerKey *key,
  guint keyval,
  guint keycode,
  GdkModifierType state,
  GHexCharTable *self)
{
	if (keyval == GDK_KEY_Escape)
	{
		gtk_window_close (GTK_WINDOW(self));
		return GDK_EVENT_STOP;
	}
	return GDK_EVENT_PROPAGATE;
}

static void
ghex_char_table_init (GHexCharTable *self)
{
	gtk_widget_init_template (GTK_WIDGET(self));

	g_object_bind_property_full (self, "hex", self->main_box, "sensitive", G_BINDING_DEFAULT, util_have_object_transform_to, NULL, NULL, NULL);

	/* Make ESC close the dialog */
	{
		GtkEventController *key = gtk_event_controller_key_new ();

		gtk_widget_add_controller (GTK_WIDGET(self), key);

		g_signal_connect_object (key, "key-pressed", G_CALLBACK(esc_key_press_cb), self, G_CONNECT_DEFAULT);
	}

	setup_columnview (self->columnview);
}

static void
ghex_char_table_dispose (GObject *object)
{
	GHexCharTable *self = GHEX_CHAR_TABLE(object);

	gtk_widget_dispose_template (GTK_WIDGET(self), GHEX_TYPE_CHAR_TABLE);
}

static void
ghex_char_table_class_init (GHexCharTableClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->set_property = ghex_char_table_set_property;
	object_class->get_property = ghex_char_table_get_property;
	object_class->dispose = ghex_char_table_dispose;

	properties[PROP_HEX_VIEW] = g_param_spec_object ("hex", NULL, NULL,
			HEX_TYPE_VIEW,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	gtk_widget_class_set_template_from_resource (widget_class, RESOURCE_BASE_PATH "/chartable.ui");

	gtk_widget_class_bind_template_child (widget_class, GHexCharTable, main_box);
	gtk_widget_class_bind_template_child (widget_class, GHexCharTable, columnview);
	gtk_widget_class_bind_template_child (widget_class, GHexCharTable, insert_button);
	gtk_widget_class_bind_template_child (widget_class, GHexCharTable, close_button);

	gtk_widget_class_bind_template_callback (widget_class, insert_button_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, columnview_activate_cb);
}

GtkWidget *
ghex_char_table_new (GtkWindow *parent_win)
{
	g_return_val_if_fail (GTK_IS_WINDOW (parent_win), NULL);

	return g_object_new (GHEX_TYPE_CHAR_TABLE,
			"transient-for", parent_win,
			NULL);
}
