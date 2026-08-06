// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-text.h"

G_BEGIN_DECLS

#define HEX_TYPE_TEXT_OFFSETS (hex_text_offsets_get_type ())
G_DECLARE_FINAL_TYPE (HexTextOffsets, hex_text_offsets, HEX, TEXT_OFFSETS, HexText)

int hex_text_offsets_get_offset_cpl (HexTextOffsets *self);
void hex_text_offsets_set_offset_cpl (HexTextOffsets *self, int offset_cpl);

G_END_DECLS
