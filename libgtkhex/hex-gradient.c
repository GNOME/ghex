// vim: linebreak breakindent breakindentopt=shift\:4

#include "hex-gradient.h"

enum
{
	PROP_0,
	PROP_N_STOPS,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

enum
{
	SIG_CHANGED,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

struct _HexGradient
{
	GObject parent_instance;

	GPtrArray *stops;
};

G_DEFINE_FINAL_TYPE (HexGradient, hex_gradient, G_TYPE_OBJECT)

static int
compare_stops (gconstpointer a, gconstpointer b)
{
	float offset_a, offset_b;

	g_assert (HEX_IS_COLOR_STOP ((gpointer) a));
	g_assert (HEX_IS_COLOR_STOP ((gpointer) b));

	offset_a = hex_color_stop_get_offset ((gpointer) a);
	offset_b = hex_color_stop_get_offset ((gpointer) b);

    return (offset_a > offset_b) - (offset_a < offset_b);
}

static void
sort_stops (HexGradient *self)
{
	g_ptr_array_sort_values (self->stops, compare_stops);
}

static void
emit_changed (HexGradient *self)
{
	g_signal_emit (self, signals[SIG_CHANGED], 0);
}

void
hex_gradient_set_n_stops (HexGradient *self, int n_stops)
{
	g_return_if_fail (HEX_IS_GRADIENT (self));

	g_object_freeze_notify (G_OBJECT(self));

	n_stops = CLAMP (n_stops, HEX_GRADIENT_MIN_STOPS, HEX_GRADIENT_MAX_STOPS);

	while ((int) self->stops->len < n_stops)
	{
		const float offset = self->stops->len ? 1.0f : 0.0f;
		GskColorStop s = { offset, {1.0f, 1.0f, 1.0f, 1.0f}};;
		HexColorStop *stop = hex_color_stop_new_from_gsk_color_stop (&s);

		hex_gradient_add_color_stop (self, stop);
	}

	while ((int) self->stops->len > n_stops)
		g_ptr_array_remove_index (self->stops, self->stops->len - 1);

	g_assert ((int) self->stops->len == n_stops);

	sort_stops (self);

	g_object_thaw_notify (G_OBJECT(self));

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_N_STOPS]);
}

int
hex_gradient_get_n_stops (HexGradient *self)
{
	g_return_val_if_fail (HEX_IS_GRADIENT (self), HEX_GRADIENT_MIN_STOPS);

	g_assert (self->stops != NULL);

	return self->stops->len;
}

static void
hex_gradient_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexGradient *self = HEX_GRADIENT(object);

	switch (property_id)
	{
		case PROP_N_STOPS:
			hex_gradient_set_n_stops (self, g_value_get_int (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_gradient_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexGradient *self = HEX_GRADIENT(object);

	switch (property_id)
	{
		case PROP_N_STOPS:
			g_value_set_int (value, hex_gradient_get_n_stops (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_gradient_class_init (HexGradientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->set_property = hex_gradient_set_property;
	object_class->get_property = hex_gradient_get_property;

	properties[PROP_N_STOPS] = g_param_spec_int ("n-stops", NULL, NULL,
			HEX_GRADIENT_MIN_STOPS, HEX_GRADIENT_MAX_STOPS,
			HEX_GRADIENT_MIN_STOPS,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	signals[SIG_CHANGED] = g_signal_new_class_handler ("changed",
			G_OBJECT_CLASS_TYPE (object_class),
			G_SIGNAL_RUN_LAST, 
			NULL,
			NULL, NULL, NULL,
			G_TYPE_NONE,
			0);
}

static void
hex_gradient_init (HexGradient *self)
{
	self->stops = g_ptr_array_new_with_free_func (g_object_unref);
}

HexGradient *
hex_gradient_new (void)
{
	return g_object_new (HEX_TYPE_GRADIENT, NULL);
}

/* @stop = Transfer full */

gboolean
hex_gradient_add_color_stop (HexGradient *self, HexColorStop *stop)
{
	g_return_val_if_fail (HEX_IS_GRADIENT (self), FALSE);
	g_return_val_if_fail (HEX_IS_COLOR_STOP (stop), FALSE);

	if (self->stops->len >= HEX_GRADIENT_MAX_STOPS)
		return FALSE;

	g_signal_connect_object (stop, "notify::offset", G_CALLBACK(sort_stops), self, G_CONNECT_SWAPPED);

	g_signal_connect_object (stop, "notify::offset", G_CALLBACK(emit_changed), self, G_CONNECT_SWAPPED);
	g_signal_connect_object (stop, "notify::color", G_CALLBACK(emit_changed), self, G_CONNECT_SWAPPED);

	g_ptr_array_add (self->stops, stop);

	sort_stops (self);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_N_STOPS]);

	return TRUE;
}

/*
 * Returns: (array length=n_colors) (element-type GdkRGBA) (transfer full):
 *   An array of `GdkRGBA` of length `n_colors`, which should be freed with
 *   `g_free()`.
 */

GdkRGBA *
hex_gradient_to_rgba_array (HexGradient *self, guint n_colors)
{
	GdkRGBA *out = NULL;
	GdkRGBA tmp;

	g_return_val_if_fail (HEX_IS_GRADIENT (self), NULL);
	g_return_val_if_fail (n_colors > 0, NULL);

	out = g_new0 (GdkRGBA, n_colors);

	for (guint i = 0; i < n_colors; ++i)
		out[i].alpha = 1.0f;

	for (int i = 0; i < (int) self->stops->len; ++i)
	{
		HexColorStop *csp = g_ptr_array_index (self->stops, i);
		HexColorStop *last_csp = NULL;
		int color_mod_idx = 0;
		int last_idx = 0;

		/* Calculate last color index if applicable */

		if (i - 1 >= 0)
		{
			last_csp = g_ptr_array_index (self->stops, i - 1);
			last_idx = hex_color_stop_get_offset (last_csp) * 255.0f;
			last_idx = CLAMP (last_idx, 0, 255);
		}

		/* Calculate this color index */

		color_mod_idx = hex_color_stop_get_offset (csp) * 255.0f;
		color_mod_idx = CLAMP (color_mod_idx, 0, 255);

		/* Get from last color to this color by linear gradient */

		hex_color_stop_get_color (csp, &tmp);
		out[color_mod_idx] = tmp;

		for (int j = last_idx, num = 1; j < color_mod_idx; ++j, ++num)
		{
			const float denom = color_mod_idx - last_idx;
			float frac;

			g_assert (denom > FLT_EPSILON);

			frac = num / denom;

			out[j].red += frac * (tmp.red - out[last_idx].red);
			out[j].green += frac * (tmp.green - out[last_idx].green);
			out[j].blue += frac * (tmp.blue - out[last_idx].blue);
		}

		/* Pad remaining colors with the current one as of the current offset */

		for (int j = color_mod_idx + 1; j <= 255; ++j)
		{
			out[j] = tmp;
		}
	}

	return out;
}

/* transfer none */

HexColorStop *
hex_gradient_get_stop_at_index (HexGradient *self, guint index)
{
	HexColorStop *ret = NULL;

	g_return_val_if_fail (HEX_IS_GRADIENT (self), NULL);
	g_return_val_if_fail (index < self->stops->len, NULL);

	ret = g_ptr_array_index (self->stops, index);
	g_assert (HEX_IS_COLOR_STOP (ret));

	return ret;
}
