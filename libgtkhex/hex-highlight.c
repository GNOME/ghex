// vim: linebreak breakindent breakindentopt=shift\:4

#include "hex-highlight-private.h"

enum
{
	CHANGED,
	N_SIGNALS
};

guint signals[N_SIGNALS];

enum
{
	PROP_0,
	PROP_START_OFFSET,
	PROP_END_OFFSET,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE (HexHighlight, hex_highlight, G_TYPE_OBJECT)

void
hex_highlight_update (HexHighlight *self, gint64 start_offset, gint64 end_offset)
{
	GParamSpecInt64 *pspec = G_PARAM_SPEC_INT64(properties[PROP_START_OFFSET]);

	g_return_if_fail (HEX_IS_HIGHLIGHT (self));

	start_offset = CLAMP (start_offset, pspec->minimum, pspec->maximum);
	end_offset = CLAMP (end_offset, pspec->minimum, pspec->maximum);

	if (start_offset == self->start_offset && end_offset == self->end_offset)
		return;

	self->start_offset = start_offset;
	self->end_offset = end_offset;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_START_OFFSET]);
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_END_OFFSET]);
	g_signal_emit (self, signals[CHANGED], 0);
}

void
hex_highlight_clear (HexHighlight *self)
{
	g_return_if_fail (HEX_IS_HIGHLIGHT (self));

	self->start_offset = 0;
	self->end_offset = 0;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_START_OFFSET]);
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_END_OFFSET]);
	g_signal_emit (self, signals[CHANGED], 0);
}

void
hex_highlight_set_start_offset (HexHighlight *self, gint64 start_offset)
{
	GParamSpecInt64 *pspec = G_PARAM_SPEC_INT64(properties[PROP_START_OFFSET]);

	g_return_if_fail (HEX_IS_HIGHLIGHT (self));

	start_offset = CLAMP (start_offset, pspec->minimum, pspec->maximum);

	if (start_offset == self->start_offset)
		return;

	self->start_offset = start_offset;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_START_OFFSET]);
	g_signal_emit (self, signals[CHANGED], 0);
}

gint64
hex_highlight_get_start_offset (HexHighlight *self)
{
	g_return_val_if_fail (HEX_IS_HIGHLIGHT (self), 0);

	return self->start_offset;
}

void
hex_highlight_set_end_offset (HexHighlight *self, gint64 end_offset)
{
	GParamSpecInt64 *pspec = G_PARAM_SPEC_INT64(properties[PROP_END_OFFSET]);

	g_return_if_fail (HEX_IS_HIGHLIGHT (self));

	end_offset = CLAMP (end_offset, pspec->minimum, pspec->maximum);

	if (end_offset == self->end_offset)
		return;

	self->end_offset = end_offset;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_END_OFFSET]);
	g_signal_emit (self, signals[CHANGED], 0);
}

gint64
hex_highlight_get_end_offset (HexHighlight *self)
{
	g_return_val_if_fail (HEX_IS_HIGHLIGHT (self), 0);

	return self->end_offset;
}

static void
hex_highlight_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexHighlight *self = HEX_HIGHLIGHT(object);

	switch (property_id)
	{
		case PROP_START_OFFSET:
			hex_highlight_set_start_offset (self, g_value_get_int64 (value));
			break;

		case PROP_END_OFFSET:
			hex_highlight_set_end_offset (self, g_value_get_int64 (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_highlight_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexHighlight *self = HEX_HIGHLIGHT(object);

	switch (property_id)
	{
		case PROP_START_OFFSET:
			g_value_set_int64 (value, hex_highlight_get_start_offset (self));
			break;

		case PROP_END_OFFSET:
			g_value_set_int64 (value, hex_highlight_get_end_offset (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_highlight_init (HexHighlight *self)
{
}

static void
hex_highlight_dispose (GObject *object)
{
	HexHighlight *self = HEX_HIGHLIGHT(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_highlight_parent_class)->dispose (object);
}

static void
hex_highlight_finalize (GObject *object)
{
	HexHighlight *self = HEX_HIGHLIGHT(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_highlight_parent_class)->finalize (object);
}

static void
hex_highlight_class_init (HexHighlightClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose =  hex_highlight_dispose;
	object_class->finalize = hex_highlight_finalize;
	object_class->set_property = hex_highlight_set_property;
	object_class->get_property = hex_highlight_get_property;

	properties[PROP_START_OFFSET] = g_param_spec_int64 ("start-offset", NULL, NULL,
			0, INT64_MAX, 0,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_END_OFFSET] = g_param_spec_int64 ("end-offset", NULL, NULL,
			0, INT64_MAX, 0,
			default_flags | G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	signals[CHANGED] = g_signal_new ("changed",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			0,
			NULL, NULL, NULL,
			G_TYPE_NONE,
			0);
}

HexHighlight *
hex_highlight_new (void)
{
	return g_object_new (HEX_TYPE_HIGHLIGHT, NULL);
}

gsize
hex_highlight_get_n_selected (HexHighlight *self)
{
	g_return_val_if_fail (HEX_IS_HIGHLIGHT (self), 0);

	return ABS (self->end_offset - self->start_offset) + 1;
}

int
_hex_highlight_compare_func (gconstpointer a, gconstpointer b, gpointer user_data)
{
	const HexHighlight *hl_a = a;
	const HexHighlight *hl_b = b;

	return hl_a->start_offset - hl_b->start_offset;
}
