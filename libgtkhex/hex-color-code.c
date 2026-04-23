// vim: linebreak breakindent breakindentopt=shift\:4

#include "hex-color-code.h"

/* < HexGradient > */

/* This will be migrated to GskGradient if/when it becomes public. */

#define HEX_GRADIENT_MAX_COLOR_STOPS	4

typedef struct HexGradient HexGradient;
struct HexGradient
{
	GArray *color_stops;
};

static int
color_stop_compare (gconstpointer a, gconstpointer b)
{
	g_return_val_if_fail (a && b, 0);

	return (int) ( ((GskColorStop *)a)->offset * INT16_MAX ) - (int) ( ((GskColorStop *)b)->offset * INT16_MAX );
}

static void
hex_gradient_add_color_stop (HexGradient *gradient, const GskColorStop *color_stop)
{
	g_return_if_fail (gradient != NULL && gradient->color_stops != NULL);
	g_return_if_fail (color_stop != NULL);
	g_return_if_fail (gradient->color_stops->len <= HEX_GRADIENT_MAX_COLOR_STOPS);
	g_return_if_fail (color_stop->offset >= 0.0 && color_stop->offset <= 1.0);

	g_array_append_val (gradient->color_stops, *color_stop);

	g_array_sort (gradient->color_stops, color_stop_compare);
}

static HexGradient *
hex_gradient_new (void)
{
	HexGradient *gradient = g_new0 (HexGradient, 1);

	gradient->color_stops = g_array_new (TRUE, TRUE, sizeof (GskColorStop));

	return gradient;
}

static void
hex_gradient_destroy (HexGradient *gradient)
{
	g_return_if_fail (gradient != NULL);
	g_return_if_fail (gradient->color_stops != NULL);

	g_clear_pointer (&gradient->color_stops, g_array_unref);
	g_free (gradient);
}

/* </ HexGradient > */

struct _HexColorCode
{
	GObject parent_instance;

	GdkRGBA colors[256];
};

G_DEFINE_TYPE (HexColorCode, hex_color_code, G_TYPE_OBJECT)

static void
hex_color_code_sync_from_gradient (HexColorCode *self, HexGradient *gradient)
{
	g_return_if_fail (HEX_IS_COLOR_CODE (self));
	g_return_if_fail (gradient != NULL);

	for (int i = 0; i < (int) gradient->color_stops->len; ++i)
	{
		GskColorStop *csp = &g_array_index (gradient->color_stops, GskColorStop, i);
		GskColorStop *last_csp = NULL;
		int color_mod_idx = 0;
		int last_idx = 0;

		/* Calculate last color index if applicable */

		if (i - 1 >= 0)
		{
			last_csp = &g_array_index (gradient->color_stops, GskColorStop,  i - 1);
			last_idx = last_csp->offset * 255.0f;
			last_idx = CLAMP (last_idx, 0, 255);
		}

		/* Calculate this color index */

		color_mod_idx = csp->offset * 255.0f;
		color_mod_idx = CLAMP (color_mod_idx, 0, 255);

		/* Get from last color to this color by linear gradient */

		self->colors[color_mod_idx] = csp->color;

		for (int j = last_idx, num = 1; j < color_mod_idx; ++j, ++num)
		{
			const float denom = color_mod_idx - last_idx;
			float frac;

			g_assert (denom > FLT_EPSILON);

			frac = num / denom;

			self->colors[j].red += frac * (csp->color.red - self->colors[last_idx].red);
			self->colors[j].green += frac * (csp->color.green - self->colors[last_idx].green);
			self->colors[j].blue += frac * (csp->color.blue - self->colors[last_idx].blue);
		}

		/* Pad remaining colors with the current one as of the current offset */

		for (int j = color_mod_idx + 1; j <= 255; ++j)
		{
			self->colors[j] = csp->color;
		}
	}
}

static void
hex_color_code_init (HexColorCode *self)
{
	/* Init all colors to solid black */

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
}

HexColorCode *
hex_color_code_new (void)
{
	//return g_object_new (HEX_TYPE_COLOR_CODE, NULL);

	// TEST
	{
		HexColorCode *self = g_object_new (HEX_TYPE_COLOR_CODE, NULL);
		HexGradient *gradient = hex_gradient_new ();
		GskColorStop cs1, cs2, cs3, cs4, cs5;

		cs1.color = (GdkRGBA){.red=0.5f, .green=0.5f, .blue=0.5f, .alpha=1.0f};
		cs1.offset = 0.0f;

		cs2.color = (GdkRGBA){.red=1.0f, .green=0.0f, .blue=0.0f, .alpha=1.0f};
		cs2.offset = 0.25f;

		cs3.color = (GdkRGBA){.red=0.0f, .green=1.0f, .blue=0.0f, .alpha=1.0f};
		cs3.offset = 0.50f;

		cs4.color = (GdkRGBA){.red=0.0f, .green=0.0f, .blue=1.0f, .alpha=1.0f};
		cs4.offset = 0.75f;

		cs5.color = (GdkRGBA){.red=0.95f, .green=0.95f, .blue=0.95f, .alpha=1.0f};
		cs5.offset = 1.0f;

		hex_gradient_add_color_stop (gradient, &cs1);
		hex_gradient_add_color_stop (gradient, &cs2);
		hex_gradient_add_color_stop (gradient, &cs3);
		hex_gradient_add_color_stop (gradient, &cs4);
		hex_gradient_add_color_stop (gradient, &cs5);

		hex_color_code_sync_from_gradient (self, gradient);

		hex_gradient_destroy (gradient);

		return self;
	}
}

void
hex_color_code_get_color_at (HexColorCode *self, guint8 pos, GdkRGBA *color)
{
	g_return_if_fail (HEX_IS_COLOR_CODE (self));
	g_return_if_fail (color != NULL);

	*color = self->colors[pos];
}
