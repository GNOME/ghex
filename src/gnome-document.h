/*
 * gnome-document.h - definition of a Gnome Document object
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */

#ifndef __GNOME_DOCUMENT_H__
#define __GNOME_DOCUMENT_H__

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgnome/libgnome.h>

#include <libgnomeui/libgnomeui.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GNOME_DOCUMENT(obj)          GTK_CHECK_CAST (obj, gnome_document_get_type (), GnomeDocument)
#define GNOME_DOCUMENT_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gnome_document_get_type (), GnomeDocumentClass)
#define GNOME_IS_DOCUMENT(obj)       GTK_CHECK_TYPE (obj, gnome_document_get_type ())

#define VIEW_GET_DOCUMENT(view) GNOME_DOCUMENT(gtk_object_get_data(GTK_OBJECT(view), "document_data"))
#define VIEW_GET_WINDOW(v)      GNOME_APP(gtk_widget_get_toplevel(GTK_WIDGET(v)))
#define VIEW_GET_TITLE(v)       (VIEW_GET_DOCUMENT(v)->title)

#define DOC_LAST_VIEW(doc)      (g_list_length(doc->views) == 1)

typedef struct _GnomeDocument       GnomeDocument;
typedef struct _GnomeDocumentClass  GnomeDocumentClass;

struct _GnomeDocument
{
  GtkObject object;

  gchar *title;

  gpointer owner;

  GList *views;

  gboolean changed;
};

struct _GnomeDocumentClass
{
  GtkObjectClass parent_class;

  GtkWidget * (*create_view)(GnomeDocument *); 
  GList     * (*create_menus)(GnomeDocument *, GtkWidget *); 
  void        (*document_changed)(GnomeDocument *, gpointer);
};

GnomeDocument *gnome_document_new();
GList *gnome_document_get_views(GnomeDocument *);
GtkWidget *gnome_document_add_view(GnomeDocument *);
void gnome_document_remove_view(GnomeDocument *, GtkWidget *view);
void gnome_document_changed(GnomeDocument *, gpointer);
void gnome_document_set_title(GnomeDocument *, gchar *);

#endif /* __GNOME_DOCUMENT_H__ */














