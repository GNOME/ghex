/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * bonobo-mdi.c - implementation of a BonoboMDI object
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

#include "bonobo-mdi.h"

#include <bonobo/bonobo-i18n.h>
#include <ghex-marshal.h>
#include <bonobo/bonobo-dock-layout.h>
#include <bonobo/bonobo-ui-util.h>

#include <string.h>

struct _MessageInfo {
  BonoboWindow * win;
  guint timeoutid;
  guint handlerid;
};

typedef struct _MessageInfo MessageInfo;

#define BONOBO_MDI_KEY "BonoboMDI"
#define BONOBO_MDI_CHILD_KEY "BonoboMDIChild"
#define UI_COMPONENT_KEY "UIComponent"

static void            bonobo_mdi_class_init     (BonoboMDIClass  *);
static void            bonobo_mdi_instance_init  (BonoboMDI *);
static void            bonobo_mdi_destroy        (GtkObject *);
static void            bonobo_mdi_finalize       (GObject *);
static void            child_list_menu_create    (BonoboMDI *, BonoboWindow *);

static void            child_list_activated_cb   (BonoboUIComponent *uic, gpointer user_data, 
						  const gchar* verbname);

void                   child_list_menu_remove_item(BonoboMDI *, BonoboMDIChild *);
void                   child_list_menu_add_item   (BonoboMDI *, BonoboMDIChild *);

static void            app_create               (BonoboMDI *, gchar *);
static void            app_clone                (BonoboMDI *, BonoboWindow *);
static void            app_destroy              (BonoboWindow *, BonoboMDI *);
static void            app_set_view             (BonoboMDI *, BonoboWindow *, GtkWidget *);

static gint            app_close_top            (BonoboWindow *, GdkEventAny *, BonoboMDI *);
static gint            app_close_book           (BonoboWindow *, GdkEventAny *, BonoboMDI *);

static GtkWidget       *book_create             (BonoboMDI *);
static void            book_switch_page         (GtkNotebook *, GtkNotebookPage *,
						 gint, BonoboMDI *);
static gint            book_motion              (GtkWidget *widget, GdkEventMotion *e,
						 gpointer data);
static gint            book_button_press        (GtkWidget *widget, GdkEventButton *e,
						 gpointer data);
static gint            book_button_release      (GtkWidget *widget, GdkEventButton *e,
						 gpointer data);
static void            book_add_view            (GtkNotebook *, GtkWidget *);
static void            set_page_by_widget       (GtkNotebook *, GtkWidget *);

static void            toplevel_focus           (BonoboWindow *, GdkEventFocus *, BonoboMDI *);

static void            top_add_view             (BonoboMDI *, BonoboMDIChild *, GtkWidget *);

static void            set_active_view          (BonoboMDI *, GtkWidget *);

/* convenience functions that call child's "virtual" functions */
#if 0
static GList           *child_create_menus      (BonoboMDIChild *, GtkWidget *);
#endif
static GtkWidget       *child_set_label         (BonoboMDIChild *, GtkWidget *);

static void 		child_name_changed 	(BonoboMDIChild *mdi_child, gchar* old_name, 
										BonoboMDI *bonobo_mdi);
static void          	bonobo_mdi_update_child (BonoboMDI *bonobo_mdi, BonoboMDIChild *child);

static gchar* 		escape_underscores 	(const gchar* text);

static GdkCursor *drag_cursor = NULL;

enum {
	ADD_CHILD,
	REMOVE_CHILD,
	ADD_VIEW,
	REMOVE_VIEW,
	CHILD_CHANGED,
	VIEW_CHANGED,
	TOP_WINDOW_CREATED,
	LAST_SIGNAL
};


struct _BonoboMDIPrivate
{
	BonoboMDIMode 	mode;

	GtkPositionType tab_pos;

	guint 		signal_id;
	gint 		in_drag : 1;

	gchar 		*mdi_name; 
	gchar		*title;

	gchar 		*ui_xml;
	gchar	        *ui_file_name;
	BonoboUIVerb    *verbs;

	/* Probably only one of these would do, but... redundancy rules ;) */
	BonoboMDIChild 	*active_child;
	GtkWidget 	*active_view;  
	BonoboWindow 	*active_window;

	GList 		*windows;     	/* toplevel windows -  BonoboWindows widgets */
	GList 		*children;    	/* children - BonoboMDIChild objects*/

	GSList 		*registered; 	/* see comment for bonobo_mdi_(un)register() functions 
					 * below for an explanation. */
	
    	/* Paths for insertion of mdi-child list menu via */
	gchar *child_list_path;
};

typedef gboolean   (*BonoboMDISignal1) (GtkObject *, gpointer, gpointer);
typedef void       (*BonoboMDISignal2) (GtkObject *, gpointer, gpointer);

static GtkObjectClass *parent_class = NULL;
static guint mdi_signals [LAST_SIGNAL] = { 0 };

GType
bonobo_mdi_get_type (void)
{
	static GType bonobo_mdi_type = 0;

  	if (bonobo_mdi_type == 0)
    	{
      		static const GTypeInfo our_info =
      		{
        		sizeof (BonoboMDIClass),
        		NULL,		/* base_init */
        		NULL,		/* base_finalize */
        		(GClassInitFunc) bonobo_mdi_class_init,
        		NULL,           /* class_finalize */
        		NULL,           /* class_data */
        		sizeof (BonoboMDI),
        		0,              /* n_preallocs */
        		(GInstanceInitFunc) bonobo_mdi_instance_init
      		};

      		bonobo_mdi_type = g_type_register_static (GTK_TYPE_OBJECT,
                				    "BonoboMDI",
                                       	 	    &our_info,
                                       		    0);
    	}

	return bonobo_mdi_type;
}

static void 
bonobo_mdi_class_init (BonoboMDIClass *class)
{
	GtkObjectClass *object_class;
	GObjectClass *gobject_class;
	
	object_class = (GtkObjectClass*) class;
	gobject_class = (GObjectClass*) class;
	
	object_class->destroy = bonobo_mdi_destroy;
	gobject_class->finalize = bonobo_mdi_finalize;

	mdi_signals[ADD_CHILD] = gtk_signal_new ("add_child",
					GTK_RUN_LAST,
					GTK_CLASS_TYPE (object_class),
					GTK_SIGNAL_OFFSET (BonoboMDIClass, add_child),
					ghex_marshal_BOOLEAN__OBJECT,
					GTK_TYPE_BOOL, 1, BONOBO_TYPE_MDI_CHILD);
	mdi_signals[REMOVE_CHILD] = gtk_signal_new ("remove_child",
					GTK_RUN_LAST,
					GTK_CLASS_TYPE (object_class),
					GTK_SIGNAL_OFFSET (BonoboMDIClass, remove_child),
					ghex_marshal_BOOLEAN__OBJECT,
					GTK_TYPE_BOOL, 1, BONOBO_TYPE_MDI_CHILD);
	mdi_signals[ADD_VIEW] = gtk_signal_new ("add_view",
					GTK_RUN_LAST,
					GTK_CLASS_TYPE (object_class),
					GTK_SIGNAL_OFFSET (BonoboMDIClass, add_view),
					ghex_marshal_BOOLEAN__OBJECT,
					GTK_TYPE_BOOL, 1, GTK_TYPE_WIDGET);
	mdi_signals[REMOVE_VIEW] = gtk_signal_new ("remove_view",
					GTK_RUN_LAST,
					GTK_CLASS_TYPE (object_class),
					GTK_SIGNAL_OFFSET (BonoboMDIClass, remove_view),
					ghex_marshal_BOOLEAN__OBJECT,
					GTK_TYPE_BOOL, 1, GTK_TYPE_WIDGET);
	mdi_signals[CHILD_CHANGED] = gtk_signal_new ("child_changed",
					GTK_RUN_LAST,
					GTK_CLASS_TYPE (object_class),
					GTK_SIGNAL_OFFSET (BonoboMDIClass, child_changed),
					gtk_marshal_VOID__OBJECT,
					GTK_TYPE_NONE, 1, BONOBO_TYPE_MDI_CHILD);
	mdi_signals[VIEW_CHANGED] = gtk_signal_new ("view_changed",
					GTK_RUN_LAST,
					GTK_CLASS_TYPE (object_class),
					GTK_SIGNAL_OFFSET(BonoboMDIClass, view_changed),
					gtk_marshal_VOID__OBJECT,
					GTK_TYPE_NONE, 1, GTK_TYPE_WIDGET);
	mdi_signals[TOP_WINDOW_CREATED] = gtk_signal_new ("top_window_created",
					GTK_RUN_LAST,
					GTK_CLASS_TYPE (object_class),
					GTK_SIGNAL_OFFSET (BonoboMDIClass, top_window_created),
					gtk_marshal_VOID__OBJECT,
					GTK_TYPE_NONE, 1, BONOBO_TYPE_WINDOW);
	
	class->add_child 		= NULL;
	class->remove_child 		= NULL;
	class->add_view 		= NULL;
	class->remove_view 		= NULL;
	class->child_changed 		= NULL;
	class->view_changed 		= NULL;
	class->top_window_created 	= NULL;

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);
}

static void 
bonobo_mdi_finalize (GObject *object)
{
    	BonoboMDI *bonobo_mdi;

	g_return_if_fail (BONOBO_IS_MDI (object));
	
	bonobo_mdi = BONOBO_MDI (object);
	g_return_if_fail (bonobo_mdi->priv != NULL);
	
	g_free (bonobo_mdi->priv->child_list_path);
	bonobo_mdi->priv->child_list_path = NULL;
	
	g_free (bonobo_mdi->priv->mdi_name);
	bonobo_mdi->priv->mdi_name = NULL;
	g_free (bonobo_mdi->priv->title);
	bonobo_mdi->priv->title = NULL;

	if (bonobo_mdi->priv->ui_xml != NULL)
	{
		g_free (bonobo_mdi->priv->ui_xml);
		bonobo_mdi->priv->ui_xml = NULL;
	}
	
	if (bonobo_mdi->priv->ui_file_name != NULL)
	{
		g_free (bonobo_mdi->priv->ui_file_name);
		bonobo_mdi->priv->ui_file_name = NULL;
	}
	
	g_free (bonobo_mdi->priv);
	bonobo_mdi->priv = NULL;

	if(G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize)(object);
}

static void 
bonobo_mdi_destroy (GtkObject *object)
{
	BonoboMDI *bonobo_mdi;

	g_return_if_fail (BONOBO_IS_MDI (object));
	
	bonobo_mdi = BONOBO_MDI (object);
	g_return_if_fail (bonobo_mdi->priv != NULL);

	bonobo_mdi_remove_all (bonobo_mdi, TRUE);

	/* Note: This is BROOOOOKEN.  We need to figure out
	 * some better way to do this.  This may not even
	 * work right now, but that's what the 1.0 version did
	 *  -George*/

	/* this call tries to behave in a manner similar to
	   destruction of toplevel windows: it unrefs itself,
	   thus taking care of the initial reference added
	   upon mdi creation. */
	/*
	if(G_OBJECT (object)->ref_count > 0 && !GTK_OBJECT_DESTROYED (object))
		gtk_object_unref (object);
	*/

	if(GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy)(object);
}

static void 
bonobo_mdi_instance_init (BonoboMDI *bonobo_mdi)
{
	g_return_if_fail (BONOBO_IS_MDI (bonobo_mdi));

	bonobo_mdi->priv = g_new0 (BonoboMDIPrivate, 1);
	
	/* FIXME!
	bonobo_mdi->priv->mode = gnome_preferences_get_mdi_mode();
	bonobo_mdi->priv->tab_pos = gnome_preferences_get_mdi_tab_pos();
	 */
	bonobo_mdi->priv->mode = BONOBO_MDI_NOTEBOOK;
	bonobo_mdi->priv->tab_pos = GTK_POS_TOP;
		
	bonobo_mdi->priv->signal_id 		= 0;
	bonobo_mdi->priv->in_drag 		= FALSE;

	bonobo_mdi->priv->children 		= NULL;
	bonobo_mdi->priv->windows 		= NULL;
	bonobo_mdi->priv->registered 		= NULL;
	
	bonobo_mdi->priv->active_child 	= NULL;
	bonobo_mdi->priv->active_window 	= NULL;
	bonobo_mdi->priv->active_view 		= NULL;

/*	
	bonobo_mdi->priv->menu_template 	= NULL;
	bonobo_mdi->priv->toolbar_template 	= NULL;
	bonobo_mdi->priv->child_menu_path 	= NULL;
*/
	bonobo_mdi->priv->ui_xml		= NULL;
	bonobo_mdi->priv->ui_file_name		= NULL;
	bonobo_mdi->priv->verbs		= NULL;	
	
	bonobo_mdi->priv->child_list_path 	= NULL;

	g_object_ref (G_OBJECT (bonobo_mdi));
	gtk_object_sink (GTK_OBJECT (bonobo_mdi));
}


/**
 * bonobo_mdi_new:
 * @mdi_name: Application name as used in filenames and paths.
 * @title: Title of the application windows.
 * 
 * Description:
 * Creates a new MDI object. @mdi_name and @title are used for
 * MDI's calling bonobo_window_new (). 
 * 
 * Return value:
 * A pointer to a new BonoboMDI object.
 **/
GtkObject *
bonobo_mdi_new (const gchar *mdi_name, const gchar *title) 
{
	BonoboMDI *bonobo_mdi;
	
	bonobo_mdi = gtk_type_new (BONOBO_TYPE_MDI);
  
	bonobo_mdi->priv->mdi_name = g_strdup (mdi_name);
	bonobo_mdi->priv->title    = g_strdup (title);

	return GTK_OBJECT (bonobo_mdi);
}

/*
static GList *
child_create_menus (BonoboMDIChild *child, GtkWidget *view)
{
	if(BONOBO_MDI_CHILD_GET_CLASS(child)->create_menus)
		return BONOBO_MDI_CHILD_GET_CLASS(child)->create_menus(child, view, NULL);

	return NULL;
}
*/

static GtkWidget *
child_set_label (BonoboMDIChild *child, GtkWidget *label)
{
	return BONOBO_MDI_CHILD_GET_CLASS (child)->set_label (child, label, NULL);
}

static void 
set_page_by_widget (GtkNotebook *book, GtkWidget *child)
{
	gint i;
	
	i = gtk_notebook_page_num (book, child);
	
	if (gtk_notebook_get_current_page (book) != i)
		gtk_notebook_set_current_page (book, i);
}


static void 
child_list_activated_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	BonoboMDI* bonobo_mdi;
	BonoboMDIChild *child = BONOBO_MDI_CHILD (user_data);
	g_return_if_fail (child != NULL);

	bonobo_mdi = BONOBO_MDI (gtk_object_get_data (GTK_OBJECT (child), BONOBO_MDI_KEY));
	g_return_if_fail (bonobo_mdi != NULL);
		
	if (child && (child != bonobo_mdi->priv->active_child)) 
	{
		GList *views = bonobo_mdi_child_get_views (child);
		
		if (views)
			bonobo_mdi_set_active_view (bonobo_mdi, views->data);
		else
			bonobo_mdi_add_view (bonobo_mdi, child);
	}
}

static gchar* 
escape_underscores (const gchar* text)
{
	GString *str;
	gint length;
	const gchar *p;
 	const gchar *end;

  	g_return_val_if_fail (text != NULL, NULL);

    	length = strlen (text);

	str = g_string_new ("");

  	p = text;
  	end = text + length;

  	while (p != end)
    	{
      		const gchar *next;
      		next = g_utf8_next_char (p);

		switch (*p)
        	{
       			case '_':
          			g_string_append (str, "__");
          			break;
        		default:
          			g_string_append_len (str, p, next - p);
          			break;
        	}

      		p = next;
    	}

	return g_string_free (str, FALSE);
}

static void 
child_list_menu_create (BonoboMDI *bonobo_mdi, BonoboWindow *win)
{
	GList *child;
	BonoboUIComponent *ui_component;

	if (bonobo_mdi->priv->child_list_path == NULL)
		return;
	
	ui_component = BONOBO_UI_COMPONENT (
			gtk_object_get_data (GTK_OBJECT (win), UI_COMPONENT_KEY));
			
	child = bonobo_mdi->priv->children;
	
	bonobo_ui_component_freeze (ui_component, NULL);
	
	while (child) 
	{
		gchar *xml = NULL;
		gchar *cmd = NULL;
		gchar *verb_name = NULL;
		gchar *tip;
		gchar *escaped_name;
		gchar *child_name = bonobo_mdi_child_get_name (BONOBO_MDI_CHILD (child->data));

		escaped_name = escape_underscores (child_name);
		
		tip =  g_strdup_printf (_("Activate %s"), child_name);
		verb_name = g_strdup_printf ("Child_%p", child->data);
		xml = g_strdup_printf ("<menuitem name=\"%s\" verb=\"%s\""
				" _label=\"%s\"/>", verb_name, verb_name, escaped_name);
		cmd =  g_strdup_printf ("<cmd name = \"%s\" _label=\"%s\""
				" _tip=\"%s\"/>", verb_name, escaped_name, tip);

		g_free (tip);
		g_free (child_name);
		g_free (escaped_name);

		bonobo_ui_component_set_translate (ui_component, bonobo_mdi->priv->child_list_path, xml, NULL);
		bonobo_ui_component_set_translate (ui_component, "/commands/", cmd, NULL);
		bonobo_ui_component_add_verb (ui_component, verb_name, child_list_activated_cb, child->data); 
		
		g_free (xml); 
		g_free (cmd);
		g_free (verb_name);

		child = g_list_next (child);
	}

	bonobo_ui_component_thaw (ui_component, NULL);
}


void 
child_list_menu_remove_item (BonoboMDI *bonobo_mdi, BonoboMDIChild *child)
{
	GList *win_node;
	gchar *path, *cmd, *verb_name;
	
	if(bonobo_mdi->priv->child_list_path == NULL)
		return;
	
	win_node = bonobo_mdi->priv->windows;

	verb_name = g_strdup_printf ("Child_%p", child);
	path = g_strdup_printf ("%s%s", bonobo_mdi->priv->child_list_path, verb_name);
	cmd = g_strdup_printf ("/commands/%s", verb_name);

	while (win_node) 
	{
		BonoboUIComponent *ui_component;
		
		ui_component = BONOBO_UI_COMPONENT (
			gtk_object_get_data (GTK_OBJECT (win_node->data), UI_COMPONENT_KEY));
		
		bonobo_ui_component_remove_verb (ui_component, verb_name);
		bonobo_ui_component_rm (ui_component, path, NULL);
		bonobo_ui_component_rm (ui_component, cmd, NULL);
		win_node = g_list_next (win_node);
	}

	g_free (path);
	g_free (cmd);
	g_free (verb_name);
}


void 
child_list_menu_add_item (BonoboMDI *bonobo_mdi, BonoboMDIChild *child)
{
	GList *win_node;
	gchar *child_name, *escaped_name;
	gchar* xml, *cmd, *verb_name, *tip;
	
	if(bonobo_mdi->priv->child_list_path == NULL)
		return;
	
	win_node = bonobo_mdi->priv->windows;

	child_name = bonobo_mdi_child_get_name (child);
	escaped_name = escape_underscores (child_name);
	verb_name = g_strdup_printf ("Child_%p", child);
	
	tip = g_strdup_printf (_("Activate %s"), child_name);
	xml = g_strdup_printf ("<menuitem name=\"%s\" verb=\"%s\""
				" _label=\"%s\"/>", verb_name, verb_name, escaped_name);
	cmd =  g_strdup_printf ("<cmd name = \"%s\" _label=\"%s\""
				" _tip=\"%s\"/>", verb_name, escaped_name, tip);
	
	while (win_node) 
	{
		BonoboUIComponent *ui_component;
		
		ui_component = BONOBO_UI_COMPONENT (
			gtk_object_get_data (GTK_OBJECT (win_node->data), UI_COMPONENT_KEY));
		

		bonobo_ui_component_set_translate (ui_component, bonobo_mdi->priv->child_list_path, xml, NULL);
		bonobo_ui_component_set_translate (ui_component, "/commands/", cmd, NULL);
		bonobo_ui_component_add_verb (ui_component, verb_name, child_list_activated_cb, child); 

		win_node = g_list_next (win_node);
	}
	
	g_free (tip);
	g_free (escaped_name);
	g_free (child_name);
	g_free (xml);
	g_free (cmd);
	g_free (verb_name);
}

static gint 
book_motion (GtkWidget *widget, GdkEventMotion *e, gpointer data)
{
	BonoboMDI *bonobo_mdi;

	bonobo_mdi = BONOBO_MDI (data);

	if (!drag_cursor)
		drag_cursor = gdk_cursor_new (GDK_HAND2);

	if (e->window == GTK_NOTEBOOK (widget)->event_window) 
	{
		bonobo_mdi->priv->in_drag = TRUE;
		gtk_grab_add (widget);
		gdk_pointer_grab (widget->window, FALSE,
						 GDK_POINTER_MOTION_MASK |
						 GDK_BUTTON_RELEASE_MASK, NULL,
						 drag_cursor, GDK_CURRENT_TIME);
		if (bonobo_mdi->priv->signal_id) 
		{
			gtk_signal_disconnect (GTK_OBJECT (widget), bonobo_mdi->priv->signal_id);
			bonobo_mdi->priv->signal_id = 0;
		}
	}

	return FALSE;
}

static gint 
book_button_press (GtkWidget *widget, GdkEventButton *e, gpointer data)
{
	BonoboMDI *bonobo_mdi;

	bonobo_mdi = BONOBO_MDI (data);

	if ((e->button == 1) && (e->window == GTK_NOTEBOOK (widget)->event_window))
		bonobo_mdi->priv->signal_id = gtk_signal_connect (GTK_OBJECT (widget), 
				"motion_notify_event",
				GTK_SIGNAL_FUNC (book_motion), bonobo_mdi);

	return FALSE;
}

static gint 
book_button_release (GtkWidget *widget, GdkEventButton *e, gpointer data)
{
	gint x = e->x_root, y = e->y_root;
	BonoboMDI *bonobo_mdi;

	bonobo_mdi = BONOBO_MDI(data);
	
	if (bonobo_mdi->priv->signal_id) 
	{
		gtk_signal_disconnect (GTK_OBJECT (widget), bonobo_mdi->priv->signal_id);
		bonobo_mdi->priv->signal_id = 0;
	}

	if ((e->button == 1) && bonobo_mdi->priv->in_drag) 
	{	
		GdkWindow *window;
		GList *child;
		BonoboWindow *win;
		GtkWidget *view, *new_book;
		GtkNotebook *old_book = GTK_NOTEBOOK (widget);

		bonobo_mdi->priv->in_drag = FALSE;
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
		gtk_grab_remove (widget);

		window = gdk_window_at_pointer (&x, &y);
		if (window)
			window = gdk_window_get_toplevel (window);

		child = bonobo_mdi->priv->windows;
		
		while (child) 
		{
			if (window == GTK_WIDGET (child->data)->window) 
			{
				int cur_page;

				/* page was dragged to another notebook */

				old_book = GTK_NOTEBOOK(widget);
				new_book = bonobo_window_get_contents (BONOBO_WINDOW(child->data));

				if (old_book == (GtkNotebook *) new_book) 
					/* page has been dropped on the source notebook */
					return FALSE;

				cur_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (old_book));
	
				if (cur_page >= 0) 
				{
					view = gtk_notebook_get_nth_page (GTK_NOTEBOOK (old_book), cur_page);
					gtk_container_remove (GTK_CONTAINER(old_book), view);

					book_add_view (GTK_NOTEBOOK (new_book), view);

					win = bonobo_mdi_get_window_from_view (view);
					gdk_window_raise (GTK_WIDGET(win)->window);

					cur_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (old_book));

					if (cur_page < 0) 
					{
						bonobo_mdi->priv->active_window = win;
						win = BONOBO_WINDOW (gtk_widget_get_toplevel (
									GTK_WIDGET (old_book)));
						bonobo_mdi->priv->windows = g_list_remove(bonobo_mdi->priv->windows, win);
						gtk_widget_destroy (GTK_WIDGET(win));
					}
				}

				return FALSE;
			}
				
			child = child->next;
		}

		if (g_list_length (old_book->children) == 1)
			return FALSE;

		/* create a new toplevel */
		if (old_book->cur_page) 
		{
			gint width, height;
			int cur_page = gtk_notebook_get_current_page (old_book);

			view = gtk_notebook_get_nth_page (old_book, cur_page);
				
			win = bonobo_mdi_get_window_from_view (view);

			width = view->allocation.width;
			height = view->allocation.height;
		
			gtk_container_remove (GTK_CONTAINER (old_book), view);
			
			app_clone (bonobo_mdi, win);
				
			new_book = book_create (bonobo_mdi);
	
			book_add_view (GTK_NOTEBOOK (new_book), view);

			gtk_window_set_position (GTK_WINDOW (bonobo_mdi->priv->active_window), GTK_WIN_POS_MOUSE);
	
			gtk_widget_set_usize (view, width, height);

			if (!GTK_WIDGET_VISIBLE (bonobo_mdi->priv->active_window))
				gtk_widget_show (GTK_WIDGET (bonobo_mdi->priv->active_window));
		}
	}

	return FALSE;
}

static GtkWidget *
book_create (BonoboMDI *bonobo_mdi)
{
	GtkWidget *us;

	us = gtk_notebook_new ();

	gtk_widget_add_events (us, GDK_BUTTON1_MOTION_MASK);

	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (us), bonobo_mdi->priv->tab_pos);
	
	gtk_widget_show (us);

	bonobo_window_set_contents (bonobo_mdi->priv->active_window, us);

	gtk_signal_connect (GTK_OBJECT (us), "switch_page",
			    GTK_SIGNAL_FUNC (book_switch_page), bonobo_mdi);

	gtk_signal_connect (GTK_OBJECT (us), "button_press_event",
			    GTK_SIGNAL_FUNC (book_button_press), bonobo_mdi);
	gtk_signal_connect (GTK_OBJECT (us), "button_release_event",
			    GTK_SIGNAL_FUNC (book_button_release), bonobo_mdi);

	gtk_notebook_set_scrollable (GTK_NOTEBOOK (us), TRUE);
	
	return us;
}

static void 
book_add_view (GtkNotebook *book, GtkWidget *view)
{
	BonoboMDIChild *child;
	GtkWidget *title;

	child = bonobo_mdi_get_child_from_view (view);

	title = child_set_label (child, NULL);

	gtk_notebook_append_page (book, view, title);

	if (g_list_length (book->children) > 1)
		set_page_by_widget (book, view);
}

static void 
book_switch_page(GtkNotebook *book, GtkNotebookPage *pg, gint page_num, BonoboMDI *bonobo_mdi)
{
	BonoboWindow *win;
	GtkWidget *page;
	
	win = BONOBO_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (book)));

	page = gtk_notebook_get_nth_page (book, page_num);

	if(page != NULL) 
	{
		if(page != bonobo_mdi->priv->active_view)
			app_set_view (bonobo_mdi, win, page);
	} 
	else 
	{
		app_set_view (bonobo_mdi, win, NULL);  
	}
}

static void 
toplevel_focus (BonoboWindow *win, GdkEventFocus *event, BonoboMDI *bonobo_mdi)
{
	GtkWidget *contents;
	/* updates active_view and active_child when a new toplevel receives focus */
	g_return_if_fail (BONOBO_IS_WINDOW (win));
	
	bonobo_mdi->priv->active_window = win;
	
	contents = bonobo_window_get_contents (win);
	
	if ((bonobo_mdi->priv->mode == BONOBO_MDI_TOPLEVEL) || (bonobo_mdi->priv->mode == BONOBO_MDI_MODAL)) 
	{
		set_active_view (bonobo_mdi, contents);
	} 
	else 
		if((bonobo_mdi->priv->mode == BONOBO_MDI_NOTEBOOK) && GTK_NOTEBOOK(contents)->cur_page) 
		{
			int cur_page;
			GtkWidget *child;
			
		       	cur_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (contents));
			child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (contents), cur_page);
			set_active_view (bonobo_mdi, child);
		} 
		else 
		{
			set_active_view (bonobo_mdi, NULL);
		}
	
}

static void 
app_clone(BonoboMDI *bonobo_mdi, BonoboWindow *win)
{
	BonoboDockLayout *layout;
	gchar *layout_string = NULL;

	if (win != NULL) 
	{
		BonoboDock* dock = NULL;
		GList *children_list = gtk_container_get_children (GTK_CONTAINER (win));
	
		while (children_list) 
		{
		
			if (BONOBO_IS_DOCK (children_list->data))
			{
				dock = BONOBO_DOCK (children_list->data);
				break;
			}
			
			children_list = children_list->next;
		}		

		if (dock != NULL)
		{
			layout = bonobo_dock_get_layout (dock);
			if (layout)
			layout_string = bonobo_dock_layout_create_string (layout);
			g_object_unref (G_OBJECT (layout));
		}
	}

	app_create (bonobo_mdi, layout_string);

	if (layout_string)
		g_free(layout_string);
}

static gint 
app_close_top (BonoboWindow *win, GdkEventAny *event, BonoboMDI *bonobo_mdi)
{
	BonoboMDIChild *child = NULL;
	
	if (g_list_length (bonobo_mdi->priv->windows) == 1) 
	{
		if (!bonobo_mdi_remove_all (bonobo_mdi, FALSE))
			return TRUE;

		bonobo_mdi->priv->windows = g_list_remove (bonobo_mdi->priv->windows, win);
		gtk_widget_destroy (GTK_WIDGET (win));
		
		/* only destroy mdi if there are no external objects registered
		   with it. */
		if (bonobo_mdi->priv->registered == NULL)
			gtk_object_destroy (GTK_OBJECT (bonobo_mdi));
	}
	else
	{	
		if (bonobo_window_get_contents (win) != NULL) 
		{
			child = bonobo_mdi_get_child_from_view (
					bonobo_window_get_contents (win));
		
			if (g_list_length (bonobo_mdi_child_get_views (child)) == 1) 
			{
				/* if this is the last view, we have to remove the child! */
				if (!bonobo_mdi_remove_child (bonobo_mdi, child, FALSE))
					return TRUE;
			}
			else
				bonobo_mdi_remove_view (bonobo_mdi,
						bonobo_window_get_contents (win), FALSE);
		}
		else 
		{
			bonobo_mdi->priv->windows = g_list_remove (bonobo_mdi->priv->windows, win);
			gtk_widget_destroy (GTK_WIDGET (win));
		}
	}
	
	return FALSE;
}

static gint 
app_close_book (BonoboWindow *win, GdkEventAny *event, BonoboMDI *bonobo_mdi)
{
	BonoboMDIChild *child;
	GtkWidget *view;
	gint handler_ret = TRUE;
	
	if (g_list_length(bonobo_mdi->priv->windows) == 1) 
	{		
		if (!bonobo_mdi_remove_all (bonobo_mdi, FALSE))
			return TRUE;

		bonobo_mdi->priv->windows = g_list_remove (bonobo_mdi->priv->windows, win);
		gtk_widget_destroy (GTK_WIDGET (win));
		
		/* only destroy mdi if there are no non-MDI windows registered
		   with it. */
		if (bonobo_mdi->priv->registered == NULL)
			gtk_object_destroy (GTK_OBJECT (bonobo_mdi));
	}
	else 
	{
		GList *children = gtk_container_get_children (
				GTK_CONTAINER (bonobo_window_get_contents (win)));
		GList *li;

		/* first check if all the children in this notebook can be removed */
		if (children == NULL) 
		{
			bonobo_mdi->priv->windows = g_list_remove (bonobo_mdi->priv->windows, win);
			gtk_widget_destroy (GTK_WIDGET (win));
			return FALSE;
		}

		for (li = children; li != NULL; li = li->next) 
		{
			GList *node;
			view = li->data;

			child = bonobo_mdi_get_child_from_view (view);
			
			node = bonobo_mdi_child_get_views (child);
			
			while (node) 
			{
				if (bonobo_mdi_get_window_from_view (node->data) != win)
					break;
				
				node = node->next;
			}
			
			if (node == NULL) 
			{   
				/* all the views reside in this BonoboWindow */
				gtk_signal_emit(GTK_OBJECT(bonobo_mdi), mdi_signals [REMOVE_CHILD],
						child, &handler_ret);
				if (handler_ret == FALSE) 
				{
					g_list_free (children);
					return TRUE;
				}
			}
		}
		
		/* now actually remove all children/views! */
		for (li = children; li != NULL; li = li->next) 
		{
			view = li->data;

			child = bonobo_mdi_get_child_from_view (view);
			
			/* if this is the last view, remove the child */
			if (g_list_length (bonobo_mdi_child_get_views (child)) == 1)
				bonobo_mdi_remove_child (bonobo_mdi, child, TRUE);
			else
				bonobo_mdi_remove_view (bonobo_mdi, view, TRUE);
		}

		g_list_free (children);
	}
	
	return FALSE;
}

static void app_set_view 
(BonoboMDI *bonobo_mdi, BonoboWindow *win, GtkWidget *view)
{
#if 0
	GList *menu_list = NULL; 
	GList *children;
	GtkWidget *parent = NULL;
#endif
	BonoboMDIChild *child;

#if 0
	GnomeUIInfo *ui_info;
	gint pos, items;
	

	/* free previous child ui-info */
	ui_info = gtk_object_get_data(GTK_OBJECT(app), GNOME_MDI_CHILD_MENU_INFO_KEY);
	if(ui_info != NULL) {
		free_ui_info_tree(ui_info);
		gtk_object_set_data(GTK_OBJECT(app), GNOME_MDI_CHILD_MENU_INFO_KEY, NULL);
	}
	ui_info = NULL;
	
	if(bonobo_mdi->child_menu_path)
		parent = gnome_app_find_menu_pos(app->menubar, bonobo_mdi->child_menu_path, &pos);

	/* remove old child-specific menus */
	items = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(app), GNOME_MDI_ITEM_COUNT_KEY));
	if(items > 0 && parent) {
		GtkWidget *widget;

		/* remove items; should be kept in sync with gnome_app_remove_menus! */
		children = g_list_nth(GTK_MENU_SHELL(parent)->children, pos);
		while(children && items > 0) {
			widget = GTK_WIDGET(children->data);
			children = children->next;

			/* if this item contains a gtkaccellabel, we have to set its
			   accel_widget to NULL so that the item gets unrefed. */
			if(GTK_IS_ACCEL_LABEL(GTK_BIN(widget)->child))
				gtk_accel_label_set_accel_widget(GTK_ACCEL_LABEL(GTK_BIN(widget)->child), NULL);

			gtk_container_remove(GTK_CONTAINER(parent), widget);
			items--;
		}
	}
	
	items = 0;
#endif	
	
	if (view) 
	{
		child = bonobo_mdi_get_child_from_view (view);
		
		/* set the title */
		if ((bonobo_mdi->priv->mode == BONOBO_MDI_MODAL) || 
		    (bonobo_mdi->priv->mode == BONOBO_MDI_TOPLEVEL)) 
		{
			/* in MODAL and TOPLEVEL modes the window title includes the active
			   child name: "child_name - mdi_title" */
			gchar *fullname;
			gchar *name;

			name = bonobo_mdi_child_get_name (child);
			
			fullname = g_strconcat (name, " - ", bonobo_mdi->priv->title, NULL);
			gtk_window_set_title (GTK_WINDOW (win), fullname);
			
			g_free (name);
			g_free (fullname);
		}
		else
			gtk_window_set_title (GTK_WINDOW (win), bonobo_mdi->priv->title);
#if 0	
		/* create new child-specific menus */
		if(parent) {
			if( child->menu_template &&
				( (ui_info = copy_ui_info_tree(child->menu_template)) != NULL) ) {
				gnome_app_insert_menus_with_data(app, bonobo_mdi->child_menu_path, ui_info, child);
				gtk_object_set_data(GTK_OBJECT(app), GNOME_MDI_CHILD_MENU_INFO_KEY, ui_info);
				items = count_ui_info_items(ui_info);
			}
			else {
				menu_list = child_create_menus(child, view);
			
				if(menu_list) {
					GList *menu;

					items = 0;
					menu = menu_list;					
					while(menu) {
						gtk_menu_shell_insert(GTK_MENU_SHELL(parent), GTK_WIDGET(menu->data), pos);
						menu = menu->next;
						pos++;
						items++;
					}
					
					g_list_free(menu_list);
				}
				else
					items = 0;
			}
		}
#endif
	}
	else
		gtk_window_set_title (GTK_WINDOW (win), bonobo_mdi->priv->title);
#if 0
	gtk_object_set_data (GTK_OBJECT(app), GNOME_MDI_ITEM_COUNT_KEY, GINT_TO_POINTER(items));
	
	if (parent)
		gtk_widget_queue_resize (parent);
#endif
	set_active_view (bonobo_mdi, view);
}

static void 
app_destroy (BonoboWindow *win, BonoboMDI *bonobo_mdi)
{
	if (bonobo_mdi->priv->active_window == win)
		bonobo_mdi->priv->active_window = 
			(bonobo_mdi->priv->windows != NULL) ? BONOBO_WINDOW (bonobo_mdi->priv->windows->data) : NULL;
#if 0
	/* free stuff that got allocated for this BonoboWindow */

	ui_info = gtk_object_get_data(GTK_OBJECT(app), GNOME_MDI_MENUBAR_INFO_KEY);
	if(ui_info)
		free_ui_info_tree(ui_info);
	
	ui_info = gtk_object_get_data(GTK_OBJECT(app), GNOME_MDI_TOOLBAR_INFO_KEY);
	if(ui_info)
		free_ui_info_tree(ui_info);
	
	ui_info = gtk_object_get_data(GTK_OBJECT(app), GNOME_MDI_CHILD_MENU_INFO_KEY);
	if(ui_info)
		free_ui_info_tree(ui_info);
#endif
}

static void app_create (BonoboMDI *bonobo_mdi, gchar *layout_string)
{
	GtkWidget *window;
	BonoboWindow *bw;
	GtkSignalFunc func = NULL;
	gchar* config_path;
	BonoboUIContainer* ui_container = NULL;
  	BonoboUIComponent* ui_component = NULL;

#if 0
	GnomeUIInfo *ui_info;
#endif	
	window = bonobo_window_new (bonobo_mdi->priv->mdi_name, bonobo_mdi->priv->title);

	bw = BONOBO_WINDOW (window);
#if 0
	/* don't do automagical layout saving */
	app->enable_layout_config = FALSE;
#endif	
	gtk_window_set_wmclass (GTK_WINDOW (window), 
			bonobo_mdi->priv->mdi_name, bonobo_mdi->priv->mdi_name);
  
	gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);
  
	bonobo_mdi->priv->windows = g_list_append (bonobo_mdi->priv->windows, window);

	if ((bonobo_mdi->priv->mode == BONOBO_MDI_TOPLEVEL) || (bonobo_mdi->priv->mode == BONOBO_MDI_MODAL))
		func = GTK_SIGNAL_FUNC (app_close_top);
	else 
		if (bonobo_mdi->priv->mode == BONOBO_MDI_NOTEBOOK)
			func = GTK_SIGNAL_FUNC (app_close_book);
	
	gtk_signal_connect (GTK_OBJECT (window), "delete_event", 
			func, bonobo_mdi);
	gtk_signal_connect (GTK_OBJECT (window), "focus_in_event",
			GTK_SIGNAL_FUNC(toplevel_focus), bonobo_mdi);
	gtk_signal_connect (GTK_OBJECT (window), "destroy",
			GTK_SIGNAL_FUNC(app_destroy), bonobo_mdi);

	/* Create Container: */
 	ui_container = bonobo_window_get_ui_container (bw);

	config_path = g_strdup_printf ("/%s/UIConfig/kvps/", bonobo_mdi->priv->mdi_name);
  	bonobo_ui_engine_config_set_path (bonobo_window_get_ui_engine (bw),
                                     config_path);
	g_free (config_path);

	/* Create a UI component with which to communicate with the window */
	ui_component = bonobo_ui_component_new_default ();

	/* Associate the BonoboUIComponent with the container */
  	bonobo_ui_component_set_container (
        	ui_component, BONOBO_OBJREF (ui_container), NULL);

	/* set up UI */
	if (bonobo_mdi->priv->ui_xml != NULL)
	{
		bonobo_ui_component_set_translate (ui_component, 
				"/", bonobo_mdi->priv->ui_xml, NULL);
	}
	else
		if (bonobo_mdi->priv->ui_file_name)
		{
			bonobo_ui_util_set_ui (ui_component, "", bonobo_mdi->priv->ui_file_name,
				       bonobo_mdi->priv->mdi_name, NULL);
		}

	if (bonobo_mdi->priv->verbs)
		bonobo_ui_component_add_verb_list_with_data (ui_component, 
				bonobo_mdi->priv->verbs, bonobo_mdi);
	
	bonobo_mdi->priv->active_window = bw;
	bonobo_mdi->priv->active_child = NULL;
	bonobo_mdi->priv->active_view = NULL;

	gtk_object_set_data (GTK_OBJECT (bw), UI_COMPONENT_KEY, ui_component);

	gtk_signal_emit (GTK_OBJECT (bonobo_mdi), mdi_signals [TOP_WINDOW_CREATED], window);

	child_list_menu_create (bonobo_mdi, bw);
#if 0

	if (layout_string /*&& app->layout*/)
		bonobo_dock_layout_parse_string(app->layout, layout_string);
#endif
}

static void 
top_add_view (BonoboMDI *bonobo_mdi, BonoboMDIChild *child, GtkWidget *view)
{
	BonoboWindow *window;

	if (bonobo_window_get_contents (bonobo_mdi->priv->active_window) != NULL)
		app_clone (bonobo_mdi, bonobo_mdi->priv->active_window);
	
	window = bonobo_mdi->priv->active_window;

	if (child && view)
		bonobo_window_set_contents (window, view);

	app_set_view (bonobo_mdi, window, view);

	if (!GTK_WIDGET_VISIBLE (window))
		gtk_widget_show (GTK_WIDGET (window));
}

static void 
set_active_view (BonoboMDI *bonobo_mdi, GtkWidget *view)
{
	BonoboMDIChild *old_child;
	GtkWidget *old_view;

	old_child = bonobo_mdi->priv->active_child;
	old_view = bonobo_mdi->priv->active_view;

	if (!view) 
	{
		bonobo_mdi->priv->active_view = NULL;
		bonobo_mdi->priv->active_child = NULL;
	}

	if (view)
		gtk_widget_grab_focus (GTK_WIDGET (view));

	if (view == old_view)
		return;
	
	if (view) 
	{
		bonobo_mdi->priv->active_child = bonobo_mdi_get_child_from_view (view);
		bonobo_mdi->priv->active_window = bonobo_mdi_get_window_from_view (view);
	}

	bonobo_mdi->priv->active_view = view;
	
	if (bonobo_mdi->priv->active_child != old_child)
		gtk_signal_emit (GTK_OBJECT (bonobo_mdi), mdi_signals [CHILD_CHANGED], old_child);

	gtk_signal_emit (GTK_OBJECT (bonobo_mdi), mdi_signals [VIEW_CHANGED], old_view);
}

/**
 * bonobo_mdi_set_active_view:
 * @mdi: A pointer to an MDI object.
 * @view: A pointer to the view that is to become the active one.
 * 
 * Description:
 * Sets the active view to @view. It also raises the window containing it
 * and gives it focus.
 **/
void 
bonobo_mdi_set_active_view (BonoboMDI *bonobo_mdi, GtkWidget *view)
{
	GtkWindow *window;
	
	g_return_if_fail (bonobo_mdi != NULL);
	g_return_if_fail (BONOBO_IS_MDI (bonobo_mdi));
	g_return_if_fail (view != NULL);
	g_return_if_fail (GTK_IS_WIDGET (view));
	
	if (bonobo_mdi->priv->mode == BONOBO_MDI_NOTEBOOK)
		set_page_by_widget (GTK_NOTEBOOK (view->parent), view);
	
	if (bonobo_mdi->priv->mode == BONOBO_MDI_MODAL) 
	{
		if (bonobo_window_get_contents (bonobo_mdi->priv->active_window)) 
		{
			bonobo_mdi_remove_view (bonobo_mdi, 
					bonobo_window_get_contents (bonobo_mdi->priv->active_window), TRUE);
			/*
			bonobo_window_set_contents (bonobo_mdi->priv->active_window, NULL);
			*/
		}
		
		bonobo_window_set_contents (bonobo_mdi->priv->active_window, view);
		app_set_view (bonobo_mdi, bonobo_mdi->priv->active_window, view);
	}

	window = GTK_WINDOW (bonobo_mdi_get_window_from_view (view));
	
	/* TODO: hmmm... I dont know how to give focus to the window, so that it
	   would receive keyboard events */
	gdk_window_raise (GTK_WIDGET (window)->window);

	set_active_view (bonobo_mdi, view);
}

/**
 * bonobo_mdi_add_view:
 * @mdi: A pointer to a BonoboMDI object.
 * @child: A pointer to a child.
 * 
 * Description:
 * Creates a new view of the child and adds it to the MDI. BonoboMDIChild
 * @child has to be added to the MDI with a call to bonobo_mdi_add_child
 * before its views are added to the MDI. 
 * An "add_view" signal is emitted to the MDI after the view has been
 * created, but before it is shown and added to the MDI, with a pointer to
 * the created view as its parameter. The view is added to the MDI only if
 * the signal handler (if it exists) returns %TRUE. If the handler returns
 * %FALSE, the created view is destroyed and not added to the MDI. 
 * 
 * Return value:
 * %TRUE if adding the view succeeded and %FALSE otherwise.
 **/
gint 
bonobo_mdi_add_view (BonoboMDI *bonobo_mdi, BonoboMDIChild *child)
{
	GtkWidget *view;
	gint ret = TRUE;
	
	g_return_val_if_fail (bonobo_mdi != NULL, FALSE);
	g_return_val_if_fail (BONOBO_IS_MDI (bonobo_mdi), FALSE);
	g_return_val_if_fail (child != NULL, FALSE);
	g_return_val_if_fail (BONOBO_IS_MDI_CHILD (child), FALSE);
	
	if (bonobo_mdi->priv->mode != BONOBO_MDI_MODAL || 
	    bonobo_mdi_child_get_views (child) == NULL)
	{
		view = bonobo_mdi_child_add_view (child);
	}
	else 
	{
		view = GTK_WIDGET ((bonobo_mdi_child_get_views (child))->data);

		if (child == bonobo_mdi->priv->active_child)
			return TRUE;
	}

	gtk_signal_emit (GTK_OBJECT (bonobo_mdi), mdi_signals [ADD_VIEW], view, &ret);

	if (ret == FALSE) 
	{
		bonobo_mdi_child_remove_view (child, view);
		return FALSE;
	}

	if (bonobo_mdi->priv->active_window == NULL) 
	{
		app_create (bonobo_mdi, NULL);
		gtk_widget_show (GTK_WIDGET (bonobo_mdi->priv->active_window));
	}

	/* this reference will compensate the view's unrefing
	   when removed from its parent later, as we want it to
	   stay valid until removed from the child with a call
	   to bonobo_mdi_child_remove_view() */
	gtk_widget_ref (view);

	if (!GTK_WIDGET_VISIBLE (view))
		gtk_widget_show (view);

	if (bonobo_mdi->priv->mode == BONOBO_MDI_NOTEBOOK) 
	{
		if (bonobo_window_get_contents (bonobo_mdi->priv->active_window) == NULL)
			book_create (bonobo_mdi);
		book_add_view (GTK_NOTEBOOK (bonobo_window_get_contents (bonobo_mdi->priv->active_window)), view);
	}
	else 
		if (bonobo_mdi->priv->mode == BONOBO_MDI_TOPLEVEL)
		{
			/* add a new toplevel unless the remaining one is empty */
			top_add_view (bonobo_mdi, child, view);
		}
		else 
			if(bonobo_mdi->priv->mode == BONOBO_MDI_MODAL) 
			{
				/* replace the existing view if there is one */
				if  (bonobo_window_get_contents (bonobo_mdi->priv->active_window))  
				{
					bonobo_mdi_remove_view (bonobo_mdi, 
						bonobo_window_get_contents (bonobo_mdi->priv->active_window), 
						TRUE);
					/*
					bonobo_window_set_contents (bonobo_mdi->priv->active_window, NULL);
					*/
				}

				bonobo_window_set_contents (bonobo_mdi->priv->active_window, view);
				app_set_view (bonobo_mdi, bonobo_mdi->priv->active_window, view);
			}

	gtk_object_set_data (GTK_OBJECT (view), BONOBO_MDI_CHILD_KEY, child);
	
	return TRUE;
}

/**
 * bonobo_mdi_add_toplevel_view:
 * @mdi: A pointer to a BonoboMDI object.
 * @child: A pointer to a BonoboMDIChild object to be added to the MDI.
 * 
 * Description:
 * Creates a new view of the child and adds it to the MDI; it behaves the
 * same way as bonobo_mdi_add_view in %BONOBO_MDI_MODAL and %BONOBO_MDI_TOPLEVEL
 * modes, but in %BONOBO_MDI_NOTEBOOK mode, the view is added in a new
 * toplevel window unless the active one has no views in it. 
 * 
 * Return value: 
 * %TRUE if adding the view succeeded and %FALSE otherwise.
 **/
gint 
bonobo_mdi_add_toplevel_view (BonoboMDI *bonobo_mdi, BonoboMDIChild *child)
{
	GtkWidget *view;
	gint ret = TRUE;
	
	g_return_val_if_fail (bonobo_mdi != NULL, FALSE);
	g_return_val_if_fail (BONOBO_IS_MDI (bonobo_mdi), FALSE);
	g_return_val_if_fail (child != NULL, FALSE);
	g_return_val_if_fail (BONOBO_IS_MDI_CHILD (child), FALSE);
	
	if (bonobo_mdi->priv->mode != BONOBO_MDI_MODAL || 
	    bonobo_mdi_child_get_views (child) == NULL)
	{
		view = bonobo_mdi_child_add_view (child);
	}
	else 
	{
		view = GTK_WIDGET ((bonobo_mdi_child_get_views (child))->data);

		if (child == bonobo_mdi->priv->active_child)
			return TRUE;
	}

	if (!view)
		return FALSE;

	gtk_signal_emit (GTK_OBJECT (bonobo_mdi), mdi_signals [ADD_VIEW], view, &ret);

	if (ret == FALSE) 
	{
		bonobo_mdi_child_remove_view (child, view);
		return FALSE;
	}

	bonobo_mdi_open_toplevel (bonobo_mdi);

	/* this reference will compensate the view's unrefing
	   when removed from its parent later, as we want it to
	   stay valid until removed from the child with a call
	   to bonobo_mdi_child_remove_view() */
	gtk_widget_ref (view);

	if (!GTK_WIDGET_VISIBLE (view))
		gtk_widget_show (view);

	if (bonobo_mdi->priv->mode == BONOBO_MDI_NOTEBOOK)
	{
		book_add_view (GTK_NOTEBOOK(bonobo_window_get_contents (bonobo_mdi->priv->active_window)), view);
	}
	else 
		if (bonobo_mdi->priv->mode == BONOBO_MDI_TOPLEVEL)
		{
			/* add a new toplevel unless the remaining one is empty */
			top_add_view (bonobo_mdi, child, view);
		}
		else 
			if (bonobo_mdi->priv->mode == BONOBO_MDI_MODAL) 
			{
				/* replace the existing view if there is one */
				if (bonobo_window_get_contents (bonobo_mdi->priv->active_window)) 
				{
					bonobo_mdi_remove_view (bonobo_mdi, 
						bonobo_window_get_contents (bonobo_mdi->priv->active_window),
						TRUE);
					/*
					bonobo_window_set_contents (bonobo_mdi->priv->active_window, NULL);
					*/
				}

				bonobo_window_set_contents (bonobo_mdi->priv->active_window, view);
				app_set_view (bonobo_mdi, bonobo_mdi->priv->active_window, view);
			}

	gtk_object_set_data (GTK_OBJECT (view), BONOBO_MDI_CHILD_KEY, child);
	
	return TRUE;
}

/**
 * bonobo_mdi_remove_view:
 * @mdi: A pointer to a BonoboMDI object.
 * @view: View to remove.
 * @force: If TRUE, the "remove_view" signal is not emitted.
 * 
 * Description:
 * Removes a view from an MDI. 
 * A "remove_view" signal is emitted to the MDI before actually removing
 * view. The view is removed only if the signal handler (if it exists and
 * the @force is set to %FALSE) returns %TRUE. 
 * 
 * Return value: 
 * %TRUE if the view was removed and %FALSE otherwise.
 **/
gint 
bonobo_mdi_remove_view (BonoboMDI *bonobo_mdi, GtkWidget *view, gint force)
{
	GtkWidget *parent;
	BonoboWindow *window;
	BonoboMDIChild *child;
	gint ret = TRUE;
	
	g_return_val_if_fail (bonobo_mdi != NULL, FALSE);
	g_return_val_if_fail (BONOBO_IS_MDI (bonobo_mdi), FALSE);
	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (view), FALSE);

	if (!force)
		gtk_signal_emit (GTK_OBJECT (bonobo_mdi), mdi_signals [REMOVE_VIEW], view, &ret);

	if (ret == FALSE)
		return FALSE;

	child = bonobo_mdi_get_child_from_view (view);

	parent = view->parent;

	if (!parent)
		return TRUE;

	window = bonobo_mdi_get_window_from_view (view);

	gtk_container_remove (GTK_CONTAINER (parent), view);

	if (view == bonobo_mdi->priv->active_view)
	   bonobo_mdi->priv->active_view = NULL;

	if ((bonobo_mdi->priv->mode == BONOBO_MDI_TOPLEVEL) || 
	    (bonobo_mdi->priv->mode == BONOBO_MDI_MODAL)) 
	{
		/*
		bonobo_window_set_contents (window, NULL);
		*/
		/* if this is NOT the last toplevel or a registered object exists,
		   destroy the toplevel */
		if (g_list_length (bonobo_mdi->priv->windows) > 1 || 
		    bonobo_mdi->priv->registered) 
		{
			bonobo_mdi->priv->windows = g_list_remove (bonobo_mdi->priv->windows, window);
			gtk_widget_destroy (GTK_WIDGET (window));
			
			if (bonobo_mdi->priv->active_window && view == bonobo_mdi->priv->active_view)
				bonobo_mdi_set_active_view(bonobo_mdi, 
					bonobo_mdi_get_view_from_window (bonobo_mdi, bonobo_mdi->priv->active_window));
		}
		else
			app_set_view (bonobo_mdi, window, NULL);
	}
	else 
		if (bonobo_mdi->priv->mode == BONOBO_MDI_NOTEBOOK) 
		{
			if (GTK_NOTEBOOK (parent)->cur_page == NULL) 
			{
				if (g_list_length (bonobo_mdi->priv->windows) > 1 || 
				    bonobo_mdi->priv->registered) 
				{
					/* if this is NOT the last toplevel or a registered object
				   	exists, destroy the toplevel */
					bonobo_mdi->priv->windows = g_list_remove (bonobo_mdi->priv->windows, window);
					gtk_widget_destroy (GTK_WIDGET (window));
				
					if (bonobo_mdi->priv->active_window && 
					    view == bonobo_mdi->priv->active_view)
						bonobo_mdi->priv->active_view = 
							bonobo_mdi_get_view_from_window (bonobo_mdi, 
									bonobo_mdi->priv->active_window);
				}
				else
					app_set_view (bonobo_mdi, window, NULL);
			}
			else
				app_set_view (bonobo_mdi, window, 
						gtk_notebook_get_nth_page (GTK_NOTEBOOK(parent), 
							gtk_notebook_get_current_page (
								GTK_NOTEBOOK(parent))));
		}

	/* remove this view from the child's view list unless in MODAL mode */
	if(bonobo_mdi->priv->mode != BONOBO_MDI_MODAL)
		bonobo_mdi_child_remove_view (child, view);

	return TRUE;
}

static void 
child_name_changed (BonoboMDIChild *mdi_child, gchar* old_name, BonoboMDI *bonobo_mdi)
{
	bonobo_mdi_update_child (bonobo_mdi, mdi_child);
}

/**
 * bonobo_mdi_add_child:
 * @mdi: A pointer to a BonoboMDI object.
 * @child: A pointer to a BonoboMDIChild to add to the MDI.
 * 
 * Description:
 * Adds a new child to the MDI. No views are added: this has to be done with
 * a call to bonobo_mdi_add_view. 
 * First an "add_child" signal is emitted to the MDI with a pointer to the
 * child as its parameter. The child is added to the MDI only if the signal
 * handler (if it exists) returns %TRUE. If the handler returns %FALSE, the
 * child is not added to the MDI. 
 * 
 * Return value: 
 * %TRUE if the child was added successfully and %FALSE otherwise.
 **/
gint 
bonobo_mdi_add_child (BonoboMDI *bonobo_mdi, BonoboMDIChild *child)
{
	gint ret = TRUE;

	g_return_val_if_fail (bonobo_mdi != NULL, FALSE);
	g_return_val_if_fail (BONOBO_IS_MDI (bonobo_mdi), FALSE);
	g_return_val_if_fail (child != NULL, FALSE);
	g_return_val_if_fail (BONOBO_IS_MDI_CHILD (child), FALSE);

	gtk_signal_emit (GTK_OBJECT (bonobo_mdi), mdi_signals [ADD_CHILD], child, &ret);

	if (ret == FALSE)
		return FALSE;

	bonobo_mdi_child_set_parent (child, GTK_OBJECT (bonobo_mdi));

	bonobo_mdi->priv->children = g_list_append (bonobo_mdi->priv->children, child);

	gtk_signal_connect (GTK_OBJECT (child), "name_changed",
			GTK_SIGNAL_FUNC (child_name_changed), bonobo_mdi);

	child_list_menu_add_item (bonobo_mdi, child);

	gtk_object_set_data (GTK_OBJECT (child), BONOBO_MDI_KEY, bonobo_mdi);

	return TRUE;
}

/**
 * bonobo_mdi_remove_child:
 * @mdi: A pointer to a BonoboMDI object.
 * @child: Child to remove.
 * @force: If TRUE, the "remove_child" signal is not emitted
 * 
 * Description:
 * Removes a child and all of its views from the MDI. 
 * A "remove_child" signal is emitted to the MDI with @child as its parameter
 * before actually removing the child. The child is removed only if the signal
 * handler (if it exists and the @force is set to %FALSE) returns %TRUE. 
 * 
 * Return value: 
 * %TRUE if the removal was successful and %FALSE otherwise.
 **/
gint 
bonobo_mdi_remove_child (BonoboMDI *bonobo_mdi, BonoboMDIChild *child, gint force)
{
	gint ret = TRUE;
	GList *view_node;
	GtkWidget *view;

	g_return_val_if_fail (bonobo_mdi != NULL, FALSE);
	g_return_val_if_fail (BONOBO_IS_MDI (bonobo_mdi), FALSE);
	g_return_val_if_fail (child != NULL, FALSE);
	g_return_val_if_fail (BONOBO_IS_MDI_CHILD (child), FALSE);

	/* if force is set to TRUE, don't call the remove_child handler (ie there is no way for the
	   user to stop removal of the child) */
	if (!force)
		gtk_signal_emit (GTK_OBJECT (bonobo_mdi), mdi_signals [REMOVE_CHILD], child, &ret);

	if (ret == FALSE)
		return FALSE;

	view_node = bonobo_mdi_child_get_views (child);
	
	while (view_node) 
	{
		view = GTK_WIDGET (view_node->data);
		view_node = view_node->next;
		bonobo_mdi_remove_view (bonobo_mdi, GTK_WIDGET (view), TRUE);
	}

	bonobo_mdi->priv->children = g_list_remove (bonobo_mdi->priv->children, child);

	child_list_menu_remove_item (bonobo_mdi, child);

	if (child == bonobo_mdi->priv->active_child)
		bonobo_mdi->priv->active_child = NULL;

	bonobo_mdi_child_set_parent (child, NULL);

	gtk_signal_disconnect_by_func (GTK_OBJECT (child), 
			GTK_SIGNAL_FUNC (child_name_changed), bonobo_mdi);

	gtk_object_unref (GTK_OBJECT (child));

	if (bonobo_mdi->priv->mode == BONOBO_MDI_MODAL && bonobo_mdi->priv->children) 
	{
		BonoboMDIChild *next_child = bonobo_mdi->priv->children->data;

		if (bonobo_mdi_child_get_views (next_child)) 
		{
			bonobo_window_set_contents (bonobo_mdi->priv->active_window,
					GTK_WIDGET (bonobo_mdi_child_get_views (next_child)->data));
			app_set_view (bonobo_mdi, bonobo_mdi->priv->active_window,
					GTK_WIDGET (bonobo_mdi_child_get_views (next_child)->data));
		}
		else
			bonobo_mdi_add_view (bonobo_mdi, next_child);
	}

	return TRUE;
}

/**
 * bonobo_mdi_remove_all:
 * @mdi: A pointer to a BonoboMDI object.
 * @force: If TRUE, the "remove_child" signal is not emitted
 * 
 * Description:
 * Removes all children and all views from the MDI. 
 * A "remove_child" signal is emitted to the MDI for each child before
 * actually trying to remove any. If signal handlers for all children (if
 * they exist and the @force is set to %FALSE) return %TRUE, all children
 * and their views are removed and none otherwise. 
 * 
 * Return value:
 * %TRUE if the removal was successful and %FALSE otherwise.
 **/
gint 
bonobo_mdi_remove_all (BonoboMDI *bonobo_mdi, gint force)
{
	GList *child_node;
	gint handler_ret = TRUE;

	g_return_val_if_fail (bonobo_mdi != NULL, FALSE);
	g_return_val_if_fail (BONOBO_IS_MDI (bonobo_mdi), FALSE);

	/* first check if removal of any child will be prevented by the
	   remove_child signal handler */
	if (!force) 
	{
		child_node = bonobo_mdi->priv->children;
		while (child_node) 
		{
			gtk_signal_emit (GTK_OBJECT(bonobo_mdi), mdi_signals [REMOVE_CHILD],
				child_node->data, &handler_ret);

			/* if any of the children may not be removed, none will be */
			if (handler_ret == FALSE)
				return FALSE;

			child_node = child_node->next;
		}
	}

	/* remove all the children with force arg set to true so that remove_child
	   handlers are not called again */
	while (bonobo_mdi->priv->children)
		bonobo_mdi_remove_child (bonobo_mdi, BONOBO_MDI_CHILD (bonobo_mdi->priv->children->data), TRUE);

	return TRUE;
}

/**
 * bonobo_mdi_open_toplevel:
 * @mdi: A pointer to a BonoboMDI object.
 * 
 * Description:
 * Opens a new toplevel window (unless in %BONOBO_MDI_MODAL mode and a
 * toplevel window is already open). This is usually used only for opening
 * the initial window on startup (just before calling gtkmain()) if no
 * windows were open because a session was restored or children were added
 * because of command line args).
 **/
void 
bonobo_mdi_open_toplevel (BonoboMDI *bonobo_mdi)
{
	g_return_if_fail (bonobo_mdi != NULL);
	g_return_if_fail (BONOBO_IS_MDI (bonobo_mdi));

	if (bonobo_mdi->priv->mode != BONOBO_MDI_MODAL || 
	    bonobo_mdi->priv->windows == NULL) 
	{
		app_clone (bonobo_mdi, bonobo_mdi->priv->active_window);

		if (bonobo_mdi->priv->mode == BONOBO_MDI_NOTEBOOK)
			book_create (bonobo_mdi);
		
		gtk_widget_show (GTK_WIDGET (bonobo_mdi->priv->active_window));
	}
}

/**
 * bonobo_mdi_update_child:
 * @mdi: A pointer to a BonoboMDI object.
 * @child: Child to update.
 * 
 * Description:
 * Updates all notebook labels of @child's views and their window titles
 * after its name changes. It is not required if bonobo_mdi_child_set_name()
 * is used for setting the child's name.
 **/
void 
bonobo_mdi_update_child (BonoboMDI *bonobo_mdi, BonoboMDIChild *child)
{
	GtkWidget *view, *title;
	GList *view_node;
	GList *win_node;
	gchar* child_name, *path, *path_cmd, *tip, *escaped_name;

	g_return_if_fail (bonobo_mdi != NULL);
	g_return_if_fail (BONOBO_IS_MDI (bonobo_mdi));
	g_return_if_fail (child != NULL);
	g_return_if_fail (BONOBO_IS_MDI_CHILD (child));

	view_node = bonobo_mdi_child_get_views (child);
	
	while (view_node) 
	{
		view = GTK_WIDGET (view_node->data);

		/* for the time being all that update_child() does is update the
		   children's names */
		if ((bonobo_mdi->priv->mode == BONOBO_MDI_MODAL) ||
		    (bonobo_mdi->priv->mode == BONOBO_MDI_TOPLEVEL)) 
		{
			/* in MODAL and TOPLEVEL modes the window title includes the active
			   child name: "child_name - mdi_title" */
			gchar *fullname;
			gchar *name;

			name = bonobo_mdi_child_get_name (child);
      
			fullname = g_strconcat (name, " - ", bonobo_mdi->priv->title, NULL);
			gtk_window_set_title (GTK_WINDOW (bonobo_mdi_get_window_from_view (view)),
						fullname);
			
			g_free (name);
			g_free (fullname);
		}
		else 
			if(bonobo_mdi->priv->mode == BONOBO_MDI_NOTEBOOK) 
			{
				GtkWidget *tab_label;

				tab_label = gtk_notebook_get_tab_label
					(GTK_NOTEBOOK (view->parent), view);
				if (tab_label != NULL)
					title = child_set_label (child, tab_label);
			}
		
		view_node = g_list_next (view_node);
	}

	/* Update child list menus */	
	if(bonobo_mdi->priv->child_list_path == NULL)
		return;
	
	win_node = bonobo_mdi->priv->windows;

	child_name = bonobo_mdi_child_get_name (child);
	escaped_name = escape_underscores (child_name);
	path = g_strdup_printf ("%sChild_%p", bonobo_mdi->priv->child_list_path, child);
	path_cmd =  g_strdup_printf ("/commands/Child_%p", child);
	tip = g_strdup_printf (_("Activate %s"), child_name);
			
	while (win_node) 
	{
		BonoboUIComponent *ui_component;
		
		ui_component = BONOBO_UI_COMPONENT (
			gtk_object_get_data (GTK_OBJECT (win_node->data), UI_COMPONENT_KEY));

		bonobo_ui_component_set_prop (ui_component, path, "label", escaped_name, NULL);
		bonobo_ui_component_set_prop (ui_component, path, "tip", tip, NULL);

		win_node = g_list_next (win_node);
	}

	g_free (escaped_name);
	g_free (path);
	g_free (path_cmd);
	g_free (tip);
	g_free (child_name);
}

/**
 * bonobo_mdi_find_child:
 * @mdi: A pointer to a BonoboMDI object.
 * @name: A string with a name of the child to find.
 * 
 * Description:
 * Finds a child named @name.
 * 
 * Return value: 
 * A pointer to the BonoboMDIChild object if the child was found and NULL
 * otherwise.
 **/
BonoboMDIChild *
bonobo_mdi_find_child (BonoboMDI *bonobo_mdi, const gchar *name)
{
	GList *child_node;

	g_return_val_if_fail (bonobo_mdi != NULL, NULL);
	g_return_val_if_fail (BONOBO_IS_MDI (bonobo_mdi), NULL);

	child_node = bonobo_mdi->priv->children;
	while (child_node) 
	{
		gchar* child_name = bonobo_mdi_child_get_name (BONOBO_MDI_CHILD (child_node->data));
		 
		if (strcmp (child_name, name) == 0)
		{
			g_free (child_name);	
			return BONOBO_MDI_CHILD (child_node->data);
		}
		
		g_free (child_name);
		
		child_node = g_list_next (child_node);
	}

	return NULL;
}


/**
 * bonobo_mdi_set_mode:
 * @mdi: A pointer to a BonoboMDI object.
 * @mode: New mode.
 *
 * Description:
 * Sets the MDI mode to mode. Possible values are %BONOBO_MDI_TOPLEVEL,
 * %BONOBO_MDI_NOTEBOOK, %BONOBO_MDI_MODAL and %BONOBO_MDI_DEFAULT.
 **/
void 
bonobo_mdi_set_mode (BonoboMDI *bonobo_mdi, BonoboMDIMode mode)
{
	GtkWidget *view;
	BonoboMDIChild *child;
	GList *child_node, *view_node, *app_node;
	gint windows = (bonobo_mdi->priv->windows != NULL);
	guint16 width = 0, height = 0;

	g_return_if_fail(bonobo_mdi != NULL);
	g_return_if_fail(BONOBO_IS_MDI(bonobo_mdi));

	 
	if (mode == BONOBO_MDI_DEFAULT_MODE)
		/*
		mode = bonobo_preferences_get_mdi_mode();
		*/
		mode = BONOBO_MDI_NOTEBOOK;

	if (bonobo_mdi->priv->active_view) 
	{
		width  = bonobo_mdi->priv->active_view->allocation.width;
		height = bonobo_mdi->priv->active_view->allocation.height;
	}

	/* remove all views from their parents */
	child_node = bonobo_mdi->priv->children;
	while (child_node) 
	{
		child = BONOBO_MDI_CHILD (child_node->data);
		view_node = bonobo_mdi_child_get_views (child);
		while (view_node) 
		{
			view = GTK_WIDGET (view_node->data);

			if (view->parent) 
			{
				/*
				if ((bonobo_mdi->priv->mode == BONOBO_MDI_TOPLEVEL) ||
				    (bonobo_mdi->priv->mode == BONOBO_MDI_MODAL) )
					bonobo_window_set_contents (
							bonobo_mdi_get_window_from_view (view), NULL);
				*/
				gtk_container_remove (GTK_CONTAINER (view->parent), view);
			}

			view_node = view_node->next;

			/* if we are to change mode to MODAL, destroy all views except
			   the active one */
			/* if( (mode == BONOBO_MDI_MODAL) && (view != bonobo_mdi->active_view) )
			   bonobo_mdi_child_remove_view(child, view); */
		}
		
		child_node = child_node->next;
	}

	/* remove all BonoboWindows but the active one */
	app_node = bonobo_mdi->priv->windows;
	while (app_node) 
	{
		if (BONOBO_WINDOW (app_node->data) != bonobo_mdi->priv->active_window)
			gtk_widget_destroy (GTK_WIDGET (app_node->data));
		app_node = app_node->next;
	}

	if (bonobo_mdi->priv->windows)
		g_list_free(bonobo_mdi->priv->windows);

	if (bonobo_mdi->priv->active_window) 
	{
#if 0
		if (bonobo_mdi->priv->mode == BONOBO_MDI_NOTEBOOK)
			gtk_container_remove (GTK_CONTAINER (bonobo_mdi->priv->active_window->dock),
					     BONOBO_DOCK(bonobo_mdi->active_window->dock)->client_area);
#endif
		/*
		bonobo_window_set_contents (bonobo_mdi->priv->active_window, NULL);
		*/
		if ((bonobo_mdi->priv->mode == BONOBO_MDI_TOPLEVEL) || 
		    (bonobo_mdi->priv->mode == BONOBO_MDI_MODAL))
			gtk_signal_disconnect_by_func (GTK_OBJECT (bonobo_mdi->priv->active_window),
						GTK_SIGNAL_FUNC(app_close_top), bonobo_mdi);
		else 
			if(bonobo_mdi->priv->mode == BONOBO_MDI_NOTEBOOK)
				gtk_signal_disconnect_by_func (GTK_OBJECT (bonobo_mdi->priv->active_window),
							GTK_SIGNAL_FUNC (app_close_book), bonobo_mdi);

		if ((mode == BONOBO_MDI_TOPLEVEL) || (mode == BONOBO_MDI_MODAL))
			gtk_signal_connect (GTK_OBJECT (bonobo_mdi->priv->active_window), "delete_event",
					GTK_SIGNAL_FUNC (app_close_top), bonobo_mdi);
		else 
			if(mode == BONOBO_MDI_NOTEBOOK)
				gtk_signal_connect (GTK_OBJECT (bonobo_mdi->priv->active_window), "delete_event",
							   GTK_SIGNAL_FUNC (app_close_book), bonobo_mdi);

		bonobo_mdi->priv->windows = g_list_append (NULL, bonobo_mdi->priv->active_window);

		if (mode == BONOBO_MDI_NOTEBOOK) 
			book_create (bonobo_mdi);
	}

	bonobo_mdi->priv->mode = mode;

	/* re-implant views in proper containers */
	child_node = bonobo_mdi->priv->children;
	while (child_node) 
	{
		child = BONOBO_MDI_CHILD (child_node->data);
		view_node = bonobo_mdi_child_get_views (child);
		
		while (view_node) 
		{
			view = GTK_WIDGET(view_node->data);

			if(width != 0)
				gtk_widget_set_usize (view, width, height);

			if(bonobo_mdi->priv->mode == BONOBO_MDI_NOTEBOOK)
				book_add_view (GTK_NOTEBOOK (
					bonobo_window_get_contents (bonobo_mdi->priv->active_window)), view);
			else 
				if(bonobo_mdi->priv->mode == BONOBO_MDI_TOPLEVEL)
					/* add a new toplevel unless the remaining one is empty */
					top_add_view (bonobo_mdi, child, view);
				else 
					if (bonobo_mdi->priv->mode == BONOBO_MDI_MODAL) 
					{
						/* replace the existing view if there is one */
						if (bonobo_window_get_contents (bonobo_mdi->priv->active_window))
						{
							bonobo_mdi_remove_view (bonobo_mdi, 
								bonobo_window_get_contents (
									bonobo_mdi->priv->active_window),
								TRUE);
							/*
							bonobo_window_set_contents (
									bonobo_mdi->priv->active_window, NULL);
							*/
						}
				
						bonobo_window_set_contents (bonobo_mdi->priv->active_window, view);
						app_set_view (bonobo_mdi, bonobo_mdi->priv->active_window, view);

						bonobo_mdi->priv->active_view = view;
					}

			view_node = view_node->next;

			gtk_widget_show (GTK_WIDGET (bonobo_mdi->priv->active_window));
		}
		
		child_node = child_node->next;
	}

	if(windows && !bonobo_mdi->priv->active_window)
		bonobo_mdi_open_toplevel (bonobo_mdi);
}

/**
 * bonobo_mdi_get_active_child:
 * @mdi: A pointer to a BonoboMDI object.
 * 
 * Description:
 * Returns a pointer to the active BonoboMDIChild object.
 * 
 * Return value: 
 * A pointer to the active BonoboMDIChild object. %NULL, if there is none.
 **/
BonoboMDIChild *
bonobo_mdi_get_active_child (BonoboMDI *bonobo_mdi)
{
	g_return_val_if_fail (bonobo_mdi != NULL, NULL);
	g_return_val_if_fail (BONOBO_IS_MDI (bonobo_mdi), NULL);

	if (bonobo_mdi->priv->active_view)
		return (bonobo_mdi_get_child_from_view (bonobo_mdi->priv->active_view));

	return NULL;
}

/**
 * bonobo_mdi_get_active_view:
 * @mdi: A pointer to a BonoboMDI object.
 * 
 * Description:
 * Returns a pointer to the active view (the one with the focus).
 * 
 * Return value: 
 * A pointer to a GtkWidget *.
 **/
GtkWidget *
bonobo_mdi_get_active_view (BonoboMDI *bonobo_mdi)
{
	g_return_val_if_fail (bonobo_mdi != NULL, NULL);
	g_return_val_if_fail (BONOBO_IS_MDI (bonobo_mdi), NULL);

	return bonobo_mdi->priv->active_view;
}

/**
 * bonobo_mdi_get_active_window:
 * @mdi: A pointer to a BonoboMDI object.
 * 
 * Description:
 * Returns a pointer to the toplevel window containing the active view.
 * 
 * Return value:
 * A pointer to a BonoboWindow that has the focus.
 **/
BonoboWindow *
bonobo_mdi_get_active_window (BonoboMDI *bonobo_mdi)
{
	g_return_val_if_fail (bonobo_mdi != NULL, NULL);
	g_return_val_if_fail (BONOBO_IS_MDI (bonobo_mdi), NULL);

	return bonobo_mdi->priv->active_window;
}

void 
bonobo_mdi_set_ui_template (BonoboMDI *bonobo_mdi, const gchar *xml, BonoboUIVerb verbs[])
{
	g_return_if_fail (bonobo_mdi != NULL);
	g_return_if_fail (BONOBO_IS_MDI (bonobo_mdi));
	g_return_if_fail (xml != NULL);

	if (bonobo_mdi->priv->ui_xml != NULL)
	       g_free (bonobo_mdi->priv->ui_xml);
	
	bonobo_mdi->priv->ui_xml = g_strdup (xml);

	/* FIXME */
	bonobo_mdi->priv->verbs = verbs;
}

void          
bonobo_mdi_set_ui_template_file (BonoboMDI *bonobo_mdi, const gchar *file_name, BonoboUIVerb verbs[])
{
	g_return_if_fail (bonobo_mdi != NULL);
	g_return_if_fail (BONOBO_IS_MDI (bonobo_mdi));
	g_return_if_fail (file_name != NULL);

	if (bonobo_mdi->priv->ui_file_name != NULL)
		g_free (bonobo_mdi->priv->ui_file_name);
	
	bonobo_mdi->priv->ui_file_name = g_strdup (file_name);

	/* FIXME */
	bonobo_mdi->priv->verbs = verbs;
}

/**
 * bonobo_mdi_set_child_list_path:
 * @mdi: A pointer to a BonoboMDI object.
 * @path: A menu path where the child list menu should be inserted
 * 
 * Description:
 * Sets the position for insertion of menu items used to activate the MDI
 * children that were added to the MDI. See gnome_app_find_menu_pos for
 * details on menu paths. If the path is not set or set to %NULL, these menu
 * items aren't going to be inserted in the MDI menu structure. Note that if
 * you want all menu items to be inserted in their own submenu, you have to
 * create that submenu (and leave it empty, of course).
 **/
void 
bonobo_mdi_set_child_list_path (BonoboMDI *bonobo_mdi, const gchar *path)
{
	g_return_if_fail (bonobo_mdi != NULL);
	g_return_if_fail (BONOBO_IS_MDI (bonobo_mdi));

	if (bonobo_mdi->priv->child_list_path)
		g_free (bonobo_mdi->priv->child_list_path);

	bonobo_mdi->priv->child_list_path = g_strdup (path);
}

/**
 * bonobo_mdi_register:
 * @mdi: A pointer to a BonoboMDI object.
 * @object: Object to register.
 * 
 * Description:
 * Registers GtkObject @object with MDI. 
 * This is mostly intended for applications that open other windows besides
 * those opened by the MDI and want to continue to run even when no MDI
 * windows exist (an example of this would be GIMP's window with tools, if
 * the pictures were MDI children). As long as there is an object registered
 * with the MDI, the MDI will not destroy itself when the last of its windows
 * is closed. If no objects are registered, closing the last MDI window
 * results in MDI being destroyed. 
 **/
void 
bonobo_mdi_register (BonoboMDI *bonobo_mdi, GtkObject *object)
{
	if (!g_slist_find (bonobo_mdi->priv->registered, object))
		bonobo_mdi->priv->registered = g_slist_append (bonobo_mdi->priv->registered, object);
}

/**
 * bonobo_mdi_unregister:
 * @mdi: A pointer to a BonoboMDI object.
 * @object: Object to unregister.
 * 
 * Description:
 * Removes GtkObject @object from the list of registered objects. 
 **/
void 
bonobo_mdi_unregister (BonoboMDI *bonobo_mdi, GtkObject *object)
{
	bonobo_mdi->priv->registered = g_slist_remove (bonobo_mdi->priv->registered, object);
}

/**
 * bonobo_mdi_get_child_from_view:
 * @view: A pointer to a GtkWidget.
 * 
 * Description:
 * Returns a child that @view is a view of.
 * 
 * Return value:
 * A pointer to the BonoboMDIChild the view belongs to.
 **/
BonoboMDIChild *
bonobo_mdi_get_child_from_view (GtkWidget *view)
{
	return BONOBO_MDI_CHILD (gtk_object_get_data (GTK_OBJECT(view), BONOBO_MDI_CHILD_KEY));
}

/**
 * bonobo_mdi_get_window_from_view:
 * @view: A pointer to a GtkWidget.
 * 
 * Description:
 * Returns the toplevel window for this view.
 * 
 * Return value:
 * A pointer to the BonoboWindow containg the specified view.
 **/
BonoboWindow *
bonobo_mdi_get_window_from_view (GtkWidget *view)
{
	return BONOBO_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view)));
}

/**
 * bonobo_mdi_get_view_from_window:
 * @mdi: A pointer to a BonoboMDI object.
 * @win: A pointer to a BonoboWindow widget.
 * 
 * Description:
 * Returns the pointer to the view in the MDI toplevel window @win.
 * If the mode is set to %GNOME_MDI_NOTEBOOK, the view in the current
 * page is returned.
 * 
 * Return value: 
 * A pointer to a view.
 **/
GtkWidget *
bonobo_mdi_get_view_from_window (BonoboMDI *bonobo_mdi, BonoboWindow *win)
{
	g_return_val_if_fail (bonobo_mdi != NULL, NULL);
	g_return_val_if_fail (BONOBO_IS_MDI (bonobo_mdi), NULL);
	g_return_val_if_fail (win != NULL, NULL);
	g_return_val_if_fail (BONOBO_IS_WINDOW (win), NULL);

	if ((bonobo_mdi->priv->mode == BONOBO_MDI_TOPLEVEL) || 
	    (bonobo_mdi->priv->mode == BONOBO_MDI_MODAL)) 
	{
		return bonobo_window_get_contents (win);
	} 
	else 
		if ((bonobo_mdi->priv->mode == BONOBO_MDI_NOTEBOOK) &&
		    GTK_NOTEBOOK (bonobo_window_get_contents (win))->cur_page) 
		{
			int cur_page = gtk_notebook_get_current_page (
					GTK_NOTEBOOK (bonobo_window_get_contents (win)));
		
			return gtk_notebook_get_nth_page (
					GTK_NOTEBOOK (bonobo_window_get_contents (win)), cur_page);
		} 
		else 
		{
			return NULL;
		}
}

BonoboMDIMode 
bonobo_mdi_get_mode (BonoboMDI *bonobo_mdi)
{
	return bonobo_mdi->priv->mode;
}

void 
bonobo_mdi_construct (BonoboMDI *bonobo_mdi, gchar* name, gchar* title, GtkPositionType tab_pos)
{
	g_return_if_fail (bonobo_mdi->priv->mdi_name == NULL);
	g_return_if_fail (bonobo_mdi->priv->title == NULL);

	if (name != NULL)
		bonobo_mdi->priv->mdi_name = g_strdup (name);

	if (title != NULL)
		bonobo_mdi->priv->title = g_strdup (title);

	bonobo_mdi->priv->tab_pos = tab_pos;
}

GList *
bonobo_mdi_get_children (BonoboMDI *bonobo_mdi)
{
	g_return_val_if_fail (BONOBO_IS_MDI (bonobo_mdi), FALSE);

	return bonobo_mdi->priv->children;
}

GList *
bonobo_mdi_get_windows (BonoboMDI *bonobo_mdi)
{
	g_return_val_if_fail (BONOBO_IS_MDI (bonobo_mdi), FALSE);

	return bonobo_mdi->priv->windows;
}

	

BonoboUIComponent*
bonobo_mdi_get_ui_component_from_window (BonoboWindow* win)
{
	return BONOBO_UI_COMPONENT (
			gtk_object_get_data (GTK_OBJECT (win), UI_COMPONENT_KEY));
}

static gint
remove_message_timeout (MessageInfo * mi)
{
	BonoboUIEngine *ui_engine;

	GDK_THREADS_ENTER ();

	ui_engine = bonobo_window_get_ui_engine (mi->win);
	g_return_val_if_fail (ui_engine != NULL, FALSE);

	bonobo_ui_engine_remove_hint (ui_engine);

	g_free (mi);

	GDK_THREADS_LEAVE ();

	return FALSE; /* removes the timeout */
}

/* Called if the win is destroyed before the timeout occurs. */
static void
remove_timeout_cb (GtkWidget *win, MessageInfo *mi )
{
	g_return_if_fail (mi != NULL);

	gtk_timeout_remove (mi->timeoutid);
	g_free (mi);
}

static const guint32 flash_length = 3000; /* 3 seconds, I hope */


/**
 * bonobo_win_set_status
 * @win: Pointer a Bonobo window object.
 * @msg: Text of message to be shown on the status bar.
 *
 * Description:
 * Show the message in the status bar; if no status bar do nothing.
 * -- SnM
 */
void
bonobo_window_show_status (BonoboWindow *win, const gchar *msg)
{
	BonoboUIEngine *ui_engine;
	g_return_if_fail (win != NULL);
	g_return_if_fail (BONOBO_IS_WINDOW (win));
	g_return_if_fail (msg != NULL);

	ui_engine = bonobo_window_get_ui_engine (win);
	g_return_if_fail (ui_engine != NULL);

	if (bonobo_ui_engine_xml_node_exists (ui_engine, "/status"))
	{
		bonobo_ui_engine_remove_hint (ui_engine);
		bonobo_ui_engine_add_hint (ui_engine, msg);
	}

	/* Update UI */
	while (gtk_events_pending ())
		gtk_main_iteration ();
}



/**
 * bonobo_win_flash
 * @app: Pointer a Bonobo window object
 * @flash: Text of message to be flashed
 *
 * Description:
 * Flash the message in the statusbar for a few moments; if no
 * statusbar, do nothing. For trivial little status messages,
 * e.g. "Auto saving..."
 **/

void
bonobo_window_flash (BonoboWindow * win, const gchar * flash)
{
	BonoboUIEngine *ui_engine;
	g_return_if_fail (win != NULL);
	g_return_if_fail (BONOBO_IS_WINDOW (win));
	g_return_if_fail (flash != NULL);

	ui_engine = bonobo_window_get_ui_engine (win);
	g_return_if_fail (ui_engine != NULL);

	if (bonobo_ui_engine_xml_node_exists (ui_engine, "/status"))
	{
		MessageInfo * mi;

		mi = g_new (MessageInfo, 1);

		bonobo_ui_engine_remove_hint (ui_engine);
		bonobo_ui_engine_add_hint (ui_engine, flash);

		mi->timeoutid =
			gtk_timeout_add (flash_length,
				(GtkFunction) remove_message_timeout,
				mi);

		mi->handlerid =
			gtk_signal_connect (GTK_OBJECT(win),
				"destroy",
				GTK_SIGNAL_FUNC (remove_timeout_cb),
				mi );

		mi->win       = win;
	}

	/* Update UI */
	while (gtk_events_pending ())
		gtk_main_iteration ();
}
