/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ui.c - main menus and callbacks; utility functions

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
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprintui/gnome-print-dialog.h>
#include <libgnomeprintui/gnome-print-master-preview.h>
#include "ghex.h"

static void open_selected_file(GtkWidget *);
static void save_selected_file(GtkWidget *, GtkWidget *view);
static void export_html_selected_file(GtkWidget *w, GtkHex *view);
static void ghex_print(gboolean preview);
static gboolean ghex_print_run_dialog(GHexPrintJobInfo *pji);
static void ghex_print_preview_real(GHexPrintJobInfo *pji);

/* callbacks to nullify widget pointer after a delete event */
static void about_destroy_cb(GtkObject *, GtkWidget **);

/* callbacks for global menus */
#ifdef SNM
static void quit_app_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
#endif

static void open_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
static void close_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
static void save_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
static void save_as_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
static void print_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
static void print_preview_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
static void export_html_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
static void revert_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
static void prefs_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
static void converter_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
static void char_table_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
static void about_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
static void help_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);

#if 0
GnomeUIInfo file_menu[] = {
	GNOMEUIINFO_MENU_OPEN_ITEM(open_cb, NULL),
	/* keep in sync: main.c/child_changed_cb: setting sensitivity of items 1 - 3 */
	GNOMEUIINFO_MENU_SAVE_ITEM(save_cb, NULL),
	GNOMEUIINFO_MENU_SAVE_AS_ITEM(save_as_cb, NULL),
	GNOMEUIINFO_ITEM(N_("Export to _HTML..."), N_("Export data to HTML source"), export_html_cb, NULL),
	GNOMEUIINFO_MENU_REVERT_ITEM(revert_cb, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_PRINT_ITEM(print_cb, NULL),
	GNOMEUIINFO_ITEM(N_("Print pre_view..."), N_("Preview printed data"), print_preview_cb, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CLOSE_ITEM(close_cb, NULL),
	GNOMEUIINFO_MENU_EXIT_ITEM(quit_app_cb, NULL),
	GNOMEUIINFO_END
};

GnomeUIInfo settings_menu[] = {
	GNOMEUIINFO_MENU_PREFERENCES_ITEM(prefs_cb,NULL),
	GNOMEUIINFO_END
};

GnomeUIInfo tools_menu[] = {
	GNOMEUIINFO_ITEM_NONE(N_("Con_verter..."),
						  N_("Open base conversion dialog"), converter_cb),
	GNOMEUIINFO_ITEM_NONE(N_("Character _Table..."),
						  N_("Show the character table"), char_table_cb),
	GNOMEUIINFO_END
};


GnomeUIInfo empty_menu[] = {
	GNOMEUIINFO_END
};

GnomeUIInfo help_menu[] = {
	GNOMEUIINFO_HELP("ghex"),
	GNOMEUIINFO_MENU_ABOUT_ITEM(about_cb,NULL),
	GNOMEUIINFO_END
};

GnomeUIInfo main_menu[] = {
	GNOMEUIINFO_MENU_FILE_TREE(file_menu),
	GNOMEUIINFO_SUBTREE(N_("_Tools"), tools_menu),
	GNOMEUIINFO_MENU_SETTINGS_TREE(settings_menu),
	GNOMEUIINFO_MENU_FILES_TREE(empty_menu),
	GNOMEUIINFO_MENU_HELP_TREE(help_menu),
	GNOMEUIINFO_END
};

#endif

guint group_type[3] = {
	GROUP_BYTE,
	GROUP_WORD,
	GROUP_LONG,
};

gchar *group_type_label[3] = {
	N_("Bytes"),
	N_("Words"),
	N_("Longwords"),
};

guint search_type = 0;
gchar *search_type_label[] = {
	N_("hex data"),
	N_("ASCII data"),
};

GtkWidget *file_sel = NULL;


/* Changes for Gnome 2.0 -- SnM */
BonoboUIVerb ghex_verbs [] = {
	BONOBO_UI_VERB ("FileOpen", open_cb),
	BONOBO_UI_VERB ("FileSave", save_cb),
	BONOBO_UI_VERB ("FileSaveAs", save_as_cb),
	BONOBO_UI_VERB ("ExportToHTML", export_html_cb),
	BONOBO_UI_VERB ("FileRevert", revert_cb),
	BONOBO_UI_VERB ("FilePrint", print_cb),
	BONOBO_UI_VERB ("FilePrintPreview", print_preview_cb),
	BONOBO_UI_VERB ("FileClose", close_cb),
	BONOBO_UI_VERB ("FileExit", quit_app_cb),
	BONOBO_UI_VERB ("Converter", converter_cb),
	BONOBO_UI_VERB ("CharacterTable", char_table_cb),
	BONOBO_UI_VERB ("EditUndo", undo_cb),
	BONOBO_UI_VERB ("EditRedo", redo_cb),
	BONOBO_UI_VERB ("Find", find_cb),
	BONOBO_UI_VERB ("Replace", replace_cb),
	BONOBO_UI_VERB ("GoToByte", jump_cb),
	BONOBO_UI_VERB ("AddView", add_view_cb),
	BONOBO_UI_VERB ("RemoveView", remove_view_cb),
	BONOBO_UI_VERB ("Preferences", prefs_cb),
	BONOBO_UI_VERB ("About", about_cb),
	BONOBO_UI_VERB ("Help", help_cb),
	BONOBO_UI_VERB_END
};

void cancel_cb(GtkWidget *w, GtkWidget *me)
{
	gtk_widget_hide(me);
}

gint delete_event_cb(GtkWidget *w, GdkEventAny *e)
{
	gtk_widget_hide(w);
	
	return TRUE;
}

gint ask_user(GnomeMessageBox *message_box)
{
	gtk_window_set_modal(GTK_WINDOW(message_box), TRUE);	

	return gnome_dialog_run_and_close(GNOME_DIALOG(message_box));
}

GtkWidget *create_button(GtkWidget *window, const gchar *type, gchar *text)
{
	GtkWidget *button, *pixmap, *label, *hbox;
	
	hbox = gtk_hbox_new(FALSE, 2);
	
#ifdef SNM
	pixmap = gnome_stock_pixmap_widget(window, type);
	gtk_box_pack_start(GTK_BOX(hbox), pixmap, FALSE, FALSE, 1);
#endif	

	label = gtk_label_new_with_mnemonic(text);

	pixmap = gtk_image_new_from_stock (type, GTK_ICON_SIZE_BUTTON );

	gtk_box_pack_start(GTK_BOX(hbox), pixmap, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 1);
	
	button = gtk_button_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(button));
	gtk_container_add(GTK_CONTAINER(button), hbox);
	
	gtk_widget_show(label);
	gtk_widget_show(pixmap);
	gtk_widget_show(hbox);
	
	return button;
}

void create_dialog_title(GtkWidget *window, gchar *title)
{
	gchar *full_title;

	if(!window)
		return;

	if(bonobo_mdi_get_active_child (BONOBO_MDI (mdi)))
		full_title = g_strdup_printf(title, bonobo_mdi_child_get_name (bonobo_mdi_get_active_child (BONOBO_MDI (mdi))));
	else
		full_title = g_strdup_printf(title, "");

	if(full_title) {
		gtk_window_set_title(GTK_WINDOW(window), full_title);
		g_free(full_title);
	}
}

/*
 * callbacks for global menus
 */
static void about_destroy_cb(GtkObject *obj, GtkWidget **about)
{
	*about = NULL;
}

/* Changed the function parameters -- SnM */
static void about_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	static GtkWidget *about = NULL;
	
	static const gchar *authors[] = {
		"Jaka Mocnik <jaka@gnu.org>",
		"Chema Celorio <chema@celorio.com>",
		"Shivram U <shivaram.upadhyayula@wipro.com>",
		NULL
	};

	if(!about) {
		about = gnome_about_new ( _("GHex, a binary file editor"), VERSION,
								  "(C) 1998 - 2002 Jaka Mocnik",
								  _("Released under the terms of GNU Public License"),
								  authors, NULL, NULL, NULL);
		g_signal_connect(G_OBJECT(about), "destroy",
						 G_CALLBACK(about_destroy_cb), &about);
		gtk_widget_show (about);
	}
	else
		gdk_window_raise(GTK_WIDGET(about)->window);
}

static void help_cb (BonoboUIComponent *uic, gpointer user_data, const gchar *verbname)
{
	GError *error = NULL;

	gnome_help_display ("ghex2", NULL, &error);

	if(error) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						_("There was an error displaying help: \n%s"),
						error->message);

		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

/* Changed the function parameters -- SnM */
void quit_app_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	gboolean ret;

	if (!bonobo_mdi_remove_all (BONOBO_MDI (mdi), FALSE)) {
		return;
	}

	/* Got this from gedit2. 
	 * We need to disconnect the signal because mdi "destroy" event is
	 * connected to quit_app_cb.
	 * 18th Jan 2001 -- SnM
	 */
	gtk_signal_disconnect_by_func (GTK_OBJECT (mdi),
			GTK_SIGNAL_FUNC (quit_app_cb), NULL);

	save_configuration();

	g_object_unref (G_OBJECT (mdi));
	gtk_main_quit();

#ifdef SNM 
	if(bonobo_mdi_remove_all(mdi, FALSE))
		gtk_object_destroy(GTK_OBJECT(mdi));
#endif

}

/* Changed the function parameters -- SnM */
static void save_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	HexDocument *doc;

	if (bonobo_mdi_get_active_child ( BONOBO_MDI (mdi)) == NULL)
		return;

	doc = HEX_DOCUMENT (bonobo_mdi_get_active_child ( BONOBO_MDI (mdi)));
	if(!hex_document_write(doc))
		display_error_dialog (bonobo_mdi_get_active_window (BONOBO_MDI (mdi)), _("Error saving file!"));
	else {
		gchar *flash;
		
		flash = g_strdup_printf(_("Saved buffer to file %s"), doc->file_name);

		bonobo_window_flash (bonobo_mdi_get_active_window (BONOBO_MDI (mdi)), flash);

		g_free(flash);
	}
}

/* Changed the function parameters -- SnM */
static void open_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	HexDocument *doc;

	if(file_sel && GTK_WIDGET_VISIBLE(file_sel))
		return;

	if(file_sel == NULL)
		file_sel = gtk_file_selection_new(NULL);

	if (bonobo_mdi_get_active_child (BONOBO_MDI (mdi)) != NULL) {
		doc = HEX_DOCUMENT (bonobo_mdi_get_active_child (BONOBO_MDI (mdi)));
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_sel), doc->file_name);
	}

	gtk_window_set_title(GTK_WINDOW(file_sel), _("Select a file to open"));
	
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->ok_button),
						"clicked", GTK_SIGNAL_FUNC(open_selected_file),
						NULL);
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->cancel_button),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						file_sel);
	gtk_signal_connect (GTK_OBJECT (file_sel),
						"delete-event", GTK_SIGNAL_FUNC(delete_event_cb),
						file_sel);

	gtk_window_set_modal (GTK_WINDOW(file_sel), TRUE);

	if(!GTK_WIDGET_VISIBLE(file_sel)) {
		gtk_window_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
		gtk_widget_show (file_sel);
	}

}

/* Changed the function parameters -- SnM */
static void save_as_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	HexDocument *doc;

	if (bonobo_mdi_get_active_child (BONOBO_MDI (mdi)) == NULL  ||
	   (file_sel && GTK_WIDGET_VISIBLE(file_sel)))
		return;
	
	doc = HEX_DOCUMENT (bonobo_mdi_get_active_child (BONOBO_MDI(mdi)));

	if(file_sel == NULL)
		file_sel = gtk_file_selection_new(NULL);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_sel), doc->file_name);

	gtk_window_set_title(GTK_WINDOW(file_sel), _("Select a file to save buffer as"));
	
	gtk_window_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->ok_button),
						"clicked", GTK_SIGNAL_FUNC(save_selected_file),
						bonobo_mdi_get_active_view (BONOBO_MDI (mdi)));
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->cancel_button),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						file_sel);

	gtk_window_set_modal(GTK_WINDOW(file_sel), TRUE);

	gtk_widget_show (file_sel);
}

/* Changed the function parameters -- SnM */
static void print_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	ghex_print(FALSE);
}

/* Changed the function parameters -- SnM */
static void print_preview_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	ghex_print(TRUE);
}

/* Changed the function parameters -- SnM */
static void export_html_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	HexDocument *doc;

	if (bonobo_mdi_get_active_child (BONOBO_MDI(mdi)) == NULL ||
	   (file_sel && GTK_WIDGET_VISIBLE(file_sel)))
		return;

	doc = HEX_DOCUMENT (bonobo_mdi_get_active_child (BONOBO_MDI(mdi)));

	if(file_sel == NULL)
		file_sel = gtk_file_selection_new(NULL);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_sel), doc->file_name);

	gtk_window_set_title(GTK_WINDOW(file_sel), _("Select path and file name for the HTML source"));
	
	gtk_window_position(GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	
	gtk_signal_connect(GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->ok_button),
						"clicked", GTK_SIGNAL_FUNC(export_html_selected_file),
						bonobo_mdi_get_active_view (BONOBO_MDI(mdi)));
	gtk_signal_connect(GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->cancel_button),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						file_sel);
	gtk_window_set_modal(GTK_WINDOW(file_sel), TRUE);
	gtk_widget_show(file_sel);
}

static void export_html_selected_file(GtkWidget *w, GtkHex *view)
{
	gchar *html_path = g_strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_sel)));
	gchar *sep, *base_name;
	HexDocument *doc;

	gtk_widget_hide(file_sel);

	sep = html_path + strlen(html_path) - 1;
	while(sep >= html_path && *sep != '/')
		sep--;
	if(sep >= html_path)
		*sep = 0;
	base_name = sep + 1;
	sep = strstr(base_name, ".htm");
	if(sep)
		*sep = 0;

	if(*base_name == 0) {
		g_free(html_path);
		return;
	}

	doc = HEX_DOCUMENT(bonobo_mdi_get_child_from_view(GTK_WIDGET(view)));

	hex_document_export_html(doc, html_path, base_name, 0, doc->file_size,
							 view->cpl, view->vis_lines, view->group_type);

	gtk_widget_destroy(GTK_WIDGET(file_sel));
	file_sel = NULL;
	g_free(html_path);
}

/* Defined in converter.c: used by close_cb and converter_cb */

extern GtkWidget *get;
/* Changed the function parameters -- SnM */
static void close_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	BonoboWindow *win;

	if(bonobo_mdi_get_active_child (BONOBO_MDI (mdi)) == NULL)
		return;

	bonobo_mdi_remove_child( BONOBO_MDI (mdi), bonobo_mdi_get_active_child (BONOBO_MDI (mdi)), FALSE);

	/* Added on 18th Jan 2001 -- SnM */
	ghex_mdi_set_active_window_verbs_sensitivity (BONOBO_MDI (mdi));

	win = bonobo_mdi_get_active_window (BONOBO_MDI (mdi));

	if (win)
		bonobo_window_show_status (win, " ");

	/* If we have created the converter window disable the 
	 * "Get cursor value" button
	 */
	if (get)
		gtk_widget_set_sensitive(get, FALSE);
	

}

/* Changed the function parameters -- SnM */
static void converter_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	if(!converter)
		converter = create_converter();

	if(!GTK_WIDGET_VISIBLE(converter->window)) {
		gtk_window_position(GTK_WINDOW(converter->window), GTK_WIN_POS_MOUSE);
		gtk_widget_show(converter->window);
	}
	gdk_window_raise(converter->window->window);

	if (!bonobo_mdi_get_active_view(BONOBO_MDI(mdi)))
		gtk_widget_set_sensitive(get, FALSE);
	else
		gtk_widget_set_sensitive(get, TRUE);
}

/* Changed the function parameters -- SnM */
static void char_table_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	if(!char_table)
		char_table = create_char_table();

	if(!GTK_WIDGET_VISIBLE(char_table)) {
		gtk_window_position(GTK_WINDOW(char_table), GTK_WIN_POS_MOUSE);
		gtk_widget_show(char_table);
	}
	gdk_window_raise(char_table->window);
}


#ifdef SNM
/* Changed the function parameters -- SnM */
static
void prefs_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GtkWidget *dlg;
	gint ret;

	dlg = ghex_preferences_dialog_new (
			GTK_WINDOW (bonobo_mdi_get_active_window (BONOBO_MDI (mdi))));

	do
	{
		ret = gtk_dialog_run (GTK_DIALOG (dlg));

		switch (ret)
		{
			case GTK_RESPONSE_OK:
				break;
			case GTK_RESPONSE_HELP:
				/* FIXME -- SnM */
				break;
			default:
				gtk_widget_hide (dlg);
		}

	} while (GTK_WIDGET_VISIBLE (dlg));

	gtk_widget_destroy (dlg);	
}
#endif


/* Changed the function parameters -- SnM */
static
void prefs_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
//	if(!prefs_ui)
		prefs_ui = create_prefs_dialog();

	gnome_dialog_set_default(GNOME_DIALOG(prefs_ui->pbox), 2);
	if(!GTK_WIDGET_VISIBLE(prefs_ui->pbox)) {
		gtk_window_position (GTK_WINDOW(prefs_ui->pbox), GTK_WIN_POS_MOUSE);
		gtk_widget_show(GTK_WIDGET(prefs_ui->pbox));
	}
	gdk_window_raise(GTK_WIDGET(prefs_ui->pbox)->window);
}


/* Changed the function parameters -- SnM */
static void revert_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	static gchar msg[MESSAGE_LEN + 1];
	
	HexDocument *doc;
	GnomeMessageBox *mbox;
	gint reply;
	
	if(bonobo_mdi_get_active_child (BONOBO_MDI (mdi))) {
		doc = HEX_DOCUMENT (bonobo_mdi_get_active_child (BONOBO_MDI (mdi)));
		if(doc->changed) {
			g_snprintf(msg, MESSAGE_LEN,
					   _("Really revert file %s?"),
					   bonobo_mdi_child_get_name (BONOBO_MDI_CHILD (doc)));
			mbox = GNOME_MESSAGE_BOX(gnome_message_box_new(msg,
														   GNOME_MESSAGE_BOX_QUESTION,
														   GNOME_STOCK_BUTTON_YES,
														   GNOME_STOCK_BUTTON_NO,
														   NULL));
			gnome_dialog_set_default(GNOME_DIALOG(mbox), 2);
			reply = ask_user(mbox);
			
			if(reply == 0) {
				gchar *flash;

				hex_document_read(doc);
				flash = g_strdup_printf(_("Reverted buffer from file %s"), HEX_DOCUMENT(bonobo_mdi_get_active_child (BONOBO_MDI (mdi)))->file_name);
				bonobo_window_flash(bonobo_mdi_get_active_window (BONOBO_MDI (mdi)), flash);
				g_free(flash);
			}
		}
	}
}

static void open_selected_file(GtkWidget *w)
{
	HexDocument *new_doc;
	gchar *flash;
	
	if((new_doc = hex_document_new((gtk_file_selection_get_filename (GTK_FILE_SELECTION (file_sel))))) != NULL) {
		bonobo_mdi_add_child(BONOBO_MDI (mdi), BONOBO_MDI_CHILD(new_doc));
		bonobo_mdi_add_view(BONOBO_MDI (mdi), BONOBO_MDI_CHILD(new_doc));
		flash = g_strdup_printf(_("Loaded file %s"), new_doc->file_name);
		bonobo_window_flash(bonobo_mdi_get_active_window (BONOBO_MDI (mdi)), flash);
		g_free(flash);

		/* If we have created the converter window enable the 
	 	 * "Get cursor value" button
	 	 */
		if (get)
			gtk_widget_set_sensitive(get, TRUE);
	
	}
	else
		display_error_dialog (bonobo_mdi_get_active_window (BONOBO_MDI (mdi)), _("Can not open file!"));

	gtk_widget_destroy(GTK_WIDGET(file_sel));
	file_sel = NULL;
}

static void save_selected_file(GtkWidget *w, GtkWidget *view)
{
	HexDocument *doc;
	FILE *file;
	const gchar *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_sel));
	gchar *flash;
	int i;
	
	doc = HEX_DOCUMENT(bonobo_mdi_get_child_from_view(view));
	
	if((file = fopen(filename, "w")) != NULL) {
		if(hex_document_write_to_file(doc, file)) {
			if(doc->file_name)
				g_free(doc->file_name);
			doc->file_name = strdup(filename);
			doc->changed = FALSE;

			for(i = strlen(doc->file_name);
				(i >= 0) && (doc->file_name[i] != '/');
				i--)
				;
			if(doc->file_name[i] == '/')
				doc->path_end = &doc->file_name[i+1];
			else
				doc->path_end = doc->file_name;
			
			bonobo_mdi_child_set_name(BONOBO_MDI_CHILD(doc), doc->path_end);

			flash = g_strdup_printf(_("Saved buffer to file %s"), HEX_DOCUMENT(bonobo_mdi_get_active_child (BONOBO_MDI (mdi)))->file_name);
			bonobo_window_flash(bonobo_mdi_get_active_window (BONOBO_MDI (mdi)), flash);
			g_free(flash);
		}
		else
			display_error_dialog (bonobo_mdi_get_active_window (BONOBO_MDI (mdi)), _("Error saving file!"));
		fclose(file);
	}
	else
		display_error_dialog (bonobo_mdi_get_active_window (BONOBO_MDI (mdi)), _("Can't open file for writing!"));
	
	gtk_widget_destroy(GTK_WIDGET(file_sel));
	file_sel = NULL;
}

/**
 * ghex_print
 * @preview: Indicates whether to show only a print preview (TRUE) or
to display the print dialog.
 *
 * Prints or previews the current document.
 **/
static void
ghex_print(gboolean preview)
{
	GHexPrintJobInfo *pji;
	HexDocument *doc;
	GtkWidget *active_view;
	gboolean cancel = FALSE;

	if (!bonobo_mdi_get_active_child(BONOBO_MDI(mdi)))
		return;

	doc = HEX_DOCUMENT(bonobo_mdi_get_active_child(BONOBO_MDI(mdi)));
	active_view = bonobo_mdi_get_active_view (BONOBO_MDI (mdi));
	if (!active_view || !doc)
		return;

	pji = ghex_print_job_info_new(doc, GTK_HEX(active_view)->group_type);

	if (!pji)
		return;

	pji->preview = preview;

	if (!pji->preview)
		cancel = ghex_print_run_dialog(pji);
	else
		pji->config = gnome_print_config_default();

	/* Cancel clicked */
	if (cancel) {
		ghex_print_job_info_destroy(pji);
		return;
	}

	g_return_if_fail (pji->config != NULL);

	pji->master = gnome_print_master_new_from_config (pji->config);
	g_return_if_fail (pji->master != NULL);

	ghex_print_update_page_size_and_margins (doc, pji);
	ghex_print_job_execute(pji);

	if (pji->preview)
		ghex_print_preview_real(pji);
	else
		gnome_print_master_print(pji->master);

	ghex_print_job_info_destroy(pji);

}

/**
 * ghex_print_run_dialog
 * @pji: Pointer to a GHexPrintJobInfo object.
 *
 * Return value: TRUE if cancel was clicked, FALSE otherwise.
 *
 * Runs the GHex print dialog.
 **/
static gboolean
ghex_print_run_dialog(GHexPrintJobInfo *pji)
{
	GtkWidget *dialog;
	gint res;

	dialog = (GnomeDialog *) gnome_print_dialog_new(
			(const char *) _("Print Hex Document"),
			GNOME_PRINT_DIALOG_RANGE);

	gnome_print_dialog_construct_range_page((GnomePrintDialog *)dialog,
			GNOME_PRINT_RANGE_ALL | GNOME_PRINT_RANGE_RANGE,
			1, pji->pages, "A", _("Pages"));

	res = gtk_dialog_run(GTK_DIALOG(dialog));

	switch (res) {
		case GNOME_PRINT_DIALOG_RESPONSE_PRINT:
			break;
		case GNOME_PRINT_DIALOG_RESPONSE_PREVIEW:
			pji->preview = TRUE;
			break;
		case -1:
			return TRUE;
		default:
			gtk_widget_destroy(dialog);
			return TRUE;
	};

	g_return_val_if_fail (pji->config == NULL, TRUE);
	pji->config = gnome_print_dialog_get_config(
			GNOME_PRINT_DIALOG(dialog));

#if 0
	if (pji->printer && !pji->preview)
		gnome_print_master_set_printer(pji->master, pji->printer);
#endif

	pji->range = gnome_print_dialog_get_range_page(
			GNOME_PRINT_DIALOG(dialog),
			&pji->page_first, &pji->page_last);

	gtk_widget_destroy (dialog);
	return FALSE;
}

/**
 * ghex_print_preview_real:
 * @pji: Pointer to a GHexPrintJobInfo object.
 *
 * Previews the print job.
 **/
static void
ghex_print_preview_real(GHexPrintJobInfo *pji)
{
	GnomePrintMasterPreview *preview;
	gchar *title;

	title = g_strdup_printf(_("GHex (%s): Print Preview"),
			pji->doc->file_name);
	preview = gnome_print_master_preview_new(pji->master, title);
	g_free(title);

	gtk_widget_show(GTK_WIDGET(preview));
}

/*
 * Use this instead of gnome_app_error
 * Jan 19th 2001 -- SnM
 */

void display_error_dialog (BonoboWindow *win, const gchar *msg)
{
	GtkWidget *error_dlg;

	g_return_if_fail (win != NULL);
	g_return_if_fail (msg != NULL);
	error_dlg = gtk_message_dialog_new (
			GTK_WINDOW (win),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_OK,
			msg);

	gtk_dialog_set_default_response (GTK_DIALOG (error_dlg), GTK_RESPONSE_OK);
	gtk_window_set_resizable (GTK_WINDOW (error_dlg), FALSE);
	gtk_dialog_run (GTK_DIALOG (error_dlg));
	gtk_widget_destroy (error_dlg);
}
