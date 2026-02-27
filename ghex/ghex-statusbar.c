// vim: ts=4 sw=4 colorcolumn=80
/*
 * ghex-statusbar.c: definition of GHexStatusbar widget
 *
 * Copyright © 2022-2026 Logan Rathbone <poprocks@gmail.com>
 *
 *  GHex is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  GHex is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with GHex; see the file COPYING.
 *  If not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ghex-statusbar.h"

#include "configuration.h"
#include "ghex-enums.h"

#include <glib/gi18n.h>

enum
{
	PROP_0,
	PROP_OFFSET_FORMAT,
	PROP_SELECTION,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _GHexStatusbar
{
	GtkWidget parent_instance;

	GtkWidget *label;
	HexSelection *selection;
	GHexStatusbarOffsetFormat offset_format;
};

G_DEFINE_TYPE (GHexStatusbar, ghex_statusbar, GTK_TYPE_WIDGET)

static void
_hex_statusbar_set_str (GHexStatusbar *self, const char *msg)
{
	g_assert (GHEX_IS_STATUSBAR (self));

	gtk_label_set_markup (GTK_LABEL(self->label), msg ? msg : "");
}

static void
update_status_message (GHexStatusbar *self)
{
	gint64 start_offset, end_offset, cursor_pos;
	g_autofree char *status = NULL;
	gboolean is_range;

	g_assert (GHEX_IS_STATUSBAR (self));

	if (!self->selection)
		return;

	start_offset = hex_selection_get_start_offset (self->selection);
	end_offset = hex_selection_get_end_offset (self->selection);
	cursor_pos = hex_selection_get_cursor_pos (self->selection);

	is_range = !(start_offset == end_offset && end_offset == cursor_pos);

	switch (self->offset_format) {
		case GHEX_STATUSBAR_OFFSET_HEX:
			if (is_range) {
				status = g_strdup_printf (
					_("Offset: <tt>0x%lX</tt>; <tt>0x%lX</tt> bytes from <tt>0x%lX</tt> to <tt>0x%lX</tt> selected"),
					cursor_pos, end_offset - start_offset + 1, start_offset, end_offset);
			} else {
				status = g_strdup_printf (_("Offset: <tt>0x%lX</tt>"), cursor_pos);
			}
			break;

		case GHEX_STATUSBAR_OFFSET_DEC:
			if (is_range) {
				status = g_strdup_printf (
					_("Offset: <tt>%ld</tt>; <tt>%ld</tt> bytes from <tt>%ld</tt> to <tt>%ld</tt> selected"),
					cursor_pos, end_offset - start_offset + 1, start_offset, end_offset);
			} else {
				status = g_strdup_printf (_("Offset: <tt>%ld</tt>"), cursor_pos);
			}
			break;

		case GHEX_STATUSBAR_OFFSET_BOTH:
			if (is_range) {
				status = g_strdup_printf (
					/* Weird rendering if you don't put the space at the start */
					_(" <sub>HEX</sub> Offset: <tt>0x%lX</tt>; <tt>0x%lX</tt> bytes from <tt>0x%lX</tt> to <tt>0x%lX</tt> selected <sub>DEC</sub> Offset: <tt>%ld</tt>; <tt>%ld</tt> bytes from <tt>%ld</tt> to <tt>%ld</tt> selected"),
					cursor_pos, end_offset - start_offset + 1, start_offset, end_offset,
					cursor_pos, end_offset - start_offset + 1, start_offset, end_offset);
			} else {
				status = g_strdup_printf (_(" <sub>HEX</sub> Offset: <tt>0x%lX</tt> <sub>DEC</sub> Offset: <tt>%ld</tt>"), cursor_pos, cursor_pos);
			}
			break;

		default:
			g_assert_not_reached ();
	}

	_hex_statusbar_set_str (self, status);
}

/* Clear selection and signal handlers for same. Also safe to be called in
 * _dispose().
 */
static void
dispose_selection (GHexStatusbar *self)
{
	if (self->selection)
	{
		g_signal_handlers_disconnect_by_data (self->selection, self);
		g_clear_object (&self->selection);
	}
}

/* Transfer none */
void
ghex_statusbar_set_selection (GHexStatusbar *self, HexSelection *selection)
{
	g_return_if_fail (GHEX_IS_STATUSBAR (self));
	g_return_if_fail (selection == NULL || HEX_IS_SELECTION (selection));

	dispose_selection (self);

	if (selection)
	{
		self->selection = g_object_ref (selection);

		g_signal_connect_object (self->selection, "notify::cursor-pos", G_CALLBACK(update_status_message), self, G_CONNECT_SWAPPED);
	}

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_SELECTION]);
}

/* Transfer none */
HexSelection *
ghex_statusbar_get_selection (GHexStatusbar *self)
{
	g_return_val_if_fail (GHEX_IS_STATUSBAR (self), NULL);

	return self->selection;
}

void
ghex_statusbar_set_offset_format (GHexStatusbar *self, GHexStatusbarOffsetFormat offset_format)
{
	g_return_if_fail (GHEX_IS_STATUSBAR (self));

	self->offset_format = offset_format;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_OFFSET_FORMAT]);
}

GHexStatusbarOffsetFormat
ghex_statusbar_get_offset_format (GHexStatusbar *self)
{
	g_return_val_if_fail (GHEX_IS_STATUSBAR (self), GHEX_STATUSBAR_OFFSET_HEX);

	return self->offset_format;
}

static void
ghex_statusbar_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GHexStatusbar *self = GHEX_STATUSBAR(object);

	switch (property_id)
	{
		case PROP_SELECTION:
			ghex_statusbar_set_selection (self, g_value_get_object (value));
			break;

		case PROP_OFFSET_FORMAT:
			ghex_statusbar_set_offset_format (self, g_value_get_enum (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_statusbar_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	GHexStatusbar *self = GHEX_STATUSBAR(object);

	switch (property_id)
	{
		case PROP_SELECTION:
			g_value_set_object (value, ghex_statusbar_get_selection (self));
			break;

		case PROP_OFFSET_FORMAT:
			g_value_set_enum (value, ghex_statusbar_get_offset_format (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_statusbar_init (GHexStatusbar *self)
{
	self->label = gtk_label_new (NULL);
	gtk_widget_set_parent (self->label, GTK_WIDGET(self));

	g_signal_connect (self, "notify::offset-format", G_CALLBACK(update_status_message), NULL);

	{
		GSettings *settings = ghex_get_global_settings ();

		g_settings_bind (settings, "statusbar-offset-format", self, "offset-format", G_SETTINGS_BIND_DEFAULT);
	}
}

static void
ghex_statusbar_dispose (GObject *object)
{
	GHexStatusbar *self = GHEX_STATUSBAR(object);

	g_clear_pointer (&self->label, gtk_widget_unparent);
	dispose_selection (self);

	/* Chain up */
	G_OBJECT_CLASS(ghex_statusbar_parent_class)->dispose (object);
}

static void
ghex_statusbar_finalize (GObject *object)
{
	GHexStatusbar *self = GHEX_STATUSBAR(object);

	/* Chain up */
	G_OBJECT_CLASS(ghex_statusbar_parent_class)->finalize (object);
}

static void
ghex_statusbar_class_init (GHexStatusbarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose =  ghex_statusbar_dispose;
	object_class->finalize = ghex_statusbar_finalize;
	object_class->set_property = ghex_statusbar_set_property;
	object_class->get_property = ghex_statusbar_get_property;

	properties[PROP_SELECTION] = g_param_spec_object ("selection", NULL, NULL,
			HEX_TYPE_SELECTION,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_OFFSET_FORMAT] = g_param_spec_enum ("offset-format", NULL, NULL,
			GHEX_TYPE_STATUSBAR_OFFSET_FORMAT,
			GHEX_STATUSBAR_OFFSET_HEX,
			default_flags | G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
	gtk_widget_class_set_css_name (widget_class, "statusbar");
}

GtkWidget *
ghex_statusbar_new (void)
{
	return g_object_new (GHEX_TYPE_STATUSBAR, NULL);
}
