// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include <gtk/gtk.h>

#include "hex-document.h"
#include "hex-selection.h"
#include "hex-mark.h"
#include "hex-auto-highlight.h"

G_BEGIN_DECLS

#define HEX_TYPE_VIEW (hex_view_get_type ())
G_DECLARE_DERIVABLE_TYPE (HexView, hex_view, HEX, VIEW, GtkWidget)

struct _HexViewClass
{
	GtkWidgetClass parent_class;
};

void hex_view_set_document (HexView *self, HexDocument *document);
HexDocument * hex_view_get_document (HexView *self);
void hex_view_set_cpl (HexView *self, int cpl);
int hex_view_get_cpl (HexView *self);
void hex_view_set_auto_geometry (HexView *self, gboolean auto_geometry);
gboolean hex_view_get_auto_geometry (HexView *self);
void hex_view_set_vadjustment (HexView *self, GtkAdjustment *vadj);
GtkAdjustment * hex_view_get_vadjustment (HexView *self);
const char * hex_view_get_font (HexView *self);
void hex_view_set_font (HexView *self, const char *font);
void hex_view_set_blink_cursor (HexView *self, gboolean blink);
gboolean hex_view_get_blink_cursor (HexView *self);
void hex_view_set_selection (HexView *self, HexSelection *selection);
HexSelection * hex_view_get_selection (HexView *self);
void hex_view_set_marks (HexView *self, GListModel *marks);
GListModel * hex_view_get_marks (HexView *self);
void hex_view_set_auto_highlights (HexView *self, GListModel *auto_highlights);
GListModel * hex_view_get_auto_highlights (HexView *self);
int hex_view_get_n_vis_lines (HexView *self);
void hex_view_set_insert_mode (HexView *self, gboolean insert_mode);
gboolean hex_view_get_insert_mode (HexView *self);

void hex_view_insert_auto_highlight (HexView *self, HexAutoHighlight *auto_highlight);
gboolean hex_view_remove_auto_highlight (HexView *self, HexAutoHighlight *auto_highlight);
void hex_view_clear_auto_highlights (HexView *self);

HexMark *hex_view_add_mark (HexView *self, gint64 start, gint64 end, GdkRGBA *color);
void hex_view_delete_mark (HexView *self, HexMark *mark);
void hex_view_goto_mark (HexView *self, HexMark *mark);

G_END_DECLS
