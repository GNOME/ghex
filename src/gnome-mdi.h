/*
 * gnome-mdi.h - definition of a Gnome MDI container
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */

/*
 * GnomeMDI signals:
 *
 * gint add_document(GnomeMDI *, GnomeDocument *)
 * gint add_view(GnomeMDI *, GtkWidget *)
 *   are called before actually adding a document or a view to the MDI. if the handler returns
 *   TRUE, the action proceeds otherwise the document or view are not added.
 *
 * gint remove_document(GnomeMDI *, GnomeDocument *)
 * gint remove_view(GnomeMDI *, GtkWidget *)
 *   are called before removing document or view. the handler should return true if the object
 *   should be removed from MDI
 *
 * GList *create_menus(GnomeMDI *)
 *   should return a GList of menuitems to be added to the MDI menubar when the GnomeUIInfo way
 *   with using menu template is not sufficient. This signal is emitted when a new GnomeApp that
 *   needs new menubar is created but ONLY if the menu template is NULL!
 *
 * void app_created(GnomeMDI *, GnomeApp *)
 *   is called with each newly created GnomeApp to allow the MDI user to customize it.
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

#define GNOME_MDI_MS         (1L << 0)      /* for MS-like MDI */
#define GNOME_MDI_NOTEBOOK   (1L << 1)      /* notebook */
#define GNOME_MDI_MODAL      (1L << 2)      /* "modal" app */
#define GNOME_MDI_TOPLEVEL   (1L << 3)      /* many toplevel windows */

#define GNOME_MDI_MODE_FLAGS (GNOME_MDI_MS | GNOME_MDI_NOTEBOOK | GNOME_MDI_MODAL | GNOME_MDI_TOPLEVEL)

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

  GnomeUIInfo *menu_template;
  GnomeUIInfo *toolbar_template;

  gchar *menu_insertion_point;

  GnomeRootWin *root_window; /* this is needed for DND */
};

struct _GnomeMDIClass
{
  GtkObjectClass parent_class;

  GtkMenuBar *(*create_menus)(GnomeMDI *);
  GtkToolbar *(*create_toolbar)(GnomeMDI *);
  gint        (*add_document)(GnomeMDI *, GnomeDocument *); 
  gint        (*remove_document)(GnomeMDI *, GnomeDocument *); 
  gint        (*add_view)(GnomeMDI *, GtkWidget *); 
  gint        (*remove_view)(GnomeMDI *, GtkWidget *); 
  void        (*document_changed)(GnomeMDI *, GnomeDocument *, gpointer);
  void        (*app_created)(GnomeMDI *, GnomeApp *);
};

GtkObject *gnome_mdi_new(gchar *, gchar *);

void gnome_mdi_set_mode(GnomeMDI *, gint);

void gnome_mdi_set_menu_template(GnomeMDI*, GnomeUIInfo *);
void gnome_mdi_set_menu_template(GnomeMDI*, GnomeUIInfo *);

GnomeDocument *gnome_mdi_active_document(GnomeMDI*);

gint gnome_mdi_add_view(GnomeMDI *, GnomeDocument *);
gint gnome_mdi_remove_view(GnomeMDI *, GtkWidget *, gint);

gint gnome_mdi_add_document(GnomeMDI *, GnomeDocument *);
gint gnome_mdi_remove_document(GnomeMDI *, GnomeDocument *, gint);
gint gnome_mdi_remove_all_documents(GnomeMDI *, gint);

void gnome_mdi_menu_place(GnomeMDI *, gint);

#endif /* __GNOME_MDI_H__ */














