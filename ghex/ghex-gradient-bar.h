// vim: linebreak breakindent breakindentopt=shift\:4

#include "hex-gradient.h"

#define GHEX_TYPE_GRADIENT_BAR (ghex_gradient_bar_get_type ())
G_DECLARE_FINAL_TYPE (GHexGradientBar, ghex_gradient_bar, GHEX, GRADIENT_BAR, GtkWidget)

GtkWidget * ghex_gradient_bar_new (void);
void ghex_gradient_bar_set_gradient (GHexGradientBar *self, HexGradient *gradient);
HexGradient *ghex_gradient_bar_get_gradient (GHexGradientBar *self);
HexColorStop * ghex_gradient_bar_get_active_stop (GHexGradientBar *self);
