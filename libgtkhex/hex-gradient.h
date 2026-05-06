// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

/* This will be migrated to GskGradient if/when it becomes public. */

#include "hex-color-stop.h"

G_BEGIN_DECLS

#define HEX_GRADIENT_MIN_STOPS	2
#define HEX_GRADIENT_MAX_STOPS	4

#define HEX_TYPE_GRADIENT (hex_gradient_get_type ())
G_DECLARE_FINAL_TYPE (HexGradient, hex_gradient, HEX, GRADIENT, GObject)

HexGradient *hex_gradient_new (void);
gboolean hex_gradient_add_color_stop (HexGradient *self, HexColorStop *stop);
int hex_gradient_get_n_stops (HexGradient *self);
void hex_gradient_set_n_stops (HexGradient *self, int n_stops);
GdkRGBA * hex_gradient_to_rgba_array (HexGradient *self, guint n_colors);
HexColorStop * hex_gradient_get_stop_at_index (HexGradient *self, guint index);

G_END_DECLS
