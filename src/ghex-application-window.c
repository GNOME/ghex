/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ghex-application-window.c - GHex main application window

   Copyright © 2021 Logan Rathbone <poprocks@gmail.com>

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
   If not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Original GHex Author: Jaka Mocnik <jaka@gnu.org>
*/

#include "ghex-application-window.h"

/* These aren't co-dependencies per se, but they call functions from each
 * other, so keep this header here in the source file to avoid issues. */
#include "paste-special.h"

#include <locale.h>

#include <config.h>

/* DEFINES */

#define ACTIVE_GH	\
	(ghex_application_window_get_hex (self))

static GFile *tmp_global_gfile_for_nag_screen;

/* This is dumb, but right now I can't think of a simpler solution. */
static gpointer extra_user_data;

/* ----------------------- */
/* MAIN GOBJECT DEFINITION */
/* ----------------------- */

struct _GHexApplicationWindow
{
	GtkApplicationWindow parent_instance;

	HexWidget *gh;
	HexDialog *dialog;
	GtkWidget *dialog_widget;
	GtkCssProvider *conversions_box_provider;
	GtkAdjustment *adj;
	gboolean can_save;
	gboolean insert_mode;

	GtkWidget *find_dialog;
	GtkWidget *replace_dialog;
	GtkWidget *jump_dialog;
	GtkWidget *chartable;
	GtkWidget *converter;
	GtkWidget *prefs_dialog;
	GtkWidget *paste_special_dialog;
	GtkWidget *copy_special_dialog;

	/* From GtkBuilder: */
	GtkWidget *no_doc_label;
	GtkWidget *hex_notebook;
	GtkWidget *conversions_box;
	GtkWidget *findreplace_box;
	GtkWidget *pane_toggle_button;
	GtkWidget *insert_mode_button;
	GtkWidget *statusbar;
	GtkWidget *pane_revealer;
	GtkWidget *conversions_revealer;
};

/* GHexApplicationWindow - Globals for Properties and Signals */

typedef enum
{
	PROP_CHARTABLE_OPEN = 1,
	PROP_CONVERTER_OPEN,
	PROP_FIND_OPEN,
	PROP_REPLACE_OPEN,
	PROP_JUMP_OPEN,
	PROP_CAN_SAVE,
	PROP_INSERT_MODE,
	N_PROPERTIES
} GHexApplicationWindowProperty;

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/* "Main" actions that should all be greyed out and enabled around the same
 * time - eg, state goes from 'no doc loaded' to 'doc loaded'.
 *
 * Don't put open here, as even when we can do nothing else, the user
 * still can open a document.
 */
static const char *main_actions[] = {
	"ghex.save",
	"ghex.save-as",
	"ghex.revert",
	"ghex.print",
	"ghex.print-preview",
	"win.group-data-by",
	"ghex.show-conversions",
	"ghex.insert-mode",
	"ghex.find",
	"ghex.replace",
	"ghex.jump",
	"ghex.chartable",
	"ghex.converter",
	"ghex.copy-special",
	"ghex.paste-special",
	NULL				/* last action */
};

G_DEFINE_TYPE (GHexApplicationWindow, ghex_application_window,
		GTK_TYPE_APPLICATION_WINDOW)

/* ---- */


/* PRIVATE FORWARD DECLARATIONS */

/* NOTE: see also *SET_SHOW_TEMPLATE macros defined below. */
static void ghex_application_window_set_show_chartable (GHexApplicationWindow *self,
		gboolean show);
static void ghex_application_window_set_show_converter (GHexApplicationWindow *self,
		gboolean show);
static void ghex_application_window_set_show_find (GHexApplicationWindow *self,
		gboolean show);
static void ghex_application_window_set_show_replace (GHexApplicationWindow *self,
		gboolean show);
static void ghex_application_window_set_show_jump (GHexApplicationWindow *self,
		gboolean show);
static void ghex_application_window_set_can_save (GHexApplicationWindow *self,
		gboolean can_save);
static void ghex_application_window_remove_tab (GHexApplicationWindow *self,
		GHexNotebookTab *tab);
static GHexNotebookTab * ghex_application_window_get_current_tab (GHexApplicationWindow *self);

static void update_status_message (GHexApplicationWindow *self);
static void update_gui_data (GHexApplicationWindow *self);
static gboolean assess_can_save (HexDocument *doc);
static void do_close_window (GHexApplicationWindow *self);
static void close_doc_confirmation_dialog (GHexApplicationWindow *self, GHexNotebookTab *tab);
static void show_no_file_loaded_label (GHexApplicationWindow *self);

static void doc_read_ready_cb (GObject *source_object, GAsyncResult *res,
		gpointer user_data);

/* GHexApplicationWindow -- PRIVATE FUNCTIONS */

/* Common macro to apply something to the 'gh' of each tab of the notebook.
 *
 * Between _START and _END, put in function calls that use `gh` to be applied
 * to each gh in a tab. Technically you'll also have access to `notebook`
 * and `tab`, but there is generally no reason to directly access these.
 */
#define NOTEBOOK_GH_FOREACH_START											\
{																			\
	GtkNotebook *notebook = GTK_NOTEBOOK(self->hex_notebook);				\
	int i;																	\
	for (i = gtk_notebook_get_n_pages(notebook) - 1; i >= 0; --i) {			\
		HexWidget *gh;														\
		gh = HEX_WIDGET(gtk_notebook_get_nth_page (notebook, i));			\
/* !NOTEBOOK_GH_FOREACH_START */

#define NOTEBOOK_GH_FOREACH_END												\
	} 																		\
}																			\
/* !NOTEBOOK_GH_FOREACH_END	*/

/* set_*_from_settings
 * These functions all basically set properties from the GSettings global
 * variables. They should be used when a new gh is created, and when we're
 * `foreaching` through all of our tabs via a settings-change cb. See also
 * the `NOTEBOOK_GH_FOREACH_{START,END}` macros above.
 */

static void
set_gtkhex_offsets_column_from_settings (HexWidget *gh)
{
	hex_widget_show_offsets (gh, show_offsets_column);
}

static void
set_gtkhex_group_type_from_settings (HexWidget *gh)
{
	hex_widget_set_group_type (gh, def_group_type);
}

static void
set_dark_mode_from_settings (GHexApplicationWindow *self)
{
	GtkSettings *gtk_settings;

	gtk_settings = gtk_settings_get_default ();

	if (def_dark_mode == DARK_MODE_SYSTEM) {
		g_object_set (G_OBJECT(gtk_settings),
				"gtk-application-prefer-dark-theme",
				sys_default_is_dark,
				NULL);
	} else {
		g_object_set (G_OBJECT(gtk_settings),
				"gtk-application-prefer-dark-theme",
				def_dark_mode == DARK_MODE_ON ? TRUE : FALSE,
				NULL);
	}
}
	

static void
settings_font_changed_cb (GSettings   *settings,
		const gchar	*key,
		gpointer 	user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	NOTEBOOK_GH_FOREACH_START

	common_set_gtkhex_font_from_settings (gh);

	NOTEBOOK_GH_FOREACH_END
}

static void
settings_offsets_column_changed_cb (GSettings   *settings,
		const gchar	*key,
		gpointer 	user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	NOTEBOOK_GH_FOREACH_START

	set_gtkhex_offsets_column_from_settings (gh);

	NOTEBOOK_GH_FOREACH_END
}

static void
settings_group_type_changed_cb (GSettings   *settings,
		const gchar	*key,
		gpointer 	user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	NOTEBOOK_GH_FOREACH_START

	set_gtkhex_group_type_from_settings (gh);

	NOTEBOOK_GH_FOREACH_END
}

/* ! settings*changed_cb 's */

static void
refresh_dialogs (GHexApplicationWindow *self)
{
	if (ACTIVE_GH)
	{
		pane_dialog_set_hex (PANE_DIALOG(self->find_dialog), ACTIVE_GH);
		pane_dialog_set_hex (PANE_DIALOG(self->replace_dialog), ACTIVE_GH);
		pane_dialog_set_hex (PANE_DIALOG(self->jump_dialog), ACTIVE_GH);
	}
}

static GHexNotebookTab *
ghex_application_window_get_current_tab (GHexApplicationWindow *self)
{
	GtkNotebook *notebook;
	HexWidget *gh;
	GHexNotebookTab *tab;

	notebook = GTK_NOTEBOOK(self->hex_notebook);
	gh = HEX_WIDGET(gtk_notebook_get_nth_page (notebook,
			gtk_notebook_get_current_page (notebook)));

	if (gh)
		tab = GHEX_NOTEBOOK_TAB(gtk_notebook_get_tab_label (notebook,
					GTK_WIDGET(gh)));
	else
		tab = NULL;

	return tab;
}

static void
ghex_application_window_remove_tab (GHexApplicationWindow *self,
		GHexNotebookTab *tab)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(self->hex_notebook);
	int page_num;
	HexWidget *tab_gh;

	tab_gh = ghex_notebook_tab_get_hex (tab);
	page_num = gtk_notebook_page_num (notebook, GTK_WIDGET(tab_gh));
	gtk_notebook_remove_page (notebook, page_num);

	if (gtk_notebook_get_n_pages (GTK_NOTEBOOK(self->hex_notebook)) == 1)
		gtk_notebook_set_show_tabs (GTK_NOTEBOOK(self->hex_notebook), FALSE);

	update_gui_data (self);

	if (gtk_notebook_get_n_pages (GTK_NOTEBOOK(self->hex_notebook)) == 0)
		show_no_file_loaded_label (self);
}

static void
file_save_write_cb (HexDocument *doc,
		GAsyncResult *res,
		GHexApplicationWindow *self)
{
	GError *local_error = NULL;
	gboolean write_successful;

	write_successful = hex_document_write_finish (doc, res, &local_error);

	if (write_successful)
	{
		g_debug ("%s: File saved successfully.", __func__);
	}
	else
	{
		GString *full_errmsg = g_string_new (
				_("There was an error saving the file."));

		if (local_error)
			g_string_append_printf (full_errmsg, "\n\n%s", local_error->message);

		display_error_dialog (GTK_WINDOW(self), full_errmsg->str);

		g_string_free (full_errmsg, TRUE);
	}
}

static void
file_save (GHexApplicationWindow *self)
{
	HexDocument *doc;

	doc = hex_widget_get_document (ACTIVE_GH);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	hex_document_write_async (doc,
			NULL,
			(GAsyncReadyCallback)file_save_write_cb,
			self);
}

static void
do_close_window (GHexApplicationWindow *self)
{
	gtk_window_set_application (GTK_WINDOW(self), NULL);
}

static void
close_all_tabs (GHexApplicationWindow *self)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(self->hex_notebook);
	int i;

	g_debug("%s: %d", __func__, gtk_notebook_get_n_pages (notebook));

	for (i = gtk_notebook_get_n_pages(notebook) - 1; i >= 0; --i)
	{
		GHexNotebookTab *tab;
		HexWidget *gh;

		gh = HEX_WIDGET(gtk_notebook_get_nth_page (notebook, i));
		tab = GHEX_NOTEBOOK_TAB(gtk_notebook_get_tab_label (notebook,
					GTK_WIDGET(gh)));

		ghex_application_window_remove_tab (self, tab);
	}
}

static void
close_all_tabs_response_cb (GtkDialog *dialog,
		int        response_id,
		gpointer   user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	
	/* Regardless of what the user chose, get rid of the dialog. */
	gtk_window_destroy (GTK_WINDOW(dialog));

	if (response_id == GTK_RESPONSE_ACCEPT)
	{
		g_debug ("%s: Decided to CLOSE despite changes.",	__func__);
		close_all_tabs (self);
		do_close_window (self);
	}
}

static void
close_all_tabs_confirmation_dialog (GHexApplicationWindow *self)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW(self),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			(_("<b>You have one or more files open with unsaved changes.</b>\n\n"
			   "Are you sure you want to close the window?\n\n")));

	gtk_dialog_add_buttons (GTK_DIALOG(dialog),
			_("_Close Anyway"),		GTK_RESPONSE_ACCEPT,
			_("_Go Back"),			GTK_RESPONSE_REJECT,
			NULL);

	g_signal_connect (dialog, "response",
			G_CALLBACK(close_all_tabs_response_cb), self);

	gtk_widget_show (dialog);
}

static void
check_close_window (GHexApplicationWindow *self)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(self->hex_notebook);
	gboolean unsaved_found = FALSE;
	int i;
	const int num_pages = gtk_notebook_get_n_pages (notebook);

	/* We have only one tab open: */
	if (num_pages == 1)
	{
		GHexNotebookTab *tab = ghex_application_window_get_current_tab (self);
		HexDocument *doc = hex_widget_get_document (ACTIVE_GH);

		if (hex_document_has_changed (doc))
		{
			close_doc_confirmation_dialog (self,
					ghex_application_window_get_current_tab (self));
		}
		else
		{
			do_close_window (self);
		}
		return;
	}

	/* We have more than one tab open: */
	for (i = num_pages - 1; i >= 0; --i)
	{
		HexWidget *gh;
		HexDocument *doc = NULL;

		gh = HEX_WIDGET(gtk_notebook_get_nth_page (notebook, i));
		doc = hex_widget_get_document (gh);

		if (hex_document_has_changed (doc))
			unsaved_found = TRUE;
	}

	if (unsaved_found) {
		close_all_tabs_confirmation_dialog (self);
	} else {
		close_all_tabs (self);
		do_close_window (self);
	}
}

static gboolean
close_request_cb (GtkWindow *window,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	check_close_window (self);

	return GDK_EVENT_STOP;
}

static void
close_doc_response_cb (GtkDialog *dialog,
		int        response_id,
		gpointer   user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	GHexNotebookTab *tab;
	
	tab = ghex_application_window_get_current_tab (self);

	if (response_id == GTK_RESPONSE_ACCEPT)
	{
		file_save (self);
		ghex_application_window_remove_tab (self, tab);
	}
	else if (response_id == GTK_RESPONSE_REJECT)
	{
		ghex_application_window_remove_tab (self, tab);
	}

	gtk_window_destroy (GTK_WINDOW(dialog));

	/* GtkNotebook likes to grab the focus. Possible TODO would be to subclass
	 * GtkNotebook so we can maintain the grab on the gh widget more easily.
	 */
	gtk_widget_grab_focus (GTK_WIDGET (ACTIVE_GH));
}

static void
close_doc_confirmation_dialog (GHexApplicationWindow *self,
		GHexNotebookTab *tab)
{
	GtkWidget *dialog;
	HexDocument *doc;
	GFile *file;
	char *message;
	char *basename = NULL;

	HexWidget *gh = ghex_notebook_tab_get_hex (tab);
	doc = hex_widget_get_document (gh);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	if (G_IS_FILE (file = hex_document_get_file (doc)))
		basename = g_file_get_basename (hex_document_get_file (doc));

	if (basename) {
		message = g_strdup_printf (
				/* Translators: %s is the filename that is currently being
				 * edited. */
				_("<big><b>%s has been edited since opening.</b></big>\n\n"
			   "Would you like to save your changes?"), basename);
		g_free (basename);
	}
	else {
		message = g_strdup (
				_("<b>The buffer has been edited since opening.</b>\n\n"
			   "Would you like to save your changes?"));
	}

	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW(self),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			NULL);
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),
			message);
	g_free (message);

	gtk_dialog_add_buttons (GTK_DIALOG(dialog),
			_("_Save Changes"),		GTK_RESPONSE_ACCEPT,
			_("_Discard Changes"),	GTK_RESPONSE_REJECT,
			_("_Go Back"),			GTK_RESPONSE_CANCEL,
			NULL);

	g_signal_connect (dialog, "response",
			G_CALLBACK(close_doc_response_cb), self);

	gtk_widget_show (dialog);
}

static void
enable_main_actions (GHexApplicationWindow *self, gboolean enable)
{
	for (int i = 0; main_actions[i] != NULL; ++i)
	{
		gtk_widget_action_set_enabled (GTK_WIDGET(self),
				main_actions[i], enable);
	}
}


/* CALLBACKS */

static gboolean
close_tab_shortcut_cb (GtkWidget *widget,
		GVariant *args,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
	GHexNotebookTab *tab = ghex_application_window_get_current_tab (self);

	if (tab)
		g_signal_emit_by_name (tab, "close-request");
	else
		do_close_window (self);

	return TRUE;
}

static void
copy_special (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
	GdkClipboard *clipboard;

	g_return_if_fail (HEX_IS_WIDGET (ACTIVE_GH));

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET(ACTIVE_GH));

	if (! self->copy_special_dialog)
	{
		self->copy_special_dialog = create_copy_special_dialog (self,
				clipboard);
		g_object_add_weak_pointer (G_OBJECT(self->copy_special_dialog),
				(gpointer *)&self->copy_special_dialog);
	}

	gtk_window_present (GTK_WINDOW(self->copy_special_dialog));
}

static void 
paste_special (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
	GdkClipboard *clipboard;

	g_return_if_fail (HEX_IS_WIDGET (ACTIVE_GH));

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET(ACTIVE_GH));

	if (! self->paste_special_dialog)
	{
		self->paste_special_dialog = create_paste_special_dialog (self,
				clipboard);
		g_object_add_weak_pointer (G_OBJECT(self->paste_special_dialog),
				(gpointer *)&self->paste_special_dialog);
	}

	gtk_window_present (GTK_WINDOW(self->paste_special_dialog));
}

static gboolean
assess_can_save (HexDocument *doc)
{
	gboolean can_save = FALSE;
	GFile *file = hex_document_get_file (doc);

	/* Can't save if we have a new document that is still untitled. */
	if (G_IS_FILE (file)  &&  g_file_peek_path (file))
		can_save = hex_document_has_changed (doc);

	return can_save;
}

static void
show_hex_notebook (GHexApplicationWindow *self)
{
	gtk_widget_hide (self->no_doc_label);
	gtk_widget_show (self->hex_notebook);
}

static void
show_no_file_loaded_label (GHexApplicationWindow *self)
{
	gtk_widget_hide (self->hex_notebook);
	gtk_widget_show (self->no_doc_label);
}

static void
file_saved_cb (HexDocument *doc,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	ghex_application_window_set_can_save (self, assess_can_save (doc));
}

static void
document_loaded_or_saved_common (GHexApplicationWindow *self,
		HexDocument *doc)
{
	/* The appwindow as a whole not interested in any document changes that
	 * don't pertain to the one that is actually in view.
	 */
	if (doc != hex_widget_get_document (ACTIVE_GH))
		return;

	ghex_application_window_set_can_save (self, assess_can_save (doc));
}

static void
file_loaded (HexDocument *doc, GHexApplicationWindow *self)
{
	document_loaded_or_saved_common (self, doc);
	update_gui_data (self);
}


static void
document_changed_cb (HexDocument *doc,
		gpointer change_data,
		gboolean push_undo,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	document_loaded_or_saved_common (self, doc);
}

static void
tab_close_request_cb (GHexNotebookTab *tab,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	HexDocument *doc;
	HexWidget *gh;

	gh = ghex_notebook_tab_get_hex (tab);
	doc = hex_widget_get_document (gh);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	if (hex_document_has_changed (doc)) {
		close_doc_confirmation_dialog (self, tab);
	} else {
		ghex_application_window_remove_tab (self, tab);
	}
}

static void
notebook_page_changed_cb (GtkNotebook *notebook,
		GParamSpec *pspec,
		gpointer    user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	HexDocument *doc;

	if (! ACTIVE_GH)	return;

	refresh_dialogs (self);

	/* Assess saveability based on new tab we've switched to */
	doc = hex_widget_get_document (ACTIVE_GH);
	ghex_application_window_set_can_save (self, assess_can_save (doc));

	/* Update dialogs, offset status bar, etc. */
	update_gui_data (self);
}

static void
notebook_page_added_cb (GtkNotebook *notebook,
		GtkWidget   *child,
		guint        page_num,
		gpointer     user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	
	g_assert (GTK_WIDGET(notebook) == self->hex_notebook);

	/* Let's play this super dumb. If a page is added, that will generally
	 * mean we don't have to count the pages to see if we have > 0.
	 */
	enable_main_actions (self, TRUE);
}

static void
notebook_page_removed_cb (GtkNotebook *notebook,
		GtkWidget   *child,
		guint        page_num,
		gpointer     user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	g_assert (GTK_WIDGET(notebook) == self->hex_notebook);
	
	if (gtk_notebook_get_n_pages (notebook) == 0) {
		enable_main_actions (self, FALSE);
		ghex_application_window_set_show_find (self, FALSE);
		ghex_application_window_set_show_replace (self, FALSE);
		ghex_application_window_set_show_jump (self, FALSE);
		ghex_application_window_set_show_chartable (self, FALSE);
		ghex_application_window_set_show_converter (self, FALSE);

		show_no_file_loaded_label (self);
	}
}

static void
pane_close_cb (PaneDialog *pane, gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	if (ACTIVE_GH)
		gtk_widget_grab_focus (GTK_WIDGET(ACTIVE_GH));

	gtk_revealer_set_transition_type (GTK_REVEALER(self->pane_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
	gtk_revealer_set_reveal_child (GTK_REVEALER(self->pane_revealer), FALSE);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_FIND_OPEN]);
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_REPLACE_OPEN]);
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_JUMP_OPEN]);
}

static void
chartable_close_cb (GtkWindow *window,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	ghex_application_window_set_show_chartable (self, FALSE);
}

static void
converter_close_cb (GtkWindow *window,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	ghex_application_window_set_show_converter (self, FALSE);
}

static void
setup_chartable (GHexApplicationWindow *self)
{
	g_return_if_fail (HEX_IS_WIDGET(ACTIVE_GH));

	self->chartable = create_char_table (GTK_WINDOW(self), ACTIVE_GH);

    g_signal_connect(self->chartable, "close-request",
                     G_CALLBACK(chartable_close_cb), self);
}

static void
setup_converter (GHexApplicationWindow *self)
{
	g_return_if_fail (HEX_IS_WIDGET(ACTIVE_GH));

	self->converter = create_converter (GTK_WINDOW(self), ACTIVE_GH);

	g_signal_connect(self->converter, "close-request",
                     G_CALLBACK(converter_close_cb), self);
}

static void
update_titlebar (GHexApplicationWindow *self)
{
	GFile *file;

	if (ACTIVE_GH	&&
			/* This is kind of cheap, but checking at this exact time whether
			 * we hold any further references to gh doesn't seem to work
			 * reliably.
			 */
			gtk_notebook_get_n_pages (GTK_NOTEBOOK(self->hex_notebook)))
	{
		char *basename;
		char *title;
		HexDocument *doc = hex_widget_get_document (ACTIVE_GH);

		if (G_IS_FILE (file = hex_document_get_file (doc)))
			basename = g_file_get_basename (file);
		else
			/* Translators: this is the string for an untitled buffer that will
			 * be displayed in the titlebar when a user does File->New
			 */
			basename = g_strdup (_("Untitled"));

		title = g_strdup_printf ("%s - GHex", basename);
		gtk_window_set_title (GTK_WINDOW(self), title);

		g_free (basename);
		g_free (title);
	}
	else
	{
		gtk_window_set_title (GTK_WINDOW(self), "GHex");
	}
}

static void
update_gui_data (GHexApplicationWindow *self)
{
	int current_pos;
	HexDialogVal64 val;
	char *titlebar_label;

	update_status_message (self);
	update_titlebar (self);

	if (ACTIVE_GH)
	{
		current_pos = hex_widget_get_cursor (ACTIVE_GH);

		for (int i = 0; i < 8; i++)
		{
			/* returns 0 on buffer overflow, which is what we want */
			val.v[i] = hex_widget_get_byte (ACTIVE_GH, current_pos+i);
		}
		hex_dialog_updateview (self->dialog, &val);
		refresh_dialogs (self);
	}
}

static void
cursor_moved_cb (HexWidget *gh, gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	int current_pos;
	HexDialogVal64 val;

	/* If the cursor has been moved by a function call for a HexWidget that is
	 * *not* in view, we're not interested. */
	if (ACTIVE_GH != gh) {
		return;
	}
	else {
		update_gui_data (self);
	}
}

/* ACTIONS */

#define DIALOG_SET_SHOW_TEMPLATE(WIDGET, SETUP_FUNC, PROP_ARR_ENTRY)		\
static void																	\
ghex_application_window_set_show_ ##WIDGET (GHexApplicationWindow *self,	\
		gboolean show)														\
{																			\
	if (show)																\
	{																		\
		if (! GTK_IS_WIDGET(self->WIDGET)) {								\
			SETUP_FUNC;														\
		}																	\
		gtk_widget_show (self->WIDGET);										\
	}																		\
	else																	\
	{																		\
		if (GTK_IS_WIDGET (self->WIDGET) &&									\
				gtk_widget_is_visible (self->WIDGET)) {						\
			gtk_widget_hide (self->WIDGET);									\
			gtk_widget_grab_focus (GTK_WIDGET(ACTIVE_GH));					\
		}																	\
	}																		\
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_ARR_ENTRY]);	\
}												/* DIALOG_SET_SHOW_TEMPLATE */

DIALOG_SET_SHOW_TEMPLATE(chartable, setup_chartable(self), PROP_CHARTABLE_OPEN)
DIALOG_SET_SHOW_TEMPLATE(converter, setup_converter(self), PROP_CONVERTER_OPEN)

/* Note that in this macro, it's up to the "closed" cb functions to notify
 * by pspec
 */
#define PANE_SET_SHOW_TEMPLATE(WIDGET, OTHER1, OTHER2)								\
static void																			\
ghex_application_window_set_show_ ##WIDGET (GHexApplicationWindow *self,			\
		gboolean show)																\
{																					\
	if (show) {																		\
		gtk_widget_hide (self->OTHER1 ## _dialog);									\
		gtk_widget_hide (self->OTHER2 ## _dialog);									\
		gtk_widget_show (self->WIDGET ## _dialog);									\
		gtk_widget_grab_focus (self->WIDGET ## _dialog);							\
		if (! gtk_revealer_get_reveal_child (GTK_REVEALER(self->pane_revealer)))	\
		{																			\
			gtk_revealer_set_transition_type (GTK_REVEALER(self->pane_revealer),	\
					GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);							\
			gtk_revealer_set_reveal_child (GTK_REVEALER(self->pane_revealer), TRUE);\
		}																			\
		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_FIND_OPEN]);		\
		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_REPLACE_OPEN]);	\
		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_JUMP_OPEN]);		\
	} else {																		\
		g_signal_emit_by_name (self->WIDGET ## _dialog, "closed");					\
	}																				\
}												/* PANE_SET_SHOW_TEMPLATE */

PANE_SET_SHOW_TEMPLATE(find,		replace, jump)
PANE_SET_SHOW_TEMPLATE(replace,		find, jump)
PANE_SET_SHOW_TEMPLATE(jump,		find, replace)

/* Property setters without templates: */

static void
ghex_application_window_set_can_save (GHexApplicationWindow *self,
		gboolean can_save)
{
	self->can_save = can_save;

	gtk_widget_action_set_enabled (GTK_WIDGET(self),
			"ghex.save", can_save);
	gtk_widget_action_set_enabled (GTK_WIDGET(self),
			"ghex.revert", can_save);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_CAN_SAVE]);
}

static void
ghex_application_window_set_insert_mode (GHexApplicationWindow *self,
		gboolean insert_mode)
{
	self->insert_mode = insert_mode;

	NOTEBOOK_GH_FOREACH_START

	hex_widget_set_insert_mode (gh, insert_mode);

	NOTEBOOK_GH_FOREACH_END

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_INSERT_MODE]);
}

/* For now, at least, this is a mostly pointless wrapper around 'file_save'.
 */
static void
save_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);

	file_save (self);
}

static void
save_as_write_to_file_cb (HexDocument *doc,
		GAsyncResult *res,
		GHexApplicationWindow *self)
{
	GFile *gfile = extra_user_data;
	GError *local_error = NULL;
	gboolean write_successful;

	write_successful = hex_document_write_finish (doc, res, &local_error);

	if (! write_successful)
	{
		GString *full_errmsg = g_string_new (
				_("There was an error saving the file to the path specified."));

		if (local_error)
			g_string_append_printf (full_errmsg, "\n\n%s", local_error->message);

		display_error_dialog (GTK_WINDOW(self), full_errmsg->str);

		g_string_free (full_errmsg, TRUE);
		return;
	}

	if (hex_document_set_file (doc, gfile))
	{
		extra_user_data = ACTIVE_GH;
		hex_document_read_async (doc, NULL, doc_read_ready_cb, self);
	}
	else
	{
		display_error_dialog (GTK_WINDOW(self),
				_("An unknown error has occurred in attempting to reload the "
					"file you have just saved."));
	}
}

/* save_as helper */
static void
save_as_response_cb (GtkNativeDialog *dialog,
		int resp,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
	HexDocument *doc;
	GFile *gfile;

	/* If user doesn't click Save, just bail out now. */
	if (resp != GTK_RESPONSE_ACCEPT)
		goto end;

	/* Fetch doc. No need for sanity checks as this is just a helper. */
	doc = hex_widget_get_document (ACTIVE_GH);

	extra_user_data = gfile = gtk_file_chooser_get_file (chooser);

	hex_document_write_to_file_async (doc,
			gfile,
			NULL,
			(GAsyncReadyCallback)save_as_write_to_file_cb,
			self);

end:
	gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (dialog));
}

static void
save_as (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
	GtkFileChooserNative *file_sel;
	GtkResponseType resp;
	HexDocument *doc;
	GFile *default_file;

	doc = hex_widget_get_document (ACTIVE_GH);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	default_file = hex_document_get_file (doc);
	file_sel = gtk_file_chooser_native_new (
			_("Select a file to save buffer as"),
				GTK_WINDOW(self),
				GTK_FILE_CHOOSER_ACTION_SAVE,
				NULL,
				NULL);

	/* Default suggested file == existing file. */
	if (G_IS_FILE (default_file))
	{
		gtk_file_chooser_set_file (GTK_FILE_CHOOSER(file_sel),
				hex_document_get_file (doc),
				NULL);	/* GError **error */
	}

	g_signal_connect (file_sel, "response",
			G_CALLBACK(save_as_response_cb), self);

	gtk_native_dialog_show (GTK_NATIVE_DIALOG(file_sel));
}


static void
revert_response_cb (GtkDialog *dialog,
		int response_id,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	HexDocument *doc;

	if (response_id != GTK_RESPONSE_ACCEPT)
		goto end;

	doc = hex_widget_get_document (ACTIVE_GH);

	extra_user_data = ACTIVE_GH;
	hex_document_read_async (doc, NULL, doc_read_ready_cb, self);

end:
	gtk_window_destroy (GTK_WINDOW(dialog));
}

static void
revert (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
   	HexDocument *doc;
	GtkWidget *dialog;
	gint reply;
	char *basename = NULL;
	
	g_return_if_fail (HEX_IS_WIDGET (ACTIVE_GH));

	doc = hex_widget_get_document (ACTIVE_GH);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	/* Yes, this *is* a programmer error. The Revert menu should not be shown
	 * to the user at all if there is nothing to revert. */
	g_return_if_fail (hex_document_has_changed (doc));

	basename = g_file_get_basename (hex_document_get_file (doc));

	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW(self),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			/* Translators: %s here is the filename the user is being asked to
			 * confirm whether they want to revert. */
			_("<big><b>Are you sure you want to revert %s?</b></big>\n\n"
			"Your changes will be lost.\n\n"
			"This action cannot be undone."),
			basename);

	gtk_dialog_add_buttons (GTK_DIALOG(dialog),
			_("_Revert"), GTK_RESPONSE_ACCEPT,
			_("_Go Back"), GTK_RESPONSE_REJECT,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	g_signal_connect (dialog, "response",
			G_CALLBACK (revert_response_cb), self);

	gtk_widget_show (dialog);
	g_free (basename);
}
			
static void
print_preview (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);

	g_return_if_fail (HEX_IS_WIDGET(ACTIVE_GH));

	common_print (GTK_WINDOW(self), ACTIVE_GH, /* preview: */ TRUE);
}

static void
do_print (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);

	g_return_if_fail (HEX_IS_WIDGET(ACTIVE_GH));

	common_print (GTK_WINDOW(self), ACTIVE_GH, /* preview: */ FALSE);
}

static void
new_file (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
	HexDocument *doc;
	HexWidget *gh;

	doc = hex_document_new ();
	gh = HEX_WIDGET(hex_widget_new (doc));

	ghex_application_window_add_hex (self, gh);
	refresh_dialogs (self);
	ghex_application_window_activate_tab (self, gh);
	ghex_application_window_set_insert_mode (self, TRUE);
	file_loaded (doc, self);
}

static void
open_response_cb (GtkNativeDialog *dialog,
		int resp,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
	GFile *file;

	if (resp == GTK_RESPONSE_ACCEPT)
	{
		file = gtk_file_chooser_get_file (chooser);

		ghex_application_window_open_file (self, file);
	}

	g_object_unref (dialog);
}

static void
open_file_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
	GtkFileChooserNative *file_sel;
	GtkResponseType resp;

	file_sel =
		gtk_file_chooser_native_new (_("Select a file to open"),
				GTK_WINDOW(self),
				GTK_FILE_CHOOSER_ACTION_OPEN,
				NULL,	/* const char *accept_label } NULL == default.	*/
				NULL);	/* const char *cancel_label }					*/

	g_signal_connect (file_sel, "response",
			G_CALLBACK(open_response_cb), self);

	gtk_native_dialog_show (GTK_NATIVE_DIALOG(file_sel));
}

static void
toggle_conversions (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
	GtkRevealer *revealer = GTK_REVEALER (self->conversions_revealer);

	if (gtk_revealer_get_reveal_child (revealer))
	{
		gtk_revealer_set_transition_type (revealer,
				GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
		gtk_revealer_set_reveal_child (revealer, FALSE);
		gtk_button_set_icon_name (GTK_BUTTON(self->pane_toggle_button),
				"pan-up-symbolic");
	}
	else
	{
		gtk_revealer_set_transition_type (revealer,
				GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
		gtk_revealer_set_reveal_child (revealer, TRUE);
		gtk_button_set_icon_name (GTK_BUTTON(self->pane_toggle_button),
				"pan-down-symbolic");
	}
}

static void
open_help_ready_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(source_object);
	gboolean help_success;
	GError *local_error = NULL;

	help_success = gtk_show_uri_full_finish (GTK_WINDOW(self),
			res,
			&local_error);

	/* If we can't handle the help:/// URI scheme, try opening static HTML
	 * help docs, if they exist.
	 */
	if (! help_success)
	{
#ifdef STATIC_HTML_HELP
#define URI_PREFIX "file:" PACKAGE_DOCDIR "/HTML/"
		char *uri = NULL;
		GFile *file = NULL;
		char *locale_str = g_strdup (setlocale (LC_MESSAGES, NULL));
		char *cp;
		char *full_errmsg = NULL;

		/* strip off codeset (.*) and modifier (@*) - see ISO/IEC 15897 */
		strtok (locale_str, ".");
		cp = strtok (NULL, ".");
		if (cp)
			*cp = 0;

		strtok (locale_str, "@");
		cp = strtok (NULL, "@");
		if (cp)
			*cp = 0;

		/* first, try full locale (eg, en_US) ... */
		uri = g_strdup_printf (URI_PREFIX "%s/" PACKAGE_NAME "/index.html",
				locale_str);

		file = g_file_new_for_uri (uri);

		/* ... if that didn't work, try stripping the territory (_*) */
		if (! g_file_query_exists (file, NULL))
		{
			g_object_unref (file);
			g_free (uri);

			strtok (locale_str, "_");
			cp = strtok (NULL, "_");
			if (cp)
				*cp = 0;
			else
				goto error;

			uri = g_strdup_printf (URI_PREFIX "%s/" PACKAGE_NAME "/index.html",
					locale_str);
			file = g_file_new_for_uri (uri);
		}

		if (g_file_query_exists (file, NULL))
		{
			gtk_show_uri (GTK_WINDOW(self), uri, GDK_CURRENT_TIME);
			goto out;
		}
error:
		full_errmsg = g_strdup_printf (_("Sorry, but help could not be opened.\n\n"
					"Various attempts were made to locate the help documentation "
					"unsuccessfully.\n\n"
					"Please ensure the application was properly installed.\n\n"
					"The specific error message is:\n\n"
					"%s"),
				local_error->message);
		display_error_dialog (GTK_WINDOW(self), full_errmsg);
out:
		g_clear_object (&file);
		g_free (uri);
		g_free (full_errmsg);
		g_clear_pointer (&local_error, g_error_free);
#undef URI_PREFIX
#else
		display_error_dialog (GTK_WINDOW(self), local_error->message);
#endif
	}
}

static void
open_help (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);

	gtk_show_uri_full (GTK_WINDOW (self),
			"help:" PACKAGE_NAME,
			GDK_CURRENT_TIME,
			NULL,
			open_help_ready_cb,
			NULL);
}

static void
open_about (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);

	common_about_cb (GTK_WINDOW(self));
}

static void
open_preferences (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);

	if (! GTK_IS_WIDGET (self->prefs_dialog) ||
			! gtk_widget_get_visible (self->prefs_dialog)) {
		self->prefs_dialog = create_preferences_dialog (GTK_WINDOW(self));
	}
	gtk_widget_show (self->prefs_dialog);
}

/* --- */

static void
update_status_message (GHexApplicationWindow *self)
{
	char *status = NULL;
	gint64 current_pos, ss, se;
	int len;

	if (! ACTIVE_GH)
		goto out;

	current_pos = hex_widget_get_cursor (ACTIVE_GH);

	if (hex_widget_get_selection (ACTIVE_GH, &ss, &se))
	{
		status = g_strdup_printf (
				_("Offset: <tt>0x%lX</tt>; <tt>0x%lX</tt> bytes from <tt>0x%lX</tt> to <tt>0x%lX</tt> selected"),
				current_pos, se - ss + 1, ss, se);
	}
	else {
		status = g_strdup_printf (_("Offset: <tt>0x%lX</tt>"), current_pos);
	}

	hex_statusbar_set_status (HEX_STATUSBAR(self->statusbar), status);
	g_free (status);
	return;
	
out:
	hex_statusbar_clear (HEX_STATUSBAR(self->statusbar));
}

/* helpers for _get_property */

static gboolean
get_dialog_visible (GtkWidget *widget)
{
	return (GTK_IS_WIDGET(widget) && gtk_widget_get_visible (widget));
}

static gboolean
get_pane_visible (GHexApplicationWindow *self, PaneDialog *pane)
{
	return (GTK_IS_WIDGET(GTK_WIDGET(pane)) &&
		gtk_widget_get_visible (GTK_WIDGET(pane)) &&
		gtk_revealer_get_reveal_child (GTK_REVEALER(self->pane_revealer)));
}

/* PROPERTIES */

static void
ghex_application_window_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW (object);

	switch ((GHexApplicationWindowProperty) property_id)
	{
		case PROP_CHARTABLE_OPEN:
			ghex_application_window_set_show_chartable (self,
					g_value_get_boolean (value));
			break;

		case PROP_CONVERTER_OPEN:
			ghex_application_window_set_show_converter (self,
					g_value_get_boolean (value));
			break;

		case PROP_FIND_OPEN:
			ghex_application_window_set_show_find (self,
					g_value_get_boolean (value));
			break;

		case PROP_REPLACE_OPEN:
			ghex_application_window_set_show_replace (self,
					g_value_get_boolean (value));
			break;

		case PROP_JUMP_OPEN:
			ghex_application_window_set_show_jump (self,
					g_value_get_boolean (value));
			break;

		case PROP_CAN_SAVE:
			ghex_application_window_set_can_save (self,
					g_value_get_boolean (value));
			break;

		case PROP_INSERT_MODE:
			ghex_application_window_set_insert_mode (self,
					g_value_get_boolean (value));
			break;

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_application_window_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW (object);

	switch ((GHexApplicationWindowProperty) property_id)
	{
		case PROP_CHARTABLE_OPEN:
			g_value_set_boolean (value, get_dialog_visible (self->chartable));
			break;

		case PROP_CONVERTER_OPEN:
			g_value_set_boolean (value, get_dialog_visible (self->converter));
			break;

		case PROP_FIND_OPEN:
			g_value_set_boolean (value,
					get_pane_visible (self, PANE_DIALOG(self->find_dialog)));
			break;

		case PROP_REPLACE_OPEN:
			g_value_set_boolean (value,
					get_pane_visible (self, PANE_DIALOG(self->replace_dialog)));
			break;

		case PROP_JUMP_OPEN:
			g_value_set_boolean (value,
					get_pane_visible (self, PANE_DIALOG(self->jump_dialog)));
			break;

		case PROP_CAN_SAVE:
			g_value_set_boolean (value, self->can_save);
			break;

		case PROP_INSERT_MODE:
			g_value_set_boolean (value, self->insert_mode);
			break;

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}


/* GHexApplicationWindow -- CONSTRUCTORS AND DESTRUCTORS */

static void
ghex_application_window_init (GHexApplicationWindow *self)
{
	GtkWidget *widget = GTK_WIDGET(self);
	GtkStyleContext *context;
	GAction *action;

	gtk_widget_init_template (widget);

	/* Cache system default of prefer-dark-mode; gtk does not do this. This
	 * is run here as it cannot be done until we have a 'screen'. */
	get_sys_default_is_dark ();
	/* Do dark mode if requested */
	set_dark_mode_from_settings (self);

	/* Setup conversions box and pane */
	self->dialog = hex_dialog_new ();
	self->dialog_widget = hex_dialog_getview (self->dialog);

	gtk_box_append (GTK_BOX(self->conversions_box), self->dialog_widget);

	/* CSS - conversions_box */

	context = gtk_widget_get_style_context (self->conversions_box);
	self->conversions_box_provider = gtk_css_provider_new ();

	gtk_css_provider_load_from_data (self->conversions_box_provider,
									 "box {\n"
									 "   padding: 12px;\n"
									 "}\n", -1);

	/* add the provider to our widget's style context. */
	gtk_style_context_add_provider (context,
			GTK_STYLE_PROVIDER (self->conversions_box_provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	/* Setup signals */

	g_signal_connect (self, "close-request",
			G_CALLBACK(close_request_cb), self);

	/* Signals - SETTINGS */

    g_signal_connect (settings, "changed::" GHEX_PREF_OFFSETS_COLUMN,
                      G_CALLBACK (settings_offsets_column_changed_cb), self);

    g_signal_connect (settings, "changed::" GHEX_PREF_FONT,
                      G_CALLBACK (settings_font_changed_cb), self);

    g_signal_connect (settings, "changed::" GHEX_PREF_GROUP,
                      G_CALLBACK (settings_group_type_changed_cb), self);

    g_signal_connect_swapped (settings, "changed::" GHEX_PREF_DARK_MODE,
                      G_CALLBACK (set_dark_mode_from_settings), self);

	/* Actions - SETTINGS */

	/* for the 'group data by' stuff. There isn't a function to do this from
	 * class-init, so we end up with the 'win' namespace when we do it from
	 * here. Be aware of that.
	 */
	action = g_settings_create_action (settings, GHEX_PREF_GROUP);
	g_action_map_add_action (G_ACTION_MAP(self), action);

	/* Setup notebook signals */

	g_signal_connect (self->hex_notebook, "notify::page",
			G_CALLBACK(notebook_page_changed_cb), self);

	g_signal_connect (self->hex_notebook, "page-added",
			G_CALLBACK(notebook_page_added_cb), self);

	g_signal_connect (self->hex_notebook, "page-removed",
			G_CALLBACK(notebook_page_removed_cb), self);

	/* Get find_dialog and friends geared up */

	self->find_dialog = find_dialog_new ();
	gtk_widget_set_hexpand (self->find_dialog, TRUE);
	gtk_box_append (GTK_BOX(self->findreplace_box), self->find_dialog);
	gtk_widget_hide (self->find_dialog);
	g_signal_connect (self->find_dialog, "closed",
			G_CALLBACK(pane_close_cb), self);

	self->replace_dialog = replace_dialog_new ();
	gtk_widget_set_hexpand (self->replace_dialog, TRUE);
	gtk_box_append (GTK_BOX(self->findreplace_box), self->replace_dialog);
	gtk_widget_hide (self->replace_dialog);
	g_signal_connect (self->replace_dialog, "closed",
			G_CALLBACK(pane_close_cb), self);

	self->jump_dialog = jump_dialog_new ();
	gtk_widget_set_hexpand (self->jump_dialog, TRUE);
	gtk_box_append (GTK_BOX(self->findreplace_box), self->jump_dialog);
	gtk_widget_hide (self->jump_dialog);
	g_signal_connect (self->jump_dialog, "closed",
			G_CALLBACK(pane_close_cb), self);

	hex_statusbar_clear (HEX_STATUSBAR(self->statusbar));

	/* Grey out main actions at the beginning */
	enable_main_actions (self, FALSE);

	/* Grey out save (special case - it's not lumped in with mains */
	gtk_widget_action_set_enabled (GTK_WIDGET(self),
			"ghex.save", FALSE);
}

static void
ghex_application_window_dispose(GObject *object)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(object);
	GtkWidget *widget = GTK_WIDGET(self);
	GtkWidget *child;

	/* Unparent children
	 */
	g_clear_pointer (&self->find_dialog, gtk_widget_unparent);
	g_clear_pointer (&self->replace_dialog, gtk_widget_unparent);
	g_clear_pointer (&self->jump_dialog, gtk_widget_unparent);
	g_clear_pointer (&self->chartable, gtk_widget_unparent);
	g_clear_pointer (&self->converter, gtk_widget_unparent);

	/* Clear conversions box CSS provider */
	g_clear_object (&self->conversions_box_provider);

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(ghex_application_window_parent_class)->dispose(object);
}

static void
ghex_application_window_finalize(GObject *gobject)
{
	/* here, you would free stuff. I've got nuthin' for ya. */

	/* --- */

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(ghex_application_window_parent_class)->finalize(gobject);
}

static void
ghex_application_window_class_init(GHexApplicationWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags prop_flags = G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose = ghex_application_window_dispose;
	object_class->finalize = ghex_application_window_finalize;
	object_class->get_property = ghex_application_window_get_property;
	object_class->set_property = ghex_application_window_set_property;

	/* PROPERTIES */


	properties[PROP_CHARTABLE_OPEN] =
		g_param_spec_boolean ("chartable-open",
			"Character table open",
			"Whether the character table dialog is currently open",
			FALSE,	/* gboolean default_value */
			prop_flags);

	properties[PROP_CONVERTER_OPEN] =
		g_param_spec_boolean ("converter-open",
			"Base converter open",
			"Whether the base converter dialog is currently open",
			FALSE,	/* gboolean default_value */
			prop_flags);

	properties[PROP_FIND_OPEN] =
		g_param_spec_boolean ("find-open",
			"Find pane open",
			"Whether the Find pane is currently open",
			FALSE,	/* gboolean default_value */
			prop_flags);

	properties[PROP_REPLACE_OPEN] =
		g_param_spec_boolean ("replace-open",
			"Replace pane open",
			"Whether the Find and Replace pane is currently open",
			FALSE,	/* gboolean default_value */
			prop_flags);

	properties[PROP_JUMP_OPEN] =
		g_param_spec_boolean ("jump-open",
			"Jump pane open",
			"Whether the Jump to Byte pane is currently open",
			FALSE,	/* gboolean default_value */
			prop_flags);

	properties[PROP_CAN_SAVE] =
		g_param_spec_boolean ("can-save",
			"Can save",
			"Whether the Save (or Revert) button should currently be clickable",
			FALSE,	/* gboolean default_value */
			prop_flags);
	
	properties[PROP_INSERT_MODE] =
		g_param_spec_boolean ("insert-mode",
			"Insert mode",
			"Whether insert-mode (versus overwrite) is currently engaged",
			FALSE,	/* gboolean default_value */
			prop_flags);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);


	/* ACTIONS */

	gtk_widget_class_install_action (widget_class, "ghex.new",
			NULL,	/* GVariant string param_type */
			new_file);

	gtk_widget_class_install_action (widget_class, "ghex.open",
			NULL,	/* GVariant string param_type */
			open_file_action);

	gtk_widget_class_install_action (widget_class, "ghex.save",
			NULL,	/* GVariant string param_type */
			save_action);

	gtk_widget_class_install_action (widget_class, "ghex.save-as",
			NULL,	/* GVariant string param_type */
			save_as);

	gtk_widget_class_install_action (widget_class, "ghex.revert",
			NULL,	/* GVariant string param_type */
			revert);

	gtk_widget_class_install_action (widget_class, "ghex.copy-special",
			NULL,	/* GVariant string param_type */
			copy_special);

	gtk_widget_class_install_action (widget_class, "ghex.paste-special",
			NULL,	/* GVariant string param_type */
			paste_special);

	gtk_widget_class_install_action (widget_class, "ghex.print",
			NULL,	/* GVariant string param_type */
			do_print);

	gtk_widget_class_install_action (widget_class, "ghex.print-preview",
			NULL,	/* GVariant string param_type */
			print_preview);

	gtk_widget_class_install_action (widget_class, "ghex.show-conversions",
			NULL,	/* GVariant string param_type */
			toggle_conversions);

	gtk_widget_class_install_action (widget_class, "ghex.preferences",
			NULL,	/* GVariant string param_type */
			open_preferences);

	gtk_widget_class_install_action (widget_class, "ghex.help",
			NULL,	/* GVariant string param_type */
			open_help);

	gtk_widget_class_install_action (widget_class, "ghex.about",
			NULL,	/* GVariant string param_type */
			open_about);

	gtk_widget_class_install_property_action (widget_class,
			"ghex.find", "find-open");

	gtk_widget_class_install_property_action (widget_class,
			"ghex.replace", "replace-open");

	gtk_widget_class_install_property_action (widget_class,
			"ghex.jump", "jump-open");

	gtk_widget_class_install_property_action (widget_class,
			"ghex.chartable", "chartable-open");

	gtk_widget_class_install_property_action (widget_class,
			"ghex.converter", "converter-open");

	gtk_widget_class_install_property_action (widget_class,
			"ghex.insert-mode", "insert-mode");

	/* SHORTCUTS */

	/* ensure these are synced with help-overlay.ui */

	/* F1 - show help */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_F1,
			0,
			"ghex.help",
			NULL);	/* no args. */

	/* Insert - toggle insert mode */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_Insert,
			0,
			"ghex.insert-mode",
			NULL);	/* no args. */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_Insert,
			0,
			"ghex.insert-mode",
			NULL);	/* no args. */

	/* Ctrl+F - find */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_f,
			GDK_CONTROL_MASK,
			"ghex.find",
			NULL);	/* no args. */

	/* Ctrl+H - replace */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_h,
			GDK_CONTROL_MASK,
			"ghex.replace",
			NULL);	/* no args. */

	/* Ctrl+J - jump */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_j,
			GDK_CONTROL_MASK,
			"ghex.jump",
			NULL);	/* no args. */

	/* Ctrl+Shift+V - paste special */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_v,
			GDK_CONTROL_MASK | GDK_SHIFT_MASK,
			"ghex.paste-special",
			NULL);

	/* Ctrl+Shift+C - copy special */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_c,
			GDK_CONTROL_MASK | GDK_SHIFT_MASK,
			"ghex.copy-special",
			NULL);

	/* Ctrl+N - new */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_n,
			GDK_CONTROL_MASK,
			"ghex.new",
			NULL);

	/* Ctrl+O - open */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_o,
			GDK_CONTROL_MASK,
			"ghex.open",
			NULL);

	/* Ctrl+S - save */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_s,
			GDK_CONTROL_MASK,
			"ghex.save",
			NULL);

	/* Ctrl+Shift+S - save as */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_s,
			GDK_CONTROL_MASK | GDK_SHIFT_MASK,
			"ghex.save-as",
			NULL);

	/* Ctrl+P - print */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_p,
			GDK_CONTROL_MASK,
			"ghex.print",
			NULL);

	/* Ctrl+W - close tab */
	gtk_widget_class_add_binding (widget_class,
			GDK_KEY_w,
			GDK_CONTROL_MASK,
			close_tab_shortcut_cb,
			NULL);

	/* In case you're looking for Ctrl+PageUp & PageDown - those are baked in
	 * via GtkNotebook */

	/* WIDGET TEMPLATE .UI */

	gtk_widget_class_set_template_from_resource (widget_class,
					RESOURCE_BASE_PATH "/ghex-application-window.ui");

	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			no_doc_label);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			hex_notebook);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			conversions_box);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			findreplace_box);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			pane_toggle_button);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			insert_mode_button);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			statusbar);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			pane_revealer);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			conversions_revealer);
}

GtkWidget *
ghex_application_window_new (GtkApplication *app)
{
	return g_object_new (GHEX_TYPE_APPLICATION_WINDOW,
			"application", app,
			NULL);
}

void
ghex_application_window_activate_tab (GHexApplicationWindow *self,
		HexWidget *gh)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(self->hex_notebook);
	int page_num;

	g_return_if_fail (HEX_IS_WIDGET (gh));
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	page_num = gtk_notebook_page_num (notebook, GTK_WIDGET(gh));

	gtk_notebook_set_current_page (notebook, page_num);
	gtk_widget_grab_focus (GTK_WIDGET(gh));
}

void
ghex_application_window_add_hex (GHexApplicationWindow *self,
		HexWidget *gh)
{
	GtkWidget *tab;
	HexDocument *doc;

	g_return_if_fail (HEX_IS_WIDGET(gh));

	doc = hex_widget_get_document (gh);
	g_return_if_fail (HEX_IS_DOCUMENT(doc));

	/* HexWidget: Setup based on global settings. */
	common_set_gtkhex_font_from_settings (gh);

	/* HexWidget: Sync up appwindow-specific settings. */
	set_gtkhex_offsets_column_from_settings (gh);
	set_gtkhex_group_type_from_settings (gh);
	
	/* HexWidget: Set insert mode based on our global appwindow prop */
	hex_widget_set_insert_mode (gh, self->insert_mode);

	/* Generate a tab */
	tab = ghex_notebook_tab_new (gh);

	g_signal_connect (tab, "close-request",
			G_CALLBACK(tab_close_request_cb), self);

	gtk_notebook_append_page (GTK_NOTEBOOK(self->hex_notebook),
			GTK_WIDGET(gh),
			tab);

	/* Because we ellipsize labels in ghex-notebook-tab, we need to set this
	 * property to TRUE according to TFM, otherwise all the labels will just
	 * show up as '...' */
	g_object_set (gtk_notebook_get_page (GTK_NOTEBOOK(self->hex_notebook), GTK_WIDGET(gh)),
			"tab-expand", TRUE,
			NULL);

	/* Only show the tab bar if there's more than one (1) tab. */
	if (gtk_notebook_get_n_pages (GTK_NOTEBOOK(self->hex_notebook)) > 1)
		gtk_notebook_set_show_tabs (GTK_NOTEBOOK(self->hex_notebook), TRUE);

	/* FIXME - this seems to result in GTK_IS_BOX assertion failures.
	 * These seem harmless as everything still seems to *work*, but just
	 * documenting it here in case it becomes an issue in future.
	 */
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK(self->hex_notebook),
			GTK_WIDGET(gh),
			TRUE);

	/* set text of context menu tab switcher to the filename rather than
	 * 'Page X' */
	gtk_notebook_set_menu_label_text (GTK_NOTEBOOK(self->hex_notebook),
			GTK_WIDGET(gh),
			ghex_notebook_tab_get_filename (GHEX_NOTEBOOK_TAB(tab)));

	/* Setup signals */
    g_signal_connect (gh, "cursor-moved",
			G_CALLBACK(cursor_moved_cb), self);

	g_signal_connect (doc, "document-changed",
			G_CALLBACK(document_changed_cb), self);

	g_signal_connect (doc, "file-saved",
			G_CALLBACK(file_saved_cb), self);

	show_hex_notebook (self);
}

/* Helper */
static void
nag_screen_response_cb (GtkDialog *nag_screen,
		int response,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW (user_data);

	switch (response)
	{
		case GTK_RESPONSE_YES:
			ghex_application_window_open_file (self,
					tmp_global_gfile_for_nag_screen);
			break;

		default:
			break;
	}
	gtk_window_destroy (GTK_WINDOW(nag_screen));
}

/* Helper */
static void
do_nag_screen (GHexApplicationWindow *self)
{
		GtkWidget *nag_screen;
		char *msg = _("You are attempting to open a file 1GB or larger.\n\n"
				"This can make GHex and your machine unstable as the file "
				"will be loaded into memory, using the active backend.\n\n"
				"Are you sure you want to proceed?\n\n"
				"This message will not be shown again for the remainder of "
				"this GHex session.\n\n"
				"To avoid this message from appearing, try using a different "
				"buffer backend.");
	
		g_printerr ("%s", msg);
		g_printerr ("\n");

		nag_screen = gtk_message_dialog_new (GTK_WINDOW(self),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_YES_NO,
				"%s",
				msg);
		g_signal_connect (nag_screen, "response",
				G_CALLBACK(nag_screen_response_cb), self);
		gtk_widget_show (nag_screen);
}

/* also takes extra_user_data ! Hooray for cheap shortcuts! */
static void
doc_read_ready_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	HexWidget *gh = HEX_WIDGET(extra_user_data);
	HexDocument *doc = HEX_DOCUMENT(source_object);
	gboolean result;
	GError *local_error = NULL;

	result = hex_document_read_finish (doc, res, &local_error);

	ghex_application_window_activate_tab (self, gh);

	if (result)
	{
		refresh_dialogs (self);
		file_loaded (doc, self);
	}
	else
	{
		ghex_application_window_remove_tab (self,
				ghex_application_window_get_current_tab (self));

		if (local_error)
		{
			display_error_dialog (GTK_WINDOW(self), local_error->message);
			g_error_free (local_error);
		}
		else
		{
			char *generic_errmsg = N_("There was an error reading the file.");

			display_error_dialog (GTK_WINDOW(self), generic_errmsg);
		}
	}
	extra_user_data = NULL;
}
			
void
ghex_application_window_open_file (GHexApplicationWindow *self, GFile *file)
{
	HexDocument *doc;
	HexWidget *gh = NULL;
	static gboolean nag_screen_shown = FALSE;

	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW(self));

	/* If we get it from the GApp :open signal, it's tfr:none - once
	 * HexDocument gets hold of it, though, it _refs it itself so we don't need
	 * to hold onto it.
	 */
	g_object_ref (file);
	doc = hex_document_new_from_file (file);
	g_object_unref (file);

	if (HEX_IS_BUFFER_MALLOC (hex_document_get_buffer (doc)) &&
			! nag_screen_shown)
	{
		if (hex_buffer_util_get_file_size (file) >= 1073741824)
		{
			nag_screen_shown = TRUE;
			tmp_global_gfile_for_nag_screen = file;
			do_nag_screen (self);
			g_object_unref (doc);
			return;
		}
	}

	if (doc)
	{
		GFileType type;

		type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL);
		if (type == G_FILE_TYPE_SPECIAL)
		{
			HexBuffer *buf;

			g_debug ("%s: attempting to set buffer to `direct`", __func__);
			buf = hex_buffer_util_new ("direct", file);
			if (buf)
				hex_document_set_buffer (doc, buf);
			else
				g_debug ("%s: setting buffer to `direct` failed. If this is "
						"an attempt to open a block device, it will likely fail.",
						__func__);
		}

		gh = HEX_WIDGET(hex_widget_new (doc));
	}

	/* Display a fairly generic error message if we can't even get this far. */
	if (! gh)
	{
		char *error_msg = N_("There was an error loading the requested file. "
				"The file either no longer exists, is inaccessible, "
				"or you may not have permission to access the file.");

		display_error_dialog (GTK_WINDOW(self), error_msg);

		/* This fcn is also used to handle open operations on the cmdline. */
		g_printerr ("%s\n", error_msg);

		return;
	}

	ghex_application_window_add_hex (self, gh);
	extra_user_data = gh;
	hex_document_read_async (doc, NULL, doc_read_ready_cb, self);
}

HexWidget *
ghex_application_window_get_hex (GHexApplicationWindow *self)
{
	GHexNotebookTab *tab;
	HexWidget *gh = NULL;

	g_return_val_if_fail (GHEX_IS_APPLICATION_WINDOW (self), NULL);

	tab = ghex_application_window_get_current_tab (self);
	if (tab)
		gh = ghex_notebook_tab_get_hex (tab);

	return gh;
}
