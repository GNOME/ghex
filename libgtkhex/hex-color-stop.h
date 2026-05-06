#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define HEX_TYPE_COLOR_STOP (hex_color_stop_get_type ())
G_DECLARE_FINAL_TYPE (HexColorStop, hex_color_stop, HEX, COLOR_STOP, GObject)

HexColorStop * hex_color_stop_new (void);
HexColorStop * hex_color_stop_new_from_gsk_color_stop (const GskColorStop *color_stop);
void hex_color_stop_set_offset (HexColorStop *self, float offset);
float hex_color_stop_get_offset (HexColorStop *self);
void hex_color_stop_set_color (HexColorStop *self, const GdkRGBA *color);
void hex_color_stop_get_color (HexColorStop *self, GdkRGBA *color);
const GskColorStop * hex_color_stop_as_gsk_color_stop (HexColorStop *self);

G_END_DECLS
