// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-view.h"
#include "hex-color-code.h"

G_BEGIN_DECLS

#define HEX_TYPE_TEXT (hex_text_get_type ())
G_DECLARE_DERIVABLE_TYPE (HexText, hex_text, HEX, TEXT, HexView)

typedef struct
{
	int line_height;
	int top_disp_line;
	int fine_translate_value;
} HexTextRenderData;

struct _HexTextClass
{
	GtkWidgetClass parent_class;

	/* transfer full */
	char *	(*format_line)	(HexText *self, int line_num, gint64 line_start_offset, size_t line_len);

	void	(*render_line)	(HexText *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout);

	void	(*pressed)		(HexText *self, GdkModifierType state, PangoLayout *layout, int click_line, int rel_x, int rel_y);

	void	(*released)		(HexText *self, PangoLayout *layout, int click_line, int rel_x, int rel_y);

	void	(*dragged)		(HexText *self, PangoLayout *layout, int click_line, int rel_x, int rel_y, GtkScrollType scroll);

	void	(*right_click)	(HexText *self, PangoLayout *layout, int click_line, int rel_x, int rel_y, int abs_x, int abs_y);

	gpointer padding[12];
};

HexTextRenderData *hex_text_get_render_data (HexText *self);
void hex_text_set_cursor_visible (HexText *self, gboolean visible);
gboolean hex_text_get_cursor_visible (HexText *self);
gboolean hex_text_offset_is_visible (HexText *self, gint64 offset, int *line_num);
gboolean hex_text_highlight_is_visible (HexText *self, HexHighlight *highlight, int disp_line_num, int *disp_line_offset_start, int *disp_line_offset_end);
gboolean hex_text_get_use_color_code (HexText *self);
void hex_text_set_use_color_code (HexText *self, gboolean use_color_code);
HexColorCode * hex_text_get_color_code (HexText *self);
void hex_text_set_color_code (HexText *self, HexColorCode *color_code);

G_END_DECLS
