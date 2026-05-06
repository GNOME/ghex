// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-gradient.h"

G_BEGIN_DECLS

/* Type declaration */

#define HEX_TYPE_COLOR_CODE hex_color_code_get_type()
G_DECLARE_FINAL_TYPE (HexColorCode, hex_color_code, HEX, COLOR_CODE, GObject)

/* Method declarations */

HexColorCode *	hex_color_code_new (void);

void hex_color_code_get_color_at (HexColorCode *self, guint8 pos, GdkRGBA *color);
void hex_color_code_sync_from_gradient (HexColorCode *self, HexGradient *gradient);
void hex_color_code_sync_from_color_array (HexColorCode *self, const GdkRGBA *colors);

G_END_DECLS
