#ifndef __GNOME_MDI_ROO_H__
#define __GNOME_MDI_ROO_H__

#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>

#include <libgnomeui/libgnomeui.h>

#include <gnome-mdi-pouch.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GNOME_MDI_ROO(obj)          (GTK_CHECK_CAST ((obj), gnome_mdi_roo_get_type (), GnomeMDIRoo))
#define GNOME_MDI_ROO_CLASS(klass)  (GTK_CHECK_CLASS_CAST ((klass), gnome_mdi_roo_get_type (), GnomeMDIRooClass))
#define GNOME_IS_MDI_ROO(obj)       (GTK_CHECK_TYPE ((obj), gnome_mdi_roo_get_type ()))


typedef struct _GnomeMDIRoo       GnomeMDIRoo;
typedef struct _GnomeMDIRooClass  GnomeMDIRooClass;

struct _GnomeMDIRoo
{
  GtkContainer container;

  GtkWidget *child;

  GnomeMDIPouch *parent;

  gint x, y;
  guint min_width, min_height;

  guint16 title_height;

  gint x_base, y_base;

  gint8 flags;
};

struct _GnomeMDIRooClass
{
  GtkContainerClass parent_class;

  void (* close)  (GnomeMDIRoo *);
  void (* minimize) (GnomeMDIRoo *);
  void (* maximize)  (GnomeMDIRoo *);
};


guint      gnome_mdi_roo_get_type       (void);
GtkWidget *gnome_mdi_roo_new            (void);
void       gnome_mdi_roo_raise          (GnomeMDIRoo *);
void       gnome_mdi_roo_close          (GnomeMDIRoo *);
void       gnome_mdi_roo_minimize       (GnomeMDIRoo *);
void       gnome_mdi_roo_maximize       (GnomeMDIRoo *);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GNOME_MDI_ROO_H__ */
