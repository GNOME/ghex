// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-highlight.h"

G_BEGIN_DECLS

#define HEX_TYPE_SELECTION (hex_selection_get_type ())
G_DECLARE_DERIVABLE_TYPE (HexSelection, hex_selection, HEX, SELECTION, GObject)

struct _HexSelectionClass
{
	GObjectClass parent_class;

	void (*set_cursor_pos) (HexSelection *self, gint64 cursor_pos);
	void (*set_selection_anchor) (HexSelection *self, gint64 selection_anchor);
};

HexHighlight * hex_selection_get_highlight (HexSelection *self);
void hex_selection_set_cursor_pos (HexSelection *self, gint64 cursor_pos);
gint64 hex_selection_get_cursor_pos (HexSelection *self);
void hex_selection_set_selection_anchor (HexSelection *self, gint64 selection_anchor);
gint64 hex_selection_get_selection_anchor (HexSelection *self);
gint64 hex_selection_get_start_offset (HexSelection *self);
gint64 hex_selection_get_end_offset (HexSelection *self);
void hex_selection_collapse (HexSelection *self, gint64 pos);

HexSelection *hex_selection_new (void);

G_END_DECLS
