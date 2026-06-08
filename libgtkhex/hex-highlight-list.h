// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-highlight.h"

G_BEGIN_DECLS

#define HEX_TYPE_HIGHLIGHT_LIST hex_highlight_list_get_type()
G_DECLARE_FINAL_TYPE (HexHighlightList, hex_highlight_list, HEX, HIGHLIGHT_LIST, GObject)

HexHighlightList * hex_highlight_list_new (void);
HexHighlight ** hex_highlight_list_get_highlights_for_range (HexHighlightList *self, gint64 start, gint64 end, guint *n_found);
void hex_highlight_list_append (HexHighlightList *self, gpointer item);
void hex_highlight_list_remove (HexHighlightList *self, HexHighlight *highlight);
void hex_highlight_list_remove_all (HexHighlightList *self);

G_END_DECLS
