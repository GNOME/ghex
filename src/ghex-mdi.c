/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * ghex-mdi.c
 * This file is part of ghex
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001, 2002 Chema Celorio, Paolo Maggi, Jaka Mocnik
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
 */
 
/*
 * Modified by the ghex Team, 1998-2001. See the AUTHORS file for a 
 * list of people on the ghex Team.  
 * See the ChangeLog files for a list of changes. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomevfs/gnome-vfs.h>

#include "ghex-mdi.h"
#include "ghex.h"

#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-ui-component.h>

#define MDI_KEY "MDI"

struct _GhexMDIPrivate
{
	gint untitled_number;
};

static void ghex_mdi_class_init 	(GhexMDIClass	*klass);
static void ghex_mdi_init 		(GhexMDI 	*mdi);
static void ghex_mdi_finalize 		(GObject 	*object);

static void ghex_mdi_app_created_handler	(BonoboMDI *mdi, BonoboWindow *win);
static void ghex_mdi_drag_data_received_handler (GtkWidget *widget, GdkDragContext *context, 
						 gint x, gint y, 
						 GtkSelectionData *selection_data, 
						 guint info, guint time, gpointer data);

#ifdef SNM /* Do we need this for ghex2? -- SnM */
static void ghex_mdi_set_app_toolbar_style 	(BonoboWindow *win);
#endif

static void ghex_mdi_set_app_statusbar_style 	(BonoboWindow *win);

static gint ghex_mdi_add_child_handler (BonoboMDI *mdi, BonoboMDIChild *child);
static gint ghex_mdi_add_view_handler (BonoboMDI *mdi, GtkWidget *view);
static gint ghex_mdi_remove_child_handler (BonoboMDI *mdi, BonoboMDIChild *child);
static gint ghex_mdi_remove_view_handler (BonoboMDI *mdi, GtkWidget *view);

static void ghex_mdi_view_changed_handler (BonoboMDI *mdi, GtkWidget *old_view);
static void ghex_mdi_child_changed_handler (BonoboMDI *mdi, BonoboMDIChild *old_child);
static void ghex_mdi_child_state_changed_handler (HexDocument *child);

static void ghex_mdi_set_active_window_undo_redo_verbs_sensitivity (BonoboMDI *mdi);

static void ghex_mdi_listener(BonoboUIComponent           *uic,
			      const char                  *path,
			      Bonobo_UIComponent_EventType type,
			      const char                  *state,
			      gpointer                     user_data);

static void cursor_moved_cb (GtkHex *gtkhex);

static BonoboMDIClass *parent_class = NULL;

GType
ghex_mdi_get_type (void)
{
	static GType mdi_type = 0;

  	if (mdi_type == 0)
    	{
      		static const GTypeInfo our_info =
      		{
        		sizeof (GhexMDIClass),
        		NULL,		/* base_init */
        		NULL,		/* base_finalize */
        		(GClassInitFunc) ghex_mdi_class_init,
        		NULL,           /* class_finalize */
        		NULL,           /* class_data */
        		sizeof (GhexMDI),
        		0,              /* n_preallocs */
        		(GInstanceInitFunc) ghex_mdi_init
      		};

      		mdi_type = g_type_register_static (BONOBO_TYPE_MDI,
                				    "GhexMDI",
                                       	 	    &our_info,
                                       		    0);
    	}

	return mdi_type;
}

static void
ghex_mdi_class_init (GhexMDIClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

  	parent_class = g_type_class_peek_parent (klass);

  	object_class->finalize = ghex_mdi_finalize;
}

static void 
ghex_mdi_init (GhexMDI  *mdi)
{
	bonobo_mdi_construct (BONOBO_MDI (mdi), "ghex2", "ghex", GTK_POS_TOP/*FIXME: settings->tab_pos*/);
	
	mdi->priv = g_new0 (GhexMDIPrivate, 1);

	mdi->priv->untitled_number = 0;	

	bonobo_mdi_set_ui_template_file (BONOBO_MDI (mdi), GHEX_UI_DIR "ghex-ui.xml", ghex_verbs);
	
	bonobo_mdi_set_child_list_path (BONOBO_MDI (mdi), "/menu/Files/");

	bonobo_mdi_set_mode (BONOBO_MDI (mdi), mdi_mode);

	/* Connect signals */
	gtk_signal_connect (GTK_OBJECT (mdi), "top_window_created",
			    GTK_SIGNAL_FUNC (ghex_mdi_app_created_handler), NULL);
	gtk_signal_connect (GTK_OBJECT (mdi), "add_child",
			    GTK_SIGNAL_FUNC (ghex_mdi_add_child_handler), NULL);
	gtk_signal_connect (GTK_OBJECT (mdi), "add_view",
			    GTK_SIGNAL_FUNC (ghex_mdi_add_view_handler), NULL);
	gtk_signal_connect (GTK_OBJECT (mdi), "remove_child",
			    GTK_SIGNAL_FUNC (remove_doc_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (mdi), "remove_view",
			    GTK_SIGNAL_FUNC (ghex_mdi_remove_view_handler), NULL);

	gtk_signal_connect (GTK_OBJECT (mdi), "child_changed",
			    GTK_SIGNAL_FUNC (ghex_mdi_child_changed_handler), NULL);
	gtk_signal_connect (GTK_OBJECT (mdi), "view_changed",
			    GTK_SIGNAL_FUNC (ghex_mdi_view_changed_handler), NULL);

	gtk_signal_connect (GTK_OBJECT (mdi), "destroy",
			    GTK_SIGNAL_FUNC (quit_app_cb), NULL);

}

static void
ghex_mdi_finalize (GObject *object)
{
	GhexMDI *mdi;

	g_return_if_fail (object != NULL);
	
   	mdi = GHEX_MDI (object);

	g_return_if_fail (GHEX_IS_MDI (mdi));
	g_return_if_fail (mdi->priv != NULL);

	G_OBJECT_CLASS (parent_class)->finalize (object);

	g_free (mdi->priv);
}

/**
 * ghex_mdi_new:
 * 
 * Creates a new #GhexMDI object.
 *
 * Return value: a new #GhexMDI
 **/
GhexMDI*
ghex_mdi_new (void)
{
	GhexMDI *mdi;

	mdi = GHEX_MDI (g_object_new (GHEX_TYPE_MDI, NULL));
  	g_return_val_if_fail (mdi != NULL, NULL);
	
	return mdi;
}

static void
ghex_mdi_listener (BonoboUIComponent           *uic,
		   const char                  *path,
		   Bonobo_UIComponent_EventType type,
		   const char                  *state,
		   gpointer                     user_data)
{
	BonoboWindow *win;
	GtkWidget *view;
	BonoboMDI *mdi;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (BONOBO_IS_WINDOW (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	win = BONOBO_WINDOW(user_data);

	mdi = BONOBO_MDI(g_object_get_data(G_OBJECT(win), MDI_KEY));

	view = bonobo_mdi_get_view_from_window(mdi, win);

	if (!strcmp (path, "InsertMode")) {
		gtk_hex_set_insert_mode(GTK_HEX(view), *state == '1');
		return;
	}

	if (!state || !atoi (state))
		return;

	if (!strcmp (path, "Bytes"))
		gtk_hex_set_group_type(GTK_HEX(view), GROUP_BYTE);
	else if (!strcmp (path, "Words"))
		gtk_hex_set_group_type(GTK_HEX(view), GROUP_WORD);
	else if (!strcmp (path, "Longwords"))
		gtk_hex_set_group_type(GTK_HEX(view), GROUP_LONG);
	else {
		g_warning("Unknown event: '%s'.", path);
	}
}

static void
ghex_mdi_app_created_handler (BonoboMDI *mdi, BonoboWindow *win)
{
	BonoboUIComponent *uic;

	static GtkTargetEntry drag_types[] =
	{
		{ "text/uri-list", 0, 0 },
	};

	static gint n_drag_types = sizeof (drag_types) / sizeof (drag_types [0]);

	/* Drag and drop support */
	gtk_drag_dest_set (GTK_WIDGET (win),
			   GTK_DEST_DEFAULT_MOTION |
			   GTK_DEST_DEFAULT_HIGHLIGHT |
			   GTK_DEST_DEFAULT_DROP,
			   drag_types, n_drag_types,
			   GDK_ACTION_COPY);
		
	g_signal_connect (G_OBJECT (win), "drag_data_received",
			  G_CALLBACK (ghex_mdi_drag_data_received_handler), 
			  mdi);

	ghex_mdi_set_app_statusbar_style (win);

#ifdef SNM /* ghex didnt have this. Do we want this? -- SnM */	
	/* Set the toolbar style according to prefs */
	ghex_mdi_set_app_toolbar_style (win);
#endif
		
	/* Set the window prefs. */
	/* gtk_window_set_policy () has been deprecated -- SnM */
	gtk_window_set_policy (GTK_WINDOW (win), TRUE, TRUE, FALSE);
	gtk_window_set_resizable (GTK_WINDOW (win), TRUE);

	uic = bonobo_mdi_get_ui_component_from_window(win);

	bonobo_ui_component_add_listener (uic, "Bytes",
					  ghex_mdi_listener, win);
	bonobo_ui_component_add_listener (uic, "Words",
					  ghex_mdi_listener, win);
	bonobo_ui_component_add_listener (uic, "Longwords",
					  ghex_mdi_listener, win);
	bonobo_ui_component_add_listener (uic, "InsertMode",
					  ghex_mdi_listener, win);

#ifdef SNM
	gtk_window_set_default_size (GTK_WINDOW (win), settings->width, settings->height);
	gtk_window_set_default_size (GTK_WINDOW (win), 300, 300 );
	gtk_window_set_policy (GTK_WINDOW (win), TRUE, TRUE, FALSE);
#endif
	g_object_set_data(G_OBJECT(win), MDI_KEY, mdi);
}

static void 
ghex_mdi_drag_data_received_handler (GtkWidget *widget, GdkDragContext *context, 
				     gint x, gint y, GtkSelectionData *selection_data, 
				     guint info, guint time, gpointer data)
{
	gchar **uris_to_open, **uri;
	GhexMDI *mdi = GHEX_MDI(data);
	HexDocument *newdoc;

        uris_to_open = g_strsplit(selection_data->data, "\r\n", 0);

	uri = uris_to_open;

	while(*uri) {
		if(!g_strncasecmp("file:", *uri, 5)) {
			newdoc = hex_document_new((*uri) + 5);
			if(newdoc) {
				bonobo_mdi_add_child(BONOBO_MDI (mdi), BONOBO_MDI_CHILD(newdoc));
				bonobo_mdi_add_view(BONOBO_MDI (mdi), BONOBO_MDI_CHILD(newdoc));
			}
		}
		uri++;
	}
	g_strfreev(uris_to_open);
}

#ifdef SNM /* We dont need this for ghex2. Atleast for now -- SnM */
static void
ghex_mdi_set_app_toolbar_style (BonoboWindow *win)
{
	BonoboUIComponent *uic;
	
	g_return_if_fail (BONOBO_IS_WINDOW (win));
			
	uic = bonobo_mdi_get_ui_component_from_window(win);

#ifdef SNM	
	if (!settings->have_toolbar) {
		bonobo_ui_component_set_prop (uic, "/Toolbar",
					      "hidden", "1", NULL);
		return;
	}
#endif
	
	bonobo_ui_component_freeze (uic);

	bonobo_ui_component_set_prop (uic, "/Toolbar",
				      "hidden", "0", NULL);

#ifdef SNM
	bonobo_ui_component_set_prop (uic, "/Toolbar",
				      "tips", settings->show_tooltips ? "1" : "0", NULL);


	switch (settings->toolbar_labels) {
	case GHEX_TOOLBAR_SYSTEM:
		/* FIXME
		   if (gnome_preferences_get_toolbar_labels())
		   {
		*/
		bonobo_ui_component_set_prop (uic, "/Toolbar",
					      "look", "both");
		/*
		  }
		  else
		  {
		  bonobo_ui_component_set_prop (uic, "/Toolbar",
		  "look", "icons", NULL);
		  }*/
		break;
	case GHEX_TOOLBAR_ICONS:
		bonobo_ui_component_set_prop (uic, "/Toolbar",
					      "look", "icon", NULL);
		break;
	case GHEX_TOOLBAR_ICONS_AND_TEXT:
		bonobo_ui_component_set_prop (uic, "/Toolbar",
					      "look", "both", NULL);
		break;
	default:
		break;
	}
#endif
	
	bonobo_ui_component_thaw (uic);
	
	return;
}
#endif

static void
ghex_mdi_set_app_statusbar_style (BonoboWindow *win)
{
	BonoboUIComponent *uic;
	
	g_return_if_fail (BONOBO_IS_WINDOW (win));
			
	uic = bonobo_mdi_get_ui_component_from_window(win);

	bonobo_ui_component_set_prop (uic, "/status",
				      "hidden", "0",
				      NULL);

#ifdef SNM
	bonobo_ui_component_set_prop (uic, "/status",
				      "hidden", settings->show_status?"0":"1",
				      NULL);
#endif
}

static void 
ghex_mdi_child_state_changed_handler (HexDocument *child)
{
	if (bonobo_mdi_get_active_child (BONOBO_MDI (mdi)) != BONOBO_MDI_CHILD (child))
		return;
	
	ghex_mdi_set_active_window_title (BONOBO_MDI (mdi));
	ghex_mdi_set_active_window_verbs_sensitivity (BONOBO_MDI (mdi));
}

static void 
ghex_mdi_child_undo_redo_state_changed_handler (HexDocument *child)
{
	if (bonobo_mdi_get_active_child (BONOBO_MDI (mdi)) != BONOBO_MDI_CHILD (child))
		return;
	
	ghex_mdi_set_active_window_undo_redo_verbs_sensitivity (BONOBO_MDI (mdi));
}

static gint 
ghex_mdi_add_child_handler (BonoboMDI *mdi, BonoboMDIChild *child)
{
#ifdef SNM
	gtk_signal_connect (GTK_OBJECT (child), "state_changed",
			    GTK_SIGNAL_FUNC (ghex_mdi_child_state_changed_handler), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (child), "undo_redo_state_changed",
			    GTK_SIGNAL_FUNC (ghex_mdi_child_undo_redo_state_changed_handler), 
			    NULL);
#endif
	return TRUE;
}

static gint 
ghex_mdi_add_view_handler (BonoboMDI *mdi, GtkWidget *view)
{
	gtk_signal_connect (GTK_OBJECT(view), "cursor_moved",
			    GTK_SIGNAL_FUNC (cursor_moved_cb),
			    mdi);
	gtk_hex_show_offsets (GTK_HEX(view), show_offsets_column);

	return TRUE;
}

static gint 
ghex_mdi_remove_child_handler (BonoboMDI *mdi, BonoboMDIChild *child)
{
	HexDocument* doc;
	gboolean close = TRUE;
	
	g_return_val_if_fail (child != NULL, FALSE);
	g_return_val_if_fail (HEX_DOCUMENT (child) != NULL, FALSE);

	doc = HEX_DOCUMENT (child);

#ifdef SNM
	if (ghex_document_get_modified (doc))
	{
		GtkWidget *msgbox, *w;
		gchar *fname = NULL, *msg = NULL;
		gint ret;

		w = GTK_WIDGET (g_list_nth_data (bonobo_mdi_child_get_views (child), 0));
			
		if(w != NULL)
			bonobo_mdi_set_active_view ( BONOBO_MDI (mdi), w);

		fname = ghex_document_get_short_name (doc);

		msgbox = gtk_message_dialog_new (GTK_WINDOW (bonobo_mdi_get_active_window ( BONOBO_MDI (mdi))),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_NONE,
				_("Do you want to save the changes you made to the document ``%s''? \n\n"
				  "Your changes will be lost if you don't save them."),
				fname);

	      	gtk_dialog_add_button (GTK_DIALOG (msgbox),
                             _("_Don't save"),
                             GTK_RESPONSE_NO);

		gtk_dialog_add_button (GTK_DIALOG (msgbox),
                             GTK_STOCK_CANCEL,
                             GTK_RESPONSE_CANCEL);

		gtk_dialog_add_button (GTK_DIALOG (msgbox),
                             GTK_STOCK_SAVE,
                             GTK_RESPONSE_YES);

		gtk_dialog_set_default_response	(GTK_DIALOG (msgbox), GTK_RESPONSE_YES);

		ret = gtk_dialog_run (GTK_DIALOG (msgbox));
		
		gtk_widget_destroy (msgbox);

		g_free (fname);
		g_free (msg);
		
		switch (ret)
		{
			case GTK_RESPONSE_YES:
				close = ghex_file_save (HEX_DOCUMENT (child));
				break;
			case GTK_RESPONSE_NO:
				close = TRUE;
				break;
			default:
				close = FALSE;
		}

	}
	
	if (close)
	{
		gtk_signal_disconnect_by_func (GTK_OBJECT (child), 
				       GTK_SIGNAL_FUNC (ghex_mdi_child_state_changed_handler),
				       NULL);
		gtk_signal_disconnect_by_func (GTK_OBJECT (child), 
				       GTK_SIGNAL_FUNC (ghex_mdi_child_undo_redo_state_changed_handler),
				       NULL);
	}
#endif
	
	return close;
}

static gint 
ghex_mdi_remove_view_handler (BonoboMDI *mdi,  GtkWidget *view)
{
	return TRUE;
}

void 
ghex_mdi_set_active_window_title (BonoboMDI *mdi)
{
	BonoboMDIChild* active_child = NULL;
	HexDocument* doc = NULL;
	gchar* docname = NULL;
	gchar* title = NULL;
	
	active_child = bonobo_mdi_get_active_child ( BONOBO_MDI (mdi));
	if (active_child == NULL)
		return;

	doc = HEX_DOCUMENT (active_child);
	g_return_if_fail (doc != NULL);
	
	/* Set active window title */
#ifdef SNM
	docname = ghex_document_get_uri (doc);
	g_return_if_fail (docname != NULL);
#endif

#ifdef SNM
	if (ghex_document_get_modified (doc))
	{
		title = g_strdup_printf ("%s %s - ghex", docname, _("(modified)"));
	} 
	else 
	{
		if (ghex_document_is_readonly (doc)) 
		{
			title = g_strdup_printf ("%s %s - ghex", docname, _("(readonly)"));
		} 
		else 
		{
			title = g_strdup_printf ("%s - ghex", docname);
		}

	}
#endif

	title = g_strdup_printf("%s", doc->path_end); /* Has to be looked into -- SnM */

	gtk_window_set_title (GTK_WINDOW (bonobo_mdi_get_active_window ( BONOBO_MDI (mdi))), title);

#ifdef SNM	
	g_free (docname);
#endif
	g_free (title);
}

static 
void ghex_mdi_child_changed_handler (BonoboMDI *mdi, BonoboMDIChild *old_child)
{
	ghex_mdi_set_active_window_title (mdi);	
}

static 
void ghex_mdi_view_changed_handler (BonoboMDI *mdi, GtkWidget *old_view)
{
	GtkWidget *active_view;

	ghex_mdi_set_active_window_verbs_sensitivity (mdi);
	ghex_mdi_set_active_window_insert_state(GHEX_MDI(mdi));
	ghex_mdi_set_active_window_group_type(GHEX_MDI(mdi));
	active_view = bonobo_mdi_get_active_view (BONOBO_MDI (mdi));
	if(active_view)
		gtk_widget_grab_focus (active_view);
}

void
ghex_mdi_set_active_window_insert_state(GhexMDI *mdi)
{
	BonoboUIComponent *uic;
	BonoboWindow *win;
	GtkWidget *view;

	win = bonobo_mdi_get_active_window(BONOBO_MDI(mdi));
	if(!win)
		return;
	uic = bonobo_mdi_get_ui_component_from_window(win);
	view = bonobo_mdi_get_view_from_window(BONOBO_MDI(mdi), win);
	if(!view)
		return;
	bonobo_ui_component_set_prop(uic, "/commands/InsertMode",
				     "state", GTK_HEX(view)->insert?"1":"0",
				     NULL);
}

void
ghex_mdi_set_active_window_group_type(GhexMDI *mdi)
{
	BonoboUIComponent *uic;
	BonoboWindow *win;
	GtkWidget *view;
	gchar *path;

	win = bonobo_mdi_get_active_window(BONOBO_MDI(mdi));
	if(!win)
		return;
	uic = bonobo_mdi_get_ui_component_from_window(win);
	view = bonobo_mdi_get_view_from_window(BONOBO_MDI(mdi), win);
	if(!view)
		return;
	switch(GTK_HEX(view)->group_type) {
	case GROUP_BYTE:
		path = "/commands/Bytes";
		break;
	case GROUP_WORD:
		path = "/commands/Words";
		break;
	case GROUP_LONG:
		path = "/commands/Longwords";
		break;
	default:
		path = NULL;
		break;
	}
	if(path)
		bonobo_ui_component_set_prop(uic, path, "state", "1", NULL);
}

void 
ghex_mdi_set_active_window_verbs_sensitivity (BonoboMDI *mdi)
{
	/* FIXME: it is too slooooooow! - Paolo */
	BonoboWindow* active_window = NULL;
	BonoboMDIChild* active_child = NULL;
	HexDocument* doc = NULL;
	BonoboUIComponent *uic;
	
	active_window = bonobo_mdi_get_active_window (BONOBO_MDI (mdi));

	if (active_window == NULL)
		return;
	
	uic = bonobo_mdi_get_ui_component_from_window(active_window);
	
	active_child = bonobo_mdi_get_active_child (BONOBO_MDI (mdi));
	
	bonobo_ui_component_freeze (uic, NULL);

	if (active_child == NULL) {
		ghex_menus_set_verb_list_sensitive (uic, FALSE);
	}
	else {
		ghex_menus_set_verb_list_sensitive (uic, TRUE);
		hex_document_set_menu_sensitivity (HEX_DOCUMENT (active_child));
	}

	bonobo_ui_component_thaw (uic, NULL);

#ifdef SNM	
	doc = HEX_DOCUMENT (active_child);
	g_return_if_fail (doc != NULL);

	if (ghex_document_is_readonly (doc)) {
		ghex_menus_set_verb_list_sensitive (uic, 
						    ghex_menus_ro_sensible_verbs,
						    FALSE);
		goto end;
	}

	if (!ghex_document_can_undo (doc))
		ghex_menus_set_verb_sensitive (uic, "/commands/EditUndo", FALSE);

	if (!ghex_document_can_redo (doc))
		ghex_menus_set_verb_sensitive (uic, "/commands/EditRedo", FALSE);		

	if (!ghex_document_get_modified (doc))
	{
		ghex_menus_set_verb_list_sensitive (uic, 
						    ghex_menus_not_modified_doc_sensible_verbs,
						    FALSE);
		goto end;
	}

	if (ghex_document_is_untitled (doc))
	{
		ghex_menus_set_verb_list_sensitive (uic, 
						    ghex_menus_untitled_doc_sensible_verbs,
						    FALSE);
	}

end:
	bonobo_ui_component_thaw (uic);
#endif
}


static void 
ghex_mdi_set_active_window_undo_redo_verbs_sensitivity (BonoboMDI *mdi)
{
	BonoboWindow* active_window = NULL;
	BonoboMDIChild* active_child = NULL;
	HexDocument* doc = NULL;
	BonoboUIComponent *uic;
	
	active_window = bonobo_mdi_get_active_window (BONOBO_MDI (mdi));
	g_return_if_fail (active_window != NULL);
	
	uic = bonobo_mdi_get_ui_component_from_window(active_window);
	
	active_child = bonobo_mdi_get_active_child (BONOBO_MDI (mdi));
	doc = HEX_DOCUMENT (active_child);
	g_return_if_fail (doc != NULL);

	bonobo_ui_component_freeze (uic, NULL);

#ifdef SNM
	ghex_menus_set_verb_sensitive (uic, "/commands/EditUndo", 
				       ghex_document_can_undo (doc));	

	ghex_menus_set_verb_sensitive (uic, "/commands/EditRedo", 
				       ghex_document_can_redo (doc));	
#endif

	bonobo_ui_component_thaw (uic, NULL);
}

#ifdef SNM
void
ghex_mdi_update_ui_according_to_preferences (GhexMDI *mdi)
{
	GList *windows;		
	GList *children;
	GdkColor background;
	GdkColor text;
	GdkColor selection;
	GdkColor sel_text;
	const gchar* font;

	windows = bonobo_mdi_get_windows (BONOBO_MDI (mdi));

	while (windows != NULL)
	{
		BonoboUIComponent *uic;
		BonoboWindow *active_window = BONOBO_WINDOW (windows->data);
		g_return_if_fail (active_window != NULL);
		
		uic = bonobo_mdi_get_ui_component_from_window(active_window);
		g_return_if_fail (uic != NULL);

		bonobo_ui_component_freeze (uic);

		ghex_mdi_set_app_statusbar_style (active_window);

#ifdef SNM /* ghex didnt have this. Do we want this? -- SnM */	
		ghex_mdi_set_app_toolbar_style (active_window);
#endif

		bonobo_ui_component_thaw (uic);

		windows = windows->next;
	}

	children = bonobo_mdi_get_children (BONOBO_MDI (mdi));

#ifdef SNM
	if (settings->use_default_colors)
	{
		GtkStyle *style;
	       
		style = gtk_style_new ();

		background = style->base [GTK_STATE_NORMAL];
		text = style->text [GTK_STATE_NORMAL];
		sel_text = style->text [GTK_STATE_SELECTED];
		selection = style->base [GTK_STATE_SELECTED];

		gtk_style_unref (style);
	}
	else
	{
		background.red = settings->bg [0];
		background.green = settings->bg [1];
		background.blue = settings->bg [2];

		text.red = settings->fg [0];
		text.green = settings->fg [1];
		text.blue = settings->fg [2];

		selection.red = settings->sel [0];
		selection.green = settings->sel [1];
		selection.blue = settings->sel [2];

		sel_text.red = settings->st [0];
		sel_text.green = settings->st [1];
		sel_text.blue = settings->st [2];
	}

	if (settings->use_default_font)
	{
		GtkStyle *style;
		
		style = gtk_style_new ();

		font = pango_font_description_to_string (style->font_desc);
		
		if (font == NULL)
			/* Fallback */
			font = settings->font;

		gtk_style_unref (style);

	}
	else
		font = settings->font;

	while (children != NULL)
	{
		GList *views = bonobo_mdi_child_get_views (BONOBO_MDI_CHILD (children->data));

		while (views != NULL)
		{
			GhexView *v = GHEX_VIEW (views->data);
			
			ghex_view_set_colors (v, &background, &text, &selection, &sel_text);
			ghex_view_set_font (v, font);
			ghex_view_set_wrap_mode (v, settings->wrap_mode);
			views = views->next;
		}
		
		children = children->next;
	}
#endif

/*
	bonobo_mdi_set_mode (BONOBO_MDI (mdi), settings->mdi_mode);
*/
}
#endif

/* Added this function. Copied from main.c -- SnM */

static void cursor_moved_cb (GtkHex *gtkhex) {

	static gchar *cursor_pos, *format;

	if ((format = g_strdup_printf (_("Offset: %s"), offset_fmt)) != NULL)
	{
		if ((cursor_pos = g_strdup_printf (format, gtk_hex_get_cursor (gtkhex))) != NULL) {
			bonobo_window_show_status (bonobo_mdi_get_active_window (BONOBO_MDI (mdi)), cursor_pos);

			g_free (cursor_pos);
		}
		g_free (format);
	}
}
		

void
ghex_menus_set_verb_list_sensitive (BonoboUIComponent *uic, gboolean allmenus)
{
	bonobo_ui_component_set_prop (uic, "/menu/Edit", "hidden",
				      (TRUE == allmenus)?"0": "1", NULL);

	bonobo_ui_component_set_prop (uic, "/menu/View", "hidden", 
				      (TRUE == allmenus)?"0": "1", NULL);
}
