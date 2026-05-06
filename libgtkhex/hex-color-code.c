// vim: linebreak breakindent breakindentopt=shift\:4

#include "hex-color-code.h"

enum
{
	SIG_CHANGED,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

struct _HexColorCode
{
	GObject parent_instance;

	GdkRGBA *colors;
};

G_DEFINE_FINAL_TYPE (HexColorCode, hex_color_code, G_TYPE_OBJECT)

static void
emit_changed (HexColorCode *self)
{
	g_signal_emit (self, signals[SIG_CHANGED], 0);
}

void
hex_color_code_sync_from_color_array (HexColorCode *self, const GdkRGBA *colors)
{
	g_return_if_fail (HEX_IS_COLOR_CODE (self));
	g_return_if_fail (colors != NULL);

	memcpy (self->colors, colors, 256 * sizeof(GdkRGBA));

	emit_changed (self);
}

void
hex_color_code_sync_from_gradient (HexColorCode *self, HexGradient *gradient)
{
	g_autofree GdkRGBA *colors = NULL;

	g_return_if_fail (HEX_IS_COLOR_CODE (self));
	g_return_if_fail (HEX_IS_GRADIENT (gradient));

	colors = hex_gradient_to_rgba_array (gradient, 256);
	g_assert (colors != NULL);

	/* emits changed */
	hex_color_code_sync_from_color_array (self, colors);
}


static void
hex_color_code_init (HexColorCode *self)
{
	self->colors = g_new0 (GdkRGBA, 256);

	/* Init all colors to solid black */

	for (int i = 0; i < 256; ++i)
		self->colors[i].alpha = 1.0f;
}

static void
hex_color_code_dispose (GObject *object)
{
	HexColorCode *self = HEX_COLOR_CODE(object);

	G_OBJECT_CLASS(hex_color_code_parent_class)->dispose (object);
}

static void
hex_color_code_finalize (GObject *object)
{
	HexColorCode *self = HEX_COLOR_CODE(object);

	g_free (self->colors);

	G_OBJECT_CLASS(hex_color_code_parent_class)->finalize (object);
}

static void
hex_color_code_class_init (HexColorCodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose =  hex_color_code_dispose;
	object_class->finalize = hex_color_code_finalize;

	signals[SIG_CHANGED] = g_signal_new_class_handler ("changed",
			G_OBJECT_CLASS_TYPE (object_class),
			G_SIGNAL_RUN_LAST, 
			NULL,
			NULL, NULL, NULL,
			G_TYPE_NONE,
			0);
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
