/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* main.c - genesis of a GHex application

   Copyright (C) 1998 - 2001 Free Software Foundation

   GHex is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   GHex is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GHex; see the file COPYING.
   If not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#include <config.h>
#include <gnome.h>
#include "ghex.h"

enum {
		TARGET_URI_LIST,
};

/* callbacks for MDI signals */
#ifdef SNM 
static gint remove_doc_cb(BonoboMDI *, HexDocument *);
static gint add_view_cb(BonoboMDI *, GtkHex *);
static void cleanup_cb(BonoboMDI *);
#endif
static void view_changed_cb(BonoboMDI *, GtkHex *);
static void child_changed_cb(BonoboMDI *, HexDocument *);
static void customize_app_cb(BonoboMDI *, GnomeApp *);

static void cursor_moved_cb(GtkHex *gtkhex);

/* Changed from BonoboMDI to GhexMDI -- SnM */
GhexMDI *mdi;

gint mdi_mode = BONOBO_MDI_DEFAULT_MODE;

gint remove_doc_cb(BonoboMDI *mdi, HexDocument *doc) {
	static char msg[MESSAGE_LEN + 1];
	GtkWidget *mbox;
	gint reply;
	GtkWidget *save_btn;
	
	g_snprintf(msg, MESSAGE_LEN,
			   _("File %s has changed since last save.\n"
				 "Do you want to save changes?"),
			   bonobo_mdi_child_get_name (BONOBO_MDI_CHILD(doc)));
	
	if(hex_document_has_changed(doc)) {
		mbox = gtk_message_dialog_new(GTK_WINDOW (bonobo_mdi_get_active_window(BONOBO_MDI(mdi))),
				GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_NONE,
				msg);
		
		save_btn = create_button(mbox, GTK_STOCK_NO, _("Do_n't save"));
		gtk_widget_show (save_btn);
		gtk_dialog_add_action_widget(GTK_DIALOG(mbox), save_btn, GTK_RESPONSE_NO);
		gtk_dialog_add_button(GTK_DIALOG(mbox), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
		gtk_dialog_add_button(GTK_DIALOG(mbox), GTK_STOCK_SAVE, GTK_RESPONSE_YES);
		gtk_dialog_set_default_response(GTK_DIALOG(mbox), GTK_RESPONSE_YES);
		gtk_window_set_resizable(GTK_WINDOW(mbox), FALSE);

		reply = gtk_dialog_run(GTK_DIALOG(mbox));

		gtk_widget_destroy(mbox);
		
		if(reply == GTK_RESPONSE_YES)
			hex_document_write(doc);
		else if(reply == GTK_RESPONSE_CANCEL)
			return FALSE;
	}
	
	return TRUE;
}

#ifdef SNM
gint add_view_cb(BonoboMDI *mdi, GtkHex *view) {
	gtk_signal_connect(GTK_OBJECT(view), "cursor_moved",
					   GTK_SIGNAL_FUNC(cursor_moved_cb), mdi);
	gtk_hex_show_offsets(GTK_HEX(view), show_offsets_column);

	return TRUE;
}
#endif

void cleanup_cb(BonoboMDI *mdi) {
	save_configuration();
	gtk_main_quit();
}

void child_changed_cb(BonoboMDI *mdi, HexDocument *old_doc) {
	GnomeUIInfo *mdi_menus;
	gboolean sens;
	int i;

#ifdef SNM
	if(bonobo_mdi_get_active_child(mdi) == NULL || old_doc == NULL) {
		sens = old_doc == NULL;
		mdi_menus = bonobo_mdi_get_menubar_info(bonobo_mdi_get_active_window (BONOBO_MDI (mdi)));
		/* keep in sync: ui.c/file_menu[] */
		for(i = 1; i < 4; i++)
			gtk_widget_set_sensitive(((GnomeUIInfo *)mdi_menus[0].moreinfo)[i].widget, sens);
	}

	if(find_dialog)
		create_dialog_title(find_dialog->window, _("GHex (%s): Find Data"));
	if(replace_dialog)
		create_dialog_title(replace_dialog->window, _("GHex (%s): Find & Replace Data"));
	if(jump_dialog)
		create_dialog_title(jump_dialog->window, _("GHex (%s): Jump To Byte"));
#endif

}

void view_changed_cb(BonoboMDI *mdi, GtkHex *old_view) {

#ifdef SNM
	GnomeApp *app;
	GnomeUIInfo *uiinfo;
	GtkWidget *shell, *item;
	GList *item_node;
	HexDocument *doc;
	gint pos;
	gint group_item;

	if(bonobo_mdi_get_active_view( BONOBO_MDI(mdi)) == NULL)
		return;

	app = bonobo_mdi_get_app_from_view(bonobo_mdi_get_active_view( BONOBO_MDI(mdi)));
	
	shell = gnome_app_find_menu_pos(app->menubar, GROUP_MENU_PATH, &pos);
	if (shell) {
		group_item = GTK_HEX(bonobo_mdi_get_active_view( BONOBO_MDI(mdi)))->group_type / 2;
		item_node = GTK_MENU_SHELL(shell)->children;
		item = NULL;
		while(item_node) {
			if(!GTK_IS_TEAROFF_MENU_ITEM(item_node->data)) {
				if(group_item == 0) {
					item = GTK_WIDGET(item_node->data);
					break;
				}
				group_item--;
			}
			item_node = item_node->next;
		}
		if(item) {
			gtk_menu_shell_activate_item(GTK_MENU_SHELL(shell), item, TRUE);
		}
	}
	shell = gnome_app_find_menu_pos(app->menubar, OVERWRITE_ITEM_PATH, &pos);
	if (shell) {
		item = g_list_nth(GTK_MENU_SHELL(shell)->children, pos - 1)->data;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
									   GTK_HEX(bonobo_mdi_get_active_view( BONOBO_MDI(mdi)))->insert);
	}
	uiinfo = bonobo_mdi_get_child_menu_info(app);
	uiinfo = (GnomeUIInfo *)uiinfo[0].moreinfo;
    	doc = HEX_DOCUMENT(bonobo_mdi_get_child_from_view(bonobo_mdi_get_active_view( BONOBO_MDI(mdi))));
	gtk_widget_set_sensitive(uiinfo[0].widget, doc->undo_depth > 0);
	gtk_widget_set_sensitive(uiinfo[1].widget, doc->undo_top != doc->undo_stack);

	gnome_app_install_menu_hints(app, bonobo_mdi_get_child_menu_info(app));
#endif

}

static void app_drop_cb(GtkWidget *widget, GdkDragContext *context,
		        gint x, gint y, GtkSelectionData *selection_data,
			guint info, guint time, gpointer data) {
#ifdef SNM
	GList *names, *list;

	switch (info) {
	case TARGET_URI_LIST:
		list = names = gnome_uri_list_extract_filenames (selection_data->data);
		while (names) {
			HexDocument *doc;

			doc = hex_document_new((gchar *)names->data);
			if(doc) {
				bonobo_mdi_add_child(mdi, BONOBO_MDI_CHILD(doc));
				bonobo_mdi_add_view(mdi, BONOBO_MDI_CHILD(doc));
			}
			names = names->next;
		}
		gnome_uri_list_free_strings (list);
		break;
	default:
		break;
	}
#endif

}

void customize_app_cb(BonoboMDI *mdi, GnomeApp *app) {

#ifdef SNM
	GtkWidget *bar;
	static GtkTargetEntry drop_types [] = {
		{ "text/uri-list", 0, TARGET_URI_LIST}
	};
	static gint n_drop_types = sizeof (drop_types) / sizeof(drop_types[0]);

	gtk_drag_dest_set (GTK_WIDGET (app),
	                   GTK_DEST_DEFAULT_MOTION |
					   GTK_DEST_DEFAULT_HIGHLIGHT |
					   GTK_DEST_DEFAULT_DROP,
					   drop_types, n_drop_types,
					   GDK_ACTION_COPY);
	gtk_signal_connect (GTK_OBJECT (app), "drag_data_received",
						GTK_SIGNAL_FUNC(app_drop_cb), NULL);

	bar = gnome_appbar_new(FALSE, TRUE, GNOME_PREFERENCES_USER);
	gnome_app_set_statusbar(app, bar);
	gtk_widget_show(bar);

	gnome_app_install_menu_hints(app, bonobo_mdi_get_menubar_info(app));
#endif

}

static void cursor_moved_cb(GtkHex *gtkhex) {

#ifdef SNM
	static gchar *cursor_pos, *format;

	if((format = g_strdup_printf(_("Offset: %s"), offset_fmt)) != NULL) {
		if((cursor_pos = g_strdup_printf(format, gtk_hex_get_cursor(gtkhex))) != NULL) {
			gnome_appbar_set_status(GNOME_APPBAR(bonobo_mdi_get_app_from_view(GTK_WIDGET(gtkhex))->statusbar),
									cursor_pos);
			g_free(cursor_pos);
		}
		g_free(format);
	}
#endif

}

#ifdef SNM
int main(int argc, char **argv) {
	GnomeClient *client;
	HexDocument *doc;
	char **cl_files;
	poptContext ctx;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	gnome_init_with_popt_table("ghex", VERSION, argc, argv, options, 0, &ctx);

	client = gnome_master_client();

	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
						GTK_SIGNAL_FUNC (save_state), (gpointer) argv[0]);
	gtk_signal_connect (GTK_OBJECT (client), "die",
						GTK_SIGNAL_FUNC (client_die), NULL);

    mdi = BONOBO_MDI(bonobo_mdi_new("ghex", "GHex"));

    /* set up MDI menus */
    bonobo_mdi_set_menubar_template(mdi, main_menu);

    /* and document menu and document list paths */
    bonobo_mdi_set_child_menu_path(mdi, CHILD_MENU_PATH);
    bonobo_mdi_set_child_list_path(mdi, CHILD_LIST_PATH);

#if 0
	/* set default window icon */
	gnome_window_icon_set_default_from_file("gnome-ghex.png");
#endif

    /* connect signals */
    gtk_signal_connect(GTK_OBJECT(mdi), "remove_child", GTK_SIGNAL_FUNC(remove_doc_cb), NULL);
    gtk_signal_connect(GTK_OBJECT(mdi), "destroy", GTK_SIGNAL_FUNC(cleanup_cb), NULL);
    gtk_signal_connect(GTK_OBJECT(mdi), "view_changed", GTK_SIGNAL_FUNC(view_changed_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(mdi), "child_changed", GTK_SIGNAL_FUNC(child_changed_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(mdi), "app_created", GTK_SIGNAL_FUNC(customize_app_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(mdi), "add_view", GTK_SIGNAL_FUNC(add_view_cb), NULL);

    /* load preferences */
    ghex_prefs_init();
    load_configuration();

    /* set MDI mode */
    bonobo_mdi_set_mode(mdi, mdi_mode);

    /* restore state from previous session */
    if (gnome_client_get_flags (client) & GNOME_CLIENT_RESTORED) {

		gnome_config_push_prefix (gnome_client_get_config_prefix (client));

		restarted= bonobo_mdi_restore_state (mdi, "Session", (BonoboMDIChildCreator)hex_document_new_from_config);		
		
		gnome_config_pop_prefix ();
	}

	if (!restarted)
		bonobo_mdi_open_toplevel(mdi);
	
    cl_files = (char **)poptGetArgs(ctx);
	
    while(cl_files && *cl_files) {
		doc = hex_document_new(*cl_files);
		if(doc) {
			bonobo_mdi_add_child(mdi, BONOBO_MDI_CHILD(doc));
			bonobo_mdi_add_view(mdi, BONOBO_MDI_CHILD(doc));
		}
		cl_files++;
    }
    poptFreeContext(ctx);
	
    /* and here we go... */
    gtk_main();

	return 0;
}
#endif

int
main(int argc, char **argv)
{
	GnomeClient *client;
	GValue value = { 0, };
	GnomeProgram *program;
	poptContext ctx;
	char **args;
	HexDocument *doc;

	char **cl_files;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

        /* Initialize gnome program */
	program = gnome_program_init ("ghex2", VERSION,
				LIBGNOMEUI_MODULE, argc, argv,
				GNOME_PARAM_POPT_TABLE, options,
				GNOME_PARAM_HUMAN_READABLE_NAME,
				_("The gnome binary editor"),
				NULL);

	/* FIXME - Couldnt find gnome-ghex.png in the sources directory */
	/* Set default window icon */
	gnome_window_icon_set_default_from_file (GNOMEICONDIR "/gnome-ghex.png");

	/* load preferences */
	ghex_prefs_init();
	load_configuration();

	client = gnome_master_client();

	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
						GTK_SIGNAL_FUNC (save_state), (gpointer) argv[0]);
	gtk_signal_connect (GTK_OBJECT (client), "die",
						GTK_SIGNAL_FUNC (client_die), NULL);


	/* Create ghex_mdi and open the first top level window */
	mdi = ghex_mdi_new ();

	/* restore state from previous session */
	if (gnome_client_get_flags (client) & GNOME_CLIENT_RESTORED) {

		gnome_config_push_prefix (gnome_client_get_config_prefix (client));

		restarted = bonobo_mdi_restore_state (BONOBO_MDI(mdi), "Session", (BonoboMDIChildCreator)hex_document_new_from_config);		
		
		gnome_config_pop_prefix ();
	}

	if (!restarted)
		bonobo_mdi_open_toplevel(BONOBO_MDI(mdi));

	/* Parse args and build the list of files to be loaded at startup */
	g_value_init (&value, G_TYPE_POINTER);
	g_object_get_property (G_OBJECT (program), GNOME_PARAM_POPT_CONTEXT, &value);
	ctx = g_value_get_pointer (&value);
	g_value_unset (&value);

	args = (char**) poptGetArgs(ctx);

	cl_files = (char **)poptGetArgs(ctx);
	
	while(cl_files && *cl_files) {
		doc = hex_document_new(*cl_files);
		if(doc) {
			bonobo_mdi_add_child (BONOBO_MDI (mdi), BONOBO_MDI_CHILD(doc));
			bonobo_mdi_add_view(BONOBO_MDI (mdi), BONOBO_MDI_CHILD(doc));
		}
		cl_files++;
	}
	poptFreeContext(ctx);
	
	gtk_main();

	return 0;
}
