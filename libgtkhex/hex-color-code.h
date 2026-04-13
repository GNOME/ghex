// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Type declaration */

#define HEX_TYPE_COLOR_CODE hex_color_code_get_type()
G_DECLARE_FINAL_TYPE (HexColorCode, hex_color_code, HEX, COLOR_CODE, GObject)

/* Method declarations */

HexColorCode *	hex_color_code_new (void);

void hex_color_code_get_color_at (HexColorCode *self, guint8 pos, GdkRGBA *color);
void hex_color_code_set_start_gradient_color (HexColorCode *self, const GdkRGBA *start_gradient_color);
void hex_color_code_get_start_gradient_color (HexColorCode *self, GdkRGBA *start_gradient_color);
void hex_color_code_set_end_gradient_color (HexColorCode *self, const GdkRGBA *end_gradient_color);
void hex_color_code_get_end_gradient_color (HexColorCode *self, GdkRGBA *end_gradient_color);

G_END_DECLS
