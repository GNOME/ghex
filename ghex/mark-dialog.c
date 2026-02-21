/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* mark-dialog.c - marks dialog

   Copyright © 2023 Logan Rathbone <poprocks@gmail.com>

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

   Original GHex Author: Jaka Mocnik <jaka@gnu.org>
*/

#include "mark-dialog.h"

#include <glib/gi18n.h>
#include <config.h>

/* DEFINES */

#define MARK_DIALOG_CSS_NAME "markdialog"

struct _MarkDialog {
	PaneDialog parent_instance;

	GtkWidget *box;
	GtkWidget *spin_button;
	GtkWidget *color_button;
	GtkWidget *mark_description_label;
	GtkWidget *set_mark_button;
	GtkWidget *goto_mark_button;
	GtkWidget *delete_mark_button;
	GtkWidget *close_button;
	GtkEventController *focus_controller;
};

G_DEFINE_TYPE (MarkDialog, mark_dialog, PANE_TYPE_DIALOG)

static void
update_action_targets (MarkDialog *self)
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
sensitive_closure_cb (GObject *object, HexWidgetMark *mark, gpointer user_data)
{
	return mark ? TRUE : FALSE;
}

static HexWidgetMark *
mark_spin_button_closure_cb (GObject *object, double spin_button_value, gpointer user_data)
{
	MarkDialog *self = MARK_DIALOG(object);
	HexWidget *gh = pane_dialog_get_hex (PANE_DIALOG(self));
	char *key = NULL;
	HexWidgetMark *mark = NULL;

	if (! gh)
		goto out;

	key = g_strdup_printf ("mark%d", (int)spin_button_value);
	mark = g_object_get_data (G_OBJECT(gh), key);

	if (! mark)
		goto out;

	if (! hex_widget_mark_get_have_custom_color (mark))
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
mark_description_label_closure_cb (GObject *object, HexWidgetMark *mark, gpointer user_data)
{
	MarkDialog *self = MARK_DIALOG(object);
	char *str = NULL;
	gint64 start, end;

	if (! mark)
		return NULL;

	start = hex_widget_mark_get_start_offset (mark);
	end = hex_widget_mark_get_end_offset (mark);

	/* Translators: this is meant to show a range of hex offset values.
	 * eg, "0xAA - 0xFF"
	 */
	str = g_strdup_printf (_("0x%lX - 0x%lX"), start, end);

	return str;
}

static void
color_set_cb (MarkDialog *self, GtkColorButton *color_button)
{
	char *key = g_strdup_printf ("mark%d", gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(self->spin_button)));
	HexWidget *gh = pane_dialog_get_hex (PANE_DIALOG(self));
	HexWidgetMark *mark = g_object_get_data (G_OBJECT(gh), key);
	GdkRGBA color = {0};

	if (!mark)
		goto out;

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER(color_button), &color);
	G_GNUC_END_IGNORE_DEPRECATIONS
	color.alpha = 0.5;

	hex_widget_set_mark_custom_color (gh, mark, &color);

out:
	g_free (key);
}

static void
mark_dialog_init (MarkDialog *self)
{
	GtkWidget *widget = GTK_WIDGET(self);

	gtk_widget_init_template (widget);

	update_action_targets (self);

	g_signal_connect_swapped (self->color_button, "color-set", G_CALLBACK(color_set_cb), self);
	g_signal_connect_swapped (self->spin_button, "value-changed", G_CALLBACK(update_action_targets), self);
	g_signal_connect_swapped (self->close_button, "clicked", G_CALLBACK(pane_dialog_close), self);
}

static void
mark_dialog_dispose (GObject *object)
{
	MarkDialog *self = MARK_DIALOG(object);

	g_clear_pointer (&self->box, gtk_widget_unparent);

	/* Chain up */
	G_OBJECT_CLASS(mark_dialog_parent_class)->dispose (object);
}

static void
mark_dialog_class_init (MarkDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	object_class->dispose = mark_dialog_dispose;

	gtk_widget_class_set_css_name (widget_class, MARK_DIALOG_CSS_NAME);

	/* Setup template */

	gtk_widget_class_set_template_from_resource (widget_class,
			RESOURCE_BASE_PATH "/mark-dialog.ui");
	gtk_widget_class_bind_template_child (widget_class, MarkDialog, box);
	gtk_widget_class_bind_template_child (widget_class, MarkDialog, spin_button);
	gtk_widget_class_bind_template_child (widget_class, MarkDialog, color_button);
	gtk_widget_class_bind_template_child (widget_class, MarkDialog, mark_description_label);
	gtk_widget_class_bind_template_child (widget_class, MarkDialog, set_mark_button);
	gtk_widget_class_bind_template_child (widget_class, MarkDialog, goto_mark_button);
	gtk_widget_class_bind_template_child (widget_class, MarkDialog, delete_mark_button);
	gtk_widget_class_bind_template_child (widget_class, MarkDialog, close_button);
	gtk_widget_class_bind_template_callback (widget_class, sensitive_closure_cb);
	gtk_widget_class_bind_template_callback (widget_class, mark_spin_button_closure_cb);
	gtk_widget_class_bind_template_callback (widget_class, mark_description_label_closure_cb);
}

void
mark_dialog_refresh (MarkDialog *self)
{
	g_return_if_fail (MARK_IS_DIALOG (self));

	/* This is kind of lame, but since we've bound our spin button's value to
	 * updating sensitivity, etc., there are times we want things to react as
	 * though the spin button's value has been set to a different value, even
	 * if it hasn't been.
	 */
	g_object_notify (G_OBJECT(self->spin_button), "value");
}

void
mark_dialog_activate_mark_num (MarkDialog *self, int mark_num)
{
	g_return_if_fail (MARK_IS_DIALOG (self));

	if (mark_num < 0 || mark_num > 9) {
		g_warning ("%s: Programmer error: invalid mark number", __func__);
		return;
	}

	gtk_spin_button_set_value (GTK_SPIN_BUTTON(self->spin_button), mark_num);
}

GtkWidget *
mark_dialog_new (void)
{
	return g_object_new (MARK_TYPE_DIALOG, NULL);
}
