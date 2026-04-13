// vim: linebreak breakindent breakindentopt=shift\:4

#include "hex-color-code.h"

enum
{
	PROP_0,
	PROP_START_GRADIENT_COLOR,
	PROP_END_GRADIENT_COLOR,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _HexColorCode
{
	GObject parent_instance;

	GdkRGBA colors[256];
	GdkRGBA start_gradient_color;
	GdkRGBA end_gradient_color;
};

G_DEFINE_TYPE (HexColorCode, hex_color_code, G_TYPE_OBJECT)

static void
sync_internal_gradient (HexColorCode *self)
{
	for (int i = 0; i < 256; ++i)
		self->colors[i] = self->start_gradient_color;

	self->colors[255] = self->end_gradient_color;

	for (int i = 1; i <= 254; ++i)
	{
		GdkRGBA *cp = &self->colors[i];

		cp->red += (self->end_gradient_color.red - self->start_gradient_color.red) * (i / 256.0f);
		cp->green += (self->end_gradient_color.green - self->start_gradient_color.green) * (i / 256.0f);
		cp->blue += (self->end_gradient_color.blue- self->start_gradient_color.blue) * (i / 256.0f);
	}
}

void
hex_color_code_set_start_gradient_color (HexColorCode *self, const GdkRGBA *start_gradient_color)
{
	g_return_if_fail (HEX_IS_COLOR_CODE (self));
	g_return_if_fail (start_gradient_color != NULL);

	self->start_gradient_color = *start_gradient_color;

	sync_internal_gradient (self);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_START_GRADIENT_COLOR]);
}

void
hex_color_code_get_start_gradient_color (HexColorCode *self, GdkRGBA *start_gradient_color)
{
	g_return_if_fail (HEX_IS_COLOR_CODE (self));
	g_return_if_fail (start_gradient_color != NULL);

	*start_gradient_color = self->start_gradient_color;
}

void
hex_color_code_set_end_gradient_color (HexColorCode *self, const GdkRGBA *end_gradient_color)
{
	g_return_if_fail (HEX_IS_COLOR_CODE (self));
	g_return_if_fail (end_gradient_color != NULL);

	self->end_gradient_color = *end_gradient_color;

	sync_internal_gradient (self);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_END_GRADIENT_COLOR]);
}

void
hex_color_code_get_end_gradient_color (HexColorCode *self, GdkRGBA *end_gradient_color)
{
	g_return_if_fail (HEX_IS_COLOR_CODE (self));
	g_return_if_fail (end_gradient_color != NULL);

	*end_gradient_color = self->end_gradient_color;
}

static void
hex_color_code_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexColorCode *self = HEX_COLOR_CODE(object);

	switch (property_id)
	{
		case PROP_START_GRADIENT_COLOR:
			hex_color_code_set_start_gradient_color (self, g_value_get_boxed (value));
			break;

		case PROP_END_GRADIENT_COLOR:
			hex_color_code_set_end_gradient_color (self, g_value_get_boxed (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_color_code_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexColorCode *self = HEX_COLOR_CODE(object);

	switch (property_id)
	{
		case PROP_START_GRADIENT_COLOR:
		{
			GdkRGBA tmp = {0};
			hex_color_code_get_start_gradient_color (self, &tmp);
			g_value_set_boxed (value, &tmp);
		}
			break;

		case PROP_END_GRADIENT_COLOR:
		{
			GdkRGBA tmp = {0};
			hex_color_code_get_end_gradient_color (self, &tmp);
			g_value_set_boxed (value, &tmp);
		}
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_color_code_init (HexColorCode *self)
{
	self->start_gradient_color.alpha = 1.0f;
	self->end_gradient_color.alpha = 1.0f;

	for (int i = 0; i < 256; ++i)
		self->colors[i].alpha = 1.0f;
}

static void
hex_color_code_dispose (GObject *object)
{
	HexColorCode *self = HEX_COLOR_CODE(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_color_code_parent_class)->dispose (object);
}

static void
hex_color_code_finalize (GObject *object)
{
	HexColorCode *self = HEX_COLOR_CODE(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_color_code_parent_class)->finalize (object);
}

static void
hex_color_code_class_init (HexColorCodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose =  hex_color_code_dispose;
	object_class->finalize = hex_color_code_finalize;

	object_class->set_property = hex_color_code_set_property;
	object_class->get_property = hex_color_code_get_property;

	properties[PROP_START_GRADIENT_COLOR] = g_param_spec_boxed ("start-gradient-color", NULL, NULL,
			GDK_TYPE_RGBA,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_END_GRADIENT_COLOR] = g_param_spec_boxed ("end-gradient-color", NULL, NULL,
			GDK_TYPE_RGBA,
			default_flags | G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

HexColorCode *
hex_color_code_new (void)
{
	return g_object_new (HEX_TYPE_COLOR_CODE, NULL);
}

void
hex_color_code_get_color_at (HexColorCode *self, guint8 pos, GdkRGBA *color)
{
	g_return_if_fail (HEX_IS_COLOR_CODE (self));
	g_return_if_fail (color != NULL);

	*color = self->colors[pos];
}
