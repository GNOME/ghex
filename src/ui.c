/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ui.c - main menus and callbacks; utility functions

   Copyright (C) 1998 - 2004 Free Software Foundation

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
#include <string.h>
#include <unistd.h> /* for F_OK and W_OK */

#include <gtk/gtk.h>

#include <libgnomeprint/gnome-print.h>
#include <libgnomeprintui/gnome-print-dialog.h>
#include <libgnomeprintui/gnome-print-job-preview.h>

#include "ui.h"
#include "ghex-window.h"
#include "findreplace.h"
#include "converter.h"
#include "print.h"
#include "chartable.h"

static void ghex_print(GtkHex *gh, gboolean preview);
static void ghex_print_run_dialog(GHexPrintJobInfo *pji);
static void ghex_print_preview_real(GHexPrintJobInfo *pji);
static void ghex_print_document_real (GHexPrintJobInfo *pji, gboolean preview);

/* callbacks to nullify widget pointer after a delete event */
static void open_cb (BonoboUIComponent *uic, gpointer user_data,
					 const gchar* verbname);
static void close_cb (BonoboUIComponent *uic, gpointer user_data,
					  const gchar* verbname);
static void save_cb (BonoboUIComponent *uic, gpointer user_data,
					 const gchar* verbname);
static void save_as_cb (BonoboUIComponent *uic, gpointer user_data,
						const gchar* verbname);
static void print_cb (BonoboUIComponent *uic, gpointer user_data,
					  const gchar* verbname);
static void print_preview_cb (BonoboUIComponent *uic, gpointer user_data,
							  const gchar* verbname);
static void export_html_cb (BonoboUIComponent *uic, gpointer user_data,
							const gchar* verbname);
static void revert_cb (BonoboUIComponent *uic, gpointer user_data,
					   const gchar* verbname);
static void prefs_cb (BonoboUIComponent *uic, gpointer user_data,
					  const gchar* verbname);
static void about_cb (BonoboUIComponent *uic, gpointer user_data,
					  const gchar* verbname);
static void cut_cb (BonoboUIComponent *uic, gpointer user_data,
					const gchar* verbname);
static void copy_cb (BonoboUIComponent *uic, gpointer user_data,
					 const gchar* verbname);
static void paste_cb (BonoboUIComponent *uic, gpointer user_data,
					  const gchar* verbname);
static void help_cb (BonoboUIComponent *uic, gpointer user_data,
					 const gchar* verbname);
static void DnDNewWindow_cb(BonoboUIComponent *uic, gpointer user_data,
							const char *cname);
static void DnDCancel_cb(BonoboUIComponent *uic, gpointer user_data,
						 const char *cname);

guint group_type[3] = {
	GROUP_BYTE,
	GROUP_WORD,
	GROUP_LONG,
};

gchar *group_type_label[3] = {
	N_("_Bytes"),
	N_("_Words"),
	N_("_Longwords"),
};

guint search_type = 0;
gchar *search_type_label[] = {
	N_("hex data"),
	N_("ASCII data"),
};

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
	BONOBO_UI_VERB ("EditUndo", undo_cb),
	BONOBO_UI_VERB ("EditRedo", redo_cb),
	BONOBO_UI_VERB ("EditCut", cut_cb),
	BONOBO_UI_VERB ("EditCopy", copy_cb),
	BONOBO_UI_VERB ("EditPaste", paste_cb),
	BONOBO_UI_VERB ("Find", find_cb),
	BONOBO_UI_VERB ("AdvancedFind", advanced_find_cb),
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
delete_event_cb(GtkWidget *w, GdkEventAny *e, GtkWindow *win)
{
	gtk_widget_hide(w);
	
	return TRUE;
}

gint
ask_user(GtkMessageDialog *message_box)
{
	return gtk_dialog_run(GTK_DIALOG(message_box));
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
about_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	gchar *license_translated;

	const gchar *authors[] = {
		"Jaka Mo\304\215nik",
		"Chema Celorio",
		"Shivram Upadhyayula",
		"Rodney Dawes",
		"Jonathon Jongsma",
		NULL
	};

	const gchar *copyright = _("Copyright © 1998 - 2006 Jaka Mo\304\215nik\n"
	                           "Copyright © 2006 - 2010 GHex Contributors");

	/* For documentation_credits */
	#include "../help/ghex2-docs.h"

	const gchar *license[] = {
		N_("This program is free software; you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation; either version 2 of the License, or "
		   "(at your option) any later version."),
		N_("This program is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License "
		   "along with this program; if not, write to the Free Software Foundation, Inc., "
		   "51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA")
	};
	license_translated = g_strjoin ("\n\n",
	                                _(license[0]),
	                                _(license[1]),
	                                _(license[2]),
	                                NULL);

	gtk_show_about_dialog (NULL,
	                       "authors", authors,
	                       "comments", _("A binary file editor"),
	                       "copyright", copyright,
	                       "documenters", documentation_credits,
	                       "license", license_translated,
	                       "logo-icon-name", PACKAGE,
	                       "program-name", "GHex",
	                       "title", _("About GHex"),
	                       "translator-credits", _("translator-credits"),
	                       "version", PACKAGE_VERSION,
	                       "website", "http://live.gnome.org/Ghex",
	                       "website-label", _("GHex Website"),
	                       "wrap-license", TRUE,
	                       NULL);

	g_free (license_translated);
}

static void
help_cb (BonoboUIComponent *uic, gpointer user_data, const gchar *verbname)
{
	GError *error = NULL;

	gtk_show_uri (NULL, "ghelp:ghex2",  gtk_get_current_event_time (), &error);

	if (error != NULL) {
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
		gtk_window_present (GTK_WINDOW (dialog));

		g_error_free (error);
	}
}

void 
paste_cb(BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win = GHEX_WINDOW(user_data);

	if(win->gh)
		gtk_hex_paste_from_clipboard(win->gh);
}

void 
copy_cb(BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win = GHEX_WINDOW(user_data);

	if(win->gh)
		gtk_hex_copy_to_clipboard(win->gh);
}

void 
cut_cb(BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win = GHEX_WINDOW(user_data);

	if(win->gh)
		gtk_hex_cut_to_clipboard(win->gh);
}

void
quit_app_cb(BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	const GList *doc_node;
	GHexWindow *win;
	HexDocument *doc;

	doc_node = hex_document_get_list();
	while(doc_node) {
		doc = HEX_DOCUMENT(doc_node->data);
		win = ghex_window_find_for_doc(doc);
		if(win && !ghex_window_ok_to_close(win))
			return;
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

	if(!hex_document_is_writable(doc)) {
		display_error_dialog (win, _("You don't have the permissions to save the file!"));
		return;
	}

	if(!hex_document_write(doc))
		display_error_dialog (win, _("An error occurred while saving file!"));
	else {
		gchar *flash;
		gchar *gtk_file_name;

		gtk_file_name = g_filename_to_utf8 (doc->file_name, -1,
											NULL, NULL, NULL);
		flash = g_strdup_printf(_("Saved buffer to file %s"), gtk_file_name);

		ghex_window_flash (win, flash);
		g_free(gtk_file_name);
		g_free(flash);
	}
}

static void
open_cb(BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win;
	GtkWidget *file_sel;
	GtkResponseType resp;

	win = GHEX_WINDOW(user_data);

	file_sel = gtk_file_chooser_dialog_new(_("Select a file to open"),
										   GTK_WINDOW(win),
										   GTK_FILE_CHOOSER_ACTION_OPEN,
										   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										   GTK_STOCK_OPEN, GTK_RESPONSE_OK,
										   NULL);
	gtk_window_set_modal (GTK_WINDOW(file_sel), TRUE);
	gtk_window_set_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	gtk_widget_show (file_sel);

	resp = gtk_dialog_run(GTK_DIALOG(file_sel));

	if(resp == GTK_RESPONSE_OK) {
		gchar *flash;

		if(GHEX_WINDOW(win)->gh != NULL) {
			win = GHEX_WINDOW(ghex_window_new_from_file(gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_sel))));
			if(win != NULL)
				gtk_widget_show(GTK_WIDGET(win));
		}
		else {
			if(!ghex_window_load(GHEX_WINDOW(win),
								 gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_sel))))
				win = NULL;
		}

		if(win != NULL) {
			gchar *gtk_file_name;
			gtk_file_name = g_filename_to_utf8
				(GHEX_WINDOW(win)->gh->document->file_name, -1, 
				 NULL, NULL, NULL);
			flash = g_strdup_printf(_("Loaded file %s"), gtk_file_name);
			ghex_window_flash(GHEX_WINDOW(win), flash);
			g_free(gtk_file_name);
			g_free(flash);
			if (converter_get)
				gtk_widget_set_sensitive(converter_get, TRUE);
		}
		else
			display_error_dialog (ghex_window_get_active(), _("Can not open file!"));
	}

	gtk_widget_destroy(file_sel);
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

	ghex_window_save_as(win);
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
	GtkWidget *file_sel;
	GtkResponseType resp;

	if(win->gh)
		doc = win->gh->document;
	else
		doc = NULL;

	if(doc == NULL)
		return;

	file_sel = gtk_file_chooser_dialog_new(_("Select path and file name for the HTML source"),
										   GTK_WINDOW(win),
										   GTK_FILE_CHOOSER_ACTION_SAVE,
										   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										   GTK_STOCK_SAVE, GTK_RESPONSE_OK,
										   NULL);
#if 0
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_sel), doc->file_name);
#endif
	gtk_window_set_modal(GTK_WINDOW(file_sel), TRUE);
	gtk_window_set_position(GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	gtk_widget_show(file_sel);

	resp = gtk_dialog_run(GTK_DIALOG(file_sel));

	if(resp == GTK_RESPONSE_OK) {
		gchar *html_path = g_strdup(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_sel)));
		gchar *sep, *base_name, *check_path;
		GtkHex *view = win->gh;

		gtk_widget_destroy(file_sel);

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
			display_error_dialog(win, _("You need to specify a base name for "
										"the HTML files."));
			return;
		}

		check_path = g_strdup_printf("%s/%s.html", html_path, base_name);
		if(access(check_path, F_OK) == 0) {
			gint reply;
			GtkWidget *mbox;

			if(access(check_path, W_OK) != 0) {
				display_error_dialog(win, _("You don't have the permission to write to the selected path.\n"));
				g_free(html_path);
				g_free(check_path);
				return;
			}

			mbox = gtk_message_dialog_new(GTK_WINDOW(win),
										  GTK_DIALOG_MODAL|
										  GTK_DIALOG_DESTROY_WITH_PARENT,
										  GTK_MESSAGE_QUESTION,
										  GTK_BUTTONS_YES_NO,
										  _("Saving to HTML will overwrite some files.\n"
											"Do you want to proceed?"));
			gtk_dialog_set_default_response(GTK_DIALOG(mbox), GTK_RESPONSE_NO);
			reply = ask_user(GTK_MESSAGE_DIALOG(mbox));
			gtk_widget_destroy(mbox);
			if(reply != GTK_RESPONSE_YES) {
				g_free(html_path);
				g_free(check_path);
				return;
			}
		}
		else {
			if(access(html_path, W_OK) != 0) {
				display_error_dialog(win, _("You don't have the permission to write to the selected path.\n"));
				g_free(html_path);
				g_free(check_path);
				return;
			}
		}
		g_free(check_path);

		hex_document_export_html(doc, html_path, base_name, 0, doc->file_size,
								 view->cpl, view->vis_lines, view->group_type);
		g_free(html_path);
	}
	else
		gtk_widget_destroy(GTK_WIDGET(file_sel));
}

static void
close_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win = GHEX_WINDOW(user_data), *other_win;
	HexDocument *doc;
	const GList *window_list;

	if(win->gh == NULL) {
        if(ghex_window_get_list()->next != NULL)
            gtk_widget_destroy(GTK_WIDGET(win));
		return;
	}

	doc = win->gh->document;
	
	if(!ghex_window_ok_to_close(win))
		return;
	
	window_list = ghex_window_get_list();
	while(window_list) {
		other_win = GHEX_WINDOW(window_list->data);
		ghex_window_remove_doc_from_list(other_win, doc);
		window_list = window_list->next;
		if(other_win->gh && other_win->gh->document == doc && other_win != win)
			gtk_widget_destroy(GTK_WIDGET(other_win));
	}


	/* If we have created the converter window disable the 
	 * "Get cursor value" button
	 */
	if (converter_get)
		gtk_widget_set_sensitive(converter_get, FALSE);

    if(ghex_window_get_list()->next == NULL) {
        bonobo_window_set_contents(BONOBO_WINDOW(win), NULL);
		win->gh = NULL;
        ghex_window_set_sensitivity(win);
		ghex_window_set_doc_name(win, NULL);
    }
    else
        gtk_widget_destroy(GTK_WIDGET(win));	

    /* this implicitly destroys all views including this one */
    g_object_unref(G_OBJECT(doc));

    /* This is to clear the contents of status  bar after closing the files */
    bonobo_ui_component_set_status(win->uic, " ", NULL);

}

void
raise_and_focus_widget (GtkWidget *widget)
{
	if(!GTK_WIDGET_REALIZED (widget))
		return;

	gtk_window_present(GTK_WINDOW(widget));
}

void
file_list_activated_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win;
	HexDocument *doc = HEX_DOCUMENT(user_data);
	const GList *window_list;

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

/* Changed the function parameters -- SnM */
static
void prefs_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	if(!prefs_ui)
		prefs_ui = create_prefs_dialog();

	set_current_prefs(prefs_ui);

	if(ghex_window_get_active() != NULL)
		gtk_window_set_transient_for(GTK_WINDOW(prefs_ui->pbox),
									 GTK_WINDOW(ghex_window_get_active()));
	if(!GTK_WIDGET_VISIBLE(prefs_ui->pbox)) {
		gtk_window_set_position (GTK_WINDOW(prefs_ui->pbox), GTK_WIN_POS_MOUSE);
		gtk_widget_show(GTK_WIDGET(prefs_ui->pbox));
	}
	raise_and_focus_widget(GTK_WIDGET(prefs_ui->pbox));
}


static void
revert_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GHexWindow *win;
   	HexDocument *doc;
	GtkWidget *mbox;
	gint reply;
	
	win = GHEX_WINDOW(user_data);
	if(win->gh)
		doc = win->gh->document;
	else
		doc = NULL;

	if(doc == NULL)
		return;

	if(doc->changed) {
		mbox = gtk_message_dialog_new(GTK_WINDOW(win),
									  GTK_DIALOG_MODAL|
									  GTK_DIALOG_DESTROY_WITH_PARENT,
									  GTK_MESSAGE_QUESTION,
									  GTK_BUTTONS_YES_NO,
									  _("Really revert file %s?"),
									  doc->path_end);
		gtk_dialog_set_default_response(GTK_DIALOG(mbox), GTK_RESPONSE_NO);

		reply = ask_user(GTK_MESSAGE_DIALOG(mbox));
		
		if(reply == GTK_RESPONSE_YES) {
			gchar *flash;
			gchar *gtk_file_name;

			gtk_file_name = g_filename_to_utf8 (doc->file_name, -1,
												NULL, NULL, NULL);
			win->changed = FALSE;
			hex_document_read(doc);
			flash = g_strdup_printf(_("Reverted buffer from file %s"), gtk_file_name);
			ghex_window_flash(win, flash);
			ghex_window_set_sensitivity(win);
			g_free(gtk_file_name);
			g_free(flash);
		}

		gtk_widget_destroy (mbox);
	}
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

	doc = gh->document;

	pji = ghex_print_job_info_new(doc, gh->group_type);

	if (!pji)
		return;

	pji->preview = preview;

	if (!pji->preview)
		ghex_print_run_dialog(pji);
	else
		pji->config = gnome_print_config_default();

	if (pji->preview) {
		ghex_print_document_real(pji, preview);
		ghex_print_job_info_destroy(pji);
	}
}

static void
ghex_print_progress(gint page, gint ofpages, gpointer data)
{
	GtkProgressBar *progress_bar = GTK_PROGRESS_BAR(data);
	gchar *progress_str;

	gtk_progress_bar_set_fraction(progress_bar,
								  (gdouble)page/(gdouble)ofpages);
	progress_str = g_strdup_printf("%d/%d", page, ofpages);
	gtk_progress_bar_set_text(progress_bar, progress_str);	
	g_free(progress_str);
	while(gtk_events_pending())
		gtk_main_iteration();
}

static gboolean
ignore_cb(GtkWidget *w, GdkEventAny *e, gpointer user_data)
{
	return TRUE;
}

static void
ghex_print_document_real(GHexPrintJobInfo *pji, gboolean preview)
{
	GtkWidget *progress_dialog, *progress_bar;

	g_return_if_fail (pji->config != NULL);

	pji->master = gnome_print_job_new (pji->config);

	g_return_if_fail (pji->master != NULL);

	ghex_print_update_page_size_and_margins (pji->doc, pji);

	progress_dialog = gtk_dialog_new();
	gtk_window_set_resizable(GTK_WINDOW(progress_dialog), FALSE);
	gtk_window_set_modal(GTK_WINDOW(progress_dialog), TRUE);
	g_signal_connect(G_OBJECT(progress_dialog), "delete-event",
					 G_CALLBACK(ignore_cb), NULL);
	gtk_window_set_title(GTK_WINDOW(progress_dialog),
						 _("Printing file..."));
	progress_bar = gtk_progress_bar_new();
	gtk_widget_show(progress_bar);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(progress_dialog)->vbox),
					  progress_bar);
	gtk_widget_show(progress_dialog);

	ghex_print_job_execute(pji, ghex_print_progress, progress_bar);

	gtk_widget_destroy(progress_dialog);

	if(pji->preview)
		ghex_print_preview_real(pji);
	else
		gnome_print_job_print(pji->master);
}

static void
ghex_print_dialog_response (GtkWidget *dialog, int response,
							GHexPrintJobInfo *pji)
{
	pji->preview = FALSE;
 
	switch (response) {
	case GNOME_PRINT_DIALOG_RESPONSE_PRINT:
		break;
	case GNOME_PRINT_DIALOG_RESPONSE_PREVIEW:
		pji->preview = TRUE;
		break;
	default:
		gtk_widget_destroy(dialog);
		ghex_print_job_info_destroy (pji);
		return;
	}

	pji->range = gnome_print_dialog_get_range_page
		(GNOME_PRINT_DIALOG(dialog), &pji->page_first, &pji->page_last);
	
	ghex_print_document_real (pji, pji->preview);
	
	if (response == GNOME_PRINT_DIALOG_RESPONSE_PRINT) {
		ghex_print_job_info_destroy (pji);
		gtk_widget_destroy(dialog);
	}
}

/**
 * ghex_print_run_dialog
 * @pji: Pointer to a GHexPrintJobInfo object.
 *
 * Return value: TRUE if cancel was clicked, FALSE otherwise.
 *
 * Runs the GHex print dialog.
 **/
static void
ghex_print_run_dialog(GHexPrintJobInfo *pji)
{
	GtkWidget *dialog;

	dialog = gnome_print_dialog_new(pji->master,
			     (const char *) _("Print Hex Document"),
			     GNOME_PRINT_DIALOG_RANGE);

	if (pji->config == NULL) {
		pji->config = gnome_print_dialog_get_config(
						GNOME_PRINT_DIALOG(dialog));
	}

	ghex_print_update_page_size_and_margins (pji->doc, pji);

	gnome_print_dialog_construct_range_page((GnomePrintDialog *)dialog,
			GNOME_PRINT_RANGE_ALL | GNOME_PRINT_RANGE_RANGE,
			1, pji->pages, "A", _("Pages"));

	if(ghex_window_get_active() != NULL)
		gtk_window_set_transient_for(GTK_WINDOW(dialog),
					     GTK_WINDOW(ghex_window_get_active()));

	g_signal_connect (dialog, "response",
			  G_CALLBACK (ghex_print_dialog_response),
			  pji);

	gtk_widget_show (dialog);
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
	preview = gnome_print_job_preview_new(pji->master, title);
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
			"%s",
			msg);

	gtk_dialog_set_default_response (GTK_DIALOG (error_dlg), GTK_RESPONSE_OK);
	gtk_window_set_resizable (GTK_WINDOW (error_dlg), FALSE);
	gtk_dialog_run (GTK_DIALOG (error_dlg));
	gtk_widget_destroy (error_dlg);
}

void
display_info_dialog (GHexWindow *win, const gchar *msg, ...)
{
	GtkWidget *info_dlg;
	gchar *real_msg;
	va_list args;

	g_return_if_fail (win != NULL);
	g_return_if_fail (msg != NULL);
	va_start(args, msg);
	real_msg = g_strdup_vprintf(msg, args);
	va_end(args);
	info_dlg = gtk_message_dialog_new (
			GTK_WINDOW (win),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_OK,
			"%s",
			real_msg);
	g_free(real_msg);

	gtk_dialog_set_default_response (GTK_DIALOG (info_dlg), GTK_RESPONSE_OK);
	gtk_window_set_resizable (GTK_WINDOW (info_dlg), FALSE);
	gtk_dialog_run (GTK_DIALOG (info_dlg));
	gtk_widget_destroy (info_dlg);
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

