/*
 * gnome-mdi-pouch.h - definition of a Gnome MDI container
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 * 
 * All thanx for the names Pouch and Roo for the main MDI elements instead
 * of boring Window and Child go to A. A. Milne, author of Winnie the Pooh,
 * one of the best books ever!
 */

#ifndef __GNOME_MDI_POUCH_H__
#define __GNOME_MDI_POUCH_H__

#include <gnome.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgnome/libgnome.h>

#include <libgnomeui/libgnomeui.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GNOME_MDI_POUCH(obj)          GTK_CHECK_CAST (obj, gnome_mdi_pouch_get_type (), GnomeMDIPouch)
#define GNOME_MDI_POUCH_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gnome_mdi_pouch_get_type (), GnomeMDIPouchClass)
#define GNOME_IS_MDI_POUCH(obj)       GTK_CHECK_TYPE (obj, gnome_mdi_pouch_get_type ())

#define GNOME_MDI_POUCH_OUTLINE_DRAG (1L << 0)
#define GNOME_MDI_POUCH_SHADOW_DRAG  (1L << 1)
#define GNOME_MDI_POUCH_FULL_DRAG    (1L << 2)

typedef struct _GnomeMDIPouch       GnomeMDIPouch;
typedef struct _GnomeMDIPouchClass  GnomeMDIPouchClass;

struct _GnomeMDIPouch
{
  GtkContainer container;

  GdkGC *outline_gc;

  GList *roos;

  gint flags;

  gint move_x, move_y;
};

struct _GnomeMDIPouchClass
{
  GtkContainerClass parent_class;

  void (*move_start)(GnomeMDIPouch *, GtkObject *);
  void (*move_end)(GnomeMDIPouch *, GtkObject *, gboolean);
  void (*move_motion)(GnomeMDIPouch *, GtkObject *, gint, gint);
  void (*resize_start)(GnomeMDIPouch *, GtkObject *);
  void (*resize_end)(GnomeMDIPouch *, GtkObject *, gboolean);
  void (*resize_motion)(GnomeMDIPouch *, GtkObject *, gint, gint);
};

GtkWidget *gnome_mdi_pouch_new();
/* this should be used instead of gtk_container_remove(), since
   the widget's real parent is a GnomeMDIRoo instead of GnomeMDIPouch */
void gnome_mdi_pouch_remove(GnomeMDIPouch *, GtkWidget *);
/* there is no problem with gtk_container_add(), but it looks
   nicer if both functions are here */
void gnome_mdi_pouch_add(GnomeMDIPouch *, GtkWidget *);
void gnome_mdi_pouch_set_move_resize(GnomeMDIPouch *, gint);

#endif /* __GNOME_MDI_POUCH_H__ */












