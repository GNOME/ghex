// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include <hex-document.h>
#include "hex-highlight.h"

G_BEGIN_DECLS

/* Type declaration */

#define HEX_TYPE_AUTO_HIGHLIGHT hex_auto_highlight_get_type()
G_DECLARE_FINAL_TYPE (HexAutoHighlight, hex_auto_highlight, HEX, AUTO_HIGHLIGHT, GObject)

HexAutoHighlight *hex_auto_highlight_new (HexDocument *document, HexSearchInfo *search_info);
HexSearchFlags hex_auto_highlight_get_search_flags (HexAutoHighlight *self);
void hex_auto_highlight_add_highlight (HexAutoHighlight *self, HexHighlight *highlight);
GListModel *hex_auto_highlight_get_highlights (HexAutoHighlight *self);
HexDocument *hex_auto_highlight_get_document (HexAutoHighlight *self);
void hex_auto_highlight_set_search_info (HexAutoHighlight *self, HexSearchInfo *search_info);
HexSearchInfo * hex_auto_highlight_get_search_info (HexAutoHighlight *self);
void hex_auto_highlight_refresh_async (HexAutoHighlight *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);
gboolean hex_auto_highlight_refresh_finish (HexAutoHighlight *self, GAsyncResult *result);
void hex_auto_highlight_refresh_sync (HexAutoHighlight *self);

G_END_DECLS
