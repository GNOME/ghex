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

#include <config.h>

/* DEFINES */

#define offset_fmt	"0x%X"

/* ----------------------- */
/* MAIN GOBJECT DEFINITION */
/* ----------------------- */

struct _GHexApplicationWindow
{
	GtkApplicationWindow parent_instance;

	GtkHex *gh;
	HexDialog *dialog;
	GtkWidget *dialog_widget;
	GtkCssProvider *conversions_box_provider;
	guint statusbar_id;
	GtkAdjustment *adj;
	GList *gh_list;
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

static void set_statusbar(GHexApplicationWindow *self, const char *str);
static void update_status_message (GHexApplicationWindow *self);
static void update_gui_data (GHexApplicationWindow *self);
static gboolean assess_can_save (HexDocument *doc);

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
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));							\
	for (i = gtk_notebook_get_n_pages(notebook) - 1; i >= 0; --i) {			\
		GHexNotebookTab *tab;												\
		GtkHex *gh;															\
		g_debug ("%s: Working on %d'th page", __func__, i);					\
		gh = GTK_HEX(gtk_notebook_get_nth_page (notebook, i));				\
		g_return_if_fail (GTK_IS_HEX (gh));									\
		tab = GHEX_NOTEBOOK_TAB(gtk_notebook_get_tab_label (notebook,		\
					GTK_WIDGET(gh)));										\
		g_return_if_fail (GHEX_IS_NOTEBOOK_TAB (tab));						\
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
set_gtkhex_offsets_column_from_settings (GtkHex *gh)
{
	gtk_hex_show_offsets (gh, show_offsets_column);
}

static void
set_gtkhex_group_type_from_settings (GtkHex *gh)
{
	gtk_hex_set_group_type (gh, def_group_type);
}

static void
set_dark_mode_from_settings (GHexApplicationWindow *self)
{
	GtkSettings *gtk_settings;

	gtk_settings = gtk_settings_get_default ();

	g_debug ("%s: def_dark_mode: %d", __func__, def_dark_mode);

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
	pane_dialog_set_hex (PANE_DIALOG(self->find_dialog), self->gh);
	pane_dialog_set_hex (PANE_DIALOG(self->replace_dialog), self->gh);
	pane_dialog_set_hex (PANE_DIALOG(self->jump_dialog), self->gh);
}

static GHexNotebookTab *
ghex_application_window_get_current_tab (GHexApplicationWindow *self)
{
	GtkNotebook *notebook;
	GHexNotebookTab *tab;

	g_return_val_if_fail (GTK_IS_NOTEBOOK (self->hex_notebook), NULL);
	g_return_val_if_fail (GTK_IS_HEX (self->gh), NULL);

	notebook = GTK_NOTEBOOK(self->hex_notebook);

	tab = GHEX_NOTEBOOK_TAB(gtk_notebook_get_tab_label (notebook,
					GTK_WIDGET(self->gh)));
	g_return_val_if_fail (GHEX_IS_NOTEBOOK_TAB (tab), NULL);

	return tab;
}

static void
ghex_application_window_remove_tab (GHexApplicationWindow *self,
		GHexNotebookTab *tab)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(self->hex_notebook);
	int page_num;
	GtkHex *tab_gh;

	tab_gh = ghex_notebook_tab_get_hex (tab);
	g_return_if_fail (GTK_IS_HEX(tab_gh));

	page_num = gtk_notebook_page_num (notebook, GTK_WIDGET(tab_gh));
	gtk_notebook_remove_page (notebook, page_num);

	/* FIXME - remove as a possible optimization - but let's keep it in
	 * for now for debugging purposes. */
	g_return_if_fail (g_list_find (self->gh_list, tab_gh));
	self->gh_list = g_list_remove (self->gh_list, tab_gh);
}

static void
file_save (GHexApplicationWindow *self)
{
	HexDocument *doc;

	g_return_if_fail (GTK_IS_HEX (self->gh));

	doc = gtk_hex_get_document (self->gh);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	if (hex_document_write (doc)) {
		/* we're happy... */
		g_debug ("%s: File saved successfully.", __func__);
	}
	else {
		display_error_dialog (GTK_WINDOW(self),
				_("There was an error saving the file."
				"\n\n"
				"Your permissions of the file may have been changed "
				"by another program, or the file may have become corrupted."));
	}
}

static void
do_close_window (GHexApplicationWindow *self)
{
	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW (self));
	
	gtk_window_set_application (GTK_WINDOW(self), NULL);
}

static void
close_all_tabs (GHexApplicationWindow *self)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(self->hex_notebook);
	int i;

	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	g_debug("%s: %d", __func__, gtk_notebook_get_n_pages (notebook));

	for (i = gtk_notebook_get_n_pages(notebook) - 1; i >= 0; --i)
	{
		GHexNotebookTab *tab;
		GtkHex *gh;

		g_debug ("%s: Working on %d'th page", __func__, i);

		gh = GTK_HEX(gtk_notebook_get_nth_page (notebook, i));
		g_return_if_fail (GTK_IS_HEX (gh));

		tab = GHEX_NOTEBOOK_TAB(gtk_notebook_get_tab_label (notebook,
					GTK_WIDGET(gh)));
		g_return_if_fail (GHEX_IS_NOTEBOOK_TAB (tab));

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

	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	for (i = gtk_notebook_get_n_pages(notebook) - 1; i >= 0; --i)
	{
		GtkHex *gh;
		HexDocument *doc = NULL;

		gh = GTK_HEX(gtk_notebook_get_nth_page (notebook, i));
		g_return_if_fail (GTK_IS_HEX (gh));

		doc = gtk_hex_get_document (gh);
		g_return_if_fail (HEX_IS_DOCUMENT (doc));

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

	g_debug ("%s: Window wants to be closed.",	__func__);
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
	gtk_widget_grab_focus (GTK_WIDGET (self->gh));
}

static void
close_doc_confirmation_dialog (GHexApplicationWindow *self)
{
	GtkWidget *dialog;
	HexDocument *doc;
	char *basename = NULL;

	g_return_if_fail (GTK_IS_HEX (self->gh));

	doc = gtk_hex_get_document (self->gh);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	basename = g_file_get_basename (hex_document_get_file (doc));

	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW(self),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			/* Translators: %s is the filename that is currently being
			 * edited. */
			_("<big><b>%s has been edited since opening.</b></big>\n\n"
			   "Would you like to save your changes?"),
			basename);

	gtk_dialog_add_buttons (GTK_DIALOG(dialog),
			_("_Save Changes"),		GTK_RESPONSE_ACCEPT,
			_("_Discard Changes"),	GTK_RESPONSE_REJECT,
			_("_Go Back"),			GTK_RESPONSE_CANCEL,
			NULL);

	g_signal_connect (dialog, "response",
			G_CALLBACK(close_doc_response_cb), self);

	gtk_widget_show (dialog);
	g_free (basename);
}

/* FIXME / TODO - I could see this function being useful, but right now it is
 * not used by anything, so I'm disabling it to silence warnings about unused
 * functions.
 */
#if 0
static void
enable_all_actions (GHexApplicationWindow *self, gboolean enable)
{
	GHexApplicationWindowClass *klass =
		g_type_class_peek (GHEX_TYPE_APPLICATION_WINDOW);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	guint i = 0;
	/* Note: don't be tempted to omit any of these and pass NULL to the
	 * function below. The docs do not say you can do this, and looking at the
	 * gtkwidget.c source code, doing so may result in dereferencing a NULL ptr
	 */
	GType owner;
	const char *action_name;
	const GVariantType *parameter_type;
	const char *property_name;

	while (gtk_widget_class_query_action (widget_class,
			i,
			&owner,
			&action_name,
			&parameter_type,
			&property_name))
	{
		g_debug("%s: action %u : %s - setting enabled: %d",
				__func__, i, action_name, enable);

		gtk_widget_action_set_enabled (GTK_WIDGET(self),
				action_name, enable);
		++i;
	}
}
#endif

/* Kinda like enable_all_actions, but only for ghex-specific ones. */
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

	g_return_val_if_fail (GHEX_IS_NOTEBOOK_TAB (tab), FALSE);

	g_signal_emit_by_name (tab, "closed");

	return TRUE;
}

static void
copy_special (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
	GdkClipboard *clipboard;

	(void)action_name; (void)parameter;
	g_return_if_fail (GTK_IS_HEX (self->gh));

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET(self->gh));

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

	(void)action_name; (void)parameter;
	g_return_if_fail (GTK_IS_HEX (self->gh));

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET(self->gh));

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
ghex_application_window_file_saved_cb (HexDocument *doc,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW (self));

	ghex_application_window_set_can_save (self, assess_can_save (doc));
}

static void
ghex_application_window_document_changed_cb (HexDocument *doc,
		gpointer change_data,
		gboolean push_undo,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW (self));

	/* The appwindow as a whole not interested in any document changes that
	 * don't pertain to the one that is actually in view.
	 */
	if (doc != gtk_hex_get_document (self->gh))
		return;

	ghex_application_window_set_can_save (self, assess_can_save (doc));
}

static void
tab_close_cb (GHexNotebookTab *tab,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	HexDocument *doc;

	g_debug ("%s: start", __func__);

	doc = gtk_hex_get_document (self->gh);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	if (hex_document_has_changed (doc)) {
		close_doc_confirmation_dialog (self);
	} else {
		ghex_application_window_remove_tab (self, tab);
	}
}

static void
notebook_switch_page_cb (GtkNotebook *notebook,
		GtkWidget   *page,
		guint        page_num,
		gpointer     user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	GHexNotebookTab *tab;
	GtkHex *tab_gh;
	HexDocument *doc;

	tab = GHEX_NOTEBOOK_TAB(gtk_notebook_get_tab_label (notebook, page));
	g_return_if_fail (GHEX_IS_NOTEBOOK_TAB (tab));
	tab_gh = ghex_notebook_tab_get_hex (tab);
	g_return_if_fail (GTK_IS_HEX (tab_gh));

	if (tab_gh != self->gh) {
		ghex_application_window_set_hex (self, tab_gh);
		ghex_application_window_activate_tab (self, self->gh);
	}

	/* Assess saveability based on new tab we've switched to */
	doc = gtk_hex_get_document (self->gh);
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
	gtk_widget_show (self->hex_notebook);
	gtk_widget_hide (self->no_doc_label);
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
		gtk_widget_hide (self->hex_notebook);
		gtk_widget_show (self->no_doc_label);
	}
}

static void
pane_close_cb (PaneDialog *pane, gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	g_return_if_fail (PANE_IS_DIALOG (pane));

	gtk_widget_grab_focus (GTK_WIDGET(self->gh));

	if (FIND_IS_DIALOG (pane)) {
		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_FIND_OPEN]);
	}
	else if (REPLACE_IS_DIALOG (pane)) {
		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_REPLACE_OPEN]);
	}
	else if (JUMP_IS_DIALOG (pane)) {
		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_JUMP_OPEN]);
	}
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
	g_return_if_fail (GTK_IS_HEX(self->gh));

	self->chartable = create_char_table (GTK_WINDOW(self), self->gh);

    g_signal_connect(self->chartable, "close-request",
                     G_CALLBACK(chartable_close_cb), self);
}

static void
setup_converter (GHexApplicationWindow *self)
{
	g_return_if_fail (GTK_IS_HEX(self->gh));

	self->converter = create_converter (GTK_WINDOW(self), self->gh);

	g_signal_connect(self->converter, "close-request",
                     G_CALLBACK(converter_close_cb), self);
}

static void
update_gui_data (GHexApplicationWindow *self)
{
	int current_pos;
	HexDialogVal64 val;

	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW (self));
	g_return_if_fail (GTK_IS_HEX (self->gh));

	current_pos = gtk_hex_get_cursor (self->gh);
	update_status_message (self);

	for (int i = 0; i < 8; i++)
	{
		/* returns 0 on buffer overflow, which is what we want */
		val.v[i] = gtk_hex_get_byte (self->gh, current_pos+i);
	}
	hex_dialog_updateview (self->dialog, &val);
}

static void
cursor_moved_cb (GtkHex *gh, gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	int current_pos;
	HexDialogVal64 val;

	/* If the cursor has been moved by a function call for a GtkHex that is
	 * *not* in view, we're not interested. */
	if (self->gh != gh) {
		g_debug("%s: Cursor has been moved for a GtkHex widget not in view: "
				"%p (currently in view == %p)",
				__func__, (void *)gh, (void *)self->gh);
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
	g_debug("%s: start - show: %d", __func__, show);						\
																			\
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
			gtk_widget_grab_focus (GTK_WIDGET(self->gh));					\
		}																	\
	}																		\
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_ARR_ENTRY]);	\
}												/* DIALOG_SET_SHOW_TEMPLATE */

DIALOG_SET_SHOW_TEMPLATE(chartable, setup_chartable(self), PROP_CHARTABLE_OPEN)
DIALOG_SET_SHOW_TEMPLATE(converter, setup_converter(self), PROP_CONVERTER_OPEN)

/* Note that in this macro, it's up to the "closed" cb functions to notify
 * by pspec; otherwise, you get in an infinite loop
 */
#define PANE_SET_SHOW_TEMPLATE(WIDGET, OTHER1, OTHER2, PROP_ARR_ENTRY)		\
static void																	\
ghex_application_window_set_show_ ##WIDGET (GHexApplicationWindow *self,	\
		gboolean show)														\
{																			\
	g_debug("%s: start - show: %d", __func__, show);						\
	if (show) {																\
		ghex_application_window_set_show_ ## OTHER1 (self, FALSE);			\
		ghex_application_window_set_show_ ## OTHER2 (self, FALSE);			\
		gtk_widget_show (self->WIDGET ## _dialog);							\
		gtk_widget_grab_focus (self->WIDGET ## _dialog);					\
		g_object_notify_by_pspec (G_OBJECT(self),							\
				properties[PROP_ARR_ENTRY]);								\
	} else {																\
		g_signal_emit_by_name (self->WIDGET ## _dialog, "closed");			\
	}																		\
}												/* PANE_SET_SHOW_TEMPLATE */

PANE_SET_SHOW_TEMPLATE(find,		replace, jump,	PROP_FIND_OPEN)
PANE_SET_SHOW_TEMPLATE(replace,		find, jump,		PROP_REPLACE_OPEN)
PANE_SET_SHOW_TEMPLATE(jump,		find, replace,	PROP_JUMP_OPEN)

/* Property setters without templates: */

static void
ghex_application_window_set_can_save (GHexApplicationWindow *self,
		gboolean can_save)
{
	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW (self));

	self->can_save = can_save;
	g_debug("%s: start - can_save: %d", __func__, can_save);

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
	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW (self));

	self->insert_mode = insert_mode;

	NOTEBOOK_GH_FOREACH_START

	gtk_hex_set_insert_mode(gh, insert_mode);

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
	doc = gtk_hex_get_document (self->gh);

	gfile = gtk_file_chooser_get_file (chooser);

	if (! hex_document_write_to_file (doc, gfile))
	{
		display_error_dialog (GTK_WINDOW(self),
				_("There was an error saving the file to the path specified."
					"\n\n"
					"You may not have the required permissions."));
		goto end;
	}

	if (hex_document_set_file (doc, gfile))
	{
		hex_document_read (doc);
	}
	else
	{
		g_warning ("%s: error resetting file...", __func__);
	}

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

	g_return_if_fail (GTK_IS_HEX (self->gh));

	doc = gtk_hex_get_document (self->gh);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	file_sel =
		gtk_file_chooser_native_new (_("Select a file to save buffer as"),
				GTK_WINDOW(self),
				GTK_FILE_CHOOSER_ACTION_SAVE,
				NULL,	/* const char *accept_label } NULL == default.	*/
				NULL);	/* const char *cancel_label }					*/

	/* Default suggested file == existing file. */
	gtk_file_chooser_set_file (GTK_FILE_CHOOSER(file_sel),
			hex_document_get_file (doc),
			NULL);	/* GError **error */

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

	doc = gtk_hex_get_document (self->gh);

	hex_document_read (doc);

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
	
	g_return_if_fail (GTK_IS_HEX (self->gh));

	doc = gtk_hex_get_document (self->gh);
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

	g_return_if_fail (GTK_IS_HEX(self->gh));
	(void)widget, (void)action_name, (void)parameter;	/* unused */

	common_print (GTK_WINDOW(self), self->gh, /* preview: */ TRUE);
}

static void
do_print (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);

	g_return_if_fail (GTK_IS_HEX(self->gh));
	(void)widget, (void)action_name, (void)parameter;	/* unused */

	common_print (GTK_WINDOW(self), self->gh, /* preview: */ FALSE);
}

static void
new_file (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
	HexDocument *doc;
	GtkHex *gh;

	doc = hex_document_new ();
	gh = GTK_HEX(gtk_hex_new (doc));

	ghex_application_window_add_hex (self, gh);
	ghex_application_window_set_hex (self, gh);
	ghex_application_window_activate_tab (self, gh);
	ghex_application_window_set_insert_mode (self, TRUE);
}

/* convenience helper function to build a GtkHex widget pre-loaded with
 * a hex document, from a GFile *.
 */
static GtkHex *
new_gh_from_gfile (GFile *file)
{
	HexDocument *doc;
	GtkHex *gh;

	doc = hex_document_new_from_file (file);
	/* FIXME - just return NULL and handle error in user-friendly manner? */
	g_return_val_if_fail (HEX_IS_DOCUMENT (doc), NULL);
		
	gh = GTK_HEX(gtk_hex_new (doc));
	g_return_val_if_fail (GTK_IS_HEX (gh), NULL);

	return gh;
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
	else
	{
		g_debug ("%s: User didn't click Open. Bail out.",
				__func__);
	}
	g_object_unref (dialog);
}

static void
open_file (GtkWidget *widget,
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

	(void)parameter, (void)action_name;		/* unused */

	if (gtk_widget_is_visible (self->conversions_box)) {
		gtk_widget_set_visible (self->conversions_box, FALSE);
		gtk_button_set_icon_name (GTK_BUTTON(self->pane_toggle_button),
				"pan-up-symbolic");
	} else {
		gtk_widget_set_visible (self->conversions_box, TRUE);
		gtk_button_set_icon_name (GTK_BUTTON(self->pane_toggle_button),
				"pan-down-symbolic");
	}
}


static void
open_about (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);

	(void)parameter, (void)action_name;		/* unused */

	common_about_cb (GTK_WINDOW(self));
}

static void
open_preferences (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);

	(void)parameter, (void)action_name;		/* unused */

	if (! GTK_IS_WIDGET (self->prefs_dialog) ||
			! gtk_widget_get_visible (self->prefs_dialog)) {
		self->prefs_dialog = create_preferences_dialog (GTK_WINDOW(self));
	}
	gtk_widget_show (self->prefs_dialog);
}

/* --- */

static void
set_statusbar (GHexApplicationWindow *self, const char *str)
{
	guint id = 
		gtk_statusbar_get_context_id (GTK_STATUSBAR(self->statusbar),
				"status");

	gtk_statusbar_pop (GTK_STATUSBAR(self->statusbar), id);
	gtk_statusbar_push (GTK_STATUSBAR(self->statusbar), id, str);
}

static void
clear_statusbar (GHexApplicationWindow *self)
{
	set_statusbar (self, " ");
}

/* FIXME: This is one of the few functions lifted from the old 'ghex-window.c'.
 * I don't care for the pragmas, etc., so this could be a good candidate for a
 * possible future rewrite.
 */
#define FMT_LEN    128
#define STATUS_LEN 128

static void
update_status_message (GHexApplicationWindow *self)
{
	gchar fmt[FMT_LEN], status[STATUS_LEN];
	gint current_pos;
	gint ss, se, len;

	if (! GTK_IS_HEX(self->gh)) {
		clear_statusbar (self);
		return;
	}

#if defined(__GNUC__) && (__GNUC__ > 4)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
	current_pos = gtk_hex_get_cursor(self->gh);
	if (g_snprintf(fmt, FMT_LEN, _("Offset: %s"), offset_fmt) < FMT_LEN) {
		g_snprintf(status, STATUS_LEN, fmt, current_pos);
		if (gtk_hex_get_selection(self->gh, &ss, &se)) {
			if (g_snprintf(fmt, FMT_LEN, _("; %s bytes from %s to %s selected"),
						offset_fmt, offset_fmt, offset_fmt) < FMT_LEN) {
				len = strlen(status);
				if (len < STATUS_LEN) {
					/* Variables 'ss' and 'se' denotes the offsets of the first
					 * and the last bytes that are part of the selection. */
					g_snprintf(status + len, STATUS_LEN - len, fmt, se - ss +
					1, ss, se);
				}
			}
		}
#if defined(__GNUC__) && (__GNUC__ > 4)
#pragma GCC diagnostic pop
#endif
		set_statusbar(self, status);
	}
	else
		clear_statusbar (self);
}
#undef FMT_LEN
#undef STATUS_LEN


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
			g_value_set_boolean (value,
					GTK_IS_WIDGET(self->chartable) &&
						gtk_widget_get_visible (self->chartable));
			break;

		case PROP_CONVERTER_OPEN:
			g_value_set_boolean (value,
					GTK_IS_WIDGET(self->converter) &&
						gtk_widget_get_visible (self->converter));
			break;

		case PROP_FIND_OPEN:
			g_value_set_boolean (value,
					GTK_IS_WIDGET(self->find_dialog) &&
						gtk_widget_get_visible (self->find_dialog));
			break;

		case PROP_REPLACE_OPEN:
			g_value_set_boolean (value,
					GTK_IS_WIDGET(self->replace_dialog) &&
						gtk_widget_get_visible (self->replace_dialog));
			break;

		case PROP_JUMP_OPEN:
			g_value_set_boolean (value,
					GTK_IS_WIDGET(self->jump_dialog) &&
						gtk_widget_get_visible (self->jump_dialog));
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
	gtk_widget_hide (self->conversions_box);

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
	action = g_settings_create_action (settings,
			GHEX_PREF_GROUP);
	g_action_map_add_action (G_ACTION_MAP(self), action);

	/* Setup notebook signals */

	g_signal_connect (self->hex_notebook, "switch-page",
			G_CALLBACK(notebook_switch_page_cb), self);

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

	clear_statusbar (self);

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

	/* Unref GtkHex */
	g_list_free_full (g_steal_pointer (&self->gh_list), g_object_unref);

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
			open_file);

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

	/* F1 - show about dialog */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_F1,
			0,
			"ghex.about",
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
		GtkHex *gh)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(self->hex_notebook);
	int page_num;

	g_return_if_fail (GTK_IS_HEX (gh));
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	page_num = gtk_notebook_page_num (notebook, GTK_WIDGET(gh));
	g_debug ("%s: got page_num: %d - setting notebook to that page.",
			__func__, page_num);

	gtk_notebook_set_current_page (notebook, page_num);
	gtk_widget_grab_focus (GTK_WIDGET(gh));
}

void
ghex_application_window_set_hex (GHexApplicationWindow *self,
		GtkHex *gh)
{
	HexDocument *doc = gtk_hex_get_document (gh);

	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (HEX_IS_DOCUMENT(doc));

	g_debug ("%s: Setting active gh to: %p",
			__func__, (void *)gh);

	self->gh = gh;

	/* Update dialogs: */

	refresh_dialogs (self);
}

void
ghex_application_window_add_hex (GHexApplicationWindow *self,
		GtkHex *gh)
{
	GtkWidget *tab;
	HexDocument *doc;

	g_return_if_fail (GTK_IS_HEX(gh));

	doc = gtk_hex_get_document (gh);
	g_return_if_fail (HEX_IS_DOCUMENT(doc));

	/* GtkHex: Setup based on global settings. */
	common_set_gtkhex_font_from_settings (gh);

	/* GtkHex: Sync up appwindow-specific settings. */
	set_gtkhex_offsets_column_from_settings (gh);
	set_gtkhex_group_type_from_settings (gh);
	
	/* GtkHex: Set insert mode based on our global appwindow prop */
	gtk_hex_set_insert_mode (gh, self->insert_mode);

	/* Add this GtkHex to our internal list
	 */
	self->gh_list = g_list_append (self->gh_list, gh);
	g_object_ref (gh);

	/* Set this GtkHex as the current viewed gh if there is no currently
	 * open document
	 */
	if (! self->gh)
		ghex_application_window_set_hex (self, gh);

	/* Generate a tab */
	tab = ghex_notebook_tab_new ();
	g_debug ("%s: CREATED TAB -- %p", __func__, (void *)tab);
	ghex_notebook_tab_add_hex (GHEX_NOTEBOOK_TAB(tab), gh);
	g_signal_connect (tab, "closed",
			G_CALLBACK(tab_close_cb), self);

	gtk_notebook_append_page (GTK_NOTEBOOK(self->hex_notebook),
			GTK_WIDGET(gh),
			tab);

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
			G_CALLBACK(ghex_application_window_document_changed_cb), self);

	g_signal_connect (doc, "file-saved",
			G_CALLBACK(ghex_application_window_file_saved_cb), self);
}

GList *
ghex_application_window_get_list (GHexApplicationWindow *self)
{
	g_return_val_if_fail (GHEX_IS_APPLICATION_WINDOW (self), NULL);

	return self->gh_list;
}

void
ghex_application_window_open_file (GHexApplicationWindow *self, GFile *file)
{
	GtkHex *gh;

	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW(self));

	/* If we get it from the GApp :open signal, it's tfr:none - once
	 * HexDocument gets hold of it, though, it _refs it itself so we don't need
	 * to hold onto it.
	 */
	g_object_ref (file);

	gh = new_gh_from_gfile (file);

	ghex_application_window_add_hex (self, gh);
	ghex_application_window_set_hex (self, gh);
	ghex_application_window_activate_tab (self, gh);

	g_object_unref (file);
}

GtkHex *
ghex_application_window_get_hex (GHexApplicationWindow *self)
{
	g_return_val_if_fail (GHEX_IS_APPLICATION_WINDOW (self), NULL);

	return self->gh;
}
