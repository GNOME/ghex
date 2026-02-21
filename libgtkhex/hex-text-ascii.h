// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-text-editable.h"

G_BEGIN_DECLS

#define HEX_TYPE_TEXT_ASCII (hex_text_ascii_get_type ())
G_DECLARE_FINAL_TYPE (HexTextAscii, hex_text_ascii, HEX, TEXT_ASCII, HexTextEditable)

G_END_DECLS
