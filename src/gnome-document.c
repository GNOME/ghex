#include <gnome.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgnome/libgnome.h>

#include <libgnomeui/libgnomeui.h>

#include <gnome-document.h>

#include <gtkhex.h>

static void gnome_document_class_init       (GnomeDocumentClass *klass);
static void gnome_document_init             (GnomeDocument *);
static void gnome_document_destroy          (GtkObject *);

enum {
  CREATE_VIEW,
  CREATE_MENUS,
  DOCUMENT_CHANGED,
  LAST_SIGNAL
};

typedef GtkWidget *(*GnomeDocumentSignal1) (GtkObject *, gpointer);
typedef void (*GnomeDocumentSignal2) (GtkObject *, gpointer, gpointer);
typedef GtkWidget *(*GnomeDocumentSignal3) (GtkObject *, gpointer, gpointer);

static GtkObjectClass *parent_class = NULL;

static gint document_signals[LAST_SIGNAL];

static void gnome_document_marshal_1 (GtkObject	    *object,
				      GtkSignalFunc   func,
				      gpointer	    func_data,
				      GtkArg	    *args) {
  GnomeDocumentSignal1 rfunc;
  gpointer *return_val;
  
  rfunc = (GnomeDocumentSignal1) func;
  return_val = GTK_RETLOC_POINTER (args[0]);
  
  *return_val = (* rfunc)(object, func_data);
}

static void gnome_document_marshal_2 (GtkObject	    *object,
				      GtkSignalFunc   func,
				      gpointer	    func_data,
				      GtkArg	    *args) {
  GnomeDocumentSignal2 rfunc;
  
  rfunc = (GnomeDocumentSignal2) func;
  
  (* rfunc)(object, GTK_VALUE_POINTER(args[0]), func_data);
}

static void gnome_document_marshal_3 (GtkObject	    *object,
				      GtkSignalFunc   func,
				      gpointer	    func_data,
				      GtkArg	    *args) {
  GnomeDocumentSignal3 rfunc;
  gpointer *return_val;
  
  rfunc = (GnomeDocumentSignal3) func;
  return_val = GTK_RETLOC_POINTER (args[1]);
  
  *return_val = (* rfunc)(object, GTK_VALUE_POINTER(args[0]), func_data);
}

guint gnome_document_get_type () {
  static guint document_type = 0;

  if (!document_type) {
    GtkTypeInfo document_info = {
      "GnomeDocument",
      sizeof (GnomeDocument),
      sizeof (GnomeDocumentClass),
      (GtkClassInitFunc) gnome_document_class_init,
      (GtkObjectInitFunc) gnome_document_init,
      (GtkArgSetFunc) NULL,
      (GtkArgGetFunc) NULL,
    };
    
    document_type = gtk_type_unique (gtk_object_get_type (), &document_info);
  }
  
  return document_type;
}

static void gnome_document_class_init (GnomeDocumentClass *class) {
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  object_class->destroy = gnome_document_destroy;

  document_signals[CREATE_VIEW] = gtk_signal_new ("create_view",
						  GTK_RUN_FIRST,
						  object_class->type,
						  GTK_SIGNAL_OFFSET (GnomeDocumentClass, create_view),
						  gnome_document_marshal_1,
						  GTK_TYPE_POINTER, 0);
  document_signals[CREATE_MENUS] = gtk_signal_new ("create_menus",
						  GTK_RUN_FIRST,
						  object_class->type,
						  GTK_SIGNAL_OFFSET (GnomeDocumentClass, create_menus),
						  gnome_document_marshal_3,
						  GTK_TYPE_POINTER, 1, GTK_TYPE_POINTER);
  document_signals[DOCUMENT_CHANGED] = gtk_signal_new ("document_changed",
						       GTK_RUN_FIRST,
						       object_class->type,
						       GTK_SIGNAL_OFFSET (GnomeDocumentClass, document_changed),
						       gnome_document_marshal_2,
						       GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

  gtk_object_class_add_signals (object_class, document_signals, LAST_SIGNAL);

  class->create_view = NULL;
  class->create_menus = NULL;
  class->document_changed = NULL;

  parent_class = gtk_type_class (gtk_object_get_type ());
}

static void gnome_document_init (GnomeDocument *document) {
  document->title = NULL;
  document->views = NULL;
  document->changed = FALSE;
}

GtkWidget *gnome_document_add_view(GnomeDocument *doc) {
  GtkWidget *view;

  gtk_signal_emit (GTK_OBJECT (doc), document_signals[CREATE_VIEW], &view);

  doc->views = g_list_append(doc->views, view);

  gtk_object_set_data(GTK_OBJECT(view), "document_data", doc);

  gtk_widget_ref(view);

  return view;
}

GnomeDocument *gnome_document_new () {
  GnomeDocument *document;

  document = gtk_type_new (gnome_document_get_type ());

  return document;
}

static void gnome_document_destroy(GtkObject *obj) {
  GnomeDocument *doc;

#ifdef DEBUG
  printf("GnomeDocument: destroying!\n");
#endif

  doc = GNOME_DOCUMENT(obj);

  while(doc->views)
    gnome_document_remove_view(doc, GTK_WIDGET(doc->views->data));

  if(doc->title)
    free(doc->title);

  if(GTK_OBJECT_CLASS(parent_class)->destroy)
    (* GTK_OBJECT_CLASS(parent_class)->destroy)(GTK_OBJECT(doc));
}

void gnome_document_remove_view(GnomeDocument *doc, GtkWidget *view) {
  doc->views = g_list_remove(doc->views, view);

  gtk_widget_destroy(view);
}

void gnome_document_set_title(GnomeDocument *doc, gchar *title) {
  gchar *old_title = doc->title;

  doc->title = (gchar *)strdup(title);

  /* TODO: send a document_retitled signal to MDI */

  if(old_title)
    free(old_title);
}

void gnome_document_changed(GnomeDocument *doc, gpointer change_data) {
  gtk_signal_emit(GTK_OBJECT(doc), document_signals[DOCUMENT_CHANGED], change_data);
}

gboolean gnome_document_has_changed(GnomeDocument *doc) {
  return doc->changed;
}







