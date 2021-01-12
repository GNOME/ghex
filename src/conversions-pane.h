/* vim: colorcolumn=80 ts=4 sw=4
 */

#ifndef CONVERSIONS_PANE_H
#define CONVERSIONS_PANE_H

#include <gtk/gtk.h>

#define CONVERSIONS_TYPE_PANE (conversions_pane_get_type ())
G_DECLARE_FINAL_TYPE (ConversionsPane, conversions_pane, CONVERSIONS, PANE,
				GtkWidget)

GtkWidget *conversions_pane_new(void);

#endif
