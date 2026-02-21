// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-text-common"

#include "hex-text-common.h"

#define GRAPHENE_RECT_FROM_RECT(_r) (GRAPHENE_RECT_INIT ((_r)->x, (_r)->y, (_r)->width, (_r)->height))

void
hex_text_common_render_cursor (HexText *ht, GtkSnapshot *snapshot, PangoLayout *layout, int *range, gboolean insert_mode, gboolean at_file_end, gboolean at_new_row, gboolean lower_nibble)
{
	cairo_region_t *region = gdk_pango_layout_get_clip_region (layout, 0, 0, range, 1);
	cairo_rectangle_int_t clip_rect;
	graphene_rect_t rect;
	GdkRGBA color;
	GdkRGBA opposite_color;

	if (gtk_widget_has_focus (GTK_WIDGET (ht)) && !hex_text_get_cursor_visible (ht))
		return;

	gtk_widget_get_color (GTK_WIDGET(ht), &color);

	opposite_color = color;
	opposite_color.red = 1.0 - color.red;
	opposite_color.green = 1.0 - color.green;
	opposite_color.blue = 1.0 - color.blue;

	cairo_region_get_rectangle (region, 0, &clip_rect);
	rect = GRAPHENE_RECT_FROM_RECT (&clip_rect);

	// TEST
	if (insert_mode && at_file_end)
	{
		if (at_new_row)
		{
			rect.origin.y += rect.size.height;
		}
		else
		{
			rect.origin.x += rect.size.width;

			if (lower_nibble)
				rect.origin.x += rect.size.width;
		}
	}
#if 0
	if (insert_mode)
	{
		if (at_new_row && !at_file_end)
			rect.origin.y += rect.size.height;
		else if (at_file_end)
			rect.origin.x += rect.size.width;
	}
#endif

	gtk_snapshot_push_clip (snapshot, &rect);

	if (gtk_widget_has_focus (GTK_WIDGET(ht)))
	{
		gtk_snapshot_append_color (snapshot, &color, &rect);
		gtk_snapshot_append_layout (snapshot, layout, &opposite_color);
	}
	else
	{
		GskRoundedRect outline;
		GdkRGBA outline_color = color;

		outline_color.red *= 2;
		outline_color.green *= 2;
		outline_color.blue *= 2;

		gsk_rounded_rect_init_from_rect (&outline, &rect, 0);
		gtk_snapshot_append_border (snapshot, &outline, 
                              (float[4]) { 1, 1, 1, 1 },
                              (GdkRGBA [4]) { outline_color,outline_color,outline_color,outline_color });
	}

	gtk_snapshot_pop (snapshot);
}

void
hex_text_common_render_highlight (GtkWidget *widget, GtkSnapshot *snapshot, PangoLayout *layout, int *range, const GdkRGBA *color)
{
	cairo_region_t *region = gdk_pango_layout_get_clip_region (layout, 0, 0, range, 1);
	cairo_rectangle_int_t clip_rect;
	graphene_rect_t rect;
	GdkRGBA standard_color = {0};

	if (! hex_text_common_get_highlight_color (widget, &standard_color))
	{
		g_warning ("%s: Couldn't get standard highlight color. This should not happen.", __func__);
	}

	cairo_region_get_rectangle (region, 0, &clip_rect);
	rect = GRAPHENE_RECT_FROM_RECT (&clip_rect);

	gtk_snapshot_push_clip (snapshot, &rect);
	gtk_snapshot_append_color (snapshot, color ? color : &standard_color, &rect);
	gtk_snapshot_pop (snapshot);
}

int
hex_text_common_char_index_to_utf8_byte_index (const char *str, int index)
{
	return g_utf8_offset_to_pointer (str, index) - str;
}

/* TODO: use non-deprecated API when an alternative actually becomes available.
 * See https://gitlab.gnome.org/GNOME/gtk/-/issues/5262
 */
gboolean
hex_text_common_get_highlight_color (GtkWidget *widget, GdkRGBA *color)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget) && color, FALSE);

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	return gtk_style_context_lookup_color (gtk_widget_get_style_context (widget), "theme_selected_bg_color", color);
	G_GNUC_END_IGNORE_DEPRECATIONS
}

gint64
hex_text_common_get_cursor_display_line_ends (HexText *self, int count)
{
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	const int cpl = hex_view_get_cpl (HEX_VIEW(self));
	GtkAdjustment *adj = hex_view_get_vadjustment (HEX_VIEW(self));
	HexTextRenderData *render_data = hex_text_get_render_data (self);
	int rel_line_num;
	guint abs_line_num;
	gint64 new_cursor_pos;

	if (! hex_text_offset_is_visible (self, cursor_pos, &rel_line_num))
		return 0;

	abs_line_num = render_data->top_disp_line + rel_line_num;
	new_cursor_pos = CLAMP (cursor_pos + count * cpl, abs_line_num * cpl, abs_line_num * cpl + (cpl-1));

	return new_cursor_pos;
}

void
hex_text_common_add_move_binding (GtkWidgetClass *widget_class, guint keyval, guint modmask, GtkMovementStep step, int count)
{
	g_assert ((modmask & GDK_SHIFT_MASK) == 0);

	gtk_widget_class_add_binding_action (widget_class, keyval,
			modmask,
			"editable.move-cursor", "(iib)",
			step, count, FALSE);

	/* Selection-extending version */
	gtk_widget_class_add_binding_action (widget_class, keyval,
			modmask | GDK_SHIFT_MASK,
			"editable.move-cursor", "(iib)",
			step, count, TRUE);
}

void
hex_text_common_finish_move_cursor (HexTextEditable *self, gint64 new_cursor_pos, gboolean extend_selection)
{
	HexDocument *doc = hex_view_get_document (HEX_VIEW(self));
	HexBuffer *buf = hex_document_get_buffer (doc);
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gboolean insert_mode = hex_view_get_insert_mode (HEX_VIEW(self));
	const gint64 file_end_pos = hex_text_common_get_file_end_cursor_pos (self);

	new_cursor_pos = CLAMP (new_cursor_pos, 0, file_end_pos);

	if (extend_selection)
		hex_selection_set_cursor_pos (selection, new_cursor_pos);
	else
		hex_selection_collapse (selection, new_cursor_pos);
}

gint64
hex_text_common_get_file_end_cursor_pos (HexTextEditable *self)
{
	HexDocument *doc = hex_view_get_document (HEX_VIEW(self));
	HexBuffer *buf = hex_document_get_buffer (doc);
	const gboolean insert_mode = hex_view_get_insert_mode (HEX_VIEW(self));
	const gint64 file_end_pos = insert_mode ? hex_buffer_get_payload_size (buf) : hex_buffer_get_payload_size (buf) - 1;

	return file_end_pos;
}

gboolean
hex_text_common_get_is_cursor_at_file_end (HexTextEditable *self)
{
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	const gint64 file_end_pos = hex_text_common_get_file_end_cursor_pos (self);

	if (cursor_pos == 0)
		return FALSE;

	return cursor_pos >= file_end_pos;
}

gboolean
hex_text_common_get_is_cursor_at_new_row (HexTextEditable *self)
{
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	const int cpl = hex_view_get_cpl (HEX_VIEW(self));

	if (cpl == 0)
		return FALSE;
	if (cursor_pos == 0)
		return FALSE;

	return cursor_pos % cpl == 0;
}
