/* vim: colorcolumn=80 tw=4 sw=4
 */

#include <gtkhex.h>
#include "ghex-application-window.h"

static void
activate (GtkApplication *app,
				gpointer user_data)
{
	GtkWidget *window;
	GtkWidget *hex;
	HexDocument *doc;

	window = ghex_application_window_new ();
	doc = hex_document_new_from_file ("main.c");
	hex = gtk_hex_new (doc);
	gtk_hex_show_offsets (GTK_HEX(hex), TRUE);

	ghex_application_window_add_hex (GHEX_APPLICATION_WINDOW(window), GTK_HEX(hex));


#if 0
	GtkBuilder *builder;
	GtkWidget *window;
	GtkWidget *box;
	GtkBox *conversions_box;
	GtkWidget *conversions_pane;
	GtkWidget *hex;
	GtkHeaderBar *headerbar;
	GtkWidget *label;
	GtkStatusbar *statusbar;
	guint id;
	GtkAdjustment *adj;
	GtkScrollbar *scrollbar;
	GtkStyleContext *context;
	GtkCssProvider *provider;
	HexDocument *doc;

	(void)user_data;	/* unused for now. */

	builder = gtk_builder_new_from_resource ("/org/gnome/ghex/application.ui");
	doc = hex_document_new_from_file ("main.c");
	hex = gtk_hex_new (doc);
	gtk_hex_show_offsets (GTK_HEX(hex), TRUE);

	box = GTK_WIDGET(gtk_builder_get_object (builder, "child_box"));
	conversions_box = GTK_BOX(gtk_builder_get_object (builder, "conversions_box"));
	gtk_widget_set_name (GTK_WIDGET(conversions_box), "conversions_box");
	window = GTK_WIDGET(gtk_builder_get_object (builder, "main_window"));
	scrollbar = GTK_SCROLLBAR(gtk_builder_get_object (builder, "scrollbar"));
	headerbar = GTK_HEADER_BAR (gtk_builder_get_object (builder, "headerbar"));
	statusbar = GTK_STATUSBAR (gtk_builder_get_object (builder, "statusbar"));

	conversions_pane = conversions_pane_new ();

	label = gtk_label_new (NULL);

	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_path (provider, "style.css");
	context = gtk_widget_get_style_context (GTK_WIDGET(conversions_box));
	gtk_style_context_add_provider (context,
					GTK_STYLE_PROVIDER(provider),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	context = gtk_widget_get_style_context (label);
	gtk_style_context_add_class (context, "title");
	gtk_label_set_markup (GTK_LABEL(label), "main.c");

	gtk_header_bar_set_title_widget (headerbar, label);
	
	id = gtk_statusbar_get_context_id (statusbar, "offset");
	gtk_statusbar_push (statusbar, id, "Offset: 0x0");

	adj = gtk_hex_get_adjustment (GTK_HEX(hex));

	gtk_scrollbar_set_adjustment (scrollbar, adj);

	gtk_box_append (conversions_box, conversions_pane);
	gtk_box_prepend (GTK_BOX(box), hex);
#endif


	gtk_window_set_application (GTK_WINDOW(window), app);

	gtk_window_present (GTK_WINDOW(window));
//	gtk_widget_show (window);

#if 0
	g_object_unref (builder);
#endif
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
