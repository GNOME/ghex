// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-text-editable.h"

void hex_text_common_render_cursor (HexText *ht, GtkSnapshot *snapshot, PangoLayout *layout, int *range, gboolean insert_mode, gboolean at_file_end, gboolean at_new_row, gboolean lower_nibble);
void hex_text_common_render_highlight (GtkWidget *widget, GtkSnapshot *snapshot, PangoLayout *layout, int *range, const GdkRGBA *color);
int hex_text_common_char_index_to_utf8_byte_index (const char *str, int index);
gboolean hex_text_common_get_highlight_color (GtkWidget *widget, GdkRGBA *color);
gint64 hex_text_common_get_cursor_display_line_ends (HexText *self, int count);
void hex_text_common_add_move_binding (GtkWidgetClass *widget_class, guint keyval, guint modmask, GtkMovementStep step, int count);
void hex_text_common_finish_move_cursor (HexTextEditable *self, gint64 new_cursor_pos, gboolean extend_selection);
gint64 hex_text_common_get_file_end_cursor_pos (HexTextEditable *self);
gboolean hex_text_common_get_is_cursor_at_file_end (HexTextEditable *self);
gboolean hex_text_common_get_is_cursor_at_new_row (HexTextEditable *self);
