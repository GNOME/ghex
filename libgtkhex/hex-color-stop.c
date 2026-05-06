#include "hex-color-stop.h"

enum
{
	PROP_0,
	PROP_OFFSET,
	PROP_COLOR,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _HexColorStop
{
	GObject parent_instance;

	GskColorStop color_stop;
};

G_DEFINE_FINAL_TYPE (HexColorStop, hex_color_stop, G_TYPE_OBJECT)

void
hex_color_stop_set_offset (HexColorStop *self, float offset)
{
	g_return_if_fail (HEX_IS_COLOR_STOP (self));

	g_warn_if_fail (offset >= 0.0 && offset <= 1.0);

	offset = CLAMP (offset, 0.0f, 1.0f);
	self->color_stop.offset = offset;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_OFFSET]);
}

float
hex_color_stop_get_offset (HexColorStop *self)
{
	g_return_val_if_fail (HEX_IS_COLOR_STOP (self), 0.0f);

	return self->color_stop.offset;
}

void
hex_color_stop_set_color (HexColorStop *self, const GdkRGBA *color)
{
	g_return_if_fail (HEX_IS_COLOR_STOP (self));
	g_return_if_fail (color != NULL);

	self->color_stop.color = *color;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_COLOR]);
}

void
hex_color_stop_get_color (HexColorStop *self, GdkRGBA *color)
{
	g_return_if_fail (HEX_IS_COLOR_STOP (self));
	g_return_if_fail (color != NULL);

	*color = self->color_stop.color;
}

const GskColorStop *
hex_color_stop_as_gsk_color_stop (HexColorStop *self)
{
	g_return_val_if_fail (HEX_IS_COLOR_STOP (self), NULL);

	return &self->color_stop;
}

static void
hex_color_stop_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexColorStop *self = HEX_COLOR_STOP(object);

	switch (property_id)
	{
		case PROP_OFFSET:
			hex_color_stop_set_offset (self, g_value_get_float (value));
			break;

		case PROP_COLOR:
			hex_color_stop_set_color (self, g_value_get_boxed (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_color_stop_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexColorStop *self = HEX_COLOR_STOP(object);

	switch (property_id)
	{
		case PROP_OFFSET:
			g_value_set_float (value, hex_color_stop_get_offset (self));
			break;

		case PROP_COLOR:
		{
			GdkRGBA tmp;
			hex_color_stop_get_color (self, &tmp);
			g_value_set_boxed (value, &tmp);
		}
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_color_stop_init (HexColorStop *self)
{
}

static void
hex_color_stop_class_init (HexColorStopClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->set_property = hex_color_stop_set_property;
	object_class->get_property = hex_color_stop_get_property;

	/* PROPERTIES */

	properties[PROP_OFFSET] = g_param_spec_float ("offset", NULL, NULL,
			0.0f, 1.0f, 0.0f,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_COLOR] = g_param_spec_boxed ("color", NULL, NULL,
			GDK_TYPE_RGBA,
			default_flags | G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

HexColorStop *
hex_color_stop_new (void)
{
	return g_object_new (HEX_TYPE_COLOR_STOP, NULL);
}

HexColorStop *
hex_color_stop_new_from_gsk_color_stop (const GskColorStop *color_stop)
{
	g_return_val_if_fail (color_stop != NULL, NULL);

	return g_object_new (HEX_TYPE_COLOR_STOP,
			"offset", color_stop->offset,
			"color", &color_stop->color,
			NULL);
}
