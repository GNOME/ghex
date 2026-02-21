// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include <gtk/gtk.h>

#include "hex-view.h"

G_BEGIN_DECLS

#define HEX_TYPE_WIDGET (hex_widget_get_type ())
G_DECLARE_FINAL_TYPE (HexWidget, hex_widget, HEX, WIDGET, HexView)

GtkWidget *hex_widget_new (void);
int hex_widget_get_char_width (HexWidget *self);
int hex_widget_get_num_lines (HexWidget *self);
void hex_widget_cut_to_clipboard (HexWidget *self);
void hex_widget_copy_to_clipboard (HexWidget *self);
void hex_widget_paste_from_clipboard (HexWidget *self);
void hex_widget_set_cpl (HexWidget *self, int cpl);
int hex_widget_get_cpl (HexWidget *self);

G_END_DECLS
