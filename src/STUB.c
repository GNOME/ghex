/* vim: colorcolumn=80 tw=4 ts=4
 */

#include <gtkhex.h>

static void
activate (GtkApplication *app,
				gpointer user_data)
{
	GtkWidget *window;
	GtkWidget *box;
	GtkBuilder *builder;
	GtkWidget *hex;
	HexDocument *doc;

	(void)user_data;	/* unused for now. */

	builder = gtk_builder_new_from_file ("application.ui");
	doc = hex_document_new_from_file ("main.c");
	hex = gtk_hex_new (doc);
	gtk_hex_show_offsets (GTK_HEX(hex), TRUE);

	box = GTK_WIDGET(gtk_builder_get_object(builder, "child_box"));
	window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));

	gtk_box_append (GTK_BOX(box), hex);

	gtk_window_set_application (GTK_WINDOW(window), app);

	gtk_widget_show (window);

	g_object_unref (builder);
}

int
main (int argc, char *argv[])
{
	GtkApplication *app;
	int status;

	app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

	status = g_application_run (G_APPLICATION(app), argc, argv);

	g_object_unref(app);

	return status;
}
