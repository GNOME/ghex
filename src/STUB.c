/* vim: colorcolumn=80 tw=4 sw=4
 */

#include <gtkhex.h>
#include "ghex-application-window.h"
#include "configuration.h"

static void
activate (GtkApplication *app,
	gpointer user_data)
{
	GtkWidget *window;

	(void)user_data;	/* unused */

	window = ghex_application_window_new (app);

	gtk_window_set_application (GTK_WINDOW(window), app);
	gtk_window_present (GTK_WINDOW(window));
}

int
main (int argc, char *argv[])
{
	GtkApplication *app;
	int status;

	ghex_init_configuration ();

	app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

	status = g_application_run (G_APPLICATION(app), argc, argv);

	g_object_unref(app);

	return status;
}
