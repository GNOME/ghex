// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define HEX_TYPE_HIGHLIGHT hex_highlight_get_type()
G_DECLARE_FINAL_TYPE (HexHighlight, hex_highlight, HEX, HIGHLIGHT, GObject)

HexHighlight *	hex_highlight_new (void);
void hex_highlight_update (HexHighlight *self, gint64 start_offset, gint64 end_offset);
void hex_highlight_clear (HexHighlight *self);
void hex_highlight_set_start_offset (HexHighlight *self, gint64 start_offset);
gint64 hex_highlight_get_start_offset (HexHighlight *self);
void hex_highlight_set_end_offset (HexHighlight *self, gint64 end_offset);
gint64 hex_highlight_get_end_offset (HexHighlight *self);
gsize hex_highlight_get_n_selected (HexHighlight *self);

G_END_DECLS
