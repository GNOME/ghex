/* ghex-application-window.c
 *
 * Copyright © 2026 Logan Rathbone
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ghex-application-window.h"

#include "preferences.h"
#include "common-ui.h"
#include "util.h"
#include "configuration.h"
#include "converter.h"
#include "chartable.h"

#include "config.h"

enum
{
	PROP_0,
	PROP_ACTIVE_VIEW,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _GHexApplicationWindow
{
	AdwApplicationWindow parent_instance;

	GBinding *revert_binding;
	GBinding *save_binding;
	GBinding *save_as_binding;

	GtkWidget *prefs_dialog;

	GHexConverter *converter;
	GHexCharTable *chartable;

	/* Template widgets */

	AdwViewStack *stack;
	AdwTabView *hex_tab_view;
	GtkBox *tab_view_box;
};

G_DEFINE_FINAL_TYPE (GHexApplicationWindow, ghex_application_window, ADW_TYPE_APPLICATION_WINDOW)

/* Transfer none */
static HexDocument *
get_active_doc (GHexApplicationWindow *self)
{
	GHexViewContainer *container;
	HexDocument *doc;

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));

	container = ghex_application_window_get_active_view (self);

	if (!container)
		return NULL;

	doc = ghex_view_container_get_document (container);
	g_assert (HEX_IS_DOCUMENT (doc));

	return doc;
}

void
ghex_application_window_set_active_view (GHexApplicationWindow *self, GHexViewContainer *container)
{
	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW (self));
	g_return_if_fail (container == NULL || GHEX_IS_VIEW_CONTAINER (container));

	if (container)
	{
		AdwTabPage *page;

		adw_view_stack_set_visible_child_name (self->stack, "hex-display");

		page = adw_tab_view_get_page (self->hex_tab_view, GTK_WIDGET(container));
		adw_tab_view_set_selected_page (self->hex_tab_view, page);
	}
	else
	{
		adw_view_stack_set_visible_child_name (self->stack, "no-doc-loaded");
	}

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_ACTIVE_VIEW]);
}

GHexViewContainer *
ghex_application_window_get_active_view (GHexApplicationWindow *self)
{
	g_return_val_if_fail (GHEX_IS_APPLICATION_WINDOW (self), NULL);

	if (g_strcmp0 (adw_view_stack_get_visible_child_name (self->stack), "no-doc-loaded") == 0)
	{
		return NULL;
	}
	else
	{
		AdwTabPage *page = adw_tab_view_get_selected_page (self->hex_tab_view);
		gpointer active_view;

		if (!page)	/* eg, last page is closed */
			return NULL;

		active_view = adw_tab_page_get_child (page);

		g_assert (GHEX_IS_VIEW_CONTAINER (active_view));

		return active_view;
	}
}

gboolean
doc_changed_icon_transform_to (GBinding *binding, const GValue *from_value, GValue *to_value, gpointer data)
{
	gboolean changed = g_value_get_boolean (from_value);

	if (changed)
		g_value_take_object (to_value, g_themed_icon_new ("document-modified-symbolic"));
	else
		g_value_set_object (to_value, NULL);

	return TRUE;
}

static void
setup_doc_bindings_for_page (HexDocument *doc, AdwTabPage *page)
{
	g_object_bind_property_full (doc, "file", page, "title", G_BINDING_SYNC_CREATE, file_title_transform_to, NULL, NULL, NULL);

	g_object_bind_property_full (doc, "changed", page, "icon", G_BINDING_DEFAULT, doc_changed_icon_transform_to, NULL, NULL, NULL);
}

static GHexViewContainer *
add_container (GHexApplicationWindow *self, HexDocument *doc)
{
	gpointer container = NULL;
	AdwTabPage *page = NULL;

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));
	g_assert (doc == NULL || HEX_IS_DOCUMENT (doc));

	container = ghex_view_container_new ();

	if (doc)
		ghex_view_container_set_document (container, doc);
	else
		doc = ghex_view_container_get_document (container);

	page = adw_tab_view_append (self->hex_tab_view, container);

	setup_doc_bindings_for_page (doc, page);

	ghex_application_window_set_active_view (self, container);

	return container;
}

void
ghex_application_window_add_document (GHexApplicationWindow *self, HexDocument *doc)
{
	GtkWidget *container = NULL;
	AdwTabPage *page = NULL;

	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW (self));
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	add_container (self, doc);
}

void
ghex_application_window_new_file (GHexApplicationWindow *self)
{
	GHexViewContainer *container;
	gpointer hex;

	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW (self));

	container = add_container (self, NULL);
	hex = ghex_view_container_get_hex (container);
	hex_view_set_insert_mode (hex, TRUE);
}

void
ghex_application_window_open_file (GHexApplicationWindow *self, GFile *file)
{
	g_autoptr(HexDocument) doc = NULL;

	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW (self));
	g_return_if_fail (G_IS_FILE (file));

	doc = hex_document_new_from_file (file);

	ghex_application_window_add_document (self, doc);
}

static void
ghex_application_window_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW (object);

	switch (property_id)
	{
		case PROP_ACTIVE_VIEW:
			ghex_application_window_set_active_view (self, g_value_get_object (value));
			break;

		default:
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

	switch (property_id)
	{
		case PROP_ACTIVE_VIEW:
			g_value_set_object (value, ghex_application_window_get_active_view (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
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
ghex_application_window_open_action (GtkWidget *widget, const char *action_name, GVariant *parameter)
{
	GHexApplicationWindow *self = (GHexApplicationWindow *) widget;
	GtkFileChooserNative *file_sel;
	GtkResponseType resp;
	
	g_assert (GHEX_IS_APPLICATION_WINDOW (self));

	file_sel =
		gtk_file_chooser_native_new (_("Select a file to open"),
				GTK_WINDOW(self),
				GTK_FILE_CHOOSER_ACTION_OPEN,
				NULL,	/* const char *accept_label | NULL == default.	*/
				NULL);	/* const char *cancel_label |					*/

	g_signal_connect (file_sel, "response",
			G_CALLBACK(open_response_cb), self);

	gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (file_sel), TRUE);
	gtk_native_dialog_show (GTK_NATIVE_DIALOG(file_sel));
}

static void
doc_read_ready_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GHexApplicationWindow *self = user_data;
	HexDocument *doc = (HexDocument *) source_object;
	gboolean result;
	g_autoptr(GError) local_error = NULL;

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));
	g_assert (HEX_IS_DOCUMENT (doc));

	result = hex_document_read_finish (doc, res, &local_error);

	if (!result)
	{
		if (local_error)
			ghex_display_dialog (GTK_WINDOW(self), local_error->message);
		else
			ghex_display_dialog (GTK_WINDOW(self), _("There was an error reading the file."));
	}
}

static void
revert_response_cb (GHexApplicationWindow *self, const char *response, AdwAlertDialog *dialog)
{
	g_assert (GHEX_IS_APPLICATION_WINDOW (self));
	g_assert (ADW_IS_ALERT_DIALOG (dialog));

	if (g_strcmp0 (response, "revert") == 0)
	{
		HexDocument *doc = get_active_doc (self);

		g_assert (doc != NULL);

		hex_document_read_async (doc, NULL, doc_read_ready_cb, self);
	}

	adw_dialog_close (ADW_DIALOG(dialog));
}

static void
ghex_application_window_revert_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GHexApplicationWindow *self = user_data;
   	HexDocument *doc;
	AdwDialog *dialog;
	gint reply;
	g_autofree char *basename = NULL;

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));

	doc = get_active_doc (self);

	if G_UNLIKELY (!doc)
		return;

	/* Yes, this *is* a programmer error. The Revert menu should not be shown
	 * to the user at all if there is nothing to revert. */
	g_assert (hex_document_get_changed (doc));

	basename = common_get_ui_basename (doc);

	/* Translators: %s here is the filename the user is being asked to
	 * confirm whether they want to revert. */
	dialog = adw_alert_dialog_new (g_strdup_printf (_("Are you sure you want to revert %s?"), basename), NULL);

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

	g_signal_connect_object (dialog, "response", G_CALLBACK(revert_response_cb), self, G_CONNECT_SWAPPED);

	adw_dialog_present (dialog, GTK_WIDGET(self));
}

static void
file_save_write_cb (HexDocument *doc, GAsyncResult *res, GHexApplicationWindow *self)
{
	g_autoptr(GError) error = NULL;
	gboolean write_successful;

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));
	g_assert (HEX_IS_DOCUMENT (doc));

	write_successful = hex_document_write_finish (doc, res, &error);

	if (write_successful)
	{
		g_debug ("%s: File saved successfully.", __func__);
	}
	else
	{
		g_autoptr(GString) full_errmsg = g_string_new (_("There was an error saving the file."));

		if (error)
			g_string_append_printf (full_errmsg, "\n\n%s", error->message);

		ghex_display_dialog (GTK_WINDOW(self), full_errmsg->str);
	}
}

static void
ghex_application_window_save_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GHexApplicationWindow *self = user_data;
   	HexDocument *doc;

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));

	doc = get_active_doc (self);

	if (!hex_document_get_file (doc))
	{
		gtk_widget_activate_action (GTK_WIDGET(self), "win.save-as", NULL);
		return;
	}

	hex_document_write_async (doc, NULL, (GAsyncReadyCallback)file_save_write_cb, self);
}

static void
save_as_write_to_file_cb (HexDocument *doc,
		GAsyncResult *res,
		GHexApplicationWindow *self)
{
	GFile *gfile = g_object_get_data (G_OBJECT(self), "target-file");
	g_autoptr(GError) error = NULL;
	gboolean write_successful;

	write_successful = hex_document_write_finish (doc, res, &error);

	if (! write_successful)
	{
		GString *full_errmsg = g_string_new (
				_("There was an error saving the file to the path specified."));

		if (error)
			g_string_append_printf (full_errmsg, "\n\n%s", error->message);

		ghex_display_dialog (GTK_WINDOW(self), full_errmsg->str);

		g_string_free (full_errmsg, TRUE);
		return;
	}

	if (hex_document_set_file (doc, gfile))
	{
		hex_document_read_async (doc, NULL, doc_read_ready_cb, self);
	}
	else
	{
		ghex_display_dialog (GTK_WINDOW(self),
				_("An unknown error has occurred in attempting to reload the "
					"file you have just saved."));
	}

	g_object_set_data (G_OBJECT(self), "target-file", NULL);
}

static void
save_as_response_cb (GtkNativeDialog *dialog,
		int resp,
		gpointer user_data)
{
	GHexApplicationWindow *self = (GHexApplicationWindow *) user_data;
	GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
	HexDocument *doc;
	g_autoptr(GFile) gfile = NULL;

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));

	if (resp != GTK_RESPONSE_ACCEPT)
		goto end;

	doc = get_active_doc (self);

	gfile = gtk_file_chooser_get_file (chooser);

	g_object_set_data_full (G_OBJECT(self), "target-file", g_object_ref (gfile), g_object_unref);

	hex_document_write_to_file_async (doc,
			gfile,
			NULL,
			(GAsyncReadyCallback)save_as_write_to_file_cb,
			self);

end:
	gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (dialog));
}

static void
ghex_application_window_save_as_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GHexApplicationWindow *self = user_data;
   	HexDocument *doc;
	GtkFileChooserNative *file_sel;
	GtkResponseType resp;
	GFile *default_file;

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));

	doc = get_active_doc (self);

	default_file = hex_document_get_file (doc);
	file_sel = gtk_file_chooser_native_new (
			_("Select a file to save buffer as"),
				GTK_WINDOW(self),
				GTK_FILE_CHOOSER_ACTION_SAVE,
				NULL,
				NULL);

	/* Default suggested file == existing file. */
	if (default_file)
	{
		gtk_file_chooser_set_file (GTK_FILE_CHOOSER(file_sel),
				default_file,
				NULL);	/* GError **error */
	}

	g_signal_connect (file_sel, "response",
			G_CALLBACK(save_as_response_cb), self);

	gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (file_sel), TRUE);
	gtk_native_dialog_show (GTK_NATIVE_DIALOG(file_sel));
}

static void
ghex_application_window_close_tab_action (GtkWidget *widget, const char *action_name, GVariant *parameter)
{
	GHexApplicationWindow *self = (GHexApplicationWindow *) widget;

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));

	const int n_pages = adw_tab_view_get_n_pages (self->hex_tab_view);

	if (n_pages == 0)
	{
		gtk_window_close (GTK_WINDOW(self));
	}
	else
	{
		gpointer page = adw_tab_view_get_selected_page (self->hex_tab_view);

		adw_tab_view_close_page (self->hex_tab_view, page);

		if (n_pages == 1)
			ghex_application_window_set_active_view (self, NULL);
	}
}

static void
ghex_application_window_preferences_action (GtkWidget *widget, const char *action_name, GVariant *parameter)
{
	GHexApplicationWindow *self = (GHexApplicationWindow *) widget;

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));

	if (! self->prefs_dialog)
		self->prefs_dialog = ghex_create_preferences_dialog (GTK_WINDOW(self));

	gtk_widget_set_visible (self->prefs_dialog, TRUE);
}

static void
close_all_tabs_response_cb (GHexApplicationWindow *self, const char *response, AdwAlertDialog *dialog)
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

	g_signal_connect_object (dialog, "response", G_CALLBACK(close_all_tabs_response_cb), self, G_CONNECT_SWAPPED);

	adw_dialog_present (dialog, GTK_WIDGET(self));
}

static gboolean
ghex_application_window_close_request (GtkWindow *window)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(window);
	gboolean unsaved_found = FALSE;
	const int num_pages = adw_tab_view_get_n_pages (self->hex_tab_view);

	/* We have more than one tab open: */
	for (int i = num_pages - 1; i >= 0; --i)
	{
		AdwTabPage *page = adw_tab_view_get_nth_page (self->hex_tab_view, i);
		GHexViewContainer *container = GHEX_VIEW_CONTAINER(adw_tab_page_get_child (page));
		HexDocument *doc = ghex_view_container_get_document (container);

		if (hex_document_get_changed (doc)) {
			unsaved_found = TRUE;
			break;
		}
	}

	if (unsaved_found)
	{
		close_all_tabs_confirmation_dialog (self);
		return GDK_EVENT_STOP;
	}

	return GTK_WINDOW_CLASS(ghex_application_window_parent_class)->close_request (window);
}

static void
ghex_application_window_class_init (GHexApplicationWindowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GtkWindowClass *window_class = GTK_WINDOW_CLASS(klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->set_property = ghex_application_window_set_property;
	object_class->get_property = ghex_application_window_get_property;

	window_class->close_request = ghex_application_window_close_request;

	/* Properties */

	properties[PROP_ACTIVE_VIEW] = g_param_spec_object ("active-view", NULL, NULL,
			GHEX_TYPE_VIEW_CONTAINER,
			default_flags | G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	/* Actions */

	/* These are the actions that don't require GObject property bindings to be
	 * applied. For those that do, see init()
	 */
	gtk_widget_class_install_action (widget_class, "win.new-file", NULL, (GtkWidgetActionActivateFunc) ghex_application_window_new_file);

	gtk_widget_class_install_action (widget_class, "win.open", NULL, ghex_application_window_open_action);

	gtk_widget_class_install_action (widget_class, "win.close-tab", NULL, ghex_application_window_close_tab_action);

	gtk_widget_class_install_action (widget_class, "win.preferences", NULL, ghex_application_window_preferences_action);

	gtk_widget_class_install_action (widget_class, "win.preferences", NULL, ghex_application_window_preferences_action);

	/* Bindings */

	/* Ctrl+T - new file */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_t,
			GDK_CONTROL_MASK,
			"win.new-file",
			NULL);

	/* Ctrl+O - open */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_o,
			GDK_CONTROL_MASK,
			"win.open",
			NULL);

	/* Ctrl+S - save */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_s,
			GDK_CONTROL_MASK,
			"win.save",
			NULL);

	/* Ctrl+Shift+S - save as */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_s,
			GDK_CONTROL_MASK | GDK_SHIFT_MASK,
			"win.save-as",
			NULL);

	/* Ctrl+W - close tab */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_w,
			GDK_CONTROL_MASK,
			"win.close-tab",
			NULL);

	/* Ctrl+comma - show preferences */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_comma,
			GDK_CONTROL_MASK,
			"win.preferences",
			NULL);	/* no args. */

	/* Template */

	gtk_widget_class_set_template_from_resource (widget_class, RESOURCE_BASE_PATH "/ghex-application-window.ui");

	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow, stack);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow, hex_tab_view);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow, tab_view_box);
}

static void
tab_view_selected_notify_cb (GHexApplicationWindow *self, GParamSpec *pspec, AdwTabView *tab_view)
{
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_ACTIVE_VIEW]);
}

static void
active_view_notify_cb (GHexApplicationWindow *self)
{
	GAction *action;
	HexDocument *doc;
	GHexViewContainer *container;

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));
	
	container = ghex_application_window_get_active_view (self);

	/* Set converter and chartable's hex to that of container if applicable */

	if (!container)
	{
		ghex_converter_set_hex (self->converter, NULL);
		ghex_char_table_set_hex (self->chartable, NULL);
	}
	else
	{
		HexView *view = HEX_VIEW (ghex_view_container_get_hex (container));

		ghex_converter_set_hex (self->converter, view);
		ghex_char_table_set_hex (self->chartable, view);
	}

	/* Bind document actions if applicable */

	doc = get_active_doc (self);
	if (!doc)
		return;

	g_assert (HEX_IS_DOCUMENT (doc));

	action = g_action_map_lookup_action (G_ACTION_MAP(self), "revert");

	g_clear_pointer (&self->revert_binding, g_binding_unbind);
	self->revert_binding = g_object_bind_property (doc, "changed", action, "enabled", G_BINDING_SYNC_CREATE);

	action = g_action_map_lookup_action (G_ACTION_MAP(self), "save");

	g_clear_pointer (&self->save_binding, g_binding_unbind);
	self->save_binding = g_object_bind_property (doc, "changed", action, "enabled", G_BINDING_SYNC_CREATE);

	action = g_action_map_lookup_action (G_ACTION_MAP(self), "save-as");

	g_clear_pointer (&self->save_as_binding, g_binding_unbind);
	self->save_as_binding = g_object_bind_property (doc, "changed", action, "enabled", G_BINDING_SYNC_CREATE);
}

static void
close_doc_response_cb (GHexApplicationWindow *self, const char *response, AdwAlertDialog *dialog)
{
	AdwTabPage *page = g_object_get_data (G_OBJECT(self), "target-page");

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));
	g_assert (ADW_IS_TAB_PAGE (page));

	if (g_strcmp0 (response, "save") == 0)
	{
		gtk_widget_activate_action (GTK_WIDGET(self), "win.save", NULL);
		adw_tab_view_close_page_finish (self->hex_tab_view, page, TRUE);
	}
	else if (g_strcmp0 (response, "discard") == 0)
	{
		adw_tab_view_close_page_finish (self->hex_tab_view, page, TRUE);
	}
	else
	{
		adw_tab_view_close_page_finish (self->hex_tab_view, page, FALSE);
	}

	adw_dialog_close (ADW_DIALOG (dialog));

	g_object_set_data (G_OBJECT(self), "target-page", NULL);
}

static void
close_doc_confirmation_dialog (GHexApplicationWindow *self, AdwTabPage *page, HexDocument *doc)
{
	AdwDialog *dialog;
	GHexViewContainer *container;
	g_autofree char *basename = NULL;
	g_autofree char *title = NULL;

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));
	g_assert (ADW_IS_TAB_PAGE (page));
	g_assert (HEX_IS_DOCUMENT (doc));

	basename = common_get_ui_basename (doc);

	if (basename) {
		/* Translators: %s is the filename that is currently being
		 * edited. */
		title = g_strdup_printf (_("%s has been edited since opening."), basename);
	}
	else {
		title = g_strdup (_("The buffer has been edited since opening."));
	}

	dialog = adw_alert_dialog_new (title, NULL);

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

	g_signal_connect_object (dialog, "response", G_CALLBACK(close_doc_response_cb), self, G_CONNECT_SWAPPED);

	g_object_set_data_full (G_OBJECT(self), "target-page", g_object_ref (page), g_object_unref);

	adw_dialog_present (ADW_DIALOG(dialog), GTK_WIDGET (self));
}

static gboolean
tab_view_close_page_cb (GHexApplicationWindow *self, AdwTabPage *page, AdwTabView *tab_view)
{
	GHexViewContainer *container;
	HexDocument *doc;

	g_assert (GHEX_IS_APPLICATION_WINDOW (self));
	g_assert (ADW_IS_TAB_VIEW (tab_view));

	container = GHEX_VIEW_CONTAINER (adw_tab_page_get_child (page));
	doc = ghex_view_container_get_document (container);

	if (!hex_document_get_changed (doc))
	{
		adw_tab_view_close_page_finish (tab_view, page, TRUE);
		return GDK_EVENT_STOP;
	}
	else
	{
		ghex_application_window_set_active_view (self, container);
		close_doc_confirmation_dialog (self, page, doc);
	}

	return GDK_EVENT_STOP;
}

static void
tab_view_page_attached_cb (GHexApplicationWindow *self, AdwTabPage *page, int position, AdwTabView *tab_view)
{
	GHexViewContainer *container = GHEX_VIEW_CONTAINER(adw_tab_page_get_child (page));

	ghex_application_window_set_active_view (self, container);
}

static AdwTabView *
tab_view_create_window_cb (AdwTabView *self, gpointer user_data)
{
	GHexApplicationWindow *new_win = GHEX_APPLICATION_WINDOW (ghex_application_window_new (ADW_APPLICATION(g_application_get_default ())));

	gtk_window_present (GTK_WINDOW(new_win));

	return new_win->hex_tab_view;
}

static void
ghex_application_window_init (GHexApplicationWindow *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

	/* Setup converter */

	self->converter = (GHexConverter *) ghex_converter_new (GTK_WINDOW(self));

	{
		g_autoptr(GPropertyAction) action = g_property_action_new ("converter", self->converter, "visible");
		g_action_map_add_action (G_ACTION_MAP(self), G_ACTION(action));
	}

	/* Setup chartable */

	self->chartable = (GHexCharTable *) ghex_char_table_new (GTK_WINDOW(self));

	{
		g_autoptr(GPropertyAction) action = g_property_action_new ("chartable", self->chartable, "visible");
		g_action_map_add_action (G_ACTION_MAP(self), G_ACTION(action));
	}

	/* Tab view signals */

	g_signal_connect (self->hex_tab_view, "create-window", G_CALLBACK(tab_view_create_window_cb), NULL);

	g_signal_connect_object (self->hex_tab_view, "close-page", G_CALLBACK(tab_view_close_page_cb), self, G_CONNECT_SWAPPED);

	g_signal_connect_object (self->hex_tab_view, "notify::selected-page", G_CALLBACK(tab_view_selected_notify_cb), self, G_CONNECT_SWAPPED);

	g_signal_connect_object (self->hex_tab_view, "page-attached", G_CALLBACK(tab_view_page_attached_cb), self, G_CONNECT_SWAPPED);

	g_signal_connect_after (self, "notify::active-view", G_CALLBACK(active_view_notify_cb), NULL);

	/* Settings actions */

	{
		GSettings *settings = ghex_get_global_settings ();
		g_autoptr(GAction) action = g_settings_create_action (settings, "group-data-by");
		g_action_map_add_action (G_ACTION_MAP(self), action);
	}

	/* Setup actions which can use bindings to determine when they should be
	 * enabled/disabled.*/
	{
		GActionEntry entries[] = {
			{"revert", ghex_application_window_revert_action},
			{"save", ghex_application_window_save_action},
			{"save-as", ghex_application_window_save_as_action},
		};

		g_action_map_add_action_entries (G_ACTION_MAP(self), entries, G_N_ELEMENTS (entries), self);

		/* Disable all by default */
		for (guint i = 0; i < G_N_ELEMENTS (entries); ++i)
		{
			GActionEntry entry = entries[i];
			GSimpleAction *action = G_SIMPLE_ACTION(g_action_map_lookup_action (G_ACTION_MAP(self), entry.name));

			g_simple_action_set_enabled (action, FALSE);
		}
	}
}

GtkWidget *
ghex_application_window_new (AdwApplication *app)
{
	return g_object_new (GHEX_TYPE_APPLICATION_WINDOW,
			"application", app,
			NULL);
}
