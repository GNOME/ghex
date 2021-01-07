#include <gtkhex.h>

static void
activate (GtkApplication *app,
          gpointer        user_data)
{
  GtkWidget *window;
  HexDocument *doc;
  GtkWidget *hex;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Window");
  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);

  doc = hex_document_new_from_file ("main.c");
  hex = gtk_hex_new (doc);
//  gtk_hex_set_geometry (GTK_HEX (hex), 32, 1024);
  gtk_hex_show_offsets (GTK_HEX (hex), TRUE);
  gtk_hex_set_cursor (GTK_HEX(hex), 25);
//  gtk_hex_set_selection (GTK_HEX(hex), 20, 30);

  gtk_window_set_child (GTK_WINDOW (window), hex);

  gtk_widget_show (window);
}

int
main (int    argc,
      char **argv)
{
  GtkApplication *app;
  int status;

  app = gtk_application_new ("org.gtk.example", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
