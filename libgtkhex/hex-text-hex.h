// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-text-editable.h"

// FIXME - enum shouldn't be in here
#include "gtkhex-layout-manager.h"

G_BEGIN_DECLS

#define HEX_TYPE_TEXT_HEX (hex_text_hex_get_type ())
G_DECLARE_FINAL_TYPE (HexTextHex, hex_text_hex, HEX, TEXT_HEX, HexTextEditable)

G_END_DECLS
