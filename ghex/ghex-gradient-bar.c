// vim: linebreak breakindent breakindentopt=shift\:4

#include "ghex-gradient-bar.h"

#include <math.h>

enum
{
	PROP_0,
	PROP_GRADIENT,
	PROP_ACTIVE_STOP,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _GHexGradientBar
{
	GtkWidget parent_instance;

	HexGradient *gradient;
	int active;
};

G_DEFINE_FINAL_TYPE (GHexGradientBar, ghex_gradient_bar, GTK_TYPE_WIDGET)

static void
draw_triangle (GHexGradientBar *self, GtkSnapshot *snapshot, float x, float y, gboolean active, GdkRGBA *color)
{
	GskPathBuilder *b = gsk_path_builder_new ();
	g_autoptr(GskPath) path = NULL;
	g_autoptr(GskStroke) stroke = NULL;

	gsk_path_builder_move_to (b, x, y);
	gsk_path_builder_line_to (b, x - 7, y - 12);
	gsk_path_builder_line_to (b, x + 7, y - 12);
	gsk_path_builder_close (b);

	path = gsk_path_builder_free_to_path (b);

	if (active)
		gtk_snapshot_append_fill (snapshot, path, GSK_FILL_RULE_WINDING, color);

	stroke = gsk_stroke_new (1.0f);
	gtk_snapshot_append_stroke (snapshot, path, stroke, color);
}

static void
ghex_gradient_bar_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	GHexGradientBar *self = GHEX_GRADIENT_BAR(widget);

	const int n_stops = hex_gradient_get_n_stops (self->gradient);

	float w = gtk_widget_get_width (widget);
	float h = gtk_widget_get_height (widget);

	graphene_point_t start = GRAPHENE_POINT_INIT(0, 0);
	graphene_point_t end   = GRAPHENE_POINT_INIT(w, 0);

	GskColorStop color_stops[HEX_GRADIENT_MAX_STOPS] = {0};

	for (guint i = 0; i < n_stops; ++i)
	{
		HexColorStop *stop = hex_gradient_get_stop_at_index (self->gradient, i);
		const GskColorStop *color_stop = hex_color_stop_as_gsk_color_stop (stop);

		g_assert (color_stop != NULL);

		color_stops[i] = *color_stop;
	}

	gtk_snapshot_append_linear_gradient (snapshot,
			&GRAPHENE_RECT_INIT(0, 0, w, h),
			&start,
			&end,
			color_stops,
			n_stops);

	/* Draw handles */

	for (int i = 0; i < n_stops; ++i)
	{
		float x = color_stops[i].offset * w;
		GdkRGBA color = color_stops[i].color;

		color.red = 1.0f - color.red;
		color.green = 1.0f - color.green;
		color.blue = 1.0f - color.blue;

		draw_triangle (self, snapshot, x, h, i == self->active, &color);
	}
}

static void
pick_stop (GHexGradientBar *self, double x)
{
	float w = gtk_widget_get_width (GTK_WIDGET(self));
	float best = 1e9;
	int idx = 0;

	for (guint i = 0; i < hex_gradient_get_n_stops (self->gradient); ++i)
	{
		HexColorStop *stop = hex_gradient_get_stop_at_index (self->gradient, i);
		float px = hex_color_stop_get_offset (stop) * w;
		float d = fabs (px - x);

		if (d < best)
		{
			best = d;
			idx = i;
		}
	}

	self->active = idx;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_ACTIVE_STOP]);
}

static void
refresh_stop_bindings (GHexGradientBar *self)
{
	for (int i = 0; i < hex_gradient_get_n_stops (self->gradient); ++i)
	{
		HexColorStop *stop = hex_gradient_get_stop_at_index (self->gradient, i);

		g_signal_handlers_disconnect_by_data (stop, self);

		g_signal_connect_object (stop, "notify::offset", G_CALLBACK(gtk_widget_queue_draw), self, G_CONNECT_SWAPPED);

		g_signal_connect_object (stop, "notify::color", G_CALLBACK(gtk_widget_queue_draw), self, G_CONNECT_SWAPPED);
	}

	gtk_widget_queue_draw (GTK_WIDGET(self));
}

/* x should be the coordinate of the mouse click if applicable.
 * Pass a negative number for x to pick a slot automatically */
static void
add_stop (GHexGradientBar *self, double x)
{
	float w;
	float offset;
	GskColorStop s;
	g_autoptr(HexColorStop) stop = NULL;
	const int n_stops = hex_gradient_get_n_stops (self->gradient);

	if (n_stops >= HEX_GRADIENT_MAX_STOPS)
		return;

	w = gtk_widget_get_width (GTK_WIDGET (self));
	w = MAX (1.0f, w);

	if (x < 0)
	{
		const int total = n_stops + 1;

		if (total == 1)
			x = 0;
		else
			x = (n_stops / (float)(total - 1)) * w;
	}

	offset = CLAMP (x / w, 0.0f, 1.0f);

	// TEST
	s = (GskColorStop){ offset, {1.0f, 1.0f, 1.0f, 1.0f}};

	stop = hex_color_stop_new_from_gsk_color_stop (&s);

	hex_gradient_add_color_stop (self->gradient, g_steal_pointer (&stop));

	refresh_stop_bindings (self);
}

static void
click_cb (GHexGradientBar *self, int n_press, double x, double y, GtkGestureClick *click)
{
	g_assert (GHEX_IS_GRADIENT_BAR (self));

    if (n_press == 2)
		add_stop (self, x);
	else
		pick_stop (self, x);

    gtk_widget_queue_draw (GTK_WIDGET(self));
}

static void
drag_update_cb (GtkGestureDrag *gesture, double dx, double dy, gpointer data)
{
	GHexGradientBar *self = data;
	HexColorStop *active_stop = ghex_gradient_bar_get_active_stop (self);
	double start_x, start_y;
	double offset_x, offset_y;
	double current_x;
	float w, new_offset;

	if (! active_stop)
		return;

	gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);

	gtk_gesture_drag_get_offset (gesture, &offset_x, &offset_y);

	current_x = start_x + offset_x;

	w = gtk_widget_get_width (GTK_WIDGET(self));
	new_offset = CLAMP (current_x / w, 0.0f, 1.0f);

	hex_color_stop_set_offset (active_stop, new_offset);
}

static void
ghex_gradient_bar_init (GHexGradientBar *self)
{
	{
		g_autoptr(HexGradient) gradient = hex_gradient_new ();

		ghex_gradient_bar_set_gradient (self, gradient);

		refresh_stop_bindings (self);
	}

	{
		GtkGesture *click = gtk_gesture_click_new ();

		gtk_widget_add_controller (GTK_WIDGET(self), GTK_EVENT_CONTROLLER(click));
		g_signal_connect_swapped (click, "pressed", G_CALLBACK(click_cb), self);
	}

	{
		GtkGesture *drag = gtk_gesture_drag_new ();

		gtk_widget_add_controller (GTK_WIDGET(self), GTK_EVENT_CONTROLLER(drag));
		g_signal_connect (drag, "drag-update", G_CALLBACK(drag_update_cb), self);
	}
}

static void
ghex_gradient_bar_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GHexGradientBar *self = GHEX_GRADIENT_BAR(object);

	switch (property_id)
	{
		case PROP_GRADIENT:
			ghex_gradient_bar_set_gradient (self, g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_gradient_bar_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	GHexGradientBar *self = GHEX_GRADIENT_BAR(object);

	switch (property_id)
	{
		case PROP_GRADIENT:
			g_value_set_object (value, ghex_gradient_bar_get_gradient (self));
			break;

		case PROP_ACTIVE_STOP:
			g_value_set_object (value, ghex_gradient_bar_get_active_stop (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_gradient_bar_dispose (GObject *object)
{
	GHexGradientBar *self = GHEX_GRADIENT_BAR(object);

	g_clear_object (&self->gradient);

	G_OBJECT_CLASS(ghex_gradient_bar_parent_class)->dispose (object);
}

static void
ghex_gradient_bar_class_init (GHexGradientBarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose = ghex_gradient_bar_dispose;
	object_class->set_property = ghex_gradient_bar_set_property;
	object_class->get_property = ghex_gradient_bar_get_property;

	widget_class->snapshot = ghex_gradient_bar_snapshot;
	
	properties[PROP_GRADIENT] = g_param_spec_object ("gradient", NULL, NULL,
			HEX_TYPE_GRADIENT,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_ACTIVE_STOP] = g_param_spec_object ("active-stop", NULL, NULL,
			HEX_TYPE_COLOR_STOP,
			default_flags | G_PARAM_READABLE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

GtkWidget *
ghex_gradient_bar_new (void)
{
	return g_object_new (GHEX_TYPE_GRADIENT_BAR, NULL);
}

/* transfer none */

void
ghex_gradient_bar_set_gradient (GHexGradientBar *self, HexGradient *gradient)
{
	g_return_if_fail (GHEX_IS_GRADIENT_BAR (self));
	g_return_if_fail (HEX_IS_GRADIENT (gradient));

	g_clear_object (&self->gradient);
	self->gradient = g_object_ref (gradient);

	g_signal_connect_object (self->gradient, "notify::n-stops", G_CALLBACK(refresh_stop_bindings), self, G_CONNECT_SWAPPED);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_GRADIENT]);
}

/* transfer none */

HexGradient *
ghex_gradient_bar_get_gradient (GHexGradientBar *self)
{
	g_return_val_if_fail (GHEX_IS_GRADIENT_BAR (self), NULL);

	return self->gradient;
}

/* transfer none */

HexColorStop *
ghex_gradient_bar_get_active_stop (GHexGradientBar *self)
{
	g_return_val_if_fail (GHEX_IS_GRADIENT_BAR (self), NULL);

	g_return_val_if_fail (self->active >= 0 && self->active < hex_gradient_get_n_stops (self->gradient), NULL);

	return hex_gradient_get_stop_at_index (self->gradient, self->active);
}
