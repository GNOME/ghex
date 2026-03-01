/* ghex-pane.c
 *
 * Copyright © 2026 Logan Rathbone
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ghex-pane.h"

enum {
	CLOSE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

enum
{
	PROP_0,
	PROP_HEX,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

typedef struct
{
	HexView *hex;
} GHexPanePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GHexPane, ghex_pane, GTK_TYPE_WIDGET)

void
ghex_pane_set_hex (GHexPane *self, HexView *hex)
{
	GHexPanePrivate *priv;

	g_return_if_fail (GHEX_IS_PANE (self));
	g_return_if_fail (hex == NULL || HEX_IS_VIEW (hex));

	priv = ghex_pane_get_instance_private (self);

	g_clear_object (&priv->hex);

	if (hex)
		priv->hex = g_object_ref (hex);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_HEX]);
}

HexView *
ghex_pane_get_hex (GHexPane *self)
{
	GHexPanePrivate *priv;

	g_return_val_if_fail (GHEX_IS_PANE (self), NULL);

	priv = ghex_pane_get_instance_private (self);

	return priv->hex;
}

static void
ghex_pane_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GHexPane *self = GHEX_PANE(object);

	switch (property_id)
	{
		case PROP_HEX:
			ghex_pane_set_hex (self, g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_pane_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	GHexPane *self = GHEX_PANE(object);

	switch (property_id)
	{
		case PROP_HEX:
			g_value_set_object (value, ghex_pane_get_hex (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_pane_real_close (GHexPane *self)
{
}

static void
ghex_pane_init (GHexPane *self)
{
}

static void
ghex_pane_dispose (GObject *object)
{
	GHexPane *self = GHEX_PANE(object);

	/* Chain up */
	G_OBJECT_CLASS(ghex_pane_parent_class)->dispose (object);
}

static void
ghex_pane_finalize (GObject *object)
{
	GHexPane *self = GHEX_PANE(object);

	/* Chain up */
	G_OBJECT_CLASS(ghex_pane_parent_class)->finalize (object);
}

static void
ghex_pane_class_init (GHexPaneClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose =  ghex_pane_dispose;
	object_class->finalize = ghex_pane_finalize;
	object_class->set_property = ghex_pane_set_property;
	object_class->get_property = ghex_pane_get_property;

	klass->close = ghex_pane_real_close;

	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

	/* Signals */

	signals[CLOSE] = 
		g_signal_new ("close",
					  G_OBJECT_CLASS_TYPE(object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (GHexPaneClass, close),
					  NULL,
					  NULL,
					  NULL,
					  G_TYPE_NONE, 0);

	/* Properties */

	properties[PROP_HEX] = g_param_spec_object ("hex", NULL, NULL,
			HEX_TYPE_VIEW,
			default_flags | G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

GtkWidget *
ghex_pane_new (void)
{
	return g_object_new (GHEX_TYPE_PANE, NULL);
}

void
ghex_pane_close (GHexPane *self)
{
	g_return_if_fail (GHEX_IS_PANE (self));

	g_signal_emit (self, signals[CLOSE], 0);
}
