/*
 * bonobo-mdi-child.h - definition of a BonoboMDI object
 *
 * Copyright (C) 2001 Free Software Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA.
 *
 * Author: Paolo Maggi 
 */

#ifndef _BONOBO_MDI_CHILD_H_
#define _BONOBO_MDI_CHILD_H_

#include <gtk/gtk.h>
#include <bonobo/bonobo-window.h>

#define BONOBO_TYPE_MDI_CHILD            (bonobo_mdi_child_get_type ())
#define BONOBO_MDI_CHILD(obj)            (GTK_CHECK_CAST ((obj), BONOBO_TYPE_MDI_CHILD, BonoboMDIChild))
#define BONOBO_MDI_CHILD_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), BONOBO_TYPE_MDI_CHILD, BonoboMDIChildClass))
#define BONOBO_IS_MDI_CHILD(obj)         (GTK_CHECK_TYPE ((obj), BONOBO_TYPE_MDI_CHILD))
#define BONOBO_IS_MDI_CHILD_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), BONOBO_TYPE_MDI_CHILD))
#define BONOBO_MDI_CHILD_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), BONOBO_TYPE_MDI_CHILD, BonoboMDIChildClass))

/* BonoboMDIChild
 * is an abstract class. In order to use it, you have to either derive a
 * new class from it and set the proper virtual functions in its parent
 * BonoboMDIChildClass structure or use the BonoboMDIGenericChild class
 * that allows to specify the relevant functions on a per-instance basis
 * and can directly be used with BonoboMDI.
 */

typedef struct _BonoboMDIChildPrivate BonoboMDIChildPrivate;

typedef struct
{
	GtkObject object;

	BonoboMDIChildPrivate *priv;
} BonoboMDIChild;


typedef GtkWidget *(*BonoboMDIChildViewCreator) (BonoboMDIChild *, gpointer);
typedef GList     *(*BonoboMDIChildMenuCreator) (BonoboMDIChild *, GtkWidget *, gpointer);
typedef gchar     *(*BonoboMDIChildConfigFunc)  (BonoboMDIChild *, gpointer);
typedef GtkWidget *(*BonoboMDIChildLabelFunc)   (BonoboMDIChild *, GtkWidget *, gpointer);

/* 
 * Note that if you override the set_label virtual function, it should return
 * a new widget if its GtkWidget* parameter is NULL and modify and return the
 * old widget otherwise.
 * (see bonobo-mdi-child.c/bonobo_mdi_child_set_book_label() for an example).
 */

typedef struct
{
	GtkObjectClass parent_class;

	/* Virtual functions */
	BonoboMDIChildViewCreator create_view;
	BonoboMDIChildMenuCreator create_menus;
	BonoboMDIChildConfigFunc  get_config_string;
	BonoboMDIChildLabelFunc   set_label;

	void (* name_changed)		(BonoboMDIChild *mdi_child, gchar* old_name);

} BonoboMDIChildClass;

GType         bonobo_mdi_child_get_type         (void) G_GNUC_CONST;

GtkWidget    *bonobo_mdi_child_add_view   	(BonoboMDIChild *mdi_child);
void          bonobo_mdi_child_remove_view      (BonoboMDIChild *mdi_child, GtkWidget *view);

GList        *bonobo_mdi_child_get_views        (const BonoboMDIChild *mdi_child);

void          bonobo_mdi_child_set_name         (BonoboMDIChild *mdi_child, const gchar *name);
gchar        *bonobo_mdi_child_get_name         (const BonoboMDIChild *mdi_child);

void          bonobo_mdi_child_set_parent       (BonoboMDIChild *mdi_child, GtkObject *parent);
GtkObject    *bonobo_mdi_child_get_parent       (const BonoboMDIChild *mdi_child);


/*
void          bonobo_mdi_child_set_menu_template(BonoboMDIChild *mdi_child, GnomeUIInfo *menu_tmpl);
*/
G_END_DECLS

#endif /* _BONOBO_MDI_CHILD_H_ */



