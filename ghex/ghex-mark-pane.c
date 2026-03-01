// vim: ts=4 sw=4 breakindent breakindentopt=shift\:4
/* ghex-mark-pane.c
 *
 * Copyright © 2023-2026 Logan Rathbone
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

#include "ghex-mark-pane.h"

#include <glib/gi18n.h>

#include "config.h"

struct _GHexMarkPane
{
	GHexPane parent_instance;

	HexView *hex;
	
	/* From template: */

	GtkWidget *box;
	GtkWidget *spin_button;
	GtkWidget *color_button;
	GtkWidget *mark_description_label;
	GtkWidget *set_mark_button;
	GtkWidget *goto_mark_button;
	GtkWidget *delete_mark_button;
	GtkWidget *close_button;
};

G_DEFINE_TYPE (GHexMarkPane, ghex_mark_pane, GHEX_TYPE_PANE)

static void
update_action_targets (GHexMarkPane *self)
{
	gtk_actionable_set_action_target (GTK_ACTIONABLE(self->set_mark_button), "(sims)",
			"set",
			gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(self->spin_button)),
			NULL);

	gtk_actionable_set_action_target (GTK_ACTIONABLE(self->goto_mark_button), "(sims)",
			"jump",
			gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(self->spin_button)),
			NULL);

	gtk_actionable_set_action_target (GTK_ACTIONABLE(self->delete_mark_button), "(sims)",
			"delete",
			gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(self->spin_button)),
			NULL);
}

static gboolean
sensitive_closure_cb (GObject *object, HexMark *mark, gpointer user_data)
{
	return mark ? TRUE : FALSE;
}

static HexMark *
mark_spin_button_closure_cb (GObject *object, double spin_button_value, gpointer user_data)
{
	GHexMarkPane *self = GHEX_MARK_PANE(object);
	char *key = NULL;
	HexMark *mark = NULL;

	if (! self->hex)
		goto out;

	key = g_strdup_printf ("mark%d", (int)spin_button_value);
	mark = g_object_get_data (G_OBJECT(self->hex), key);

	if (! mark)
		goto out;

	if (! hex_mark_get_have_custom_color (mark))
		goto out;

out:
	g_free (key);

	/* GtkExpression assumes it is receiving its own ref of the object returned
	 * from the callback.
	 */
	return mark ? g_object_ref (mark) : NULL;
}

/* This function needs to return a newly allocated string, since
 * GtkExpression/the closure stuff assumes it can take ownership of it and
 * free it.
 */
static char *
mark_description_label_closure_cb (GObject *object, HexMark *mark, gpointer user_data)
{
	GHexMarkPane *self = GHEX_MARK_PANE(object);
	HexHighlight *highlight;
	char *str = NULL;
	gint64 start, end;

	if (! mark)
		return NULL;

	highlight = hex_mark_get_highlight (mark);

	start = hex_highlight_get_start_offset (highlight);
	end = hex_highlight_get_end_offset (highlight);

	/* Translators: this is meant to show a range of hex offset values.
	 * eg, "0xAA - 0xFF"
	 */
	str = g_strdup_printf (_("0x%lX - 0x%lX"), start, end);

	return str;
}

static void
color_set_cb (GHexMarkPane *self, GtkColorButton *color_button)
{
	char *key = g_strdup_printf ("mark%d", gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(self->spin_button)));
	HexMark *mark = g_object_get_data (G_OBJECT(self->hex), key);
	GdkRGBA color = {0};

	if (!mark)
		goto out;

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER(color_button), &color);
	G_GNUC_END_IGNORE_DEPRECATIONS
	color.alpha = 0.5;

	hex_mark_set_custom_color (mark, &color);

out:
	g_free (key);
}

static void
ghex_mark_pane_init (GHexMarkPane *self)
{
	GtkWidget *widget = GTK_WIDGET(self);

	gtk_widget_init_template (widget);

	update_action_targets (self);

	g_signal_connect_swapped (self->color_button, "color-set", G_CALLBACK(color_set_cb), self);
	g_signal_connect_swapped (self->spin_button, "value-changed", G_CALLBACK(update_action_targets), self);
//	g_signal_connect_swapped (self->close_button, "clicked", G_CALLBACK(ghex_mark_pane_close), self);
}

static void
ghex_mark_pane_dispose (GObject *object)
{
	GHexMarkPane *self = GHEX_MARK_PANE(object);

	g_clear_pointer (&self->box, gtk_widget_unparent);

	/* Chain up */
	G_OBJECT_CLASS(ghex_mark_pane_parent_class)->dispose (object);
}

static void
ghex_mark_pane_class_init (GHexMarkPaneClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	object_class->dispose = ghex_mark_pane_dispose;

	gtk_widget_class_set_css_name (widget_class, "markpane");

	/* Setup template */

	gtk_widget_class_set_template_from_resource (widget_class,
			RESOURCE_BASE_PATH "/ghex-mark-pane.ui");

	gtk_widget_class_bind_template_child (widget_class, GHexMarkPane, box);
	gtk_widget_class_bind_template_child (widget_class, GHexMarkPane, spin_button);
	gtk_widget_class_bind_template_child (widget_class, GHexMarkPane, color_button);
	gtk_widget_class_bind_template_child (widget_class, GHexMarkPane, mark_description_label);
	gtk_widget_class_bind_template_child (widget_class, GHexMarkPane, set_mark_button);
	gtk_widget_class_bind_template_child (widget_class, GHexMarkPane, goto_mark_button);
	gtk_widget_class_bind_template_child (widget_class, GHexMarkPane, delete_mark_button);
	gtk_widget_class_bind_template_child (widget_class, GHexMarkPane, close_button);

	gtk_widget_class_bind_template_callback (widget_class, sensitive_closure_cb);
	gtk_widget_class_bind_template_callback (widget_class, mark_spin_button_closure_cb);
	gtk_widget_class_bind_template_callback (widget_class, mark_description_label_closure_cb);
}

void
ghex_mark_pane_refresh (GHexMarkPane *self)
{
	g_return_if_fail (GHEX_IS_MARK_PANE (self));

	/* This is kind of lame, but since we've bound our spin button's value to
	 * updating sensitivity, etc., there are times we want things to react as
	 * though the spin button's value has been set to a different value, even
	 * if it hasn't been.
	 */
	g_object_notify (G_OBJECT(self->spin_button), "value");
}

void
ghex_mark_pane_activate_mark_num (GHexMarkPane *self, int mark_num)
{
	g_return_if_fail (GHEX_IS_MARK_PANE (self));

	if (mark_num < 0 || mark_num > 9) {
		g_warning ("%s: Programmer error: invalid mark number", __func__);
		return;
	}

	gtk_spin_button_set_value (GTK_SPIN_BUTTON(self->spin_button), mark_num);
}

GtkWidget *
ghex_mark_pane_new (void)
{
	return g_object_new (GHEX_TYPE_MARK_PANE, NULL);
}
