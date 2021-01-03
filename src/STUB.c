#include <gtkhex.h>

static void
activate (GtkApplication *app,
          gpointer        user_data)
{
  GtkWidget *window;
  HexDocument *doc;
  GtkWidget *hex;
  GtkWidget *box;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Window");
  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);

  doc = hex_document_new ();
  hex = gtk_hex_new (doc);

  gtk_box_append (GTK_BOX (box), hex);

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
