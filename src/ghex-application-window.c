/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ghex-application-window.c - GHex main application window

   Copyright © 2021-2023 Logan Rathbone <poprocks@gmail.com>

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

/* Translators: this is the string for an untitled buffer that will
 * be displayed in the titlebar when a user does File->New
 */
static const char *UNTITLED_STRING = N_("Untitled");

static GFile *tmp_global_gfile_for_nag_screen;

/* ----------------------- */
/* MAIN GOBJECT DEFINITION */
/* ----------------------- */

struct _GHexApplicationWindow
{
	AdwApplicationWindow parent_instance;

	HexWidget *gh;
	HexDialog *dialog;
	GtkWidget *dialog_widget;
	GtkAdjustment *adj;
	gboolean can_save;

	gboolean insert_mode;
	GBinding *insert_mode_binding;

	GtkWidget *find_dialog;
	GtkWidget *replace_dialog;
	GtkWidget *jump_dialog;
	GtkWidget *mark_dialog;

	GtkWidget *chartable;
	GtkWidget *converter;
	GtkWidget *prefs_dialog;
	GtkWidget *paste_special_dialog;
	GtkWidget *copy_special_dialog;

	/* From GtkBuilder: */
	GtkWidget *headerbar_window_title;
	GtkWidget *no_doc_label;
	GtkWidget *child_box;
	GtkWidget *hex_tab_view;
	GtkWidget *conversions_box;
	GtkWidget *findreplace_box;
	GtkWidget *pane_toggle_button;
	GtkWidget *insert_mode_button;
	GtkWidget *statusbar;
	GtkWidget *findreplace_revealer;
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
	PROP_MARK_OPEN,
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
	"ghex.mark-dialog",
	NULL				/* last action */
};

G_DEFINE_TYPE (GHexApplicationWindow, ghex_application_window, ADW_TYPE_APPLICATION_WINDOW)

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
static void ghex_application_window_set_show_mark (GHexApplicationWindow *self,
		gboolean show);
static void ghex_application_window_set_can_save (GHexApplicationWindow *self,
		gboolean can_save);
static gboolean get_pane_visible (GHexApplicationWindow *self, PaneDialog *pane);

static void enable_main_actions (GHexApplicationWindow *self, gboolean enable);
static void update_status_message (GHexApplicationWindow *self);
static void update_gui_data (GHexApplicationWindow *self);
static gboolean assess_can_save (HexDocument *doc);
static void close_doc_confirmation_dialog (GHexApplicationWindow *self,
		AdwTabPage *page);
static void show_no_file_loaded_label (GHexApplicationWindow *self);

static void doc_read_ready_cb (GObject *source_object, GAsyncResult *res,
		gpointer user_data);
static void file_loaded (HexDocument *doc, GHexApplicationWindow *self);

static void ghex_application_window_disconnect_hex_signals (GHexApplicationWindow *self,
		HexWidget *gh);
static void ghex_application_window_connect_hex_signals (GHexApplicationWindow *self,
		HexWidget *gh);

/* GHexApplicationWindow -- PRIVATE FUNCTIONS */

static HexWidget *
get_gh_for_page (GHexApplicationWindow *self, AdwTabPage *page)
{
	GtkWidget *box = adw_tab_page_get_child (page);

	for (GtkWidget *child = gtk_widget_get_first_child (box);
			child != NULL;
			child = gtk_widget_get_next_sibling (child))
	{
		if (HEX_IS_WIDGET (child))
			return HEX_WIDGET(child);
	}

	return NULL;
}

static HexWidget *
get_gh_for_tab (GHexApplicationWindow *self, AdwTabView *tv, int num)
{
	return get_gh_for_page (self, adw_tab_view_get_nth_page (tv, num));
}

/* Common macro to apply something to the 'gh' of each tab of the tab view.
 *
 * Between _START and _END, put in function calls that use `gh` to be applied
 * to each gh in a tab.
 */
#define TAB_VIEW_GH_FOREACH_START											\
{																			\
	AdwTabView *tab_view = ADW_TAB_VIEW(self->hex_tab_view);				\
	int i;																	\
	for (i = adw_tab_view_get_n_pages(tab_view) - 1; i >= 0; --i) {			\
		AdwTabPage *tab_page = adw_tab_view_get_nth_page (tab_view, i);		\
		HexWidget *gh = get_gh_for_tab (self, tab_view, i);
/* !TAB_VIEW_GH_FOREACH_START */

#define TAB_VIEW_GH_FOREACH_END												\
	} 																		\
}																			\
/* !TAB_VIEW_GH_FOREACH_END	*/

static AdwTabPage *
get_tab_for_gh (GHexApplicationWindow *self, HexWidget *target_gh)
{
	TAB_VIEW_GH_FOREACH_START

	if (gh == target_gh)
		return tab_page;

	TAB_VIEW_GH_FOREACH_END

	return NULL;
}

static HexInfoBar *
get_info_bar_for_gh (GHexApplicationWindow *self, HexWidget *target_gh)
{
	TAB_VIEW_GH_FOREACH_START

	if (gh == target_gh)
	{
		GtkWidget *box = adw_tab_page_get_child (tab_page);
		for (GtkWidget *child = gtk_widget_get_first_child (box);
				child != NULL;
				child = gtk_widget_get_next_sibling (child))
		{
			if (HEX_IS_INFO_BAR (child))
				return HEX_INFO_BAR(child);
		}
	}

	TAB_VIEW_GH_FOREACH_END

	return NULL;
}

/* set_*_from_settings
 * These functions all basically set properties from the GSettings global
 * variables. They should be used when a new gh is created, and when we're
 * `foreaching` through all of our tabs via a settings-change cb. See also
 * the `TAB_VIEW_GH_FOREACH_{START,END}` macros above.
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
settings_font_changed_cb (GSettings   *settings,
		const gchar	*key,
		gpointer 	user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	TAB_VIEW_GH_FOREACH_START

	common_set_gtkhex_font_from_settings (gh);

	TAB_VIEW_GH_FOREACH_END
}

static void
settings_offsets_column_changed_cb (GSettings   *settings,
		const gchar	*key,
		gpointer 	user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	TAB_VIEW_GH_FOREACH_START

	set_gtkhex_offsets_column_from_settings (gh);

	TAB_VIEW_GH_FOREACH_END
}

static void
settings_group_type_changed_cb (GSettings   *settings,
		const gchar	*key,
		gpointer 	user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	TAB_VIEW_GH_FOREACH_START

	set_gtkhex_group_type_from_settings (gh);

	TAB_VIEW_GH_FOREACH_END
}

static void
settings_show_control_chars_changed_cb (GSettings   *settings,
		const gchar	*key,
		gpointer 	user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	TAB_VIEW_GH_FOREACH_START

	hex_widget_set_display_control_characters (gh, def_display_control_characters);

	TAB_VIEW_GH_FOREACH_END
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
		pane_dialog_set_hex (PANE_DIALOG(self->mark_dialog), ACTIVE_GH);
		mark_dialog_refresh (MARK_DIALOG(self->mark_dialog));
	}
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
		file_loaded (doc, self);
	}
	else
	{
		GString *full_errmsg = g_string_new (
				_("There was an error saving the file."));

		if (local_error)
			g_string_append_printf (full_errmsg, "\n\n%s", local_error->message);

		display_dialog (GTK_WINDOW(self), full_errmsg->str);

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
close_all_tabs_response_cb (AdwAlertDialog *dialog,
		const char *response,
		GHexApplicationWindow *self)
{
	/* Regardless of what the user chose, get rid of the dialog. */
	adw_dialog_close (ADW_DIALOG (dialog));

	if (g_strcmp0 (response, "discard") == 0)
		gtk_window_destroy (GTK_WINDOW(self));
}

static void
close_all_tabs_confirmation_dialog (GHexApplicationWindow *self)
{
	AdwDialog *dialog = adw_alert_dialog_new (_("Save Changes?"), NULL);

	adw_alert_dialog_set_body (ADW_ALERT_DIALOG(dialog),
			_("Open documents contain unsaved changes.\n"
			   "Changes which are not saved will be permanently lost."));
	adw_alert_dialog_add_responses (ADW_ALERT_DIALOG(dialog),
			"cancel", _("_Cancel"),
			"discard", _("_Discard"),
			NULL);
	adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG(dialog),
			"discard",
			ADW_RESPONSE_DESTRUCTIVE);
	adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG(dialog), "cancel");

	g_signal_connect (dialog, "response", G_CALLBACK(close_all_tabs_response_cb), self);

	adw_dialog_present (dialog, GTK_WIDGET(self));
}

static void
check_close_window (GHexApplicationWindow *self)
{
	AdwTabView *tab_view = ADW_TAB_VIEW(self->hex_tab_view);
	gboolean unsaved_found = FALSE;
	int i;
	const int num_pages = adw_tab_view_get_n_pages (tab_view);

	/* We have more than one tab open: */
	for (i = num_pages - 1; i >= 0; --i)
	{
		HexWidget *gh;
		HexDocument *doc = NULL;

		gh = get_gh_for_tab (self, tab_view, i);
		doc = hex_widget_get_document (gh);

		if (hex_document_has_changed (doc))
			unsaved_found = TRUE;
	}

	if (unsaved_found) {
		close_all_tabs_confirmation_dialog (self);
	} else {
		gtk_window_destroy (GTK_WINDOW(self));
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
close_page_finish_helper (GHexApplicationWindow *self, AdwTabView *tab_view, AdwTabPage *page, gboolean confirm)
{
	if (confirm && adw_tab_view_get_n_pages (tab_view) == 1)
	{
		enable_main_actions (self, FALSE);
		ghex_application_window_set_show_find (self, FALSE);
		ghex_application_window_set_show_replace (self, FALSE);
		ghex_application_window_set_show_jump (self, FALSE);
		ghex_application_window_set_show_mark (self, FALSE);
		ghex_application_window_set_show_chartable (self, FALSE);
		ghex_application_window_set_show_converter (self, FALSE);

		show_no_file_loaded_label (self);
	}
	adw_tab_view_close_page_finish (tab_view, page, confirm);
	update_gui_data (self);
}

static void
close_doc_response_cb (AdwAlertDialog *dialog,
		const char *response,
		GHexApplicationWindow *self)
{
	AdwTabView *tab_view = ADW_TAB_VIEW(self->hex_tab_view);
	AdwTabPage *page = g_object_get_data (G_OBJECT(self), "target-page");

	if (g_strcmp0 (response, "save") == 0)
	{
		file_save (self);
		close_page_finish_helper (self, tab_view, page, TRUE);
	}
	else if (g_strcmp0 (response, "discard") == 0)
	{
		close_page_finish_helper (self, tab_view, page, TRUE);
	}
	else
	{
		close_page_finish_helper (self, tab_view, page, FALSE);
	}

	adw_dialog_close (ADW_DIALOG (dialog));
	gtk_widget_grab_focus (GTK_WIDGET (ACTIVE_GH));
}

static void
close_doc_confirmation_dialog (GHexApplicationWindow *self, AdwTabPage *page)
{
	AdwDialog *dialog;
	HexDocument *doc;
	char *basename = NULL;
	char *title = NULL;
	HexWidget *gh = get_gh_for_page (self, page);

	doc = hex_widget_get_document (gh);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	basename = common_get_ui_basename (doc);

	if (basename) {
		/* Translators: %s is the filename that is currently being
		 * edited. */
		title = g_strdup_printf (_("%s has been edited since opening."), basename);
	}
	else {
		title = _("The buffer has been edited since opening.");
	}
	g_free (basename);

	dialog = adw_alert_dialog_new (title, NULL);
	g_free (title);

	adw_alert_dialog_set_body (ADW_ALERT_DIALOG(dialog),
			_("Would you like to save your changes?"));
	adw_alert_dialog_add_responses (ADW_ALERT_DIALOG(dialog),
			"cancel", _("_Cancel"),
			"discard", _("_Discard"),
			"save", _("_Save"),
			NULL);
	adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG(dialog), "cancel");
	adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG(dialog),
			"discard",
			ADW_RESPONSE_DESTRUCTIVE);
	adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG(dialog),
			"save",
			ADW_RESPONSE_SUGGESTED);
	g_signal_connect (dialog, "response", G_CALLBACK(close_doc_response_cb), self);

	g_object_set_data (G_OBJECT(self), "target-page", page);

	adw_dialog_present (ADW_DIALOG(dialog), GTK_WIDGET (self));
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
	AdwTabPage *page = adw_tab_view_get_selected_page (ADW_TAB_VIEW(self->hex_tab_view));

	if (page)
		adw_tab_view_close_page (ADW_TAB_VIEW(self->hex_tab_view), page);
	else
		gtk_window_destroy (GTK_WINDOW(self));

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
show_hex_tab_view (GHexApplicationWindow *self)
{
	gtk_widget_set_visible (self->no_doc_label, FALSE);
	gtk_widget_set_visible (self->child_box, TRUE);
}

static void
show_no_file_loaded_label (GHexApplicationWindow *self)
{
	gtk_widget_set_visible (self->child_box, FALSE);
	gtk_widget_set_visible (self->no_doc_label, TRUE);
}

static void
update_tabs (GHexApplicationWindow *self)
{
	HexDocument *doc = NULL;
	GFile *gfile = NULL;

	TAB_VIEW_GH_FOREACH_START

	char *basename = NULL;

	/* set text of context menu tab switcher to the filename rather than
	 * 'Page X' */
	doc = hex_widget_get_document (gh);
	gfile = hex_document_get_file (doc);

	if (gfile)
		basename = common_get_ui_basename (doc);
	else
		basename = g_strdup (_(UNTITLED_STRING));

	adw_tab_page_set_title (get_tab_for_gh (self, gh), basename);

	g_free (basename);

	TAB_VIEW_GH_FOREACH_END
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
	adw_tab_page_set_icon (get_tab_for_gh (self, ACTIVE_GH), NULL);
	hex_info_bar_set_shown (get_info_bar_for_gh (self, ACTIVE_GH), FALSE);
}

static void
document_changed_cb (HexDocument *doc,
		HexChangeData *change_data,
		gboolean push_undo,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	document_loaded_or_saved_common (self, doc);

	TAB_VIEW_GH_FOREACH_START

	if (hex_widget_get_document (gh) == doc)
	{
		GIcon *icon;
		HexInfoBar *info_bar;

		icon = g_themed_icon_new ("document-modified-symbolic");
		adw_tab_page_set_icon (get_tab_for_gh (self, gh), icon);
		g_object_unref (icon);

		info_bar = get_info_bar_for_gh (self, gh);
		hex_info_bar_set_shown (info_bar, change_data->external_file_change);
	}

	TAB_VIEW_GH_FOREACH_END
}

static gboolean
tab_view_close_page_cb (AdwTabView *tab_view,
		AdwTabPage* page,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	HexDocument *doc;
	HexWidget *gh;

	gh = get_gh_for_page (self, page);
	doc = hex_widget_get_document (gh);

	if (hex_document_has_changed (doc)) {
		close_doc_confirmation_dialog (self, page);
	} else {
		close_page_finish_helper (self, tab_view, page, TRUE);
	}

	return GDK_EVENT_STOP;
}

static void
tab_view_page_changed_cb (AdwTabView *tab_view,
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

	/* Bind insert mode between widget and appwin */

	g_clear_pointer (&self->insert_mode_binding, g_binding_unbind);
	self->insert_mode_binding = g_object_bind_property (ACTIVE_GH, "insert-mode",
			self, "insert-mode",
			G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
	g_object_add_weak_pointer (G_OBJECT(self->insert_mode_binding),
			(gpointer *)&self->insert_mode_binding);

	update_gui_data (self);
}

static void
tab_view_page_attached_cb (AdwTabView *tab_view,
		AdwTabPage *page,
		int page_num,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	ghex_application_window_connect_hex_signals (self, get_gh_for_page (self, page));

	/* Let's play this super dumb. If a page is added, that will generally
	 * mean we don't have to count the pages to see if we have > 0.
	 */
	enable_main_actions (self, TRUE);
}

static void
tab_view_page_detached_cb (AdwTabView *tab_view,
		AdwTabPage *page,
		int page_num,

		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	ghex_application_window_disconnect_hex_signals (self, get_gh_for_page (self, page));
}

static AdwTabView *
tab_view_create_window_cb (AdwTabView *tab_view, gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	GHexApplicationWindow *new_appwin;

	new_appwin = GHEX_APPLICATION_WINDOW(ghex_application_window_new (
				ADW_APPLICATION(gtk_window_get_application (GTK_WINDOW(self)))));

	gtk_window_present (GTK_WINDOW(new_appwin));
	show_hex_tab_view (new_appwin);

	return ADW_TAB_VIEW(new_appwin->hex_tab_view);
}

static void
pane_close_cb (PaneDialog *pane, gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	if (ACTIVE_GH)
		gtk_widget_grab_focus (GTK_WIDGET(ACTIVE_GH));

	gtk_revealer_set_transition_type (GTK_REVEALER(self->findreplace_revealer),
			GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
	gtk_revealer_set_reveal_child (GTK_REVEALER(self->findreplace_revealer),
			FALSE);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_FIND_OPEN]);
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_REPLACE_OPEN]);
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_JUMP_OPEN]);
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_MARK_OPEN]);
}

static gboolean
chartable_close_cb (GHexApplicationWindow *self)
{
	ghex_application_window_set_show_chartable (self, FALSE);

	return GDK_EVENT_STOP;
}

static void
setup_chartable (GHexApplicationWindow *self)
{
	g_return_if_fail (HEX_IS_WIDGET(ACTIVE_GH));

	self->chartable = create_char_table (GTK_WINDOW(self), ACTIVE_GH);

    g_signal_connect_swapped (self->chartable, "close-request",
                     G_CALLBACK(chartable_close_cb), self);
}

static gboolean
converter_close_cb (GHexApplicationWindow *self)
{
	ghex_application_window_set_show_converter (self, FALSE);

	return GDK_EVENT_STOP;
}

static void
setup_converter (GHexApplicationWindow *self)
{
	g_return_if_fail (HEX_IS_WIDGET(ACTIVE_GH));

	self->converter = create_converter (GTK_WINDOW(self), ACTIVE_GH);

    g_signal_connect_swapped (self->converter, "close-request",
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
			adw_tab_view_get_n_pages (ADW_TAB_VIEW(self->hex_tab_view)))
	{
		char *basename;
		char *pathname = NULL;
		char *title;
		HexDocument *doc = hex_widget_get_document (ACTIVE_GH);

		if (G_IS_FILE (file = hex_document_get_file (doc)))
		{
			GFile *parent = g_file_get_parent (file);

			basename = common_get_ui_basename (doc);
			if (parent)
				pathname = g_utf8_make_valid (g_file_peek_path (parent), -1);

			g_clear_object (&parent);
		}
		else
		{
			basename = g_strdup (_(UNTITLED_STRING));
		}

		adw_window_title_set_title (
				ADW_WINDOW_TITLE(self->headerbar_window_title), basename);
		adw_window_title_set_subtitle (
				ADW_WINDOW_TITLE(self->headerbar_window_title), pathname);

		title = g_strdup_printf ("%s - GHex", basename);
		gtk_window_set_title (GTK_WINDOW(self), title);

		g_free (basename);
		g_free (title);
		g_free (pathname);
	}
	else
	{
		adw_window_title_set_title (
				ADW_WINDOW_TITLE(self->headerbar_window_title), "GHex");
		adw_window_title_set_subtitle (
				ADW_WINDOW_TITLE(self->headerbar_window_title), NULL);
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
	update_tabs (self);

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

static gboolean
dnd_drop_cb (GtkDropTarget *target, const GValue *value, double x, double y,
		gpointer data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(data);

	GSList *file_list = g_value_get_boxed (value);
	GSList *l;
	for (l = file_list; l; l = l->next)
	{
		GFile *file = l->data;
		ghex_application_window_open_file (self, file);
	}

	return TRUE;
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
		gtk_widget_set_visible (self->WIDGET, TRUE);						\
	}																		\
	else																	\
	{																		\
		if (GTK_IS_WIDGET (self->WIDGET) &&									\
				gtk_widget_is_visible (self->WIDGET)) {						\
			gtk_widget_set_visible (self->WIDGET, FALSE);					\
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
#define PANE_SET_SHOW_TEMPLATE(WIDGET, OTHER1, OTHER2, OTHER3)						\
static void																			\
ghex_application_window_set_show_ ##WIDGET (GHexApplicationWindow *self,			\
		gboolean show)																\
{																					\
	if (show) {																		\
		gtk_widget_set_visible (self->OTHER1 ## _dialog, FALSE);					\
		gtk_widget_set_visible (self->OTHER2 ## _dialog, FALSE);					\
		gtk_widget_set_visible (self->OTHER3 ## _dialog, FALSE);					\
		gtk_widget_set_visible (self->WIDGET ## _dialog, TRUE);						\
		gtk_widget_grab_focus (self->WIDGET ## _dialog);							\
		if (! gtk_revealer_get_reveal_child (GTK_REVEALER(self->findreplace_revealer)))	\
		{																			\
			gtk_revealer_set_transition_type (GTK_REVEALER(self->findreplace_revealer),	\
					GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);							\
			gtk_revealer_set_reveal_child (GTK_REVEALER(self->findreplace_revealer), TRUE); \
		}																			\
		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_FIND_OPEN]);		\
		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_REPLACE_OPEN]);	\
		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_JUMP_OPEN]);		\
		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_MARK_OPEN]);		\
	} else {																		\
		g_signal_emit_by_name (self->WIDGET ## _dialog, "closed");					\
	}																				\
}												/* PANE_SET_SHOW_TEMPLATE */

PANE_SET_SHOW_TEMPLATE(find,		replace, jump, mark)
PANE_SET_SHOW_TEMPLATE(replace,		find, jump, mark)
PANE_SET_SHOW_TEMPLATE(jump,		find, replace, mark)
PANE_SET_SHOW_TEMPLATE(mark,		find, replace, jump)

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
	GFile *gfile = g_object_get_data (G_OBJECT(self), "target-file");
	GError *local_error = NULL;
	gboolean write_successful;

	write_successful = hex_document_write_finish (doc, res, &local_error);

	if (! write_successful)
	{
		GString *full_errmsg = g_string_new (
				_("There was an error saving the file to the path specified."));

		if (local_error)
			g_string_append_printf (full_errmsg, "\n\n%s", local_error->message);

		display_dialog (GTK_WINDOW(self), full_errmsg->str);

		g_string_free (full_errmsg, TRUE);
		return;
	}

	if (hex_document_set_file (doc, gfile))
	{
		g_object_set_data (G_OBJECT(self), "target-gh", ACTIVE_GH);
		hex_document_read_async (doc, NULL, doc_read_ready_cb, self);
	}
	else
	{
		display_dialog (GTK_WINDOW(self),
				_("An unknown error has occurred in attempting to reload the "
					"file you have just saved."));
	}
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
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

	gfile = gtk_file_chooser_get_file (chooser);
	g_object_set_data (G_OBJECT(self), "target-file", gfile);

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

	gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (file_sel), TRUE);
	gtk_native_dialog_show (GTK_NATIVE_DIALOG(file_sel));
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void
revert_response_cb (AdwAlertDialog *dialog,
		const char *response,
		GHexApplicationWindow *self)
{
	HexDocument *doc;

	if (g_strcmp0 (response, "revert") != 0)
		goto end;

	doc = hex_widget_get_document (ACTIVE_GH);

	g_object_set_data (G_OBJECT(self), "target-gh", ACTIVE_GH);
	hex_document_read_async (doc, NULL, doc_read_ready_cb, self);

end:
	adw_dialog_close (ADW_DIALOG(dialog));
}

static void
revert (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
   	HexDocument *doc;
	AdwDialog *dialog;
	gint reply;
	char *basename = NULL;

	g_return_if_fail (HEX_IS_WIDGET (ACTIVE_GH));

	doc = hex_widget_get_document (ACTIVE_GH);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	/* Yes, this *is* a programmer error. The Revert menu should not be shown
	 * to the user at all if there is nothing to revert. */
	g_return_if_fail (hex_document_has_changed (doc));

	basename = common_get_ui_basename (doc);

	/* Translators: %s here is the filename the user is being asked to
	 * confirm whether they want to revert. */
	dialog = adw_alert_dialog_new (g_strdup_printf (_("Are you sure you want to revert %s?"), basename), NULL);

	g_free (basename);

	adw_alert_dialog_set_body (ADW_ALERT_DIALOG(dialog),
			_("Your changes will be lost.\n"
			"This action cannot be undone."));
	adw_alert_dialog_add_responses (ADW_ALERT_DIALOG(dialog),
			"cancel", _("_Cancel"),
			"revert", _("_Revert"),
			NULL);
	adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG(dialog), "cancel");
	adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG(dialog),
			"revert",
			ADW_RESPONSE_DESTRUCTIVE);

	g_signal_connect (dialog, "response", G_CALLBACK(revert_response_cb), self);

	adw_dialog_present (dialog, GTK_WIDGET (self));
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
	gh = g_object_new (HEX_TYPE_WIDGET,
			"document", doc,
			"fade-zeroes", TRUE,
			NULL);

	ghex_application_window_add_hex (self, gh);
	refresh_dialogs (self);
	ghex_application_window_activate_tab (self, gh);
	ghex_application_window_set_insert_mode (self, TRUE);
	file_loaded (doc, self);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
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

	gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (file_sel), TRUE);
	gtk_native_dialog_show (GTK_NATIVE_DIALOG(file_sel));
}
G_GNUC_END_IGNORE_DEPRECATIONS

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

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
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
		display_dialog (GTK_WINDOW(self), full_errmsg);
out:
		g_clear_object (&file);
		g_free (uri);
		g_free (full_errmsg);
		g_clear_pointer (&local_error, g_error_free);
#undef URI_PREFIX
#else
		display_dialog (GTK_WINDOW(self), local_error->message);
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
G_GNUC_END_IGNORE_DEPRECATIONS

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
	gtk_widget_set_visible (self->prefs_dialog, TRUE);
}

/* Mark actions */

static void
goto_mark (GHexApplicationWindow *self, const char *key)
{
	HexWidgetMark *mark;

	mark = g_object_get_data (G_OBJECT(ACTIVE_GH), key);

	if (! mark) {
		g_debug ("%s: No mark found at %s", __func__, key);
		return;
	}

	hex_widget_goto_mark (ACTIVE_GH, mark);
	gtk_widget_grab_focus (GTK_WIDGET(ACTIVE_GH));
}

static void
set_mark (GHexApplicationWindow *self, const char *key)
{
	HexWidgetMark *mark;
	gint64 ss, se;

	mark = g_object_get_data (G_OBJECT(ACTIVE_GH), key);

	if (mark) {
		g_debug ("%s: Pre-existing mark found at %s - deleting", __func__, key);
		hex_widget_delete_mark (ACTIVE_GH, mark);
	}

	hex_widget_get_selection (ACTIVE_GH, &ss, &se);
	mark = hex_widget_add_mark (ACTIVE_GH, ss, se, /*GdkRGBA *color*)*/ NULL);

	g_object_set_data (G_OBJECT(ACTIVE_GH), key, mark);

	hex_widget_clear_selection (ACTIVE_GH);
	update_gui_data (self);
	gtk_widget_grab_focus (GTK_WIDGET(ACTIVE_GH));
}

static void
delete_mark (GHexApplicationWindow *self, const char *key)
{
	HexWidgetMark *mark;

	mark = g_object_get_data (G_OBJECT(ACTIVE_GH), key);

	if (mark) {
		hex_widget_delete_mark (ACTIVE_GH, mark);
	}
	else {
		g_debug ("%s: No mark found at %s - ignoring", __func__, key);
		return;
	}

	g_object_set_data (G_OBJECT(ACTIVE_GH), key, NULL);

	update_gui_data (self);
	gtk_widget_grab_focus (GTK_WIDGET(ACTIVE_GH));
}

static void
mark_action (GtkWidget *widget, const char *action_name, GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
	HexWidgetMark *mark;
	char *mark_action;
	int mark_num;
	char *mark_name;
	char *key;

	if (! ACTIVE_GH)
		return;

	g_variant_get (parameter, "(sims)", &mark_action, &mark_num, &mark_name);
	key = g_strdup_printf ("mark%d", mark_num);

	if (g_strcmp0 (mark_action, "jump") == 0)
	{
		goto_mark (self, key);
	}
	else if (g_strcmp0 (mark_action, "set") == 0)
	{
		set_mark (self, key);
	}
	else if (g_strcmp0 (mark_action, "delete") == 0)
	{
		delete_mark (self, key);
	}
	else
	{
		g_critical ("%s: Invalid action: %s", __func__, mark_action);
	}

	g_free (mark_action);
	g_free (mark_name);
	g_free (key);
}

static void
activate_mark_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
	int mark_num = g_variant_get_int32 (parameter);

	if (mark_num < 0 || mark_num > 9) {
		g_warning ("%s: Programmer error: invalid mark number", __func__);
		return;
	}

	if (get_pane_visible (self, PANE_DIALOG(self->mark_dialog)))
		mark_dialog_activate_mark_num (MARK_DIALOG(self->mark_dialog), mark_num);
}

/* --- */

static void
update_status_message (GHexApplicationWindow *self)
{
	char *status = NULL;
	gint64 current_pos, ss, se;
	int len;
	gboolean selection;

	if (! ACTIVE_GH)
		goto out;

	current_pos = hex_widget_get_cursor (ACTIVE_GH);
	selection = hex_widget_get_selection (ACTIVE_GH, &ss, &se);

	switch (def_sb_offset_format) {
		case HEX_WIDGET_STATUS_BAR_OFFSET_HEX:
			if (selection) {
				status = g_strdup_printf (
					_("Offset: <tt>0x%lX</tt>; <tt>0x%lX</tt> bytes from <tt>0x%lX</tt> to <tt>0x%lX</tt> selected"),
					current_pos, se - ss + 1, ss, se);
			} else {
				status = g_strdup_printf (_("Offset: <tt>0x%lX</tt>"), current_pos);
			}
			break;

		case HEX_WIDGET_STATUS_BAR_OFFSET_DEC:
			if (selection) {
				status = g_strdup_printf (
					_("Offset: <tt>%ld</tt>; <tt>%ld</tt> bytes from <tt>%ld</tt> to <tt>%ld</tt> selected"),
					current_pos, se - ss + 1, ss, se);
			} else {
				status = g_strdup_printf (_("Offset: <tt>%ld</tt>"), current_pos);
			}
			break;

		case HEX_WIDGET_STATUS_BAR_OFFSET_HEX | HEX_WIDGET_STATUS_BAR_OFFSET_DEC:
			if (selection) {
				status = g_strdup_printf (
					/* Weird rendering if you don't put the space at the start */
					_(" <sub>HEX</sub> Offset: <tt>0x%lX</tt>; <tt>0x%lX</tt> bytes from <tt>0x%lX</tt> to <tt>0x%lX</tt> selected <sub>DEC</sub> Offset: <tt>%ld</tt>; <tt>%ld</tt> bytes from <tt>%ld</tt> to <tt>%ld</tt> selected"),
					current_pos, se - ss + 1, ss, se,
					current_pos, se - ss + 1, ss, se);
			} else {
				status = g_strdup_printf (_(" <sub>HEX</sub> Offset: <tt>0x%lX</tt> <sub>DEC</sub> Offset: <tt>%ld</tt>"), current_pos, current_pos);
			}
			break;
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
	return (GTK_IS_WIDGET (widget) && gtk_widget_get_visible (widget));
}

static gboolean
get_pane_visible (GHexApplicationWindow *self, PaneDialog *pane)
{
	return (GTK_IS_WIDGET(GTK_WIDGET(pane)) &&
		gtk_widget_get_visible (GTK_WIDGET(pane)) &&
		gtk_revealer_get_reveal_child (GTK_REVEALER(self->findreplace_revealer)));
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

		case PROP_MARK_OPEN:
			ghex_application_window_set_show_mark (self,
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

		case PROP_MARK_OPEN:
			g_value_set_boolean (value,
					get_pane_visible (self, PANE_DIALOG(self->mark_dialog)));
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
	GtkDropTarget *target;
	GtkCssProvider *provider;
	GtkWindowGroup *group;

	/* Ensure widgets are registered with the type system. */
	g_type_ensure (HEX_TYPE_STATUSBAR);
	g_type_ensure (HEX_TYPE_INFO_BAR);

	gtk_widget_init_template (widget);

	/* Setup conversions box and pane */
	self->dialog = hex_dialog_new ();
	self->dialog_widget = hex_dialog_getview (self->dialog);

	gtk_box_append (GTK_BOX(self->conversions_box), self->dialog_widget);

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

    g_signal_connect (settings, "changed::" GHEX_PREF_CONTROL_CHARS,
                      G_CALLBACK (settings_show_control_chars_changed_cb), self);

    g_signal_connect_swapped (settings, "changed::" GHEX_PREF_SB_OFFSET_FORMAT,
                      G_CALLBACK (update_status_message), self);

	/* Actions - SETTINGS */

	/* for the 'group data by' stuff. There isn't a function to do this from
	 * class-init, so we end up with the 'win' namespace when we do it from
	 * here. Be aware of that.
	 */
	action = g_settings_create_action (settings, GHEX_PREF_GROUP);
	g_action_map_add_action (G_ACTION_MAP(self), action);

	/* Setup tab view */

	/* Don't trample over Ctrl+Home/End & friends. */
	adw_tab_view_remove_shortcuts (ADW_TAB_VIEW(self->hex_tab_view),
			ADW_TAB_VIEW_SHORTCUT_CONTROL_END |
			ADW_TAB_VIEW_SHORTCUT_CONTROL_HOME |
			ADW_TAB_VIEW_SHORTCUT_CONTROL_SHIFT_END |
			ADW_TAB_VIEW_SHORTCUT_CONTROL_SHIFT_HOME);

	g_signal_connect (self->hex_tab_view, "notify::selected-page",
			G_CALLBACK(tab_view_page_changed_cb), self);

	g_signal_connect (self->hex_tab_view, "page-attached",
			G_CALLBACK(tab_view_page_attached_cb), self);

	g_signal_connect (self->hex_tab_view, "page-detached",
			G_CALLBACK(tab_view_page_detached_cb), self);

	g_signal_connect (self->hex_tab_view, "close-page",
			G_CALLBACK(tab_view_close_page_cb), self);

	g_signal_connect (self->hex_tab_view, "create-window",
			G_CALLBACK(tab_view_create_window_cb), self);

	/* Get find_dialog and friends geared up */

	self->find_dialog = find_dialog_new ();
	gtk_widget_set_hexpand (self->find_dialog, TRUE);
	gtk_box_append (GTK_BOX(self->findreplace_box), self->find_dialog);
	gtk_widget_set_visible (self->find_dialog, FALSE);
	g_signal_connect (self->find_dialog, "closed",
			G_CALLBACK(pane_close_cb), self);

	self->replace_dialog = replace_dialog_new ();
	gtk_widget_set_hexpand (self->replace_dialog, TRUE);
	gtk_box_append (GTK_BOX(self->findreplace_box), self->replace_dialog);
	gtk_widget_set_visible (self->replace_dialog, FALSE);
	g_signal_connect (self->replace_dialog, "closed",
			G_CALLBACK(pane_close_cb), self);

	self->jump_dialog = jump_dialog_new ();
	gtk_widget_set_hexpand (self->jump_dialog, TRUE);
	gtk_box_append (GTK_BOX(self->findreplace_box), self->jump_dialog);
	gtk_widget_set_visible (self->jump_dialog, FALSE);
	g_signal_connect (self->jump_dialog, "closed",
			G_CALLBACK(pane_close_cb), self);

	self->mark_dialog = mark_dialog_new ();
	gtk_widget_set_hexpand (self->mark_dialog, TRUE);
	gtk_box_append (GTK_BOX(self->findreplace_box), self->mark_dialog);
	gtk_widget_set_visible (self->mark_dialog, FALSE);
	g_signal_connect (self->mark_dialog, "closed",
			G_CALLBACK(pane_close_cb), self);

	hex_statusbar_clear (HEX_STATUSBAR(self->statusbar));

	/* Grey out main actions at the beginning */
	enable_main_actions (self, FALSE);

	/* Grey out save (special case - it's not lumped in with mains */
	gtk_widget_action_set_enabled (GTK_WIDGET(self),
			"ghex.save", FALSE);

	/* DnD */

	target = gtk_drop_target_new (G_TYPE_FILE, GDK_ACTION_COPY);
	gtk_drop_target_set_gtypes (target, (GType[1]) { GDK_TYPE_FILE_LIST }, 1);
	g_signal_connect (target, "drop", G_CALLBACK(dnd_drop_cb), self);
	gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER(target));

	/* CSS */

	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_resource (provider, RESOURCE_BASE_PATH "/css/ghex.css");
	gtk_style_context_add_provider_for_display (gdk_display_get_default (),
			GTK_STYLE_PROVIDER (provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	/* Add to window group */

	group = gtk_window_group_new ();
	gtk_window_group_add_window (group, GTK_WINDOW(self));
	g_object_unref (group);
}

static void
ghex_application_window_dispose (GObject *object)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(object);

	g_clear_object (&self->dialog);

	g_clear_weak_pointer (&self->copy_special_dialog);
	g_clear_weak_pointer (&self->paste_special_dialog);
	g_clear_weak_pointer (&self->insert_mode_binding);

	/* Chain up */
	G_OBJECT_CLASS(ghex_application_window_parent_class)->dispose (object);
}

static void
ghex_application_window_class_init (GHexApplicationWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags prop_flags = G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose = ghex_application_window_dispose;
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

	properties[PROP_MARK_OPEN] =
		g_param_spec_boolean ("mark-open", NULL, NULL,
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

	gtk_widget_class_install_action (widget_class, "ghex.mark",
			"(sims)",
			mark_action);

	gtk_widget_class_install_action (widget_class, "ghex.activate-mark",
			"i",
			activate_mark_action);

	gtk_widget_class_install_property_action (widget_class,
			"ghex.find", "find-open");

	gtk_widget_class_install_property_action (widget_class,
			"ghex.replace", "replace-open");

	gtk_widget_class_install_property_action (widget_class,
			"ghex.mark-dialog", "mark-open");

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

	/* Ctrl+comma - show preferences */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_comma,
			GDK_CONTROL_MASK,
			"ghex.preferences",
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

	/* Ctrl+M - marks */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_m,
			GDK_CONTROL_MASK,
			"ghex.mark-dialog",
			NULL);

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

	/* Ctrl+N - new window */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_n,
			GDK_CONTROL_MASK,
			"app.new-window",
			NULL);

	/* Ctrl+T - new tab */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_t,
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

	/* Ctrl+{0-9} - activate mark spinbtn value */
	/* r! ./generate-mark-bindings.sh
	 */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_0,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			0);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_0,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			0);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_1,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			1);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_1,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			1);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_2,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			2);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_2,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			2);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_3,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			3);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_3,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			3);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_4,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			4);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_4,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			4);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_5,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			5);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_5,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			5);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_6,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			6);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_6,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			6);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_7,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			7);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_7,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			7);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_8,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			8);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_8,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			8);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_9,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			9);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_9,
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			9);

	/* In case you're looking for Ctrl+PageUp & PageDown - those are baked in
	 * via AdwTabView */

	/* WIDGET TEMPLATE .UI */

	gtk_widget_class_set_template_from_resource (widget_class,
					RESOURCE_BASE_PATH "/ghex-application-window.ui");

	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			headerbar_window_title);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			no_doc_label);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			child_box);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			hex_tab_view);
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
			findreplace_revealer);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			conversions_revealer);
}

GtkWidget *
ghex_application_window_new (AdwApplication *app)
{
	return g_object_new (GHEX_TYPE_APPLICATION_WINDOW,
			"application", app,
			NULL);
}

void
ghex_application_window_activate_tab (GHexApplicationWindow *self,
		HexWidget *gh)
{
	AdwTabView *tab_view = ADW_TAB_VIEW(self->hex_tab_view);

	g_return_if_fail (HEX_IS_WIDGET (gh));

	adw_tab_view_set_selected_page (tab_view, get_tab_for_gh (self, gh));

	gtk_widget_grab_focus (GTK_WIDGET(gh));
}

static void
ghex_application_window_disconnect_hex_signals (GHexApplicationWindow *self,
		HexWidget *gh)
{
	HexDocument *doc;

	g_return_if_fail (HEX_IS_WIDGET(gh));
	doc = hex_widget_get_document (gh);
	g_return_if_fail (HEX_IS_DOCUMENT(doc));

	g_signal_handlers_disconnect_by_data (gh, self);
	g_signal_handlers_disconnect_by_data (doc, self);
}

static void
ghex_application_window_connect_hex_signals (GHexApplicationWindow *self,
		HexWidget *gh)
{
	HexDocument *doc;

	g_return_if_fail (HEX_IS_WIDGET(gh));
	doc = hex_widget_get_document (gh);
	g_return_if_fail (HEX_IS_DOCUMENT(doc));

	g_signal_connect (gh, "cursor-moved", G_CALLBACK(cursor_moved_cb), self);
	g_signal_connect (doc, "document-changed", G_CALLBACK(document_changed_cb), self);
	g_signal_connect (doc, "file-saved", G_CALLBACK(file_saved_cb), self);
}

void
ghex_application_window_add_hex (GHexApplicationWindow *self,
		HexWidget *gh)
{
	GtkWidget *info_bar;
	GtkWidget *box;
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
	hex_widget_set_display_control_characters (gh, def_display_control_characters);

	/* HexWidget: Set insert mode based on our global appwindow prop */
	hex_widget_set_insert_mode (gh, self->insert_mode);

	/* Setup box to hold info_bar and HexWidget */
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	info_bar = hex_info_bar_new ();

	gtk_box_append (GTK_BOX(box), info_bar);
	gtk_box_append (GTK_BOX(box), GTK_WIDGET(gh));
	gtk_widget_set_hexpand (GTK_WIDGET(gh), TRUE);
	gtk_widget_set_vexpand (GTK_WIDGET(gh), TRUE);

	/* Add tab */
	adw_tab_view_append (ADW_TAB_VIEW(self->hex_tab_view), box);

	show_hex_tab_view (self);
}

/* This nag screen is kind of stop-gap-ish anyway and will likely be removed
 * in ghex5, so we're going to be lazy and not port it to AdwMessageDialog.
 */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
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
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_YES_NO,
				"%s",
				msg);
		g_signal_connect (nag_screen, "response",
				G_CALLBACK(nag_screen_response_cb), self);
		gtk_widget_set_visible (nag_screen, TRUE);
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void
doc_read_ready_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	HexWidget *gh = g_object_get_data (G_OBJECT(self), "target-gh");
	HexDocument *doc = HEX_DOCUMENT(source_object);
	AdwTabView *tab_view = ADW_TAB_VIEW(self->hex_tab_view);
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
		adw_tab_view_close_page (tab_view,
				adw_tab_view_get_selected_page (tab_view));

		if (local_error)
		{
			display_dialog (GTK_WINDOW(self), local_error->message);
			g_error_free (local_error);
		}
		else
		{
			display_dialog (GTK_WINDOW(self),
					_("There was an error reading the file."));
		}
	}
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

	if (doc)
	{
		GFileType type;

		if (HEX_IS_BUFFER_MALLOC (hex_document_get_buffer (doc)) &&
				! nag_screen_shown &&
				hex_buffer_util_get_file_size (file) >= 1073741824)
		{
			nag_screen_shown = TRUE;
			tmp_global_gfile_for_nag_screen = file;
			do_nag_screen (self);
			g_object_unref (doc);
			return;
		}

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

		gh = g_object_new (HEX_TYPE_WIDGET,
				"document", doc,
				"fade-zeroes", TRUE,
				NULL);
	}

	/* Display a fairly generic error message if we can't even get this far. */
	if (!doc || !gh)
	{
		char *error_msg = _("There was an error loading the requested file. "
				"The file either no longer exists, is inaccessible, "
				"or you may not have permission to access the file.");

		display_dialog (GTK_WINDOW(self), error_msg);

		/* This fcn is also used to handle open operations on the cmdline. */
		g_printerr ("%s\n", error_msg);

		return;
	}

	ghex_application_window_add_hex (self, gh);
	g_object_set_data (G_OBJECT(self), "target-gh", gh);
	hex_document_read_async (doc, NULL, doc_read_ready_cb, self);
}

HexWidget *
ghex_application_window_get_hex (GHexApplicationWindow *self)
{
	AdwTabPage *page;

	g_return_val_if_fail (GHEX_IS_APPLICATION_WINDOW (self), NULL);

	page = adw_tab_view_get_selected_page (ADW_TAB_VIEW(self->hex_tab_view));

	if (page)
	{
		GtkWidget *box = adw_tab_page_get_child (page);
		for (GtkWidget *child = gtk_widget_get_first_child (box);
				child != NULL;
				child = gtk_widget_get_next_sibling (child))
		{
			if (HEX_IS_WIDGET (child))
				return HEX_WIDGET(child);
		}
		return NULL;
	}
	else
		return NULL;
}
