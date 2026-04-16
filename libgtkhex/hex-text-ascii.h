// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-text-editable.h"

G_BEGIN_DECLS

/**
 * HexTextAsciiEncodingType:
 * @HEX_TEXT_ASCII_ENCODING_ASCII: plain ASCII encoding
 * @HEX_TEXT_ASCII_ENCODING_UTF8: UTF-8 encoding
 *
 * Specifies the encoding type to be displayed in the ASCII pane.
 */
typedef enum
{
	HEX_TEXT_ASCII_ENCODING_ASCII,
	HEX_TEXT_ASCII_ENCODING_UTF8
} HexTextAsciiEncodingType;

#define HEX_TYPE_TEXT_ASCII (hex_text_ascii_get_type ())
G_DECLARE_FINAL_TYPE (HexTextAscii, hex_text_ascii, HEX, TEXT_ASCII, HexTextEditable)

void hex_text_ascii_set_display_control_characters (HexTextAscii *self, gboolean display_control_characters);
gboolean hex_text_ascii_get_display_control_characters (HexTextAscii *self);
void hex_text_ascii_set_encoding (HexTextAscii *self, HexTextAsciiEncodingType encoding);
HexTextAsciiEncodingType hex_text_ascii_get_encoding (HexTextAscii *self);

G_END_DECLS
