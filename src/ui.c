/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ui.c - main menus and callbacks; utility functions

   Copyright (C) 1998 - 2002 Free Software Foundation

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

static void open_selected_file(GtkWidget *w, gpointer user_data);
static void save_selected_file(GtkWidget *w, GHexWindow *win);
static void export_html_selected_file(GtkWidget *w, GtkHex *view);
static void ghex_print(GtkHex *gh, gboolean preview);
static gboolean ghex_print_run_dialog(GHexPrintJobInfo *pji);
static void ghex_print_preview_real(GHexPrintJobInfo *pji);

/* callbacks to nullify widget pointer after a delete event */
static void about_destroy_cb(GtkObject *, GtkWidget **);

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
static void DnDNewWindow_cb(BonoboUIComponent *uic, gpointer user_data, const char *cname);
static void DnDCancel_cb(BonoboUIComponent *uic, gpointer user_data, const char *cname);

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
	BONOBO_UI_VERB ("DnDNewWindow", DnDNewWindow_cb),
	BONOBO_UI_VERB ("DnDCancel", DnDCancel_cb),
	BONOBO_UI_VERB_END
};

static void
DnDNewWindow_cb(BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	GHexWindow *win = GHEX_WINDOW(user_data);
	gchar **uri;

	uri = win->uris_to_open;
	while(*uri) {
		GtkWidget *newwin;
		if(!g_ascii_strncasecmp("file:", *uri, 5)) {
			newwin = ghex_window_new_from_file((*uri) + 5);
			gtk_widget_show(newwin);
		}
		uri++;
	}
	g_strfreev(win->uris_to_open);
	win->uris_to_open = NULL;
}

static void
DnDCancel_cb(BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
        GHexWindow *win = GHEX_WINDOW(user_data);
        g_strfreev(win->uris_to_open);
        win->uris_to_open = NULL;	
}

void
cancel_cb(GtkWidget *w, GtkWidget *me)
{
	gtk_widget_hide(me);
}

gint
delete_event_cb(GtkWidget *w, GdkEventAny *e)
{
	gtk_widget_hide(w);
	
	return TRUE;
}

gint
ask_user(GnomeMessageBox *message_box)
{
	gtk_window_set_modal(GTK_WINDOW(message_box), TRUE);	

	return gnome_dialog_run_and_close(GNOME_DIALOG(message_box));
}

GtkWidget *
create_button(GtkWidget *window, const gchar *type, gchar *text)
{
	GtkWidget *button, *pixmap, *label, *hbox;
	
	hbox = gtk_hbox_new(FALSE, 2);
	
	label = gtk_label_new_with_mnemonic(text);

	pixmap = gtk_image_new_from_stock (type, GTK_ICON_SIZE_BUTTON);

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

void
create_dialog_title(GtkWidget *window, gchar *title)
{
	gchar *full_title;
	GHexWindow *win;

	if(!window)
		return;

	win = ghex_window_get_active();

	if(win != NULL && win->gh != NULL)
		full_title = g_strdup_printf(title, win->gh->document->path_end);
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
static void
about_destroy_cb(GtkObject *obj, GtkWidget **about)
{
	*about = NULL;
}

static void
about_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
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

static void
help_cb (BonoboUIComponent *uic, gpointer user_data, const gchar *verbname)
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

void
quit_app_cb(BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	const GList *win_node, *doc_node;
	GHexWindow *win;
	HexDocument *doc;

	doc_node = hex_document_get_list();
	while(doc_node) {
		doc = HEX_DOCUMENT(doc_node->data);
		if(hex_document_has_changed(doc)) {
			if(!hex_document_ok_to_close(doc))
				return;
		}
		doc_node = doc_node->next;
	}
	bonobo_main_quit();
}

static void
save_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win = GHEX_WINDOW(user_data);
	HexDocument *doc;

	if(win->gh)
		doc = win->gh->document;
	else
		doc = NULL;

	if(doc == NULL)
		return;

	if(!hex_document_write(doc))
		display_error_dialog (win, _("Error saving file!"));
	else {
		gchar *flash;
		
		flash = g_strdup_printf(_("Saved buffer to file %s"), doc->file_name);

		ghex_window_flash (win, flash);

		g_free(flash);
	}
}

static void
open_cb(BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win;
	HexDocument *doc;

	win = GHEX_WINDOW(user_data);

	if(win->gh)
		doc = win->gh->document;
	else
		doc = NULL;

	if(file_sel && GTK_WIDGET_VISIBLE(file_sel))
		return;

	if(file_sel == NULL)
		file_sel = gtk_file_selection_new(NULL);

	if(doc)
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_sel),
										doc->file_name);

	gtk_window_set_title(GTK_WINDOW(file_sel), _("Select a file to open"));
	
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->ok_button),
						"clicked", GTK_SIGNAL_FUNC(open_selected_file),
						win);
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

static void
save_as_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win = GHEX_WINDOW(user_data);
	HexDocument *doc;

	if(win->gh)
		doc = win->gh->document;
	else
		doc = NULL;

	if(doc == NULL)
		return;

	if (file_sel && GTK_WIDGET_VISIBLE(file_sel))
		return;
	
	if(file_sel == NULL)
		file_sel = gtk_file_selection_new(NULL);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_sel),
									doc->file_name);

	gtk_window_set_title(GTK_WINDOW(file_sel), _("Select a file to save buffer as"));
	
	gtk_window_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->ok_button),
						"clicked", GTK_SIGNAL_FUNC(save_selected_file),
						win);
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->cancel_button),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						file_sel);

	gtk_window_set_modal(GTK_WINDOW(file_sel), TRUE);

	gtk_widget_show (file_sel);
}

static void
print_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win = GHEX_WINDOW(user_data);

	if(win->gh == NULL)
		return;

	ghex_print(win->gh, FALSE);
}

static void
print_preview_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win = GHEX_WINDOW(user_data);

	if(win->gh == NULL)
		return;

	ghex_print(win->gh, TRUE);
}

static void
export_html_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win = GHEX_WINDOW(user_data);
	HexDocument *doc;

	if(win->gh)
		doc = win->gh->document;
	else
		doc = NULL;

	if(doc == NULL)
		return;

	if(file_sel == NULL)
		file_sel = gtk_file_selection_new(NULL);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_sel), doc->file_name);

	gtk_window_set_title(GTK_WINDOW(file_sel), _("Select path and file name for the HTML source"));
	
	gtk_window_position(GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	
	gtk_signal_connect(GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->ok_button),
						"clicked", GTK_SIGNAL_FUNC(export_html_selected_file),
						win);
	gtk_signal_connect(GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->cancel_button),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						file_sel);
	gtk_window_set_modal(GTK_WINDOW(file_sel), TRUE);
	gtk_widget_show(file_sel);
}

static void
export_html_selected_file(GtkWidget *w, GtkHex *view)
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

	doc = view->document;

	hex_document_export_html(doc, html_path, base_name, 0, doc->file_size,
							 view->cpl, view->vis_lines, view->group_type);

	gtk_widget_destroy(GTK_WIDGET(file_sel));
	file_sel = NULL;
	g_free(html_path);
}

/* Defined in converter.c: used by close_cb and converter_cb */
extern GtkWidget *get;

static void
close_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win = GHEX_WINDOW(user_data);
	HexDocument *doc;
	const GList *window_list;

	if(win->gh == NULL) {
		gtk_widget_destroy(GTK_WIDGET(win));
		return;
	}

	doc = win->gh->document;
	
	if(hex_document_has_changed(doc)) {
		if(!hex_document_ok_to_close(doc))
			return;
	}	

	window_list = ghex_window_get_list();
	while(window_list) {
		win = GHEX_WINDOW(window_list->data);
		window_list = window_list->next;
		if(win->gh && win->gh->document == doc)
			gtk_widget_destroy(GTK_WIDGET(win));
	}

	/* this implicitly destroys all views including this one */
	g_object_unref(G_OBJECT(doc));

	/* If we have created the converter window disable the 
	 * "Get cursor value" button
	 */
	if (get)
		gtk_widget_set_sensitive(get, FALSE);
}

void
raise_and_focus_widget (GtkWidget *widget)
{
	if(!GTK_WIDGET_REALIZED (widget))
		return;

	gdk_window_raise (widget->window);
	gtk_widget_grab_focus (widget);
}

void
file_list_activated_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win;
	HexDocument *doc = HEX_DOCUMENT(user_data);
	GList *window_list;

	window_list = ghex_window_get_list();
	while(window_list) {
		win = GHEX_WINDOW(window_list->data);
		if(win->gh && win->gh->document == doc)
			break;
		window_list = window_list->next;
	}

	if(window_list) {
		win = GHEX_WINDOW(window_list->data);
		raise_and_focus_widget(GTK_WIDGET(win));
	}
}

static void
converter_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	if(!converter)
		converter = create_converter();

	if(!GTK_WIDGET_VISIBLE(converter->window)) {
		gtk_window_position(GTK_WINDOW(converter->window), GTK_WIN_POS_MOUSE);
		gtk_widget_show(converter->window);
	}
	raise_and_focus_widget(converter->window);

	if (!ghex_window_get_active())
		gtk_widget_set_sensitive(get, FALSE);
	else
		gtk_widget_set_sensitive(get, TRUE);
}

static void
char_table_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	if(!char_table)
		char_table = create_char_table();

	if(!GTK_WIDGET_VISIBLE(char_table)) {
		gtk_window_position(GTK_WINDOW(char_table), GTK_WIN_POS_MOUSE);
		gtk_widget_show(char_table);
	}
	raise_and_focus_widget(char_table);
}


/* Changed the function parameters -- SnM */
static
void prefs_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	/* TODO: why this? */
//	if(!prefs_ui)
		prefs_ui = create_prefs_dialog();

	gnome_dialog_set_default(GNOME_DIALOG(prefs_ui->pbox), 2);
	if(!GTK_WIDGET_VISIBLE(prefs_ui->pbox)) {
		gtk_window_position (GTK_WINDOW(prefs_ui->pbox), GTK_WIN_POS_MOUSE);
		gtk_widget_show(GTK_WIDGET(prefs_ui->pbox));
	}
	raise_and_focus_widget(GTK_WIDGET(prefs_ui->pbox));
}


static void
revert_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	static gchar msg[MESSAGE_LEN + 1];
	GHexWindow *win;
   	HexDocument *doc;
	GnomeMessageBox *mbox;
	gint reply;
	
	win = GHEX_WINDOW(user_data);
	if(win->gh)
		doc = win->gh->document;
	else
		doc = NULL;

	if(doc == NULL)
		return;

	if(doc->changed) {
		g_snprintf(msg, MESSAGE_LEN,
				   _("Really revert file %s?"),
				   doc->path_end);
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
			flash = g_strdup_printf(_("Reverted buffer from file %s"), doc->file_name);
			ghex_window_flash(win, flash);
			ghex_window_set_sensitivity(win);
			g_free(flash);
		}
	}
}

static void
open_selected_file(GtkWidget *w, gpointer user_data)
{
	HexDocument *new_doc;
	GtkWidget *win;
	gchar *flash;

	win = GTK_WIDGET(user_data);
	
	if(GHEX_WINDOW(win)->gh != NULL) {
		win = ghex_window_new_from_file(gtk_file_selection_get_filename (GTK_FILE_SELECTION (file_sel)));
		if(win != NULL)
			gtk_widget_show(win);
	}
	else {
		if(!ghex_window_load(GHEX_WINDOW(win),
							 gtk_file_selection_get_filename (GTK_FILE_SELECTION (file_sel))))
			win = NULL;
	}

	if(win != NULL) {
		flash = g_strdup_printf(_("Loaded file %s"),
								GHEX_WINDOW(win)->gh->document->file_name);
		ghex_window_flash(GHEX_WINDOW(win), flash);
		g_free(flash);
		if (get)
			gtk_widget_set_sensitive(get, TRUE);
	}
	else
		display_error_dialog (ghex_window_get_active(), _("Can not open file!"));

	gtk_widget_destroy(GTK_WIDGET(file_sel));
	file_sel = NULL;
}

static void
save_selected_file(GtkWidget *w, GHexWindow *win)
{
	HexDocument *doc;
	FILE *file;
	const gchar *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_sel));
	gchar *flash;
	int i;

	if(win->gh)
		doc = win->gh->document;
	else
		doc = NULL;
	
	if(doc == NULL)
		return;
	
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
			
			ghex_window_set_doc_name(win, doc->path_end);

			flash = g_strdup_printf(_("Saved buffer to file %s"), doc->file_name);
			ghex_window_flash(win, flash);
			g_free(flash);
		}
		else
			display_error_dialog (win, _("Error saving file!"));
		fclose(file);
	}
	else
		display_error_dialog (win, _("Can't open file for writing!"));
	
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
ghex_print(GtkHex *gh, gboolean preview)
{
	GHexPrintJobInfo *pji;
	HexDocument *doc;
	gboolean cancel = FALSE;

	doc = gh->document;

	pji = ghex_print_job_info_new(doc, gh->group_type);

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

	dialog = gnome_print_dialog_new(
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
	GtkWidget *preview;
	gchar *title;

	title = g_strdup_printf(_("GHex (%s): Print Preview"),
			pji->doc->file_name);
	preview = gnome_print_master_preview_new(pji->master, title);
	g_free(title);

	gtk_widget_show(preview);
}

void
display_error_dialog (GHexWindow *win, const gchar *msg)
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

void
update_dialog_titles()
{
	if(jump_dialog)
		create_dialog_title(jump_dialog->window, _("GHex (%s): Jump To Byte"));
	if(replace_dialog)
      	create_dialog_title(replace_dialog->window, _("GHex (%s): Find & Replace Data")); 
	if(find_dialog)
		create_dialog_title(find_dialog->window, _("GHex (%s): Find Data"));
}

void
add_view_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win = GHEX_WINDOW(user_data);
	GtkWidget *newwin;

	if(win->gh == NULL)
		return;

	newwin = ghex_window_new_from_doc(win->gh->document);
	gtk_widget_show(newwin);
}

void
remove_view_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win = GHEX_WINDOW(user_data);

	ghex_window_close(win);
}

