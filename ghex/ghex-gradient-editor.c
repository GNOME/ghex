// vim: linebreak breakindent breakindentopt=shift\:4

#include "ghex-gradient-editor.h"

struct _GHexGradientEditor
{
	GtkWidget parent_instance;

	gpointer box, bar, color_btn, adj, spin;
	gpointer color_stop_popover;
};

G_DEFINE_TYPE (GHexGradientEditor, ghex_gradient_editor, GTK_TYPE_WIDGET)

static void
bar_active_stop_changed_cb (GHexGradientEditor *self)
{
	HexColorStop *stop = NULL;
	GdkRGBA color = {0};

	g_assert (GHEX_IS_GRADIENT_EDITOR (self));

	stop = ghex_gradient_bar_get_active_stop (self->bar);
	hex_color_stop_get_color (stop, &color);

	gtk_color_dialog_button_set_rgba (self->color_btn, &color);
}

static void
color_btn_color_changed_cb (GHexGradientEditor *self)
{
	const GdkRGBA *color = NULL;
	HexColorStop *stop = NULL;

	g_assert (GHEX_IS_GRADIENT_EDITOR (self));

	color = gtk_color_dialog_button_get_rgba (self->color_btn);
	stop = ghex_gradient_bar_get_active_stop (self->bar);

	hex_color_stop_set_color (stop, color);
}

static void
popover_spin_value_changed_cb (GtkSpinButton *spin, GHexGradientBar *bar)
{
	HexColorStop *stop = NULL;

	g_assert (GTK_IS_SPIN_BUTTON (spin));
	g_assert (GHEX_IS_GRADIENT_BAR (bar));

	stop = ghex_gradient_bar_get_active_stop (bar);
	hex_color_stop_set_offset (stop, gtk_spin_button_get_value (spin));
}

static gpointer
create_popover (GHexGradientEditor *self)
{
	gpointer popover = gtk_popover_new ();
	gpointer grid = gtk_grid_new ();
	gpointer label = gtk_label_new ("Color stop offset:");
	GtkAdjustment *adj = gtk_adjustment_new (0.0, 0.0, 1.0, 0.05, 0.0, 0.0);
	gpointer spin = gtk_spin_button_new (adj, 0.1, 4);
	GtkExpression *active_stop_expr, *offset_expr;

	gtk_grid_attach (grid, label, 0, 0, 1, 1);
	gtk_grid_attach (grid, spin, 0, 1, 1, 1);

	gtk_popover_set_child (popover, grid);

	active_stop_expr = gtk_property_expression_new (GHEX_TYPE_GRADIENT_BAR, NULL, "active-stop");
	offset_expr = gtk_property_expression_new (HEX_TYPE_COLOR_STOP, active_stop_expr, "offset");

	gtk_expression_bind (offset_expr, spin, "value", self->bar);

	g_signal_connect_object (spin, "value-changed", G_CALLBACK(popover_spin_value_changed_cb), self->bar, 0);

	return popover;
}

static void
right_click_cb (GHexGradientEditor *self, gint n_press, gdouble x, gdouble y, GtkGestureClick *gesture)
{
	g_assert (GHEX_IS_GRADIENT_EDITOR (self));

	gtk_popover_set_pointing_to (self->color_stop_popover, &(GdkRectangle){x,y,1,1});
	gtk_popover_popup (self->color_stop_popover);
}

static void
ghex_gradient_editor_init (GHexGradientEditor *self)
{
	self->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);

	gtk_widget_set_parent (self->box, GTK_WIDGET(self));

	self->bar = ghex_gradient_bar_new ();

	gtk_widget_set_size_request (self->bar, 400, 80);

	self->color_btn = gtk_color_dialog_button_new (gtk_color_dialog_new ());

	self->adj = gtk_adjustment_new (HEX_GRADIENT_MIN_STOPS, HEX_GRADIENT_MIN_STOPS, HEX_GRADIENT_MAX_STOPS, 1.0, 0.0, 0.0);

	self->spin = gtk_spin_button_new (self->adj, 1.0, 0);

	gtk_box_append (self->box, self->bar);
	gtk_box_append (self->box, self->color_btn);
	gtk_box_append (self->box, self->spin);

	g_signal_connect_swapped (self->bar, "notify::active-stop", G_CALLBACK(bar_active_stop_changed_cb), self);

	g_signal_connect_swapped (self->color_btn, "notify::rgba", G_CALLBACK(color_btn_color_changed_cb), self);

	{
		HexGradient *gradient = ghex_gradient_bar_get_gradient (self->bar);

		g_object_bind_property (self->spin, "value", gradient, "n-stops", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
	}

	/* Emit this notify signal to initialize the color of color_btn */
	g_object_notify (self->bar, "active-stop");

	self->color_stop_popover = create_popover (self);
	gtk_widget_set_parent (self->color_stop_popover, GTK_WIDGET(self));

	{
		gpointer right_click = gtk_gesture_click_new ();

		gtk_gesture_single_set_button (right_click, 3);
		g_signal_connect_swapped (right_click, "pressed", G_CALLBACK(right_click_cb), self);
		gtk_widget_add_controller (GTK_WIDGET(self), right_click);
	}
}

static void
ghex_gradient_editor_dispose (GObject *object)
{
	GHexGradientEditor *self = GHEX_GRADIENT_EDITOR(object);

	g_clear_pointer (&self->box, gtk_widget_unparent);
	g_clear_pointer (&self->color_stop_popover, gtk_widget_unparent);
}

static void
ghex_gradient_editor_class_init (GHexGradientEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	object_class->dispose = ghex_gradient_editor_dispose;

	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

GtkWidget *
ghex_gradient_editor_new (void)
{
	return g_object_new (GHEX_TYPE_GRADIENT_EDITOR, NULL);
}

/* transfer none */

GtkWidget *
ghex_gradient_editor_get_bar (GHexGradientEditor *self)
{
	g_return_val_if_fail (GHEX_IS_GRADIENT_EDITOR (self), NULL);

	return self->bar;
}
