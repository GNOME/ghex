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

	GHashTable *marks_ht;
	
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

static HexMark *
marks_ht_get (GHexMarkPane *self, int index)
{
	HexMark *mark;

	g_assert (GHEX_IS_MARK_PANE (self));
	g_assert (self->marks_ht != NULL);

	mark = g_hash_table_lookup (self->marks_ht, GINT_TO_POINTER (index));

	return mark;
}

static void
marks_ht_set (GHexMarkPane *self, int index, HexMark *mark)
{
	HexView *hex = ghex_pane_get_hex (GHEX_PANE(self));
	HexMark *old_mark = NULL;
	gboolean mark_is_new = FALSE;

	g_assert (self->marks_ht != NULL);
	g_assert (HEX_IS_VIEW (hex));
	g_assert (HEX_IS_MARK (mark));

	old_mark = marks_ht_get (self, index);
	if (old_mark)
	{
		g_debug ("%s: Pre-existing mark found at %d - replacing", __func__, index);

		hex_view_delete_mark (hex, old_mark);
	}

	g_hash_table_insert (self->marks_ht, GINT_TO_POINTER (index), g_object_ref (mark));

	hex_view_add_mark (hex, mark);
}

static void
marks_ht_delete (GHexMarkPane *self, int mark_num)
{
	HexMark *mark = marks_ht_get (self, mark_num);
	HexView *hex = ghex_pane_get_hex (GHEX_PANE(self));

	g_assert (HEX_IS_VIEW (hex));

	if (!mark)
	{
		g_debug ("%s: No mark found at %d", __func__, mark_num);
		return;
	}

	g_hash_table_remove (self->marks_ht, GINT_TO_POINTER (mark_num));

	hex_view_delete_mark (hex, mark);
}

static void
_ghex_mark_pane_refresh (GHexMarkPane *self)
{
	g_return_if_fail (GHEX_IS_MARK_PANE (self));

	/* This is kind of lame, but since we've bound our spin button's value to
	 * updating sensitivity, etc., there are times we want things to react as
	 * though the spin button's value has been set to a different value, even
	 * if it hasn't been.
	 */
	g_object_notify (G_OBJECT(self->spin_button), "value");
}

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
	HexMark *mark = NULL;
	const int mark_num = spin_button_value;
	HexView *hex = ghex_pane_get_hex (GHEX_PANE(self));

	if (!hex)
		return NULL;

	mark = marks_ht_get (self, mark_num);

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
	const int mark_num = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(self->spin_button));
	HexMark *mark = marks_ht_get (self, mark_num);
	GdkRGBA color = {0};

	if (!mark)
		return;

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER(color_button), &color);
	G_GNUC_END_IGNORE_DEPRECATIONS
	color.alpha = 0.5;

	hex_mark_set_custom_color (mark, &color);
}

/* Mark actions */

static void
goto_mark (GHexMarkPane *self, int mark_num)
{
	HexMark *mark;
	HexView *hex = ghex_pane_get_hex (GHEX_PANE(self));

	g_assert (HEX_IS_VIEW (hex));

	mark = marks_ht_get (self, mark_num);

	if (! mark) {
		g_debug ("%s: No mark found at %d", __func__, mark_num);
		return;
	}

	hex_view_goto_mark (hex, mark);
}

static void
set_mark (GHexMarkPane *self, int mark_num)
{
	g_autoptr(HexMark) mark = NULL;
	HexHighlight *highlight;
	HexSelection *selection;
	HexView *hex = ghex_pane_get_hex (GHEX_PANE(self));

	g_assert (HEX_IS_VIEW (hex));

	mark = hex_mark_new ();

	selection = hex_view_get_selection (hex);

	highlight = hex_mark_get_highlight (mark);
	hex_highlight_set_start_offset (highlight, hex_selection_get_start_offset (selection));
	hex_highlight_set_end_offset (highlight, hex_selection_get_end_offset (selection));

	marks_ht_set (self, mark_num, mark);

	hex_selection_collapse (selection, hex_selection_get_cursor_pos (selection));
}

static void
delete_mark (GHexMarkPane *self, int mark_num)
{
	HexView *hex = ghex_pane_get_hex (GHEX_PANE(self));

	g_assert (HEX_IS_VIEW (hex));

	marks_ht_delete (self, mark_num);
}

static void
mark_action (GtkWidget *widget, const char *action_name, GVariant *parameter)
{
	GHexMarkPane *self = (GHexMarkPane *) widget;
	g_autofree char *mark_action = NULL;
	g_autofree char *mark_name = NULL;
	int mark_num;
	HexView *hex;

	g_assert (GHEX_IS_MARK_PANE (self));

	hex = ghex_pane_get_hex (GHEX_PANE(self));
	if (!hex)
		return;

	g_variant_get (parameter, "(sims)", &mark_action, &mark_num, &mark_name);

	if (g_strcmp0 (mark_action, "jump") == 0)
	{
		goto_mark (self, mark_num);
	}
	else if (g_strcmp0 (mark_action, "set") == 0)
	{
		set_mark (self, mark_num);
	}
	else if (g_strcmp0 (mark_action, "delete") == 0)
	{
		delete_mark (self, mark_num);
	}
	else
	{
		g_critical ("%s: Invalid action: %s", __func__, mark_action);
	}

	_ghex_mark_pane_refresh (self);
}

static void
ghex_mark_pane_init (GHexMarkPane *self)
{
	GtkWidget *widget = GTK_WIDGET(self);

	gtk_widget_init_template (widget);

	self->marks_ht = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

	update_action_targets (self);

	g_signal_connect (self, "realize", G_CALLBACK(_ghex_mark_pane_refresh), NULL);

	g_signal_connect_object (self->color_button, "color-set", G_CALLBACK(color_set_cb), self, G_CONNECT_SWAPPED);

	g_signal_connect_object (self->spin_button, "value-changed", G_CALLBACK(update_action_targets), self, G_CONNECT_SWAPPED);

	g_signal_connect_object (self->close_button, "clicked", G_CALLBACK(ghex_pane_close), self, G_CONNECT_SWAPPED);
}

static void
ghex_mark_pane_dispose (GObject *object)
{
	GHexMarkPane *self = GHEX_MARK_PANE(object);

	g_clear_pointer (&self->box, gtk_widget_unparent);
	g_clear_pointer (&self->marks_ht, g_hash_table_unref);

	/* Chain up */
	G_OBJECT_CLASS(ghex_mark_pane_parent_class)->dispose (object);
}

static void
ghex_mark_pane_class_init (GHexMarkPaneClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GHexPaneClass *pane_class = GHEX_PANE_CLASS(klass);

	object_class->dispose = ghex_mark_pane_dispose;

	gtk_widget_class_set_css_name (widget_class, "markpane");

	/* Actions */

	gtk_widget_class_install_action (widget_class, "mark-pane.mark",
			"(sims)",
			mark_action);

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
