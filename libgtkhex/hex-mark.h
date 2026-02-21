// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-highlight.h"

G_BEGIN_DECLS

#define HEX_TYPE_MARK (hex_mark_get_type ())
G_DECLARE_FINAL_TYPE (HexMark, hex_mark, HEX, MARK, GObject)

void hex_mark_set_custom_color (HexMark *mark,
		GdkRGBA *color);
void hex_mark_get_custom_color (HexMark *mark, GdkRGBA *color);
gboolean hex_mark_get_have_custom_color (HexMark *mark);
gint64 hex_mark_get_start_offset (HexMark *mark);
gint64 hex_mark_get_end_offset (HexMark *mark);
HexHighlight * hex_mark_get_highlight (HexMark *self);

HexMark *hex_mark_new (void);

G_END_DECLS
