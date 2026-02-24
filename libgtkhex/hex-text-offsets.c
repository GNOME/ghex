// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-text-offsets"

#include "hex-text-offsets.h"

enum
{
	PROP_0,
	PROP_OFFSET_CPL,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _HexTextOffsets
{
	HexText parent_instance;

	int offset_cpl;
};

G_DEFINE_TYPE (HexTextOffsets, hex_text_offsets, HEX_TYPE_TEXT)

int
hex_text_offsets_get_offset_cpl (HexTextOffsets *self)
{
	g_return_val_if_fail (HEX_IS_TEXT_OFFSETS (self), 8);

	return self->offset_cpl;
}

void
hex_text_offsets_set_offset_cpl (HexTextOffsets *self, int offset_cpl)
{
	g_return_if_fail (HEX_IS_TEXT_OFFSETS (self));

	self->offset_cpl = offset_cpl;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_OFFSET_CPL]);
}

static void
hex_text_offsets_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexTextOffsets *self = HEX_TEXT_OFFSETS(object);

	switch (property_id)
	{
		case PROP_OFFSET_CPL:
			hex_text_offsets_set_offset_cpl (self, g_value_get_int (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_text_offsets_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexTextOffsets *self = HEX_TEXT_OFFSETS(object);

	switch (property_id)
	{
		case PROP_OFFSET_CPL:
			g_value_set_int (value, hex_text_offsets_get_offset_cpl (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static char *
hex_text_offsets_format_line (HexText *ht, int line_num, guchar *line_data, size_t line_len)
{
	HexTextOffsets *self = HEX_TEXT_OFFSETS(ht);
	HexTextRenderData *render_data = hex_text_get_render_data (HEX_TEXT(self));
	int data_cpl = hex_view_get_cpl (HEX_VIEW(ht));
	gint64 offset_line_num = (render_data->top_disp_line + line_num) * data_cpl;
	g_autofree char *offset_str_fmt = g_strdup_printf ("<span font=\"%s\">%%0%dlX</span>", hex_view_get_font (HEX_VIEW(self)), self->offset_cpl);
	g_autofree char *offset_str = NULL;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
	offset_str = g_strdup_printf (offset_str_fmt, offset_line_num);
#pragma GCC diagnostic pop

	return g_steal_pointer (&offset_str);
}

static void
hex_text_offsets_dispose (GObject *object)
{
	HexTextOffsets *self = HEX_TEXT_OFFSETS(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_text_offsets_parent_class)->dispose (object);
}

static void
hex_text_offsets_finalize (GObject *object)
{
	HexTextOffsets *self = HEX_TEXT_OFFSETS(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_text_offsets_parent_class)->finalize (object);
}

static void
hex_text_offsets_class_init (HexTextOffsetsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose = hex_text_offsets_dispose;
	object_class->finalize = hex_text_offsets_finalize;
	object_class->set_property = hex_text_offsets_set_property;
	object_class->get_property = hex_text_offsets_get_property;

	HEX_TEXT_CLASS(klass)->format_line = hex_text_offsets_format_line;

	properties[PROP_OFFSET_CPL] = g_param_spec_int ("offset-cpl", NULL, NULL,
			0, 100, 8,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
hex_text_offsets_init (HexTextOffsets *self)
{
}
