// vim: linebreak breakindent breakindentopt=shift\:4

/* ghex-info-bar.c - Implementation of info bar widget
 *
 * Copyright © 2025-2026 Logan Rathbone
 *
 * GHex is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GHex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GHex; see the file COPYING.
 * If not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Original GHex Author: Jaka Mocnik
 */

#include "ghex-info-bar.h"

#include <config.h>

enum {
	PROP_0,
	PROP_SHOWN,
	PROP_TITLE,
	PROP_DESCRIPTION,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _GHexInfoBar
{
	GtkWidget parent_instance;

	char *title;
	char *description;

	/* TEMPLATE */
	GtkWidget *revealer;
	GtkWidget *close_button;
};

G_DEFINE_TYPE (GHexInfoBar, ghex_info_bar, GTK_TYPE_WIDGET)

gboolean
ghex_info_bar_get_shown (GHexInfoBar *self)
{
	g_return_val_if_fail (GHEX_IS_INFO_BAR (self), FALSE);

	return gtk_revealer_get_reveal_child (GTK_REVEALER(self->revealer));
}

void
ghex_info_bar_set_shown (GHexInfoBar *self, gboolean shown)
{
	g_return_if_fail (GHEX_IS_INFO_BAR (self));

	gtk_revealer_set_reveal_child (GTK_REVEALER(self->revealer), shown);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_SHOWN]);
}

static const char *
ghex_info_bar_get_title (GHexInfoBar *self)
{
	g_return_val_if_fail (GHEX_IS_INFO_BAR (self), NULL);

	return self->title;
}

void
ghex_info_bar_set_title (GHexInfoBar *self, const char *title)
{
	g_return_if_fail (GHEX_IS_INFO_BAR (self));

	g_free (self->title);
	self->title = g_strdup (title);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_TITLE]);
}

const char *
ghex_info_bar_get_description (GHexInfoBar *self)
{
	g_return_val_if_fail (GHEX_IS_INFO_BAR (self), NULL);

	return self->description;
}

void
ghex_info_bar_set_description (GHexInfoBar *self, const char *description)
{
	g_return_if_fail (GHEX_IS_INFO_BAR (self));

	g_free (self->description);
	self->description = g_strdup (description);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_DESCRIPTION]);
}

static void
ghex_info_bar_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GHexInfoBar *self = GHEX_INFO_BAR(object);

	switch (property_id)
	{
		case PROP_SHOWN:
			ghex_info_bar_set_shown (self, g_value_get_boolean (value));
			break;

		case PROP_TITLE:
			ghex_info_bar_set_title (self, g_value_get_string (value));
			break;

		case PROP_DESCRIPTION:
			ghex_info_bar_set_description (self, g_value_get_string (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_info_bar_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	GHexInfoBar *self = GHEX_INFO_BAR(object);

	switch (property_id)
	{
		case PROP_SHOWN:
			g_value_set_boolean (value, ghex_info_bar_get_shown (self));
			break;

		case PROP_TITLE:
			g_value_set_string (value, ghex_info_bar_get_title (self));
			break;

		case PROP_DESCRIPTION:
			g_value_set_string (value, ghex_info_bar_get_description (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_info_bar_init (GHexInfoBar *self)
{
	gtk_widget_init_template (GTK_WIDGET(self));
}

static void
ghex_info_bar_dispose (GObject *object)
{
	GHexInfoBar *self = GHEX_INFO_BAR(object);

	gtk_widget_dispose_template (GTK_WIDGET(self), GHEX_TYPE_INFO_BAR);

	/* Chain up */
	G_OBJECT_CLASS(ghex_info_bar_parent_class)->dispose (object);
}

static void
ghex_info_bar_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS(ghex_info_bar_parent_class)->finalize (object);
}

static void
ghex_info_bar_class_init (GHexInfoBarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose = ghex_info_bar_dispose;
	object_class->finalize = ghex_info_bar_finalize;

	object_class->get_property = ghex_info_bar_get_property;
	object_class->set_property = ghex_info_bar_set_property;

	properties[PROP_SHOWN] = g_param_spec_boolean ("shown", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_TITLE] = g_param_spec_string ("title", NULL, NULL,
			_("File Has Changed on Disk"),
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_DESCRIPTION] = g_param_spec_string ("description", NULL, NULL,
			_("The file has been changed by another program."),
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	/* Actions */

	gtk_widget_class_install_property_action (widget_class, "infobar.shown", "shown");

	/* WIDGET TEMPLATE .UI */

	gtk_widget_class_set_template_from_resource (widget_class,
					RESOURCE_BASE_PATH "/ghex-info-bar.ui");
	gtk_widget_class_bind_template_child (widget_class, GHexInfoBar,
			revealer);
	gtk_widget_class_bind_template_child (widget_class, GHexInfoBar,
			close_button);
}

GtkWidget *
ghex_info_bar_new (void)
{
	return g_object_new (GHEX_TYPE_INFO_BAR, NULL);
}
