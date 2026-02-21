// vim: linebreak breakindent breakindentopt=shift\:4
#include "hex-selection.h"

enum {
	PROP_0,
	PROP_HIGHLIGHT,
	PROP_CURSOR_POS,
	PROP_SELECTION_ANCHOR,
	PROP_START_OFFSET,
	PROP_END_OFFSET,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

typedef struct
{
	GObject parent_instance;

	gint64 selection_anchor;
	gint64 cursor_pos;
	HexHighlight *highlight;
	
} HexSelectionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (HexSelection, hex_selection, G_TYPE_OBJECT)

static void
sync_highlight (HexSelection *self)
{
	HexSelectionPrivate *priv = hex_selection_get_instance_private (self);
	const gint64 start = MIN (priv->selection_anchor, priv->cursor_pos);
	const gint64 end = MAX (priv->selection_anchor, priv->cursor_pos);

	hex_highlight_update (priv->highlight, start, end);
}

static void
hex_selection_real_set_cursor_pos (HexSelection *self, gint64 cursor_pos)
{
	HexSelectionPrivate *priv = hex_selection_get_instance_private (self);

	priv->cursor_pos = cursor_pos;

	sync_highlight (self);
}

void
hex_selection_set_cursor_pos (HexSelection *self, gint64 cursor_pos)
{
	HexSelectionClass *klass;

	g_return_if_fail (HEX_IS_SELECTION (self));

	klass = HEX_SELECTION_GET_CLASS (self);
	g_return_if_fail (klass->set_cursor_pos != NULL);

	klass->set_cursor_pos (self, cursor_pos);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_CURSOR_POS]);
}

gint64
hex_selection_get_cursor_pos (HexSelection *self)
{
	HexSelectionPrivate *priv;

	g_return_val_if_fail (HEX_IS_SELECTION (self), 0);

	priv = hex_selection_get_instance_private (self);

	return priv->cursor_pos;
}

static void
hex_selection_real_set_selection_anchor (HexSelection *self, gint64 selection_anchor)
{
	HexSelectionPrivate *priv = hex_selection_get_instance_private (self);

	priv->selection_anchor = selection_anchor;

	sync_highlight (self);
}

void
hex_selection_set_selection_anchor (HexSelection *self, gint64 selection_anchor)
{
	HexSelectionClass *klass;

	g_return_if_fail (HEX_IS_SELECTION (self));

	klass = HEX_SELECTION_GET_CLASS (self);
	g_return_if_fail (klass->set_selection_anchor != NULL);

	klass->set_selection_anchor (self, selection_anchor);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_SELECTION_ANCHOR]);
}

gint64
hex_selection_get_selection_anchor (HexSelection *self)
{
	HexSelectionPrivate *priv;

	g_return_val_if_fail (HEX_IS_SELECTION (self), 0);

	priv = hex_selection_get_instance_private (self);

	return priv->selection_anchor;
}

gint64
hex_selection_get_start_offset (HexSelection *self)
{
	HexSelectionPrivate *priv;
	gint64 retval = 0;

	g_return_val_if_fail (HEX_IS_SELECTION (self), 0);

	priv = hex_selection_get_instance_private (self);

	retval = hex_highlight_get_start_offset (priv->highlight);

	return retval;
}

gint64
hex_selection_get_end_offset (HexSelection *self)
{
	HexSelectionPrivate *priv;
	gint64 retval = 0;

	g_return_val_if_fail (HEX_IS_SELECTION (self), 0);

	priv =  hex_selection_get_instance_private (self);

	retval = hex_highlight_get_end_offset (priv->highlight);

	return retval;
}

static void
hex_selection_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexSelection *self = HEX_SELECTION(object);

	switch (property_id)
	{
		case PROP_CURSOR_POS:
			hex_selection_set_cursor_pos (self, g_value_get_int64 (value));
			break;

		case PROP_SELECTION_ANCHOR:
			hex_selection_set_selection_anchor (self, g_value_get_int64 (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/* transfer: none */

HexHighlight *
hex_selection_get_highlight (HexSelection *self)
{
	HexSelectionPrivate *priv;

	g_return_val_if_fail (HEX_IS_SELECTION (self), NULL);

	priv  = hex_selection_get_instance_private (self);

	return priv->highlight;
}

static void
hex_selection_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexSelection *self = HEX_SELECTION(object);

	switch (property_id)
	{
		case PROP_HIGHLIGHT:
			g_value_set_object (value, hex_selection_get_highlight (self));
			break;

		case PROP_CURSOR_POS:
			g_value_set_int64 (value, hex_selection_get_cursor_pos (self));
			break;

		case PROP_SELECTION_ANCHOR:
			g_value_set_int64 (value, hex_selection_get_selection_anchor (self));
			break;

		case PROP_START_OFFSET:
			g_value_set_int64 (value, hex_selection_get_start_offset (self));
			break;

		case PROP_END_OFFSET:
			g_value_set_int64 (value, hex_selection_get_end_offset (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_selection_init (HexSelection *self)
{
	HexSelectionPrivate *priv = hex_selection_get_instance_private (self);

	priv->highlight = hex_highlight_new ();
}

static void
hex_selection_dispose (GObject *object)
{
	HexSelection *self = HEX_SELECTION(object);
	HexSelectionPrivate *priv = hex_selection_get_instance_private (self);

	g_clear_object (&priv->highlight);

	/* Chain up */
	G_OBJECT_CLASS(hex_selection_parent_class)->dispose (object);
}

static void
hex_selection_finalize (GObject *object)
{
	HexSelection *self = HEX_SELECTION(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_selection_parent_class)->finalize (object);
}

static void
hex_selection_class_init (HexSelectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->set_property = hex_selection_set_property;
	object_class->get_property = hex_selection_get_property;
	object_class->dispose = hex_selection_dispose;
	object_class->finalize = hex_selection_finalize;

	klass->set_cursor_pos = hex_selection_real_set_cursor_pos;
	klass->set_selection_anchor = hex_selection_real_set_selection_anchor;

	properties[PROP_HIGHLIGHT] = g_param_spec_object ("highlight", NULL, NULL,
			HEX_TYPE_HIGHLIGHT,
			default_flags | G_PARAM_READABLE);

	properties[PROP_CURSOR_POS] = g_param_spec_int64 ("cursor-pos", NULL, NULL,
			0, INT64_MAX, 0,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_SELECTION_ANCHOR] = g_param_spec_int64 ("selection-anchor", NULL, NULL,
			0, INT64_MAX, 0,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_START_OFFSET] = g_param_spec_int64 ("start-offset", NULL, NULL,
			0, INT64_MAX, 0,
			default_flags | G_PARAM_READABLE);

	properties[PROP_END_OFFSET] = g_param_spec_int64 ("end-offset", NULL, NULL,
			0, INT64_MAX, 0,
			default_flags | G_PARAM_READABLE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/* Convenience method to set selection_anchor and cursor_pos to the same value
 * simultaneously. Can also be used to jump to a cursor position while
 * clearing the highlight.
 */
void
hex_selection_collapse (HexSelection *self, gint64 pos)
{
	g_return_if_fail (HEX_IS_SELECTION (self));

	hex_selection_set_selection_anchor (self, pos);
	hex_selection_set_cursor_pos (self, pos);
}

HexSelection *
hex_selection_new (void)
{
	return g_object_new (HEX_TYPE_SELECTION, NULL);
}
