// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-text-hex"

#include "hex-text-hex.h"

// for enum - FIXME - probably should be moved.
#include "gtkhex-layout-manager.h"

#include "hex-text-common.h"
#include "hex-mark-private.h"
#include "util.h"

#include "libgtkhex-enums.h"

enum
{
	PROP_0,
	PROP_GROUP_TYPE,
	PROP_LOWER_NIBBLE,
	PROP_FADE_ZEROES,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _HexTextHex
{
	HexTextEditable parent_instance;

	HexWidgetGroupType group_type;
	gboolean lower_nibble;
	gboolean fade_zeroes;
};

G_DEFINE_TYPE (HexTextHex, hex_text_hex, HEX_TYPE_TEXT_EDITABLE)

static void hex_text_hex_set_group_type (HexTextHex *self, HexWidgetGroupType group_type);
static void hex_text_hex_set_lower_nibble (HexTextHex *self, gboolean lower_nibble);
static void hex_text_hex_set_fade_zeroes (HexTextHex *self, gboolean fade_zeroes);

inline static void
key_press_cb__set_nibble (HexTextHex *self, guchar val)
{
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	HexDocument *doc = hex_view_get_document (HEX_VIEW(self));
	const gboolean insert_mode = hex_view_get_insert_mode (HEX_VIEW(self));

	hex_document_set_nibble (doc, val, cursor_pos, self->lower_nibble, insert_mode, TRUE);

	hex_text_hex_set_lower_nibble (self, !self->lower_nibble);

	if (!self->lower_nibble)
		hex_selection_collapse (selection, cursor_pos + 1);
}

static gboolean
key_press_cb (GtkEventControllerKey *controller,
               guint                  keyval,
               guint                  keycode,
               GdkModifierType        state,
               gpointer               user_data)
{
	HexTextHex *self = HEX_TEXT_HEX(user_data);
	HexDocument *doc = hex_view_get_document (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gboolean insert_mode = hex_view_get_insert_mode (HEX_VIEW(self));
	int mod_mask = GDK_MODIFIER_MASK & ~GDK_SHIFT_MASK;

	if (state & mod_mask)
		return GDK_EVENT_PROPAGATE;

	if (keyval >= '0' && keyval <= '9')
	{
		key_press_cb__set_nibble (self, keyval - '0');
		return GDK_EVENT_STOP;
	}
	else if (keyval >= 'A' && keyval <= 'F')
	{
		key_press_cb__set_nibble (self, keyval - 'A' + 10);
		return GDK_EVENT_STOP;
	}
	else if (keyval >= 'a' && keyval <= 'f')
	{
		key_press_cb__set_nibble (self, keyval - 'a' + 10);
		return GDK_EVENT_STOP;
	}
	else if (keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9)
	{
		key_press_cb__set_nibble (self, keyval - GDK_KEY_KP_0);
		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}

static void
hex_text_hex_move_cursor (HexTextEditable *hte, GtkMovementStep step, int count, gboolean extend_selection)
{
	HexTextHex *self = HEX_TEXT_HEX(hte);
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	const int cpl = hex_view_get_cpl (HEX_VIEW(self));
	gint64 new_cursor_pos = -1;

	switch (step)
	{
		case GTK_MOVEMENT_VISUAL_POSITIONS:
			if (count == 1 && !self->lower_nibble)
				hex_text_hex_set_lower_nibble (self, TRUE);
			else if (count == -1 && self->lower_nibble)
				hex_text_hex_set_lower_nibble (self, FALSE);
			else if (count == 1 && self->lower_nibble) {
				new_cursor_pos = cursor_pos + 1;
				hex_text_hex_set_lower_nibble (self, FALSE);
			}
			else if (count == -1 && !self->lower_nibble) {
				new_cursor_pos = cursor_pos - 1;
				hex_text_hex_set_lower_nibble (self, TRUE);
			}
			else
				new_cursor_pos = cursor_pos + count;
			break;

		default:
			/* Chain up */
			HEX_TEXT_EDITABLE_CLASS(hex_text_hex_parent_class)->move_cursor (hte, step, count, extend_selection);
			return;
	}

	if (new_cursor_pos == -1)
		return;

	hex_text_common_finish_move_cursor (HEX_TEXT_EDITABLE(self), new_cursor_pos, extend_selection);
}

inline static int
line_byte_offset_to_hex_layout_index (HexTextHex *self, int line_byte_offset)
{
	int cpl = hex_view_get_cpl (HEX_VIEW(self));
	int spaces = (line_byte_offset % cpl) / self->group_type;
	int index = 2 * (line_byte_offset % cpl);

	index += spaces;

	return index;
}

inline static int
hex_layout_index_to_line_byte_offset (HexTextHex *self, PangoLayout *layout, int hex_layout_index, gboolean *lower_nibble)
{
	const char *str = pango_layout_get_text (layout);

	g_assert (str);

	for (int i = 0, hex_char_ct = 0, retval = 0; i < strlen (str); ++i)
	{
		if (str[i] == ' ')
			continue;

		++hex_char_ct;

		if (i >= hex_layout_index)
		{
			if (lower_nibble)
				*lower_nibble = (hex_char_ct % 2 == 0);

			return retval;
		}

		if (hex_char_ct % 2 == 0)
			++retval;
	}

	if (lower_nibble)
		*lower_nibble = FALSE;

	return 0;
}

static void
hex_text_hex_dragged (HexText *ht, PangoLayout *layout, int drag_line, int rel_x, int rel_y, GtkScrollType scroll)
{
	HexTextHex *self = HEX_TEXT_HEX(ht);
	int index = 0;
	gint64 byte_under_cursor;
	HexTextRenderData *render_data = hex_text_get_render_data (ht);
	int cpl = hex_view_get_cpl (HEX_VIEW(self));
	int line_byte_offset;
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	gboolean lower_nibble;

	if (!layout)
		goto chain_up;

	pango_layout_xy_to_index (layout, rel_x * PANGO_SCALE, rel_y * PANGO_SCALE, &index, NULL);

	line_byte_offset = hex_layout_index_to_line_byte_offset (self, layout, index, &lower_nibble);
	byte_under_cursor = (render_data->top_disp_line + drag_line) * cpl + line_byte_offset;

	hex_selection_set_cursor_pos (selection, byte_under_cursor);

	hex_text_hex_set_lower_nibble (self, lower_nibble);

chain_up:
	HEX_TEXT_CLASS(hex_text_hex_parent_class)->dragged (ht, layout, drag_line, rel_x, rel_y, scroll);
}

static void
hex_text_hex_pressed (HexText *ht, GdkModifierType state, PangoLayout *layout, int click_line, int rel_x, int rel_y)
{
	HexTextHex *self = HEX_TEXT_HEX(ht);
	int index = 0;
	gint64 cursor_pos;
	HexTextRenderData *render_data = hex_text_get_render_data (ht);
	int cpl = hex_view_get_cpl (HEX_VIEW(self));
	int line_byte_offset;
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	gboolean lower_nibble;

	if (!layout)
		goto chain_up;

	pango_layout_xy_to_index (layout, rel_x * PANGO_SCALE, rel_y * PANGO_SCALE, &index, NULL);

	line_byte_offset = hex_layout_index_to_line_byte_offset (self, layout, index, &lower_nibble);
	cursor_pos = (render_data->top_disp_line + click_line) * cpl + line_byte_offset;

	hex_text_hex_set_lower_nibble (self, lower_nibble);

	hex_selection_set_cursor_pos (selection, cursor_pos);

	if (! (state & GDK_SHIFT_MASK))
		hex_selection_set_selection_anchor (selection, cursor_pos);

chain_up:
	HEX_TEXT_CLASS(hex_text_hex_parent_class)->pressed (ht, state, layout, click_line, rel_x, rel_y);
}

static void
hex_text_hex_set_group_type (HexTextHex *self, HexWidgetGroupType group_type)
{
	self->group_type = group_type;
	gtk_widget_queue_draw (GTK_WIDGET(self));
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_GROUP_TYPE]);
}

static void
hex_text_hex_set_lower_nibble (HexTextHex *self, gboolean lower_nibble)
{
	self->lower_nibble = lower_nibble;
	gtk_widget_queue_draw (GTK_WIDGET(self));
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_LOWER_NIBBLE]);
}

static void
hex_text_hex_set_fade_zeroes (HexTextHex *self, gboolean fade_zeroes)
{
	g_return_if_fail (HEX_IS_TEXT_HEX (self));

	self->fade_zeroes = fade_zeroes;

	gtk_widget_queue_draw (GTK_WIDGET(self));

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_FADE_ZEROES]);
}

static void
hex_text_hex_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexTextHex *self = HEX_TEXT_HEX(object);

	switch (property_id)
	{
		case PROP_GROUP_TYPE:
			hex_text_hex_set_group_type (self, g_value_get_enum (value));
			break;

		case PROP_LOWER_NIBBLE:
			hex_text_hex_set_lower_nibble (self, g_value_get_boolean (value));
			break;

		case PROP_FADE_ZEROES:
			hex_text_hex_set_fade_zeroes (self, g_value_get_boolean (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_text_hex_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexTextHex *self = HEX_TEXT_HEX(object);

	switch (property_id)
	{
		case PROP_GROUP_TYPE:
			g_value_set_enum (value, self->group_type);
			break;

		case PROP_LOWER_NIBBLE:
			g_value_set_boolean (value, self->lower_nibble);
			break;

		case PROP_FADE_ZEROES:
			g_value_set_boolean (value, self->fade_zeroes);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/* transfer full */
static char *
hex_text_hex_format_line (HexText *ht, int line_num, guchar *line_data, size_t line_len)
{
	HexTextHex *self = HEX_TEXT_HEX(ht);
	g_autoptr(GString) gstr = g_string_new (NULL);

	g_string_append_printf (gstr, "<span font=\"%s\">", hex_view_get_font (HEX_VIEW(self)));

	/* Special case: for an empty or new file, set the layout text to 2 spaces
	 * so that we have one (visibly empty) set of hex pairs to render a cursor over.
	 */
	if (line_len == 0)
	{
		g_string_append (gstr, "  ");
	}
	else
	{
		g_assert (line_data != NULL);

		for (size_t i = 0; i < line_len; ++i)
		{
			guchar character = line_data[i];
			g_autofree char *char_str = g_strdup_printf ("%02X", character);

			if (self->fade_zeroes && character == 0)
				g_string_append_printf (gstr, "<span foreground=\"grey\">%s</span>", char_str);
			else
				g_string_append (gstr, char_str);

			if ((i+1) % self->group_type == 0)
				g_string_append_c (gstr, ' ');
		}
	}

	g_string_append (gstr, "</span>");

	return g_steal_pointer (&gstr->str);
}

static void
hex_text_hex_render_cursor (HexTextHex *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	const int cpl = hex_view_get_cpl (HEX_VIEW(self));
	const gboolean insert_mode = hex_view_get_insert_mode (HEX_VIEW(self));
	const gboolean at_file_end = hex_text_common_get_is_cursor_at_file_end (HEX_TEXT_EDITABLE(self));
	const gboolean at_new_row = hex_text_common_get_is_cursor_at_new_row (HEX_TEXT_EDITABLE(self));
	int cursor_line;

	if (! hex_text_offset_is_visible (HEX_TEXT(self), cursor_pos, &cursor_line))
		return;

	if (cursor_line != line_num)
	{
		/* One special case: when we're at the end of a line in insert mode, we
		 * actually render the cursor on the 0th column of the NEXT line.
		 */
		if (insert_mode && at_file_end && at_new_row && line_num == cursor_line - 1)
			;	/* continue */
		else
			return;
	}

	{
		gint64 index;
		int range[2];
		int spaces;

		spaces = (cursor_pos % cpl) / self->group_type;
		index = 2 * (cursor_pos % cpl);
		index += spaces;

		if (insert_mode && at_file_end)
		{
			if (at_new_row)
				index = 0;
			else
				--index;

			if (self->lower_nibble)
				--index;
		}

		index = MAX (0, index);

		if (self->lower_nibble) {
			range[0] = index + 1;
			range[1] = index + 2;
		} else {
			range[0] = index;
			range[1] = index + 1;
		}

		range[0] = hex_text_common_char_index_to_utf8_byte_index (pango_layout_get_text (layout), range[0]);
		range[1] = hex_text_common_char_index_to_utf8_byte_index (pango_layout_get_text (layout), range[1]);

		hex_text_common_render_cursor (HEX_TEXT(self), snapshot, layout, range, insert_mode, at_file_end, at_new_row, self->lower_nibble);
	}
}

static void
render_single_highlight (HexTextHex *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout, HexHighlight *highlight, const GdkRGBA *color)
{
	int sel_start, sel_end;
	int range[2];

	if (! hex_text_highlight_is_visible (HEX_TEXT(self), highlight, line_num, &sel_start, &sel_end))
		return;

	range[0] = line_byte_offset_to_hex_layout_index (self, sel_start);
	range[1] = line_byte_offset_to_hex_layout_index (self, sel_end);
	range[1] += 2;

	hex_text_common_render_highlight (GTK_WIDGET(self), snapshot, layout, range, color);
}

static void
render_highlights__selection (HexTextHex *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	HexHighlight *highlight = hex_selection_get_highlight (selection);

	render_single_highlight (self, snapshot, line_num, layout, highlight, NULL);
}

static void
render_highlights__marks (HexTextHex *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	GListModel *marks = hex_view_get_marks (HEX_VIEW(self));

	if (! marks)
		return;

	for (guint i = 0; i < g_list_model_get_n_items (marks); ++i)
	{
		g_autoptr(HexMark) mark = g_list_model_get_item (marks, i);

		render_single_highlight (self, snapshot, line_num, layout, mark->highlight, mark->have_custom_color ? &mark->custom_color : NULL);
	}
}

static void
render_highlights__auto_highlights (HexTextHex *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	GListModel *auto_highlights = hex_view_get_auto_highlights (HEX_VIEW(self));

	if (! auto_highlights)
		return;

	for (guint i = 0; i < g_list_model_get_n_items (auto_highlights); ++i)
	{
		g_autoptr(HexAutoHighlight) auto_highlight = g_list_model_get_item (auto_highlights, i);
		GListModel *highlights = hex_auto_highlight_get_highlights (auto_highlight);

		for (guint j = 0; j < g_list_model_get_n_items (highlights); ++j)
		{
			g_autoptr(HexHighlight) highlight = g_list_model_get_item (highlights, j);
			// TEST
			GdkRGBA color = {1.0, 1.0, 0.5, 0.75};

			render_single_highlight (self, snapshot, line_num, layout, highlight, &color);
		}
	}
}

static void	
hex_text_hex_render_highlights (HexTextHex *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	render_highlights__selection (self, snapshot, line_num, layout);
	render_highlights__marks (self, snapshot, line_num, layout);
	render_highlights__auto_highlights (self, snapshot, line_num, layout);
}

static void	
hex_text_hex_render_line (HexText *ht, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	HexTextHex *self = HEX_TEXT_HEX(ht);

	hex_text_hex_render_highlights (self, snapshot, line_num, layout);

	HEX_TEXT_CLASS(hex_text_hex_parent_class)->render_line (ht, snapshot, line_num, layout);

	hex_text_hex_render_cursor (self, snapshot, line_num, layout);
}

static void
hex_text_hex_dispose (GObject *object)
{
	HexTextHex *self = HEX_TEXT_HEX(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_text_hex_parent_class)->dispose (object);
}

static void
hex_text_hex_finalize (GObject *object)
{
	HexTextHex *self = HEX_TEXT_HEX(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_text_hex_parent_class)->finalize (object);
}

static void
hex_text_hex_class_init (HexTextHexClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose = hex_text_hex_dispose;
	object_class->finalize = hex_text_hex_finalize;
	object_class->set_property = hex_text_hex_set_property;
	object_class->get_property = hex_text_hex_get_property;

	HEX_TEXT_CLASS(klass)->format_line = hex_text_hex_format_line;
	HEX_TEXT_CLASS(klass)->render_line = hex_text_hex_render_line;
	HEX_TEXT_CLASS(klass)->pressed = hex_text_hex_pressed;
	HEX_TEXT_CLASS(klass)->dragged = hex_text_hex_dragged;

	HEX_TEXT_EDITABLE_CLASS(klass)->move_cursor = hex_text_hex_move_cursor;

	properties[PROP_GROUP_TYPE] = g_param_spec_enum ("group-type", NULL, NULL,
			HEX_TYPE_WIDGET_GROUP_TYPE,
			HEX_WIDGET_GROUP_BYTE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_LOWER_NIBBLE] = g_param_spec_boolean ("lower-nibble", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_FADE_ZEROES] = g_param_spec_boolean ("fade-zeroes", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	/* Keybindings */

	hex_text_common_add_move_binding (widget_class, GDK_KEY_Right, 0,
			GTK_MOVEMENT_VISUAL_POSITIONS, 1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_KP_Right, 0,
			GTK_MOVEMENT_VISUAL_POSITIONS, 1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_Left, 0,
			GTK_MOVEMENT_VISUAL_POSITIONS, -1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_KP_Left, 0,
			GTK_MOVEMENT_VISUAL_POSITIONS, -1);
}

static void
hex_text_hex_init (HexTextHex *self)
{
	gtk_widget_set_focusable (GTK_WIDGET(self), TRUE);

	hex_text_set_cursor_visible (HEX_TEXT(self), TRUE);

	{
		GtkEventController *controller = gtk_event_controller_key_new ();

		g_signal_connect (controller, "key-pressed", G_CALLBACK(key_press_cb), self);
		gtk_widget_add_controller (GTK_WIDGET(self), controller);
	}
}
