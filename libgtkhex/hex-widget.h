// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include <gtk/gtk.h>

#include "hex-common.h"
#include "hex-view.h"
#include "hex-text-hex.h"
#include "hex-text-ascii.h"

G_BEGIN_DECLS

#define HEX_TYPE_WIDGET (hex_widget_get_type ())
G_DECLARE_FINAL_TYPE (HexWidget, hex_widget, HEX, WIDGET, HexView)

GtkWidget *hex_widget_new (void);
void hex_widget_cut_to_clipboard (HexWidget *self);
void hex_widget_copy_to_clipboard (HexWidget *self);
void hex_widget_paste_from_clipboard (HexWidget *self);
HexTextHex *hex_widget_get_hex_display (HexWidget *self);
HexTextAscii *hex_widget_get_ascii_display (HexWidget *self);
void hex_widget_set_show_offsets (HexWidget *self, gboolean show);
gboolean hex_widget_get_show_offsets (HexWidget *self);
void hex_widget_set_show_hex (HexWidget *self, gboolean show);
gboolean hex_widget_get_show_hex (HexWidget *self);
void hex_widget_set_show_ascii (HexWidget *self, gboolean show);
gboolean hex_widget_get_show_ascii (HexWidget *self);
int hex_widget_get_offset_cpl (HexWidget *self);
int hex_widget_get_hex_cpl (HexWidget *self);
int hex_widget_get_auto_geometry_cpl (HexWidget *self);
void hex_widget_set_group_type (HexWidget *self, HexGroupType group_type);
HexGroupType hex_widget_get_group_type (HexWidget *self);

G_END_DECLS
