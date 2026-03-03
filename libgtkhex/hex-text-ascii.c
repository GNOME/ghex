// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-text-ascii"

#include "hex-text-ascii.h"
#include "hex-text-common.h"
#include "hex-mark-private.h"
#include "util.h"

#define is_displayable(c) (((c) >= 0x20) && ((c) < 0x7f))

enum
{
	PROP_0,
	PROP_DISPLAY_CONTROL_CHARACTERS,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _HexTextAscii
{
	HexTextEditable parent_instance;

	gboolean display_control_characters;
};

G_DEFINE_TYPE (HexTextAscii, hex_text_ascii, HEX_TYPE_TEXT_EDITABLE)

/* ASCII control characters - lookup table */

static const gunichar control_characters_lookup_table[] = {
    U'\u2400', // U+2400: NUL
    U'\u2401', // U+2401: SOH
    U'\u2402', // U+2402: STX
    U'\u2403', // U+2403: ETX
    U'\u2404', // U+2404: EOT
    U'\u2405', // U+2405: ENQ
    U'\u2406', // U+2406: ACK
    U'\u2407', // U+2407: BEL
    U'\u2408', // U+2408: BS
    U'\u2409', // U+2409: HT
    U'\u240A', // U+240A: LF
    U'\u240B', // U+240B: VT
    U'\u240C', // U+240C: FF
    U'\u240D', // U+240D: CR
    U'\u240E', // U+240E: SO
    U'\u240F', // U+240F: SI
    U'\u2410', // U+2410: DLE
    U'\u2411', // U+2411: DC1
    U'\u2412', // U+2412: DC2
    U'\u2413', // U+2413: DC3
    U'\u2414', // U+2414: DC4
    U'\u2415', // U+2415: NAK
    U'\u2416', // U+2416: SYN
    U'\u2417', // U+2417: ETB
    U'\u2418', // U+2418: CAN
    U'\u2419', // U+2419: EM
    U'\u241A', // U+241A: SUB
    U'\u241B', // U+241B: ESC
    U'\u241C', // U+241C: FS
    U'\u241D', // U+241D: GS
    U'\u241E', // U+241E: RS
    U'\u241F'  // U+241F: US
};

inline static gboolean
is_control_character (guint8 character)
{
	/* nb: Checking for >= 0 is redundant given the unsigned data type. */
	return character <= 0x1f;
}

inline static void
key_press_cb__set_byte (HexTextAscii *self, guchar val)
{
	HexDocument *doc = hex_view_get_document (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	const gboolean insert_mode = hex_view_get_insert_mode (HEX_VIEW(self));

	hex_document_set_byte (doc, val, cursor_pos, insert_mode, TRUE);
	hex_selection_collapse (selection, cursor_pos + 1);
}

static gboolean
key_press_cb (GtkEventControllerKey *controller,
               guint                  keyval,
               guint                  keycode,
               GdkModifierType        state,
               gpointer               user_data)
{
	HexTextAscii *self = HEX_TEXT_ASCII(user_data);
	int mod_mask = GDK_MODIFIER_MASK & ~GDK_SHIFT_MASK;

	if (state & mod_mask)
		return GDK_EVENT_PROPAGATE;

	if (is_displayable (keyval))
	{
		key_press_cb__set_byte (self, keyval);
		return GDK_EVENT_STOP;
	}
	else if (keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9)
	{
		key_press_cb__set_byte (self, keyval - GDK_KEY_KP_0 + '0');
		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}

static void
hex_text_ascii_move_cursor (HexTextEditable *hte, GtkMovementStep step, int count, gboolean extend_selection)
{
	HexTextAscii *self = HEX_TEXT_ASCII(hte);
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	const int cpl = hex_view_get_cpl (HEX_VIEW(self));
	gint64 new_cursor_pos;

	switch (step)
	{
		case GTK_MOVEMENT_VISUAL_POSITIONS:
			new_cursor_pos = cursor_pos + count;
			break;

		default:
			/* Chain up */
			HEX_TEXT_EDITABLE_CLASS(hex_text_ascii_parent_class)->move_cursor (hte, step, count, extend_selection);
			return;
	}

	hex_text_common_finish_move_cursor (HEX_TEXT_EDITABLE(self), new_cursor_pos, extend_selection);
}

static void
hex_text_ascii_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexTextAscii *self = HEX_TEXT_ASCII(object);

	switch (property_id)
	{
		case PROP_DISPLAY_CONTROL_CHARACTERS:
			hex_text_ascii_set_display_control_characters (self, g_value_get_boolean (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_text_ascii_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexTextAscii *self = HEX_TEXT_ASCII(object);

	switch (property_id)
	{
		case PROP_DISPLAY_CONTROL_CHARACTERS:
			g_value_set_boolean (value, hex_text_ascii_get_display_control_characters (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/* transfer full */
static char *
hex_text_ascii_format_line (HexText *ht, int line_num, guchar *line_data, size_t line_len)
{
	HexTextAscii *self = HEX_TEXT_ASCII(ht);
	g_autoptr(GString) gstr = g_string_new (NULL);
	
	g_string_append_printf (gstr, "<span font=\"%s\">", hex_view_get_font (HEX_VIEW(self)));

	/* Special case: for an empty or new file, set the layout text to a space
	 * so that we have one (visibly empty) character to render a cursor over.
	 */
	if (line_len == 0)
	{
		g_string_append_c (gstr, ' ');
	}
	else
	{
		g_assert (line_data != NULL);

		for (size_t i = 0; i < line_len; ++i)
		{
			guchar character = line_data[i];
			g_autofree char *char_str = NULL;
			g_autofree char *escaped_char_str = NULL;

			if (self->display_control_characters && is_control_character (character))
			{
				g_autoptr(GString) tmp = g_string_new (NULL);

				g_string_append_unichar (tmp, control_characters_lookup_table[character]);

				char_str = g_steal_pointer (&tmp->str);
			}
			else
			{
				char_str = g_strdup_printf ("%c", is_displayable (character) ? character : '.');
			}

			escaped_char_str = g_markup_escape_text (char_str, -1);

			g_string_append (gstr, escaped_char_str);
		}
	}

	g_string_append (gstr, "</span>");

	return g_steal_pointer (&gstr->str);
}

static void
hex_text_ascii_render_cursor (HexTextAscii *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
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
		if (insert_mode && at_file_end && at_new_row && line_num == cursor_line - 1)
			;	/* continue */
		else
			return;
	}

	{
		int range[2];

		range[0] = cursor_pos % cpl;

		if (insert_mode && at_file_end)
		{
			if (at_new_row)
				range[0] = 0;
			else
				--range[0];
		}

		range[0] = MAX (0, range[0]);

		range[1] = range[0] + 1;

		range[0] = hex_text_common_char_index_to_utf8_byte_index (pango_layout_get_text (layout), range[0]);
		range[1] = hex_text_common_char_index_to_utf8_byte_index (pango_layout_get_text (layout), range[1]);

		hex_text_common_render_cursor (HEX_TEXT(self), snapshot, layout, range, insert_mode, at_file_end, at_new_row, FALSE);
	}
}

static void
render_single_highlight (HexTextAscii *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout, HexHighlight *highlight, const GdkRGBA *color)
{
	const char *layout_str;
	int sel_start, sel_end;
	const char *start_cp, *end_cp;
	int range[2];

	if (! hex_text_highlight_is_visible (HEX_TEXT(self), highlight, line_num, &sel_start, &sel_end))
		return;

	layout_str = pango_layout_get_text (layout);
	start_cp = g_utf8_offset_to_pointer (layout_str, sel_start);
	end_cp = g_utf8_offset_to_pointer (layout_str, sel_end);
	end_cp = g_utf8_find_next_char (end_cp, NULL);

	range[0] = start_cp - layout_str;
	range[1] = end_cp - layout_str;

	hex_text_common_render_highlight (GTK_WIDGET(self), snapshot, layout, range, color);
}

static void
render_highlights__selection (HexTextAscii *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	HexHighlight *highlight = hex_selection_get_highlight (selection);

	render_single_highlight (self, snapshot, line_num, layout, highlight, NULL);
}

static void
render_highlights__marks (HexTextAscii *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	GListModel *marks = hex_view_get_marks (HEX_VIEW(self));

	if (! marks)
		return;

	for (guint i = 0; i < g_list_model_get_n_items (marks); ++i)
	{
		g_autoptr(HexMark) mark = g_list_model_get_item (marks, i);

		render_single_highlight (self, snapshot, line_num, layout, mark->highlight, mark->have_custom_color ? &mark->custom_color : &HEX_MARK_DEFAULT_COLOR);
	}
}

static void
render_highlights__auto_highlights (HexTextAscii *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
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
hex_text_ascii_render_highlights (HexTextAscii *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	render_highlights__selection (self, snapshot, line_num, layout);
	render_highlights__marks (self, snapshot, line_num, layout);
	render_highlights__auto_highlights (self, snapshot, line_num, layout);
}

static void	
hex_text_ascii_render_line (HexText *ht, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	HexTextAscii *self = HEX_TEXT_ASCII(ht);

	hex_text_ascii_render_highlights (self, snapshot, line_num, layout);

	HEX_TEXT_CLASS(hex_text_ascii_parent_class)->render_line (ht, snapshot, line_num, layout);

	hex_text_ascii_render_cursor (self, snapshot, line_num, layout);
}

static gint64
get_offset_from_mouse (HexTextAscii *self, PangoLayout *layout, int click_line, int rel_x, int rel_y)
{
	HexTextRenderData *render_data = hex_text_get_render_data (HEX_TEXT(self));
	const int cpl = hex_view_get_cpl (HEX_VIEW(self));
	int byte_index;
	gint64 retval;

	pango_layout_xy_to_index (layout, rel_x * PANGO_SCALE, rel_y * PANGO_SCALE, &byte_index, NULL);

	if (self->display_control_characters)
	{
		const char *layout_str = pango_layout_get_text (layout);
		long visible_index;

		g_assert (layout_str && byte_index < strlen (layout_str));

		visible_index = g_utf8_pointer_to_offset (layout_str, layout_str + byte_index);

		retval = (render_data->top_disp_line + click_line) * cpl + visible_index;
	}
	else
	{
		retval = (render_data->top_disp_line + click_line) * cpl + byte_index;
	}

	return retval;
}

static void
hex_text_ascii_pressed (HexText *ht, GdkModifierType state, PangoLayout *layout, int click_line, int rel_x, int rel_y)
{
	HexTextAscii *self = HEX_TEXT_ASCII(ht);
	int index = 0;
	gint64 cursor_pos;
	HexTextRenderData *render_data = hex_text_get_render_data (ht);
	int cpl = hex_view_get_cpl (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));

	if (!layout)
		goto chain_up;

	cursor_pos = get_offset_from_mouse (self, layout, click_line, rel_x, rel_y);

	hex_selection_set_cursor_pos (selection, cursor_pos);

	if (! (state & GDK_SHIFT_MASK))
		hex_selection_set_selection_anchor (selection, cursor_pos);

chain_up:
	HEX_TEXT_CLASS(hex_text_ascii_parent_class)->pressed (ht, state, layout, click_line, rel_x, rel_y);
}

static void
hex_text_ascii_dragged (HexText *ht, PangoLayout *layout, int drag_line, int rel_x, int rel_y, GtkScrollType scroll)
{
	HexTextAscii *self = HEX_TEXT_ASCII(ht);
	int index = 0;
	gint64 cursor_pos;
	HexTextRenderData *render_data = hex_text_get_render_data (ht);
	int cpl = hex_view_get_cpl (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));

	if (!layout)
		goto chain_up;

	cursor_pos = get_offset_from_mouse (self, layout, drag_line, rel_x, rel_y);

	hex_selection_set_cursor_pos (selection, cursor_pos);

chain_up:
	HEX_TEXT_CLASS(hex_text_ascii_parent_class)->dragged (ht, layout, drag_line, rel_x, rel_y, scroll);
}

static void
hex_text_ascii_dispose (GObject *object)
{
	HexTextAscii *self = HEX_TEXT_ASCII(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_text_ascii_parent_class)->dispose (object);
}

static void
hex_text_ascii_finalize (GObject *object)
{
	HexTextAscii *self = HEX_TEXT_ASCII(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_text_ascii_parent_class)->finalize (object);
}

static void
hex_text_ascii_class_init (HexTextAsciiClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose = hex_text_ascii_dispose;
	object_class->finalize = hex_text_ascii_finalize;
	object_class->set_property = hex_text_ascii_set_property;
	object_class->get_property = hex_text_ascii_get_property;

	HEX_TEXT_CLASS(klass)->format_line = hex_text_ascii_format_line;
	HEX_TEXT_CLASS(klass)->render_line = hex_text_ascii_render_line;
	HEX_TEXT_CLASS(klass)->pressed = hex_text_ascii_pressed;
	HEX_TEXT_CLASS(klass)->dragged = hex_text_ascii_dragged;

	HEX_TEXT_EDITABLE_CLASS(klass)->move_cursor = hex_text_ascii_move_cursor;

	/* Properties */

	/**
	 * HexTextAscii:display-control-characters:
	 *
	 * Whether ASCII control characters (ASCII characters 0x0 through
	 * 0x1F) will be rendered as unicode symbols.
	 */
	properties[PROP_DISPLAY_CONTROL_CHARACTERS] = g_param_spec_boolean ("display-control-characters", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE);

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
hex_text_ascii_init (HexTextAscii *self)
{
	gtk_widget_set_focusable (GTK_WIDGET(self), TRUE);

	hex_text_set_cursor_visible (HEX_TEXT(self), TRUE);

	{
		GtkEventController *controller = gtk_event_controller_key_new ();

		g_signal_connect (controller, "key-pressed", G_CALLBACK(key_press_cb), self);
		gtk_widget_add_controller (GTK_WIDGET(self), controller);
	}
}

void
hex_text_ascii_set_display_control_characters (HexTextAscii *self, gboolean display_control_characters)
{
	g_return_if_fail (HEX_IS_TEXT_ASCII (self));

	self->display_control_characters = display_control_characters;

	gtk_widget_queue_draw (GTK_WIDGET (self));

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_DISPLAY_CONTROL_CHARACTERS]);
}

gboolean
hex_text_ascii_get_display_control_characters (HexTextAscii *self)
{
	g_return_val_if_fail (HEX_IS_TEXT_ASCII (self), FALSE);

	return self->display_control_characters;
}
