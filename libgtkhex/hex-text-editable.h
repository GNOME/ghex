// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-text.h"

G_BEGIN_DECLS

#define HEX_TYPE_TEXT_EDITABLE (hex_text_editable_get_type ())
G_DECLARE_DERIVABLE_TYPE (HexTextEditable, hex_text_editable, HEX, TEXT_EDITABLE, HexText)

struct _HexTextEditableClass
{
	HexTextClass parent_class;

	void	(*move_cursor) (HexTextEditable *self, GtkMovementStep step, int count, gboolean extend_selection);

	void	(*render_highlights_for_line) (HexTextEditable *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout);

	void	(*render_cursor) (HexTextEditable *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout);

	gpointer padding[12];
};

void hex_text_editable_move_cursor (HexTextEditable *self, GtkMovementStep step, int count, gboolean extend_selection);
void hex_text_editable_render_highlights_for_line (HexTextEditable *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout);
void hex_text_editable_render_cursor (HexTextEditable *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout);

G_END_DECLS
