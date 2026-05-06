#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GHEX_TYPE_COLOR_GRID ghex_color_grid_get_type()
G_DECLARE_FINAL_TYPE (GHexColorGrid, ghex_color_grid, GHEX, COLOR_GRID, GtkWidget)

GHexColorGrid *	ghex_color_grid_new (void);
void ghex_color_grid_populate (GHexColorGrid *self, const GdkRGBA *colors);
GdkRGBA * ghex_color_grid_to_rgba_array (GHexColorGrid *self);

G_END_DECLS
