/* vim: colorcolumn=80 tw=4 ts=4
 */

#ifndef GHEX_APPLICATION_WINDOW_H
#define GHEX_APPLICATION_WINDOW_H

#include <gtk/gtk.h>

#define GHEX_TYPE_APPLICATION_WINDOW (ghex_application_window_get_type ())
G_DECLARE_FINAL_TYPE (GHexApplicationWindow, ghex_application_window,
				GHEX, APPLICATION_WINDOW,
				GtkApplicationWindow)

GtkWidget *	ghex_application_window_new (GtkApplication *app);
void		ghex_application_window_add_hex (GHexApplicationWindow *self,
				GtkHex *gh);
void		ghex_application_window_set_hex (GHexApplicationWindow *self,
				GtkHex *gh);
void		ghex_application_window_activate_tab (GHexApplicationWindow *self,
				GtkHex *gh);

#endif