/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ui.c - GHex user interface

   Copyright (C) 1998, 1999 Free Software Foundation

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

   Author: Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
*/

#include <config.h>
#include <gnome.h>
#include "ghex.h"

static void open_selected_file(GtkWidget *);
static void save_selected_file(GtkWidget *);

/* callbacks to nullify widget pointer after a delete event */
static void about_destroy_cb(GtkObject *, GtkWidget **);

/* callbacks for global menus */
static void quit_app_cb(GtkWidget *);
static void open_cb(GtkWidget *);
static void close_cb(GtkWidget *);
static void save_cb(GtkWidget *);
static void save_as_cb(GtkWidget *);
static void revert_cb(GtkWidget *);
static void prefs_cb(GtkWidget *);
static void converter_cb(GtkWidget *);
static void about_cb(GtkWidget *);

GnomeUIInfo file_menu[] = {
	GNOMEUIINFO_MENU_OPEN_ITEM(open_cb,NULL),
	GNOMEUIINFO_MENU_SAVE_ITEM(save_cb,NULL),
	GNOMEUIINFO_MENU_SAVE_AS_ITEM(save_as_cb,NULL),
	GNOMEUIINFO_MENU_REVERT_ITEM(revert_cb,NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE(N_("Open Con_verter..."),
			      N_("Open base conversion dialog"), converter_cb),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CLOSE_ITEM(close_cb,NULL),
	GNOMEUIINFO_MENU_EXIT_ITEM(quit_app_cb,NULL),
	GNOMEUIINFO_END
};

GnomeUIInfo settings_menu[] = {
	GNOMEUIINFO_MENU_PREFERENCES_ITEM(prefs_cb,NULL),
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
	GNOMEUIINFO_MENU_SETTINGS_TREE(settings_menu),
	GNOMEUIINFO_MENU_FILES_TREE(empty_menu),
	GNOMEUIINFO_MENU_HELP_TREE(help_menu),
	GNOMEUIINFO_END
};

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

void cancel_cb(GtkWidget *w, GtkWidget **me) {
	gtk_widget_destroy(*me);
	*me = NULL;
}

gint delete_event_cb(GtkWidget *w, gpointer who_cares, GtkWidget **me) {
	gtk_widget_destroy(*me);
	*me = NULL;
	
	return TRUE;
}

gint ask_user(GnomeMessageBox *message_box) {
	gtk_window_set_modal(GTK_WINDOW(message_box), TRUE);	

	return gnome_dialog_run_and_close(GNOME_DIALOG(message_box));
}

GtkWidget *create_button(GtkWidget *window, gchar *type, gchar *text) {
	GtkWidget *button, *pixmap, *label, *hbox;
	
	hbox = gtk_hbox_new(FALSE, 2);
	
	label = gtk_label_new(text);
	pixmap = gnome_stock_pixmap_widget(window, type);
	
	gtk_box_pack_start(GTK_BOX(hbox), pixmap, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 1);
	
	button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(button), hbox);
	
	gtk_widget_show(label);
	gtk_widget_show(pixmap);
	gtk_widget_show(hbox);
	
	return button;
}

void create_dialog_title(GtkWidget *window, gchar *title) {
	gchar *full_title;

	if(!window)
		return;

	if(mdi->active_child)
		full_title = g_strdup_printf(title, mdi->active_child->name);
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
static void about_destroy_cb(GtkObject *obj, GtkWidget **about) {
	*about = NULL;
}

static void about_cb (GtkWidget *widget) {
	static GtkWidget *about = NULL;
	
	static const gchar *authors[] = {
		"Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>",
		NULL
	};

	if(!about) {
		about = gnome_about_new ( _("GHex, a binary file editor"), VERSION,
								  "(C) 1998, 1999 Jaka Mocnik", authors,
								  _("Released under the terms of GNU Public License"), NULL);
		gtk_signal_connect(GTK_OBJECT(about), "destroy",
						   GTK_SIGNAL_FUNC(about_destroy_cb), &about);
		gtk_widget_show (about);
	}
	else
		gdk_window_raise(GTK_WIDGET(about)->window);
}

static void quit_app_cb (GtkWidget *widget) {
	if(gnome_mdi_remove_all(mdi, FALSE))
		gtk_object_destroy(GTK_OBJECT(mdi));
}

static void save_cb(GtkWidget *w) {
	if(mdi->active_child) {
		if(hex_document_write(HEX_DOCUMENT(mdi->active_child)))
			gnome_app_error(mdi->active_window, _("Error saving file!"));
		else {
			gchar *flash;

			flash = g_strdup_printf(_("Saved buffer to file %s"), HEX_DOCUMENT(mdi->active_child)->file_name);
			gnome_app_flash(mdi->active_window, flash);
			g_free(flash);
		}
	}
}

static void open_cb(GtkWidget *w) {
	if(file_sel == NULL)
		file_sel = gtk_file_selection_new(_("Select a file to open"));
	
	gtk_window_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->ok_button),
						"clicked", GTK_SIGNAL_FUNC(open_selected_file),
						NULL);
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->cancel_button),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						&file_sel);
	gtk_widget_show (file_sel);
}

static void save_as_cb(GtkWidget *w) {
	if(mdi->active_child == NULL)
		return;
	
	if(file_sel == NULL)
		file_sel = gtk_file_selection_new(_("Select a file to save buffer as"));
	
	gtk_window_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->ok_button),
						"clicked", GTK_SIGNAL_FUNC(save_selected_file),
						NULL);
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->cancel_button),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						&file_sel);
	gtk_widget_show (file_sel);
}

static void close_cb(GtkWidget *w) {
	if(mdi->active_child == NULL)
		return;
	
	gnome_mdi_remove_child(mdi, mdi->active_child, FALSE);
}

static void converter_cb(GtkWidget *w) {
	if(converter.window == NULL)
		create_converter(&converter);
	
	gtk_window_position (GTK_WINDOW(converter.window), GTK_WIN_POS_MOUSE);
	
	gtk_widget_show(converter.window);
}

static void prefs_cb(GtkWidget *w) {
	if(prefs_ui.pbox == NULL)
		create_prefs_dialog(&prefs_ui);
	
	gtk_window_position (GTK_WINDOW(prefs_ui.pbox), GTK_WIN_POS_MOUSE);
	
	gtk_widget_show(GTK_WIDGET(prefs_ui.pbox));
}

static void revert_cb(GtkWidget *w) {
	static gchar msg[512];
	
	HexDocument *doc;
	GnomeMessageBox *mbox;
	gint reply;
	
	if(mdi->active_child) {
		doc = HEX_DOCUMENT(mdi->active_child);
		if(doc->changed) {
			sprintf(msg, _("Really revert file %s?"), GNOME_MDI_CHILD(doc)->name);
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
				flash = g_strdup_printf(_("Reverted buffer from file %s"), HEX_DOCUMENT(mdi->active_child)->file_name);
				gnome_app_flash(mdi->active_window, flash);
				g_free(flash);
			}
		}
	}
}

static void open_selected_file(GtkWidget *w) {
	HexDocument *new_doc;
	gchar *flash;
	
	if((new_doc = hex_document_new((gtk_file_selection_get_filename (GTK_FILE_SELECTION (file_sel))))) != NULL) {
		gnome_mdi_add_child(mdi, GNOME_MDI_CHILD(new_doc));
		gnome_mdi_add_view(mdi, GNOME_MDI_CHILD(new_doc));
		flash = g_strdup_printf(_("Loaded file %s"), new_doc->file_name);
		gnome_app_flash(mdi->active_window, flash);
		g_free(flash);
	}
	else
		gnome_app_error(mdi->active_window, _("Can not open file!"));
	
	gtk_widget_destroy(GTK_WIDGET(file_sel));
	file_sel = NULL;
}

static void save_selected_file(GtkWidget *w) {
	HexDocument *doc;
	gchar *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_sel));
	gchar *flash;
	int i;
	
	if(mdi->active_child == NULL)
		return;
	
	doc = HEX_DOCUMENT(mdi->active_child);
	
	if((doc->file = fopen(filename, "w")) != NULL) {
		if(fwrite(doc->buffer, doc->buffer_size, 1, doc->file) == 1) {
			if(doc->file_name)
				free(doc->file_name);
			doc->file_name = strdup(filename);
			
			for(i = strlen(doc->file_name);
				(i >= 0) && (doc->file_name[i] != '/');
				i--)
				;
			if(doc->file_name[i] == '/')
				doc->path_end = &doc->file_name[i+1];
			else
				doc->path_end = doc->file_name;
			
			gnome_mdi_child_set_name(GNOME_MDI_CHILD(doc), doc->path_end);

			flash = g_strdup_printf(_("Saved buffer to file %s"), HEX_DOCUMENT(mdi->active_child)->file_name);
			gnome_app_flash(mdi->active_window, flash);
			g_free(flash);
		}
		else
			gnome_app_error(mdi->active_window, _("Error saving file!"));
		fclose(doc->file);
	}
	else
		gnome_app_error(mdi->active_window, _("Can't open file for writing!"));
	
	gtk_widget_destroy(GTK_WIDGET(file_sel));
	file_sel = NULL;
}
