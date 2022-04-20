/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * hex-statusbar.c: definition of HexStatusbar widget
 *
 * Copyright © 2022 Logan Rathbone <poprocks@gmail.com>
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

#include "hex-statusbar.h"

/* GOBJECT DEFINITION */

struct _HexStatusbar
{
	GtkWidget parent_instance;

	GtkWidget *label;
};

G_DEFINE_TYPE (HexStatusbar, hex_statusbar, GTK_TYPE_WIDGET)


/* METHOD DEFINITIONS */

static void
hex_statusbar_init (HexStatusbar *self)
{
	GtkWidget *widget = GTK_WIDGET(self);

	self->label = gtk_label_new (NULL);
	gtk_widget_set_parent (self->label, widget);
}

static void
hex_statusbar_dispose (GObject *object)
{
	HexStatusbar *self = HEX_STATUSBAR(object);
	GtkWidget *widget = GTK_WIDGET(self);

	g_clear_pointer (&self->label, gtk_widget_unparent);

	/* Chain up */
	G_OBJECT_CLASS(hex_statusbar_parent_class)->dispose (object);
}

static void
hex_statusbar_finalize (GObject *object)
{
	HexStatusbar *self = HEX_STATUSBAR(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_statusbar_parent_class)->finalize (object);
}

static void
hex_statusbar_class_init (HexStatusbarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	object_class->dispose =  hex_statusbar_dispose;
	object_class->finalize = hex_statusbar_finalize;

	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
	gtk_widget_class_set_css_name (widget_class, "statusbar");
}

/* PUBLIC METHOD DEFINITIONS */

GtkWidget *
hex_statusbar_new (void)
{
	return g_object_new (HEX_TYPE_STATUSBAR, NULL);
}

void
hex_statusbar_set_status (HexStatusbar *self, const char *msg)
{
	g_return_if_fail (HEX_IS_STATUSBAR (self));
	/* Programmer error to pass null to this. Should use _clear instead. */
	g_return_if_fail (msg && *msg);

	gtk_label_set_markup (GTK_LABEL(self->label), msg);
}

void
hex_statusbar_clear (HexStatusbar *self)
{
	g_return_if_fail (HEX_IS_STATUSBAR (self));

	/* Technically nothing in the gtk docs says you can pass NULL to this
	 * function. */
	gtk_label_set_markup (GTK_LABEL(self->label), "");
}
