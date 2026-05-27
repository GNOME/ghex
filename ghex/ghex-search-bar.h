#pragma once

#include "ghex-pane.h"

G_BEGIN_DECLS

#define GHEX_TYPE_SEARCH_BAR (ghex_search_bar_get_type ())

G_DECLARE_FINAL_TYPE (GHexSearchBar, ghex_search_bar, GHEX, SEARCH_BAR, GHexPane)

GtkWidget *	ghex_search_bar_new (void);
HexAutoHighlight * ghex_search_bar_get_auto_highlight (GHexSearchBar *self);
void ghex_search_bar_set_replace_mode (GHexSearchBar *self, gboolean replace_mode);
gboolean ghex_search_bar_get_replace_mode (GHexSearchBar *self);

G_END_DECLS
