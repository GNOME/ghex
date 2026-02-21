// vim: linebreak breakindent breakindentopt=shift\:4

#include "hex-mark-private.h"
#include "hex-highlight-private.h"

enum {
	PROP_0,
	PROP_HAVE_CUSTOM_COLOR,
	PROP_CUSTOM_COLOR,
	PROP_HIGHLIGHT,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE (HexMark, hex_mark, G_TYPE_OBJECT)

/**
 * hex_mark_get_have_custom_color:
 *
 * Returns whether the `HexMark` has a custom color associated with it.
 *
 * Returns: `TRUE` if the `HexMark` has a custom color associated with
 *   it; `FALSE` otherwise.
 */
gboolean
hex_mark_get_have_custom_color (HexMark *self)
{
	g_return_val_if_fail (HEX_IS_MARK (self), FALSE);

	return self->have_custom_color;
}

/**
 * hex_mark_get_custom_color:
 * @color: (out): A `GdkRGBA` structure to be set with the custom color
 *   associated with the `HexMark`, if applicable
 *
 * Obtains the custom color associated with a `HexMark` object, if
 * any.
 *
 * Since: 4.8
 */
void
hex_mark_get_custom_color (HexMark *self, GdkRGBA *color)
{
	g_return_if_fail (HEX_IS_MARK (self));
	g_return_if_fail (color != NULL);

	*color = self->custom_color;
}

void
hex_mark_set_custom_color (HexMark *self, GdkRGBA *color)
{
	g_return_if_fail (HEX_IS_MARK (self));
	g_return_if_fail (color != NULL);

	self->have_custom_color = TRUE;
	self->custom_color = *color;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_CUSTOM_COLOR]);
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_HAVE_CUSTOM_COLOR]);
}

HexHighlight *
hex_mark_get_highlight (HexMark *self)
{
	g_return_val_if_fail (HEX_IS_MARK (self), NULL);

	return self->highlight;
}

static void
hex_mark_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexMark *self = HEX_MARK(object);

	switch (property_id)
	{
		case PROP_CUSTOM_COLOR:
			hex_mark_set_custom_color (self, g_value_get_boxed (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_mark_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexMark *self = HEX_MARK(object);

	switch (property_id)
	{
		case PROP_HAVE_CUSTOM_COLOR:
			g_value_set_boolean (value, hex_mark_get_have_custom_color (self));
			break;

		case PROP_CUSTOM_COLOR:
		{
			GdkRGBA color;

			hex_mark_get_custom_color (self, &color);
			g_value_set_boxed (value, &color);
		}
			break;

		case PROP_HIGHLIGHT:
			g_value_set_object (value, hex_mark_get_highlight (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_mark_init (HexMark *self)
{
	self->highlight = hex_highlight_new ();
}

static void
hex_mark_dispose (GObject *object)
{
	HexMark *self = HEX_MARK(object);

	g_clear_object (&self->highlight);

	/* Chain up */
	G_OBJECT_CLASS(hex_mark_parent_class)->dispose (object);
}

static void
hex_mark_finalize (GObject *object)
{
	HexMark *self = HEX_MARK(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_mark_parent_class)->finalize (object);
}

static void
hex_mark_class_init (HexMarkClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->set_property = hex_mark_set_property;
	object_class->get_property = hex_mark_get_property;
	object_class->dispose = hex_mark_dispose;
	object_class->finalize = hex_mark_finalize;

	/**
	 * HexMark:have-custom-color:
	 *
	 * Whether the `HexMark` has a custom color.
	 */
	properties[PROP_HAVE_CUSTOM_COLOR] = g_param_spec_boolean ("have-custom-color", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READABLE);

	/**
	 * HexMark:custom-color:
	 *
	 * The custom color of the `HexMark`, if applicable.
	 */
	properties[PROP_CUSTOM_COLOR] = g_param_spec_boxed ("custom-color", NULL, NULL,
			GDK_TYPE_RGBA,
			default_flags | G_PARAM_READWRITE);

	/**
	 * HexMark:highlight:
	 *
	 * The `HexHighlight` (selection range) of the `HexMark`.
	 */
	properties[PROP_HIGHLIGHT] = g_param_spec_object ("highlight", NULL, NULL,
			HEX_TYPE_HIGHLIGHT,
			default_flags | G_PARAM_READABLE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

HexMark *
hex_mark_new (void)
{
	return g_object_new (HEX_TYPE_MARK, NULL);
}
