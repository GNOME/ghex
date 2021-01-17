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

struct _GHexNotebookTab
{
	GtkWidget parent_instance;
	
	GtkWidget *label;
	GtkWidget *close_btn;
	GtkHex *gh;				/* GtkHex widget activated when tab is clicked */
};

G_DEFINE_TYPE (GHexNotebookTab, ghex_notebook_tab, GTK_TYPE_WIDGET)

/* GHexNotebookTab - Internal Method Decls */

static GtkWidget * ghex_notebook_tab_new (void);
static void ghex_notebook_tab_add_hex (GHexNotebookTab *self, GtkHex *gh);
static const char * ghex_notebook_tab_get_filename (GHexNotebookTab *self);

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
	GtkWidget *hex_notebook;
	GtkWidget *conversions_box;
	GtkWidget *findreplace_box;
	GtkWidget *find_button;
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
	N_PROPERTIES
} GHexApplicationWindowProperty;

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GHexApplicationWindow, ghex_application_window,
		GTK_TYPE_APPLICATION_WINDOW)

/* ---- */


/* FUNCTION DECLARATIONS */

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

static void set_statusbar(GHexApplicationWindow *self, const char *str);
static void update_status_message (GHexApplicationWindow *self);


/* PRIVATE FUNCTIONS */

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

	g_return_if_fail (GHEX_IS_NOTEBOOK_TAB(tab));

	printf("%s: start - tab: %p\n",
			__func__, tab);

	if (tab->gh != self->gh) {
		ghex_application_window_set_hex (self, tab->gh);
		ghex_application_window_activate_tab (self, tab->gh);
	}
}

static void
notebook_page_added_cb (GtkNotebook *notebook,
		GtkWidget   *child,
		guint        page_num,
		gpointer     user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	
	/* Let's play this super dumb. If a page is added, that will generally
	 * mind the Find button should be activated. Change if necessary.
	 */
	gtk_widget_set_sensitive (self->find_button, TRUE);
}

static void
notebook_page_removed_cb (GtkNotebook *notebook,
		GtkWidget   *child,
		guint        page_num,
		gpointer     user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);

	g_debug("%s: n_pages: %d",
			__func__, gtk_notebook_get_n_pages(notebook));

	if (gtk_notebook_get_n_pages (notebook) == 0) {
		gtk_widget_set_sensitive (self->find_button, FALSE);
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

    g_signal_connect(G_OBJECT(self->chartable), "close-request",
                     G_CALLBACK(chartable_close_cb), self);
}

static void
setup_converter (GHexApplicationWindow *self)
{
	g_return_if_fail (GTK_IS_HEX(self->gh));

	self->converter = create_converter (GTK_WINDOW(self), self->gh);

	g_signal_connect(G_OBJECT(self->converter), "close-request",
                     G_CALLBACK(converter_close_cb), self);
}

static void
cursor_moved_cb (GtkHex *gh, gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	int current_pos;
	HexDialogVal64 val;

	current_pos = gtk_hex_get_cursor (gh);
	update_status_message (self);

	for (int i = 0; i < 8; i++)
	{
		/* returns 0 on buffer overflow, which is what we want */
		val.v[i] = gtk_hex_get_byte (gh, current_pos+i);
	}
	hex_dialog_updateview (self->dialog, &val);
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

/* convenience helper function to build a GtkHex widget pre-loaded with
 * a hex document, from a GFile *.
 */
static GtkHex *
new_gh_from_gfile (GFile *file)
{
	char *path;
	GFileInfo *info;
	const char *name;
	GError *error = NULL;
	HexDocument *doc;
	GtkHex *gh;

	path = g_file_get_path (file);
	info = g_file_query_info (file,
			G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
			G_FILE_QUERY_INFO_NONE,				// GFileQueryInfoFlags flags
			NULL,								// GCancellable *cancellable
			&error);
	name = g_file_info_get_display_name (info);

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
open_response_cb (GtkDialog *dialog,
		int resp,
		gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
	GFile *file;
	GtkHex *gh;

	if (resp == GTK_RESPONSE_OK)
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
	gtk_window_destroy (GTK_WINDOW(dialog));
}

static void
open_file (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);
	GtkWidget *file_sel;
	GtkResponseType resp;

	file_sel = gtk_file_chooser_dialog_new (_("Select a file to open"),
			GTK_WINDOW(self),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			_("_Cancel"), GTK_RESPONSE_CANCEL,
			_("_Open"), GTK_RESPONSE_OK,
			NULL);

	gtk_window_set_modal (GTK_WINDOW(file_sel), TRUE);
	gtk_widget_show (file_sel);

	g_signal_connect (file_sel, "response",
			G_CALLBACK(open_response_cb), self);
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

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/* GHexNotebookTab -- CALLBACKS */

static void
dumb_click_cb (GtkButton *button,
               gpointer   user_data)
{
	g_debug("%s: clicked btn: %p",
			__func__, (void *)button);
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
    g_signal_connect(self->close_btn, "clicked",
                     G_CALLBACK(dumb_click_cb), self);
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
}

/* GHexNotebookTab - Internal Methods */ 

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
	char *basename;

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
	basename = g_path_get_basename (doc->file_name);

	gtk_label_set_text (GTK_LABEL(self->label), basename);

	g_free (basename);
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

	/* Grey out Find button at the beginning */
	gtk_widget_set_sensitive (self->find_button, FALSE);
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

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);


	/* ACTIONS */

	gtk_widget_class_install_action (widget_class, "ghex.open",
			NULL,	// GVariant string param_type
			open_file);

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
			hex_notebook);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			conversions_box);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			findreplace_box);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			find_button);
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
	self->gh_list = g_list_append (self->gh_list, gh);

	/* Set this GtkHex as the current viewed gh if there is no currently
	 * open document */
	if (! self->gh)
		ghex_application_window_set_hex (self, gh);

	/* Generate a tab */
	tab = ghex_notebook_tab_new ();
	printf ("%s: CREATED TAB -- %p\n", __func__, (void *)tab);
	ghex_notebook_tab_add_hex (GHEX_NOTEBOOK_TAB(tab), gh);

	gtk_notebook_append_page (GTK_NOTEBOOK(self->hex_notebook),
			GTK_WIDGET(gh),
			tab);

	/* set text of context menu tab switcher to the filename rather than
	 * 'Page X' */
	gtk_notebook_set_menu_label_text (GTK_NOTEBOOK(self->hex_notebook),
			GTK_WIDGET(gh),
			ghex_notebook_tab_get_filename (GHEX_NOTEBOOK_TAB(tab)));

	/* Setup signals */
    g_signal_connect(G_OBJECT(gh), "cursor-moved",
                     G_CALLBACK(cursor_moved_cb), self);

	/* Setup find_dialog & friends. */

	find_dialog_set_hex (FIND_DIALOG(self->find_dialog), self->gh);
	replace_dialog_set_hex (REPLACE_DIALOG(self->replace_dialog), self->gh);
	jump_dialog_set_hex (JUMP_DIALOG(self->jump_dialog), self->gh);
}
