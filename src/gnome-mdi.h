/*
 * gnome-mdi.h - definition of a Gnome MDI container
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */

#ifndef __GNOME_MDI_H__
#define __GNOME_MDI_H__

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgnome/libgnome.h>

#include <libgnomeui/libgnomeui.h>

#include <gnome-document.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GNOME_MDI(obj)          GTK_CHECK_CAST (obj, gnome_mdi_get_type (), GnomeMDI)
#define GNOME_MDI_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gnome_mdi_get_type (), GnomeMDIClass)
#define GNOME_IS_MDI(obj)       GTK_CHECK_TYPE (obj, gnome_mdi_get_type ())

typedef struct _GnomeMDI       GnomeMDI;
typedef struct _GnomeMDIClass  GnomeMDIClass;

#define GNOME_MDI_MS       (1L << 0)      /* for MS-like MDI */
#define GNOME_MDI_NOTEBOOK (1L << 1)      /* notebook */
#define GNOME_MDI_MODAL    (1L << 2)      /* "modal" app */
#define GNOME_MDI_TOPLEVEL (1L << 3)      /* many toplevel windows */

struct _GnomeMDI {
  GtkObject object;

  gint32 flags;

  gchar *appname, *title;

  /* probably only one of these would do, but... */
  GnomeDocument *active_doc;
  GtkWidget *active_view;
  GnomeApp *active_window;

  GList *windows;   /* toplevel windows - GnomeApp widgets */
  GList *documents; /* documents - GnomeDocument objects*/

  GnomeRootWin *root_window; /* this will be needed for DND */
};

struct _GnomeMDIClass
{
  GtkObjectClass parent_class;

  GList * (*create_menus)(GnomeMDI *); 
  gint    (*add_document)(GnomeMDI *, GnomeDocument *); 
  gint    (*remove_document)(GnomeMDI *, GnomeDocument *); 
  gint    (*add_view)(GnomeMDI *, GtkWidget *); 
  gint    (*remove_view)(GnomeMDI *, GtkWidget *); 
  void    (*document_changed)(GnomeMDI *, GnomeDocument *, gpointer);
  void    (*document_retitled)(GnomeMDI *, GnomeDocument *);
  void    (*app_created)(GnomeMDI *, GnomeApp *);
};

GtkObject *gnome_mdi_new(gchar *, gchar *);
void gnome_mdi_set_mode(GnomeMDI *, gint);
void gnome_mdi_add_document(GnomeMDI *, GnomeDocument *);
void gnome_mdi_remove_document(GnomeMDI *, GnomeDocument *);
void gnome_mdi_add_view(GnomeMDI *, GnomeDocument *);
void gnome_mdi_remove_view(GnomeMDI *, GtkWidget *);
void gnome_mdi_menu_place(GnomeMDI *, gint);
GnomeDocument *gnome_mdi_active_document(GnomeMDI*);
gboolean gnome_mdi_remove_all_documents(GnomeMDI *);

#endif /* __GNOME_MDI_H__ */














