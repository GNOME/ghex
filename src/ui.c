/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ui.c - main menus and callbacks; utility functions

   Copyright (C) 1998, 1999, 2000 Free Software Foundation

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
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-printer-dialog.h>
#include "ghex.h"

static void open_selected_file(GtkWidget *);
static void save_selected_file(GtkWidget *, GtkWidget *view);
static void export_html_selected_file(GtkWidget *w, GtkHex *view);

/* callbacks to nullify widget pointer after a delete event */
static void about_destroy_cb(GtkObject *, GtkWidget **);

/* callbacks for global menus */
static void quit_app_cb(GtkWidget *);
static void open_cb(GtkWidget *);
static void close_cb(GtkWidget *);
static void save_cb(GtkWidget *);
static void save_as_cb(GtkWidget *);
static void print_cb(GtkWidget *w);
static void export_html_cb(GtkWidget *);
static void revert_cb(GtkWidget *);
static void prefs_cb(GtkWidget *);
static void converter_cb(GtkWidget *);
static void char_table_cb(GtkWidget *);
static void about_cb(GtkWidget *);

GnomeUIInfo file_menu[] = {
	GNOMEUIINFO_MENU_OPEN_ITEM(open_cb,NULL),
	/* keep in sync: main.c/child_changed_cb: setting sensitivity of items 1 - 3 */
	GNOMEUIINFO_MENU_SAVE_ITEM(save_cb,NULL),
	GNOMEUIINFO_MENU_SAVE_AS_ITEM(save_as_cb,NULL),
	GNOMEUIINFO_ITEM(N_("Export to HTML..."), N_("Export data to HTML source"), export_html_cb, NULL),
	GNOMEUIINFO_MENU_REVERT_ITEM(revert_cb,NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_PRINT_ITEM(print_cb,NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CLOSE_ITEM(close_cb,NULL),
	GNOMEUIINFO_MENU_EXIT_ITEM(quit_app_cb,NULL),
	GNOMEUIINFO_END
};

GnomeUIInfo settings_menu[] = {
	GNOMEUIINFO_MENU_PREFERENCES_ITEM(prefs_cb,NULL),
	GNOMEUIINFO_END
};

GnomeUIInfo tools_menu[] = {
	GNOMEUIINFO_ITEM_NONE(N_("Con_verter..."),
						  N_("Open base conversion dialog"), converter_cb),
	GNOMEUIINFO_ITEM_NONE(N_("Character Table..."),
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
	GNOMEUIINFO_SUBTREE(N_("Tools"), tools_menu),
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

void create_dialog_title(GtkWidget *window, gchar *title)
{
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
static void about_destroy_cb(GtkObject *obj, GtkWidget **about)
{
	*about = NULL;
}

static void about_cb (GtkWidget *widget)
{
	static GtkWidget *about = NULL;
	
	static const gchar *authors[] = {
		"Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>",
		"Chema Chelorio <chema@celorio.com>",
		NULL
	};

	if(!about) {
		about = gnome_about_new ( _("GHex, a binary file editor"), VERSION,
								  "(C) 1998, 1999, 2000 Jaka Mocnik", authors,
								  _("Released under the terms of GNU Public License"), NULL);
		gtk_signal_connect(GTK_OBJECT(about), "destroy",
						   GTK_SIGNAL_FUNC(about_destroy_cb), &about);
		gtk_widget_show (about);
	}
	else
		gdk_window_raise(GTK_WIDGET(about)->window);
}

static void quit_app_cb (GtkWidget *widget)
{
	if(gnome_mdi_remove_all(mdi, FALSE))
		gtk_object_destroy(GTK_OBJECT(mdi));
}

static void save_cb(GtkWidget *w)
{
	HexDocument *doc;

	if(gnome_mdi_get_active_child(mdi) == NULL)
		return;

	doc = HEX_DOCUMENT(gnome_mdi_get_active_child(mdi));
	if(!hex_document_write(doc))
		gnome_app_error(mdi->active_window, _("Error saving file!"));
	else {
		gchar *flash;
		
		flash = g_strdup_printf(_("Saved buffer to file %s"), doc->file_name);
		gnome_app_flash(mdi->active_window, flash);
		g_free(flash);
	}
}

static void open_cb(GtkWidget *w)
{
	HexDocument *doc;

	if(file_sel && GTK_WIDGET_VISIBLE(file_sel))
		return;

	if(file_sel == NULL)
		file_sel = gtk_file_selection_new(NULL);

	if(gnome_mdi_get_active_child(mdi) != NULL) {
		doc = HEX_DOCUMENT(gnome_mdi_get_active_child(mdi));
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

	if(!GTK_WIDGET_VISIBLE(file_sel)) {
		gtk_window_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
		gtk_widget_show (file_sel);
	}
}

static void save_as_cb(GtkWidget *w)
{
	HexDocument *doc;

	if(gnome_mdi_get_active_child(mdi) == NULL  ||
	   (file_sel && GTK_WIDGET_VISIBLE(file_sel)))
		return;
	
	doc = HEX_DOCUMENT(gnome_mdi_get_active_child(mdi));

	if(file_sel == NULL)
		file_sel = gtk_file_selection_new(NULL);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_sel), doc->file_name);

	gtk_window_set_title(GTK_WINDOW(file_sel), _("Select a file to save buffer as"));
	
	gtk_window_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->ok_button),
						"clicked", GTK_SIGNAL_FUNC(save_selected_file),
						gnome_mdi_get_active_view(mdi));
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->cancel_button),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						file_sel);
	gtk_widget_show (file_sel);
}

static void print_dialog_clicked_cb(GtkWidget *widget, gint button, gpointer data)
{
	if(button == 0) {
		GnomePrinter *printer;
		GnomePrinterDialog *dialog = GNOME_PRINTER_DIALOG(widget);
		HexDocument *doc = (HexDocument *)data;

		printer = gnome_printer_dialog_get_printer(dialog);

		if(printer) {
			GtkWidget *active_view;
			guint group_type = 1;

			active_view = gnome_mdi_get_active_view(mdi);
			if(active_view)
				group_type = GTK_HEX(active_view)->group_type;

			print_document(doc, group_type, printer);
		}
    }
  
	gnome_dialog_close(GNOME_DIALOG(widget));
}

static void print_cb(GtkWidget *w)
{
	HexDocument *doc;
	GtkWidget *dialog;

	if(gnome_mdi_get_active_child(mdi) == NULL  ||
	   (file_sel && GTK_WIDGET_VISIBLE(file_sel)))
		return;
	
	doc = HEX_DOCUMENT(gnome_mdi_get_active_child(mdi));

	if(doc == NULL)
		return;

	dialog = gnome_printer_dialog_new ();

	gnome_dialog_set_parent(GNOME_DIALOG(dialog),
							GTK_WINDOW(mdi->active_window));
	gtk_signal_connect(GTK_OBJECT(dialog), "clicked",
					   (GtkSignalFunc)print_dialog_clicked_cb, doc);

	gtk_widget_show_all(dialog);
}

static void export_html_cb(GtkWidget *w)
{
	HexDocument *doc;

	if(gnome_mdi_get_active_child(mdi) == NULL ||
	   (file_sel && GTK_WIDGET_VISIBLE(file_sel)))
		return;

	doc = HEX_DOCUMENT(gnome_mdi_get_active_child(mdi));

	if(file_sel == NULL)
		file_sel = gtk_file_selection_new(NULL);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_sel), doc->file_name);

	gtk_window_set_title(GTK_WINDOW(file_sel), _("Select path and file name for the HTML source"));
	
	gtk_window_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->ok_button),
						"clicked", GTK_SIGNAL_FUNC(export_html_selected_file),
						gnome_mdi_get_active_view(mdi));
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->cancel_button),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						file_sel);
	gtk_widget_show (file_sel);
}

static void export_html_selected_file(GtkWidget *w, GtkHex *view)
{
	gchar *html_path = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_sel));
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

	if(*base_name == 0)
		return;
	
	doc = HEX_DOCUMENT(gnome_mdi_get_child_from_view(GTK_WIDGET(view)));

	hex_document_export_html(doc, html_path, base_name, 0, doc->file_size,
							 view->cpl, view->vis_lines, view->group_type);
}

static void close_cb(GtkWidget *w)
{
	if(gnome_mdi_get_active_child(mdi) == NULL)
		return;

	gnome_mdi_remove_child(mdi, gnome_mdi_get_active_child(mdi), FALSE);
}

static void converter_cb(GtkWidget *w)
{
	if(!converter)
		converter = create_converter();

	if(!GTK_WIDGET_VISIBLE(converter->window)) {
		gtk_window_position(GTK_WINDOW(converter->window), GTK_WIN_POS_MOUSE);
		gtk_widget_show(converter->window);
	}
	gdk_window_raise(converter->window->window);
}

static void char_table_cb(GtkWidget *w)
{
	if(!char_table)
		char_table = create_char_table();

	if(!GTK_WIDGET_VISIBLE(char_table)) {
		gtk_window_position(GTK_WINDOW(char_table), GTK_WIN_POS_MOUSE);
		gtk_widget_show(char_table);
	}
	gdk_window_raise(char_table->window);
}

static void prefs_cb(GtkWidget *w)
{
	if(!prefs_ui)
		prefs_ui = create_prefs_dialog();

	gnome_dialog_set_default(GNOME_DIALOG(prefs_ui->pbox), 2);
	if(!GTK_WIDGET_VISIBLE(prefs_ui->pbox)) {
		gtk_window_position (GTK_WINDOW(prefs_ui->pbox), GTK_WIN_POS_MOUSE);
		gtk_widget_show(GTK_WIDGET(prefs_ui->pbox));
	}
	gdk_window_raise(GTK_WIDGET(prefs_ui->pbox)->window);
}

static void revert_cb(GtkWidget *w)
{
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

static void open_selected_file(GtkWidget *w)
{
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

static void save_selected_file(GtkWidget *w, GtkWidget *view)
{
	HexDocument *doc;
	FILE *file;
	gchar *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_sel));
	gchar *flash;
	int i;
	
	doc = HEX_DOCUMENT(gnome_mdi_get_child_from_view(view));
	
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
			
			gnome_mdi_child_set_name(GNOME_MDI_CHILD(doc), doc->path_end);

			flash = g_strdup_printf(_("Saved buffer to file %s"), HEX_DOCUMENT(mdi->active_child)->file_name);
			gnome_app_flash(mdi->active_window, flash);
			g_free(flash);
		}
		else
			gnome_app_error(mdi->active_window, _("Error saving file!"));
		fclose(file);
	}
	else
		gnome_app_error(mdi->active_window, _("Can't open file for writing!"));
	
	gtk_widget_hide(GTK_WIDGET(file_sel));
}
