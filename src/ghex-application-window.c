/* vim: ts=4 sw=4 colorcolumn=80
 */
#include <glib/gi18n.h>
#include <gtkhex.h>

#include "ghex-application-window.h"
#include "hex-dialog.h"
#include "findreplace.h"
#include "chartable.h"
#include "converter.h"

/* DEFINES */

/* FIXME/TODO - this was an option before. Not sure I see the point in
 * it. Will consider keeping it hardcoded - but if I do, it might need to
 * be moved.
 */
#define offset_fmt	"0x%X"

/* GHexNotebookTab GOBJECT DEFINITION */

/* This is just an object internal to the app window widget, so we don't
 * need to define it publicly in the header.
 */
#define GHEX_TYPE_NOTEBOOK_TAB (ghex_notebook_tab_get_type ())
G_DECLARE_FINAL_TYPE (GHexNotebookTab, ghex_notebook_tab, GHEX, NOTEBOOK_TAB,
				GtkWidget)

enum notebook_signal_types {
	CLOSED,
	NOTEBOOK_LAST_SIGNAL
};

struct _GHexNotebookTab
{
	GtkWidget parent_instance;
	
	GtkWidget *label;
	GtkWidget *close_btn;
	GtkHex *gh;				/* GtkHex widget activated when tab is clicked */
};

static guint notebook_signals[NOTEBOOK_LAST_SIGNAL];

G_DEFINE_TYPE (GHexNotebookTab, ghex_notebook_tab, GTK_TYPE_WIDGET)

/* GHexNotebookTab - Internal Method Decls */

static GtkWidget * ghex_notebook_tab_new (void);
static void ghex_notebook_tab_add_hex (GHexNotebookTab *self, GtkHex *gh);
static const char * ghex_notebook_tab_get_filename (GHexNotebookTab *self);
static void ghex_notebook_tab_refresh_file_name (GHexNotebookTab *self);

/* ---- */

/* ----------------------- */
/* MAIN GOBJECT DEFINITION */
/* ----------------------- */

struct _GHexApplicationWindow
{
	GtkApplicationWindow parent_instance;

	GtkHex *gh;
	HexDialog *dialog;
	GtkWidget *dialog_widget;
	guint statusbar_id;
	GtkAdjustment *adj;
	GList *gh_list;
	gboolean can_save;

	// TEST - NOT 100% SURE I WANNA GO THIS ROUTE YET.
	GtkWidget *find_dialog;
	GtkWidget *replace_dialog;
	GtkWidget *jump_dialog;
	GtkWidget *chartable;
	GtkWidget *converter;

/*
 * for i in `cat ghex-application-window.ui |grep -i 'id=' |sed -e 's,^\s*,,g' |sed -e 's,.*id=",,' |sed -e 's,">,,'`; do echo $i >> tmp.txt; done
 */

/* for i in `cat tmp.txt`; do echo GtkWidget *${i}; done
 */
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
	"ghex.show-conversions",
	"ghex.insert-mode",
	"ghex.find",
	"ghex.replace",
	"ghex.jump",
	"ghex.chartable",
	"ghex.converter",
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
GHexNotebookTab * ghex_application_window_get_current_tab (GHexApplicationWindow *self);

static void set_statusbar(GHexApplicationWindow *self, const char *str);
static void update_status_message (GHexApplicationWindow *self);
static void update_gui_data (GHexApplicationWindow *self);


/* GHexApplicationWindow -- PRIVATE FUNCTIONS */

// FIXME - This could be the wrong approach. I don't know if we can
// guarantee the tab will be un-switchable while the dialog is up.
	
GHexNotebookTab *
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

		page_num = gtk_notebook_page_num (notebook,
				GTK_WIDGET(tab->gh));

		gtk_notebook_remove_page (notebook, page_num);
}

static void
file_save (GHexApplicationWindow *self)
{
	HexDocument *doc;

	g_return_if_fail (GTK_IS_HEX (self->gh));

	doc = gtk_hex_get_document (self->gh);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	if (hex_document_write (doc))
	{
		/* we're happy... */
		g_debug ("%s: File saved successfully.", __func__);
	}
	else
	{
		g_debug("%s: NOT IMPLEMENTED - show following message in GUI:",
				__func__);
		g_debug(_("Error saving file!"));
	}
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
		g_debug ("%s: Decided to SAVE changes.",
				__func__);
		file_save (self);
		ghex_application_window_remove_tab (self, tab);
	}
	else if (response_id == GTK_RESPONSE_REJECT)
	{
		g_debug ("%s: Decided NOT to save changes.", __func__);
		ghex_application_window_remove_tab (self, tab);
	}
	else
	{
		g_debug ("%s: User doesn't know WHAT they wanna do!.", __func__);
	}
	gtk_window_destroy (GTK_WINDOW(dialog));
}

static void
close_doc_confirmation_dialog (GHexApplicationWindow *self)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW(self),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			(_("<big><b>You have unsaved changes.</b></big>\n\n"
			   "Would you like to save your changes?")));

	gtk_dialog_add_buttons (GTK_DIALOG(dialog),
			_("_Save Changes"),		GTK_RESPONSE_ACCEPT,
			_("_Discard Changes"),	GTK_RESPONSE_REJECT,
			NULL);

	g_signal_connect (dialog, "response",
			G_CALLBACK(close_doc_response_cb), self);

	gtk_widget_show (dialog);
}

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

/* Kinda like enable_all_actions, but only for ghex-specific ones. */
static void
enable_main_actions (GHexApplicationWindow *self, gboolean enable)
{
	for (int i = 0; main_actions[i] != NULL; ++i)
	{
		g_debug("%s: action %d : %s - setting enabled: %d",
				__func__, i, main_actions[i], enable);
		
		gtk_widget_action_set_enabled (GTK_WIDGET(self),
				main_actions[i], enable);
	}
}

/* GHexApplicationWindow -- CALLBACKS */
static void
ghex_application_window_file_saved_cb (HexDocument *doc,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW (self));

	ghex_application_window_set_can_save (self,
			hex_document_has_changed (doc));
}

static void
ghex_application_window_document_changed_cb (HexDocument *doc,
		gpointer change_data,
		gboolean push_undo,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW (self));

	ghex_application_window_set_can_save (self,
			hex_document_has_changed (doc));
}

static void
tab_close_cb (GHexNotebookTab *tab,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	HexDocument *doc;

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
	GHexApplicationWindow *self =
		GHEX_APPLICATION_WINDOW(user_data);
	GHexNotebookTab *tab =
		GHEX_NOTEBOOK_TAB(gtk_notebook_get_tab_label (notebook, page));
	HexDocument *doc;

	g_return_if_fail (GHEX_IS_NOTEBOOK_TAB(tab));

	printf("%s: start - tab: %p - tab->gh: %p - self->gh: %p\n",
			__func__, (void *)tab, (void *)tab->gh, (void *)self->gh);

	if (tab->gh != self->gh) {
		ghex_application_window_set_hex (self, tab->gh);
		ghex_application_window_activate_tab (self, self->gh);
	}

	/* Assess saveability based on new tab we've switched to */
	doc = gtk_hex_get_document (self->gh);
	ghex_application_window_set_can_save (self,
			hex_document_has_changed (doc));

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
	
	g_debug("%s: n_pages: %d",
			__func__, gtk_notebook_get_n_pages(notebook));

	if (gtk_notebook_get_n_pages (notebook) == 0) {
		enable_main_actions (self, FALSE);
		gtk_widget_hide (self->hex_notebook);
		gtk_widget_show (self->no_doc_label);
	}
}

/* POSSIBLE TODO - this is fine for a one-off, but should have a more generic
 * public property getter/setter function if we add more properties here.
 */
static void
find_close_cb (FindDialog *dialog,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	(void)dialog;

	ghex_application_window_set_show_find (self, FALSE);
}

static void
replace_close_cb (ReplaceDialog *dialog,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	(void)dialog;

	ghex_application_window_set_show_replace (self, FALSE);
}

static void
jump_close_cb (JumpDialog *dialog,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	(void)dialog;

	ghex_application_window_set_show_jump (self, FALSE);
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
		if (gtk_widget_is_visible (self->WIDGET))							\
			gtk_widget_hide (self->WIDGET);									\
	}																		\
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_ARR_ENTRY]);	\
}												/* DIALOG_SET_SHOW_TEMPLATE */

DIALOG_SET_SHOW_TEMPLATE(chartable, setup_chartable(self), PROP_CHARTABLE_OPEN)
DIALOG_SET_SHOW_TEMPLATE(converter, setup_converter(self), PROP_CONVERTER_OPEN)

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
	} else {																\
		gtk_widget_hide (self->WIDGET ## _dialog);							\
	}																		\
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_ARR_ENTRY]);	\
}												/* PANE_SET_SHOW_TEMPLATE */

PANE_SET_SHOW_TEMPLATE(find,		replace, jump,	PROP_FIND_OPEN)
PANE_SET_SHOW_TEMPLATE(replace,		find, jump,		PROP_REPLACE_OPEN)
PANE_SET_SHOW_TEMPLATE(jump,		find, replace,	PROP_JUMP_OPEN)

static void
ghex_application_window_set_can_save (GHexApplicationWindow *self,
		gboolean can_save)
{
	g_return_if_fail (GHEX_IS_APPLICATION_WINDOW (self));

	self->can_save = can_save;
	g_debug("%s: start - can_save: %d", __func__, can_save);

	gtk_widget_action_set_enabled (GTK_WIDGET(self),
			"ghex.save", can_save);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_CAN_SAVE]);
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
	char *new_file_path;
	FILE *file;
	gchar *gtk_file_name;

	/* If user doesn't click Save, just bail out now. */
	if (resp != GTK_RESPONSE_ACCEPT)
		goto end;

	/* Fetch doc. No need for sanity checks as this is just a helper. */
	doc = gtk_hex_get_document (self->gh);

	/* Get filename. */
	gfile = gtk_file_chooser_get_file (chooser);
	new_file_path = g_file_get_path (gfile);
	g_clear_object (&gfile);

	g_debug("%s: GONNA OPEN FILE FOR WRITING: %s",
			__func__, new_file_path);

	file = fopen(new_file_path, "wb");

	/* Sanity check */
	if (file == NULL) {
		g_debug ("%s: Error dialog not implemented! Can't open file rw!",
				__func__);
		goto end;
	}

	if (hex_document_write_to_file (doc, file))
	{
		gboolean change_ok;
		char *gtk_file_name;

		change_ok = hex_document_change_file_name (doc, new_file_path);

		/* "set window to file-name" 
		 * if (change_ok) ..... */

		gtk_file_name = g_filename_to_utf8 (doc->file_name,
				-1, NULL, NULL, NULL);

		g_debug("%s: NOT IMPLEMENTED - show following message in GUI:",
				__func__);
		g_debug(_("Saved buffer to file %s"), gtk_file_name);

		g_free(gtk_file_name);
	}
	else
	{
		g_debug("%s: NOT IMPLEMENTED - show following message in GUI:",
				__func__);
		g_debug(_("Error saving file!"));
	}
	fclose(file);

end:
	g_debug("%s: END.", __func__);
	g_object_unref (dialog);
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
	GFile *existing_file;

	g_return_if_fail (GTK_IS_HEX (self->gh));

	doc = gtk_hex_get_document (self->gh);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	existing_file = g_file_new_for_path (doc->file_name);

	file_sel =
		gtk_file_chooser_native_new (_("Select a file to save buffer as"),
				GTK_WINDOW(self),
				GTK_FILE_CHOOSER_ACTION_SAVE,
				NULL,	// const char *accept_label } NULL == default.
				NULL);	// const char *cancel_label }

	/* Default suggested file == existing file. */
	gtk_file_chooser_set_file (GTK_FILE_CHOOSER(file_sel), existing_file,
			NULL);	// GError **error

	g_signal_connect (file_sel, "response",
			G_CALLBACK(save_as_response_cb), self);

	gtk_native_dialog_show (GTK_NATIVE_DIALOG(file_sel));

	/* Clear the GFile ptr which is no longer necessary. */
	g_clear_object (&existing_file);
}

/* convenience helper function to build a GtkHex widget pre-loaded with
 * a hex document, from a GFile *.
 */
static GtkHex *
new_gh_from_gfile (GFile *file)
{
	char *path;
	GFileInfo *info;
	GError *error = NULL;
	HexDocument *doc;
	GtkHex *gh;

	path = g_file_get_path (file);
	info = g_file_query_info (file,
			G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
			G_FILE_QUERY_INFO_NONE,				// GFileQueryInfoFlags flags
			NULL,								// GCancellable *cancellable
			&error);

	g_debug("%s: path acc. to GFile: %s",
			__func__, path);

	doc = hex_document_new_from_file (path);
	gh = GTK_HEX(gtk_hex_new (doc));

	// FIXME - should be based on settings
	gtk_hex_show_offsets (gh, TRUE);

	if (error)	g_error_free (error);
	g_clear_object (&info);
	g_clear_object (&file);

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
	GtkHex *gh;

	if (resp == GTK_RESPONSE_ACCEPT)
	{
		file = gtk_file_chooser_get_file (chooser);
		gh = new_gh_from_gfile (file);

		ghex_application_window_add_hex (self, gh);
		ghex_application_window_set_hex (self, gh);
		ghex_application_window_activate_tab (self, gh);
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
				NULL,	// const char *accept_label } NULL == default.
				NULL);	// const char *cancel_label }

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
toggle_insert_mode (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);

	(void)parameter, (void)action_name;		/* unused */

	/* this tests whether the button is pressed AFTER its state has changed. */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(self->insert_mode_button))) {
		g_debug("%s: TOGGLING INSERT MODE", __func__);
		gtk_hex_set_insert_mode(self->gh, TRUE);
	} else {
		g_debug("%s: UNTOGGLING INSERT MODE", __func__);
		gtk_hex_set_insert_mode(self->gh, FALSE);
	}
}

/* --- */

static void
set_statusbar(GHexApplicationWindow *self, const char *str)
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

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/* GHexNotebookTab -- CALLBACKS */

/* _document_changed_cb helper fcn. */
static void
tab_bold_label (GHexNotebookTab *self, gboolean bold)
{
	GtkLabel *label = GTK_LABEL(self->label);
	const char *text;
	char *new = NULL;

	text = gtk_label_get_text (label);

	if (bold) {
		new = g_strdup_printf("<b>%s</b>", text);
	}
	else {
		new = g_strdup (text);
	}
	gtk_label_set_markup (label, new);
	g_free (new);
}

static void
ghex_notebook_tab_document_changed_cb (HexDocument *doc,
		gpointer change_data,
		gboolean push_undo,
		gpointer user_data)
{
	GHexNotebookTab *self = GHEX_NOTEBOOK_TAB(user_data);

	g_debug ("%s: DETECTED DOC CHANGED.", __func__);

	(void)change_data, (void)push_undo; 	/* unused */

	tab_bold_label (self, hex_document_has_changed (doc));
}

static void
ghex_notebook_tab_close_click_cb (GtkButton *button,
               gpointer   user_data)
{
	GHexNotebookTab *self = GHEX_NOTEBOOK_TAB(user_data);

	g_debug("%s: clicked btn: %p - EMITTING CLOSED SIGNAL",
			__func__, (void *)button);

	g_signal_emit(self,
			notebook_signals[CLOSED],
			0);	// GQuark detail (just set to 0 if unknown)
}


/* GHexNotebookTab -- CONSTRUCTORS AND DESTRUCTORS */

static void
ghex_notebook_tab_init (GHexNotebookTab *self)
{
	GtkWidget *widget = GTK_WIDGET (self);
	GtkLayoutManager *layout_manager;

	/* Set spacing between label and close button. */

	layout_manager = gtk_widget_get_layout_manager (widget);
	gtk_box_layout_set_spacing (GTK_BOX_LAYOUT(layout_manager), 12);
	
	/* Set up our label to hold the document name and the close button. */

	self->label = gtk_label_new (_("Untitled document"));
	self->close_btn = gtk_button_new ();

	gtk_widget_set_halign (self->close_btn, GTK_ALIGN_END);
	gtk_button_set_icon_name (GTK_BUTTON(self->close_btn),
			"window-close-symbolic");
	gtk_button_set_has_frame (GTK_BUTTON(self->close_btn), FALSE);

	gtk_widget_set_parent (self->label, widget);
	gtk_widget_set_parent (self->close_btn, widget);

	/* SIGNALS */
	/* Cross-reference: notebook_switch_page_cb which we can't set here,
	 * because this only pertains to the label of the tab and not the
	 * tab as a whole.
	 */
    g_signal_connect (self->close_btn, "clicked",
                     G_CALLBACK(ghex_notebook_tab_close_click_cb), self);
}

static void
ghex_notebook_tab_dispose (GObject *object)
{
	GHexNotebookTab *self = GHEX_NOTEBOOK_TAB(object);

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(ghex_notebook_tab_parent_class)->dispose(object);
}

static void
ghex_notebook_tab_finalize (GObject *gobject)
{
	/* here, you would free stuff. I've got nuthin' for ya. */

	/* --- */

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(ghex_notebook_tab_parent_class)->finalize(gobject);
}

static void
ghex_notebook_tab_class_init (GHexNotebookTabClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = ghex_notebook_tab_dispose;
	object_class->finalize = ghex_notebook_tab_finalize;

	/* Layout manager: box-style layout. */
	gtk_widget_class_set_layout_manager_type (widget_class,
			GTK_TYPE_BOX_LAYOUT);

	/* SIGNALS */

	notebook_signals[CLOSED] = g_signal_new_class_handler("closed",
			G_OBJECT_CLASS_TYPE(object_class),
			G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		/* GCallback class_handler: */
			NULL,
		/* no accumulator or accu_data */
			NULL, NULL,
		/* GSignalCMarshaller c_marshaller: */
			NULL,		/* use generic marshaller */
		/* GType return_type: */
			G_TYPE_NONE,
		/* guint n_params: */
			0);
}

/* GHexNotebookTab - Internal Methods */ 

static void
ghex_notebook_tab_refresh_file_name (GHexNotebookTab *self)
{
	HexDocument *doc;
	char *basename;

   	doc = gtk_hex_get_document (self->gh);
	basename = g_path_get_basename (doc->file_name);

	gtk_label_set_markup (GTK_LABEL(self->label), basename);
	tab_bold_label (self, hex_document_has_changed (doc));

	g_free (basename);
}

static GtkWidget *
ghex_notebook_tab_new (void)
{
	return g_object_new (GHEX_TYPE_NOTEBOOK_TAB,
			/* no properties to set */
			NULL);
}

static void
ghex_notebook_tab_add_hex (GHexNotebookTab *self, GtkHex *gh)
{
	HexDocument *doc;

	/* Do some sanity checks, as this method requires that some ducks be in
	 * a row -- we need a valid GtkHex that is pre-loaded with a valid
	 * HexDocument.
	 */
	g_return_if_fail (GHEX_IS_NOTEBOOK_TAB (self));
	g_return_if_fail (GTK_IS_HEX (gh));

	doc = gtk_hex_get_document (gh);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	/* Associate this notebook tab with a GtkHex widget. */
	self->gh = gh;

	/* Set name of tab. */
	ghex_notebook_tab_refresh_file_name (self);

	/* HexDocument - Setup signals */
	g_signal_connect (doc, "document-changed",
			G_CALLBACK(ghex_notebook_tab_document_changed_cb), self);

	g_signal_connect_swapped (doc, "file-name-changed",
			G_CALLBACK(ghex_notebook_tab_refresh_file_name), self);

	g_signal_connect_swapped (doc, "file-saved",
			G_CALLBACK(ghex_notebook_tab_refresh_file_name), self);
}

static const char *
ghex_notebook_tab_get_filename (GHexNotebookTab *self)
{
	g_return_val_if_fail (GTK_IS_LABEL (GTK_LABEL(self->label)),
			NULL);

	return gtk_label_get_text (GTK_LABEL(self->label));
}


/* ---- */


/* GHexApplicationWindow -- CONSTRUCTORS AND DESTRUCTORS */

static void
ghex_application_window_init (GHexApplicationWindow *self)
{
	GtkWidget *widget = GTK_WIDGET(self);
	GtkStyleContext *context;
	GtkCssProvider *provider;

	gtk_widget_init_template (widget);

	/* Setup conversions box and pane */
	self->dialog = hex_dialog_new ();
	self->dialog_widget = hex_dialog_getview (self->dialog);

	gtk_box_append (GTK_BOX(self->conversions_box), self->dialog_widget);
	gtk_widget_hide (self->conversions_box);

	/* CSS - conversions_box */
	context = gtk_widget_get_style_context (self->conversions_box);
	provider = gtk_css_provider_new ();

	gtk_css_provider_load_from_data (provider,
									 "box {\n"
									 "   padding: 20px;\n"
									 "}\n", -1);

	/* add the provider to our widget's style context. */
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	/* Setup notebook */

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
			G_CALLBACK(find_close_cb), self);

	self->replace_dialog = replace_dialog_new ();
	gtk_widget_set_hexpand (self->replace_dialog, TRUE);
	gtk_box_append (GTK_BOX(self->findreplace_box), self->replace_dialog);
	gtk_widget_hide (self->replace_dialog);
	g_signal_connect (self->replace_dialog, "closed",
			G_CALLBACK(replace_close_cb), self);

	self->jump_dialog = jump_dialog_new ();
	gtk_widget_set_hexpand (self->jump_dialog, TRUE);
	gtk_box_append (GTK_BOX(self->findreplace_box), self->jump_dialog);
	gtk_widget_hide (self->jump_dialog);
	g_signal_connect (self->jump_dialog, "closed",
			G_CALLBACK(jump_close_cb), self);

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

	object_class->dispose = ghex_application_window_dispose;
	object_class->finalize = ghex_application_window_finalize;
	object_class->get_property = ghex_application_window_get_property;
	object_class->set_property = ghex_application_window_set_property;

	/* PROPERTIES */

	properties[PROP_CHARTABLE_OPEN] =
		g_param_spec_boolean ("chartable-open",
			"Character table open",
			"Whether the character table dialog is currently open",
			FALSE,	// gboolean default_value
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	properties[PROP_CONVERTER_OPEN] =
		g_param_spec_boolean ("converter-open",
			"Base converter open",
			"Whether the base converter dialog is currently open",
			FALSE,	// gboolean default_value
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	properties[PROP_FIND_OPEN] =
		g_param_spec_boolean ("find-open",
			"Find pane open",
			"Whether the Find pane is currently open",
			FALSE,	// gboolean default_value
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	properties[PROP_REPLACE_OPEN] =
		g_param_spec_boolean ("replace-open",
			"Replace pane open",
			"Whether the Find and Replace pane is currently open",
			FALSE,	// gboolean default_value
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	properties[PROP_JUMP_OPEN] =
		g_param_spec_boolean ("jump-open",
			"Jump pane open",
			"Whether the Jump to Byte pane is currently open",
			FALSE,	// gboolean default_value
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	properties[PROP_CAN_SAVE] =
		g_param_spec_boolean ("can-save",
			"Can save",
			"Whether the Save button should currently be clickable",
			FALSE,	// gboolean default_value
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);


	/* ACTIONS */

	gtk_widget_class_install_action (widget_class, "ghex.open",
			NULL,	// GVariant string param_type
			open_file);

	gtk_widget_class_install_action (widget_class, "ghex.save",
			NULL,	// GVariant string param_type
			save_action);

	gtk_widget_class_install_action (widget_class, "ghex.save-as",
			NULL,	// GVariant string param_type
			save_as);

	gtk_widget_class_install_action (widget_class, "ghex.show-conversions",
			NULL,	// GVariant string param_type
			toggle_conversions);

	gtk_widget_class_install_action (widget_class, "ghex.insert-mode",
			NULL,	// GVariant string param_type
			toggle_insert_mode);

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

	/* WIDGET TEMPLATE .UI */

	gtk_widget_class_set_template_from_resource (widget_class,
					"/org/gnome/ghex/ghex-application-window.ui");

	/* 
	 * for i in `cat tmp.txt`; do echo "gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow, ${i});"; done
	 */
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
}

void
ghex_application_window_add_hex (GHexApplicationWindow *self,
		GtkHex *gh)
{
	GtkWidget *tab;
	HexDocument *doc = gtk_hex_get_document (gh);

	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (HEX_IS_DOCUMENT(doc));

	/* Add this GtkHex to our internal list */
	// FIXME / TODO - used for nothing rn.
	self->gh_list = g_list_append (self->gh_list, gh);

	/* Set this GtkHex as the current viewed gh if there is no currently
	 * open document */
	if (! self->gh)
		ghex_application_window_set_hex (self, gh);

	/* Generate a tab */
	tab = ghex_notebook_tab_new ();
	printf ("%s: CREATED TAB -- %p\n", __func__, (void *)tab);
	ghex_notebook_tab_add_hex (GHEX_NOTEBOOK_TAB(tab), gh);
	g_signal_connect (tab, "closed",
			G_CALLBACK(tab_close_cb), self);

	gtk_notebook_append_page (GTK_NOTEBOOK(self->hex_notebook),
			GTK_WIDGET(gh),
			tab);

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

	/* Setup find_dialog & friends. */

	find_dialog_set_hex (FIND_DIALOG(self->find_dialog), self->gh);
	replace_dialog_set_hex (REPLACE_DIALOG(self->replace_dialog), self->gh);
	jump_dialog_set_hex (JUMP_DIALOG(self->jump_dialog), self->gh);
}