/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * bonobo-mdi.h - definition of a BonoboMDI object
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

#ifndef _BONOBO_MDI_H_
#define _BONOBO_MDI_H_

#include <gtk/gtk.h>
#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-ui-component.h>


#include "bonobo-mdi-child.h"

#define BONOBO_TYPE_MDI            (bonobo_mdi_get_type ())
#define BONOBO_MDI(obj)            (GTK_CHECK_CAST ((obj), BONOBO_TYPE_MDI, BonoboMDI))
#define BONOBO_MDI_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), BONOBO_TYPE_MDI, BonoboMDIClass))
#define BONOBO_IS_MDI(obj)         (GTK_CHECK_TYPE ((obj), BONOBO_TYPE_MDI))
#define BONOBO_IS_MDI_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), BONOBO_TYPE_MDI))
#define BONOBO_MDI_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), BONOBO_TYPE_MDI, BonoboMDIClass))

typedef enum {
	BONOBO_MDI_NOTEBOOK = 0,
	BONOBO_MDI_TOPLEVEL,
	BONOBO_MDI_MODAL,
	BONOBO_MDI_DEFAULT_MODE = 42
} BonoboMDIMode;

typedef struct _BonoboMDIPrivate BonoboMDIPrivate;

typedef struct {
	GtkObject object;

	BonoboMDIPrivate* priv;
} BonoboMDI;

typedef struct {
	GtkObjectClass parent_class;

	/* Signals */
	gboolean	(*add_child)		(BonoboMDI *mdi, BonoboMDIChild *child); 
	gboolean	(*remove_child)		(BonoboMDI *mdi, BonoboMDIChild *child); 
	gboolean	(*add_view)		(BonoboMDI *mdi, GtkWidget *view); 
	gboolean	(*remove_view)		(BonoboMDI *mdi, GtkWidget *view); 
	void 		(*child_changed)	(BonoboMDI *mdi, BonoboMDIChild *child);
	void 		(*view_changed)		(BonoboMDI *mdi, GtkWidget *view);
	void 		(*top_window_created)	(BonoboMDI *mdi, BonoboWindow *window);
} BonoboMDIClass;

/*
 * description of BonoboMDI signals:
 *
 * gboolean add_child(BonoboMDI *, BonoboMDIChild *)
 * gboolean add_view(BonoboMDI *, GtkWidget *)
 *   are called before actually adding a mdi_child or a view to the MDI. if the handler returns
 *   TRUE, the action proceeds otherwise the mdi_child or view are not added.
 *
 * gboolean remove_child(BonoboMDI *, BonoboMDIChild *)
 * gboolean remove_view(BonoboMDI *, GtkWidget *)
 *   are called before removing mdi_child or view. the handler should return true if the object
 *   is to be removed from MDI
 *
 * void child_changed(BonoboMDI *, BonoboMDIChild *)
 *   gets called each time when active child is changed with the second argument
 *   pointing to the old child. mdi->active_view and mdi->active_child still already
 *   hold the new values
 *
 * void view_changed(BonoboMDI *, GtkWidget *)
 *   is emitted whenever a view is changed, regardless of it being the view of the same child as
 *   the old view or not. the second argument points to the old view, mdi->active_view and
 *   mdi->active_child hold the new values. if the child has also been changed, this signal is
 *   emitted after the child_changed signal.
 * 
 * void top_window_created(BonoboMDI *, BonoboWindow *)
 *   is called with each newly created BonoboWindow to allow the MDI user to customize it (add a
 *   statusbar, toolbars or menubar if the method with ui templates is not sufficient,
 *   etc.).
 *   no contents may be set since BonoboMDI uses them for storing either a view of a child
 *   or a notebook
 */

GType         bonobo_mdi_get_type            	(void);

GtkObject    *bonobo_mdi_new                	(const gchar *mdi_name, const gchar *title);

void 	      bonobo_mdi_construct (BonoboMDI *mdi, gchar* name, gchar* title, GtkPositionType tab_pos);

void          bonobo_mdi_set_mode 		(BonoboMDI *mdi, BonoboMDIMode mode);
BonoboMDIMode bonobo_mdi_get_mode 		(BonoboMDI *mdi);

/* setting the ui template*/
void          bonobo_mdi_set_ui_template	(BonoboMDI *mdi, const gchar *xml, BonoboUIVerb verbs[]);
void          bonobo_mdi_set_ui_template_file	(BonoboMDI *mdi, const gchar *file_name, 
									BonoboUIVerb verbs[]);
/*
void          bonobo_mdi_set_child_menu_path (BonoboMDI *mdi, const gchar *path);
*/
void          bonobo_mdi_set_child_list_path 	(BonoboMDI *mdi, const gchar *path);

/* manipulating views */
gboolean      bonobo_mdi_add_view            	(BonoboMDI *mdi, BonoboMDIChild *child);
gboolean      bonobo_mdi_add_toplevel_view   	(BonoboMDI *mdi, BonoboMDIChild *child);
gboolean      bonobo_mdi_remove_view         	(BonoboMDI *mdi, GtkWidget *view, gboolean force);

GtkWidget    *bonobo_mdi_get_active_view    	(BonoboMDI *mdi);
void          bonobo_mdi_set_active_view     	(BonoboMDI *mdi, GtkWidget *view);

/* manipulating children */
gint          bonobo_mdi_add_child           	(BonoboMDI *mdi, BonoboMDIChild *child);
gint          bonobo_mdi_remove_child        	(BonoboMDI *mdi, BonoboMDIChild *child, gboolean force);
gint          bonobo_mdi_remove_all          	(BonoboMDI *mdi, gboolean force);

void          bonobo_mdi_open_toplevel       	(BonoboMDI *mdi);
/*
void          bonobo_mdi_update_child        (BonoboMDI *mdi, BonoboMDIChild *child);
*/
BonoboMDIChild *bonobo_mdi_get_active_child   (BonoboMDI *mdi);
BonoboMDIChild *bonobo_mdi_find_child         (BonoboMDI *mdi, const gchar *name);

BonoboWindow   *bonobo_mdi_get_active_window  (BonoboMDI *mdi);

/*
 * the following two functions are here to make life easier if an application
 * creates objects (like opening a window) that should "keep the application
 * alive" even if there are no MDI windows open. any such windows should be
 * registered with the MDI: as long as there is a window registered, the MDI
 * will not destroy itself (even if the last of its windows is closed). on the
 * other hand, closing the last MDI window when no objects are registered
 * with the MDI will result in MDI being gtk_object_destroy()ed.
 */
void          bonobo_mdi_register            (BonoboMDI *mdi, GtkObject *object);
void          bonobo_mdi_unregister          (BonoboMDI *mdi, GtkObject *object);

/*
 * convenience functions for retrieveing BonoboMDIChild and BonoboApp
 * objects associated with a particular view and for retrieveing the
 * visible view of a certain BonoboWindow.
 */
BonoboWindow      *bonobo_mdi_get_window_from_view    (GtkWidget *view);
BonoboMDIChild    *bonobo_mdi_get_child_from_view     (GtkWidget *view);
GtkWidget         *bonobo_mdi_get_view_from_window    (BonoboMDI *mdi, BonoboWindow *window);

GList          		*bonobo_mdi_get_children 			(BonoboMDI *mdi);
GList          		*bonobo_mdi_get_windows				(BonoboMDI *mdi);

BonoboUIComponent 	*bonobo_mdi_get_ui_component_from_window 	(BonoboWindow* win);

void bonobo_window_flash (BonoboWindow * win, const gchar * flash); 
void bonobo_window_show_status (BonoboWindow * win, const gchar * msg); 

#endif /* _BONOBO_MDI_H_ */
