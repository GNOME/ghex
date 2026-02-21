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

	gpointer padding[12];
};

void hex_text_editable_move_cursor (HexTextEditable *self, GtkMovementStep step, int count, gboolean extend_selection);

G_END_DECLS
