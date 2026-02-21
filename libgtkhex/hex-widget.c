// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-widget"

#include "hex-widget.h"

#include "hex-private-common.h"
#include "gtkhex-paste-data.h"
#include "gtkhex-layout-manager.h"
#include "hex-highlight-private.h"
#include "hex-auto-highlight-private.h"

#include "util.h"

static gboolean hex_widget_get_can_undo (HexWidget *self);
static gboolean hex_widget_get_can_redo (HexWidget *self);

enum
{
	PROP_0,
	PROP_CHAR_WIDTH,
	PROP_NUM_LINES,
	PROP_CAN_UNDO,
	PROP_CAN_REDO,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _HexWidget
{
	HexView parent_instance;

	GBinding *auto_geometry_binding;
	gboolean can_undo;
	gboolean can_redo;
	
	/* From template */
	GtkWidget *offsets;
	GtkWidget *xdisp;
	GtkWidget *adisp;
};

G_DEFINE_TYPE (HexWidget, hex_widget, HEX_TYPE_VIEW)

/* --- */

static size_t
clamp_rep_len (HexWidget *self, size_t start_pos, size_t rep_len)
{
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	HexBuffer *buf = hex_document_get_buffer (document);
	size_t payload_size = hex_buffer_get_payload_size (buf);

	if (rep_len > payload_size - start_pos)
		rep_len = payload_size - start_pos;

	return rep_len;
}

static void
paste_set_data (HexWidget *self, char *data, gsize data_len)
{
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 start_offset = hex_selection_get_start_offset (selection);
	const gint64 end_offset = hex_selection_get_end_offset (selection);
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	gsize start_pos = hex_selection_get_cursor_pos (selection);
	gboolean insert_mode = hex_view_get_insert_mode (HEX_VIEW(self));
	gsize len = data_len;
	gsize rep_len = 0;

	if (start_offset != end_offset)
	{
		gsize selection_len, end_pos;

		start_pos = MIN (start_offset, end_offset);
		end_pos = MAX (start_offset, end_offset);
		selection_len = end_pos - start_pos + 1;

		if (insert_mode)
			rep_len = selection_len;
		else
			len = rep_len = MIN (selection_len, len);
	}

	if (!insert_mode)
		len = rep_len = clamp_rep_len (self, start_pos, len);

	hex_document_set_data (document,
			start_pos,
			len,
			/* rep_len is either:
			 * 0 (insert w/o replacing),
			 * len (delete and paste the same number of bytes),
			 * or a different number (delete and paste an arbitrary number
			 * of bytes)
			 */
			rep_len,
			data,
			TRUE);

	hex_selection_set_cursor_pos (selection, start_pos + len);
}

static void
plaintext_paste_received_cb (GObject *source_object,
		GAsyncResult *result,
		gpointer user_data)
{
	HexWidget *self = HEX_WIDGET(user_data);
	GdkClipboard *clipboard;
	g_autofree char *text = NULL;
	g_autoptr(GError) error = NULL;

	g_debug ("%s: We DON'T have HexPasteData. Falling back to plaintext paste",
			__func__);

	clipboard = GDK_CLIPBOARD (source_object);

	/* Get the resulting text of the read operation */
	text = gdk_clipboard_read_text_finish (clipboard, result, &error);

	if (text)
		paste_set_data (self, text, strlen(text));
	else
		g_critical ("Error pasting text: %s", error->message);
}

static void
paste_helper (HexWidget *self, GdkClipboard *clipboard)
{
	GdkContentProvider *content;
	GValue value = G_VALUE_INIT;
	HexPasteData *paste;
	gboolean have_hex_paste_data = FALSE;

	content = gdk_clipboard_get_content (clipboard);
	g_value_init (&value, HEX_TYPE_PASTE_DATA);

	/* If the clipboard contains our special HexPasteData, we'll use it.
	 * If not, just fall back to plaintext.
	 */
	have_hex_paste_data = content ?
		gdk_content_provider_get_value (content, &value, NULL) : FALSE;

	if (have_hex_paste_data)
	{
		char *doc_data;
		size_t start_pos, len, rep_len;

		g_debug("%s: We HAVE HexPasteData.", __func__);

		paste = HEX_PASTE_DATA(g_value_get_object (&value));
		doc_data = hex_paste_data_get_doc_data (paste);
		len = hex_paste_data_get_elems (paste);

		paste_set_data (self, doc_data, len);
	}
	else
	{
		gdk_clipboard_read_text_async (clipboard,
				NULL,	/* cancellable */
				plaintext_paste_received_cb,
				self);
	}
}

void
hex_widget_paste_from_clipboard (HexWidget *self)
{
	GdkClipboard *clipboard;

	g_return_if_fail (HEX_IS_WIDGET (self));

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET(self));
	g_return_if_fail (clipboard);

	paste_helper (self, clipboard);
}

static void
paste_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexWidget *self = user_data;
	hex_widget_paste_from_clipboard (self);
}

void
hex_widget_copy_to_clipboard (HexWidget *self)
{
	HexDocument *document;
	HexSelection *selection;
	HexHighlight *highlight;
	GdkClipboard *clipboard;
	HexPasteData *paste;
	GdkContentProvider *provider_union;
	GdkContentProvider *provider_array[2];
	gint64 start_pos;
	gsize len;
	char *doc_data;
	char *string;

	g_return_if_fail (HEX_IS_WIDGET (self));

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET(self));

	selection = hex_view_get_selection (HEX_VIEW(self));
	highlight = hex_selection_get_highlight (selection);

	start_pos = MIN (hex_selection_get_start_offset (selection), hex_selection_get_end_offset (selection));
	len = hex_highlight_get_n_selected (highlight);

	g_return_if_fail (len);

	/* Grab the raw data from the HexDocument. */
	document = hex_view_get_document (HEX_VIEW(self));
	doc_data = hex_buffer_get_data (hex_document_get_buffer (document),
			start_pos, len);

	/* Setup a union of HexPasteData and a plain C string */
	paste = hex_paste_data_new (doc_data, len);
	g_return_if_fail (HEX_IS_PASTE_DATA(paste));
	string = hex_paste_data_get_string (paste);

	provider_array[0] =
		gdk_content_provider_new_typed (HEX_TYPE_PASTE_DATA, paste);
	provider_array[1] =
		gdk_content_provider_new_typed (G_TYPE_STRING, string);

	provider_union = gdk_content_provider_new_union (provider_array, 2);

	/* Finally, set our content to our newly created union. */
	gdk_clipboard_set_content (clipboard, provider_union);
}

static void
copy_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexWidget *self = user_data;
	hex_widget_copy_to_clipboard (self);
}

void
hex_widget_cut_to_clipboard (HexWidget *self)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	hex_widget_copy_to_clipboard (self);

	if (hex_view_get_insert_mode (HEX_VIEW(self)))
		util_delete_selection_in_doc (self);
	else
		util_zero_selection_in_doc (self);
}

static void
cut_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexWidget *self = user_data;
	hex_widget_cut_to_clipboard (self);
}

static void
undo_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexWidget *self = user_data;
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	HexChangeData *cd;

	g_assert (HEX_IS_DOCUMENT (document));
	g_assert (hex_document_get_can_undo (document));

	cd = hex_document_get_undo_data (document);

	hex_document_undo (document);

	hex_selection_collapse (selection, hex_change_data_get_start_offset (cd));
}

static void
redo_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexWidget *self = user_data;
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	HexChangeData *cd;

	g_assert (HEX_IS_DOCUMENT (document));
	g_assert (hex_document_get_can_redo (document));

	hex_document_redo (document);

	cd = hex_document_get_undo_data (document);

	hex_selection_collapse (selection, hex_change_data_get_start_offset (cd));
}

static void
next_match_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexWidget *self = user_data;
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	GListModel *auto_highlights = hex_view_get_auto_highlights (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	g_autoptr(GListModel) all_highlights = NULL;

	all_highlights = _hex_auto_highlight_build_1d_list (auto_highlights);

	for (guint i = 0; i < g_list_model_get_n_items (all_highlights); ++i)
	{
		g_autoptr(HexHighlight) hl = g_list_model_get_item (all_highlights, i);

		g_assert (HEX_IS_HIGHLIGHT (hl));

		if (hl->start_offset > cursor_pos)
		{
			hex_selection_collapse (selection, hl->start_offset);
			return;
		}
	}
}

static void
prev_match_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexWidget *self = user_data;
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	GListModel *auto_highlights = hex_view_get_auto_highlights (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	g_autoptr(GListModel) all_highlights = NULL;

	all_highlights = _hex_auto_highlight_build_1d_list (auto_highlights);

	/* A n_items is a guint which will wraparound, so it makes the test overly
	 * convoluted. Just assert that there are no more than INT_MAX items, which
	 * would be a nutso amount of items to have in a highlight list anyway.
	 */
	g_return_if_fail (g_list_model_get_n_items (all_highlights) <= INT_MAX);

	for (int i = (int) g_list_model_get_n_items (all_highlights) - 1; i >= 0; --i)
	{
		g_autoptr(HexHighlight) hl = g_list_model_get_item (all_highlights, i);

		g_assert (HEX_IS_HIGHLIGHT (hl));

		if (hl->start_offset < cursor_pos)
		{
			hex_selection_collapse (selection, hl->start_offset);
			return;
		}
	}
}

static void
clear_matches_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexWidget *self = user_data;

	hex_view_clear_auto_highlights (HEX_VIEW(self));
}

static void
recalc_adjustment (HexWidget *self, HexWidgetLayout *layout_manager)
{
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	HexBuffer *buf = hex_document_get_buffer (document);
	gint64 payload = hex_buffer_get_payload_size (buf);
	int cpl = hex_view_get_cpl (HEX_VIEW(self));
	GtkAdjustment *vadj = hex_view_get_vadjustment (HEX_VIEW(self));
	int num_total_lines;
	int num_disp_lines;
	double upper;

	if (! (payload && cpl))
		return;

	num_disp_lines = hex_widget_get_num_lines (self);
	num_total_lines = payload / cpl;

	upper = MAX (payload / cpl * HEX_ADJ_PIXEL_MULTIPLIER,
			(num_total_lines + num_disp_lines / 2) * HEX_ADJ_PIXEL_MULTIPLIER
			);

	gtk_adjustment_set_lower (vadj, 0.0);
	gtk_adjustment_set_upper (vadj, upper);
	gtk_adjustment_set_step_increment (vadj, 1.0 * HEX_ADJ_PIXEL_MULTIPLIER);
	gtk_adjustment_set_page_increment (vadj, (num_disp_lines - 1) * HEX_ADJ_PIXEL_MULTIPLIER);
	gtk_adjustment_set_page_size (vadj, num_disp_lines * HEX_ADJ_PIXEL_MULTIPLIER);
}

/* --- */

static void
document_set_cb (HexWidget *self, GParamSpec *pspec, gpointer user_data)
{
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));

	g_object_bind_property (document, "can-undo", self, "can-undo", G_BINDING_SYNC_CREATE);
	g_object_bind_property (document, "can-redo", self, "can-redo", G_BINDING_SYNC_CREATE);
}

int
hex_widget_get_char_width (HexWidget *self)
{
	const char *font;
	/* No autoptr available :( */
	PangoContext *context;
	PangoFont *pango_font;
	PangoFontMetrics *metrics;
	PangoFontDescription *font_desc;
	int default_width = 0, width = 0, retval;

	g_return_val_if_fail (HEX_IS_WIDGET (self), 10);

	font = hex_view_get_font (HEX_VIEW(self));

	g_return_val_if_fail (font != NULL, 10);
	
	context = gtk_widget_create_pango_context (GTK_WIDGET(self));

	/* Get default */

	metrics = pango_context_get_metrics (context, NULL, NULL);
	default_width = MAX (pango_font_metrics_get_approximate_digit_width (metrics),
			pango_font_metrics_get_approximate_char_width (metrics));
	default_width = PANGO_PIXELS (default_width);
	g_clear_pointer (&metrics, pango_font_metrics_unref);

	/* Get custom */

	font_desc = pango_font_description_from_string (font);
	pango_font = pango_context_load_font (context, font_desc);

	if (pango_font)
	{
		metrics = pango_font_get_metrics (pango_font, NULL);
		width = MAX (pango_font_metrics_get_approximate_digit_width (metrics),
				pango_font_metrics_get_approximate_char_width (metrics));
		width = PANGO_PIXELS (width);
	}

	/* A width of 0 means either the font is invalid or no size info was
	 * provided, so grab the context's default width info instead.
	 */
	if (width <= 0)
		retval = default_width;
	else
		retval = width;
	
	g_object_unref (context);
	g_object_unref (pango_font);
	pango_font_description_free (font_desc);
	pango_font_metrics_unref (metrics);

	return retval;
}

int
hex_widget_get_num_lines (HexWidget *self)
{
	PangoContext *context;
	PangoFontMetrics *metrics;
	int char_height = 0;
	int pane_height = 0;
	GtkWidget *children[] = {self->offsets, self->xdisp, self->adisp};

	g_return_val_if_fail (HEX_IS_WIDGET (self), 0);

	context = gtk_widget_get_pango_context (GTK_WIDGET(self));
	metrics = pango_context_get_metrics (context, NULL, NULL);

	char_height = pango_font_metrics_get_height (metrics);
	char_height = PANGO_PIXELS (char_height);

	for (guint i = 0; i < G_N_ELEMENTS (children); ++i)
	{
		GtkWidget *child = children[i];

		if (! gtk_widget_is_visible (child) || ! gtk_widget_get_realized (child))
			continue;

		if ((pane_height = gtk_widget_get_height (child)) > 0)
			break;
	}
	
	if (pane_height && char_height)
		return pane_height / char_height;

	return 0;
}

static void
font_set_cb (HexWidget *self, GParamSpec *pspec, gpointer user_data)
{
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_CHAR_WIDTH]);
}

static void
auto_geometry_set_cb (HexWidget *self, GParamSpec *pspec, gpointer user_data)
{
	gboolean auto_geometry = hex_view_get_auto_geometry (HEX_VIEW(self));

	g_clear_object (&self->auto_geometry_binding);

	if (auto_geometry)
	{
		GtkLayoutManager *layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET(self));

		self->auto_geometry_binding = g_object_bind_property (layout_manager, "cpl", self, "cpl", G_BINDING_SYNC_CREATE);
	}
}

static void
hex_widget_set_can_undo (HexWidget *self, gboolean can_undo)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	self->can_undo = can_undo;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_CAN_UNDO]);
}

static gboolean
hex_widget_get_can_undo (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), FALSE);

	return self->can_undo;
}

static void
hex_widget_set_can_redo (HexWidget *self, gboolean can_redo)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	self->can_redo = can_redo;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_CAN_REDO]);
}

static gboolean
hex_widget_get_can_redo (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), FALSE);

	return self->can_redo;
}

static void
hex_widget_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexWidget *self = HEX_WIDGET(object);

	switch (property_id)
	{
		case PROP_CAN_UNDO:
			hex_widget_set_can_undo (self, g_value_get_boolean (value));
			break;

		case PROP_CAN_REDO:
			hex_widget_set_can_redo (self, g_value_get_boolean (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_widget_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexWidget *self = HEX_WIDGET(object);

	switch (property_id)
	{
		case PROP_CHAR_WIDTH:
			g_value_set_int (value, hex_widget_get_char_width (self));
			break;

		case PROP_NUM_LINES:
			g_value_set_int (value, hex_widget_get_num_lines (self));
			break;

		case PROP_CAN_UNDO:
			g_value_set_boolean (value, hex_widget_get_can_undo (self));
			break;

		case PROP_CAN_REDO:
			g_value_set_boolean (value, hex_widget_get_can_redo (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_widget_dispose (GObject *object)
{
	HexWidget *self = HEX_WIDGET(object);

	gtk_widget_dispose_template (GTK_WIDGET(object), HEX_TYPE_WIDGET);

	/* Chain up */
	G_OBJECT_CLASS(hex_widget_parent_class)->dispose (object);
}

static void
hex_widget_finalize (GObject *object)
{
	HexWidget *self = HEX_WIDGET(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_widget_parent_class)->finalize (object);
}

static void
hex_widget_constructed (GObject *object)
{
	HexWidget *self = HEX_WIDGET(object);
	HexWidgetLayout *layout_manager = HEX_WIDGET_LAYOUT(gtk_widget_get_layout_manager (GTK_WIDGET(self)));

	/* We need the object fully constructed before we bind the font properties so
	 * it can't be done in the ui file.
	 */
	g_object_bind_property (self, "font", self->adisp, "font", G_BINDING_SYNC_CREATE);
	g_object_bind_property (self, "font", self->xdisp, "font", G_BINDING_SYNC_CREATE);
	g_object_bind_property (self, "font", self->offsets, "font", G_BINDING_SYNC_CREATE);

	/* Pegged to :font which is also a construct property... */

	g_object_bind_property (self, "char-width", layout_manager, "char-width", G_BINDING_SYNC_CREATE);

	/* This depends on the binding between the widget and layout manager being set up, which doesn't happen until the constructor properties are set.
	 */
	g_object_bind_property (self, "cpl", self->adisp, "cpl", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
	g_object_bind_property (self, "cpl", self->xdisp, "cpl", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
	g_object_bind_property (self, "cpl", self->offsets, "cpl", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	/* Chain up */
	G_OBJECT_CLASS(hex_widget_parent_class)->constructed (object);
}

static void
hex_widget_class_init (HexWidgetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->constructed = hex_widget_constructed;
	object_class->dispose = hex_widget_dispose;
	object_class->finalize = hex_widget_finalize;
	object_class->set_property = hex_widget_set_property;
	object_class->get_property = hex_widget_get_property;

	/* TEMPLATE */

	g_type_ensure (HEX_TYPE_WIDGET_LAYOUT);

	// FIXME - resource
	gtk_widget_class_set_template (widget_class, g_file_load_bytes (g_file_new_for_path ("hex-widget.ui"),
				NULL, NULL, NULL));

	gtk_widget_class_bind_template_child (widget_class, HexWidget, offsets);
	gtk_widget_class_bind_template_child (widget_class, HexWidget, xdisp);
	gtk_widget_class_bind_template_child (widget_class, HexWidget, adisp);

	/* PROPERTIES */

	properties[PROP_CHAR_WIDTH] = g_param_spec_int ("char-width", NULL, NULL,
			0, 1000, 10,
			default_flags | G_PARAM_READABLE);

	properties[PROP_NUM_LINES] = g_param_spec_int ("num-lines", NULL, NULL,
			0, 10000, 0,
			default_flags | G_PARAM_READABLE);

	properties[PROP_CAN_UNDO] = g_param_spec_boolean ("can-undo", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_CAN_REDO] = g_param_spec_boolean ("can-redo", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	/* Keybindings */

	/* Ctrl+c - copy */
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_c, GDK_CONTROL_MASK,
			"clipboard.copy", NULL);

	/* Ctrl+x - cut */
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_x, GDK_CONTROL_MASK,
			"clipboard.cut", NULL);

	/* Ctrl+v - paste */
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_v, GDK_CONTROL_MASK,
			"clipboard.paste", NULL);

	/* Ctrl+z - undo */
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_z, GDK_CONTROL_MASK,
			"document.undo", NULL);

	/* Ctrl+y - redo */
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_y, GDK_CONTROL_MASK,
			"document.redo", NULL);

	/* INS - toggle insert mode */
	gtk_widget_class_install_property_action (widget_class, "document.insert-mode", "insert-mode");
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Insert, 0, "document.insert-mode", NULL);
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Insert, 0, "document.insert-mode", NULL);

	/* F3 - next match */
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_F3, 0, "find.next-match", NULL);

	/* Shift-F3 - prev match */
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_F3, GDK_SHIFT_MASK, "find.prev-match", NULL);

	/* ESC - clear auto-highlights */
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "find.clear-matches", NULL);

	/* Load global CSS data for widget */
	{
		GdkDisplay *display = gdk_display_get_default ();

		if (display != NULL)
		{
			g_autoptr(GtkCssProvider) css_provider = gtk_css_provider_new ();

			gtk_css_provider_load_from_resource (css_provider, "/org/gnome/libgtkhex/css/libgtkhex.css");

			gtk_style_context_add_provider_for_display (display, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_THEME-1);
		}
	}
}

static void
hex_widget_init (HexWidget *self)
{
	gtk_widget_init_template (GTK_WIDGET(self));

	g_signal_connect (self, "notify::document", G_CALLBACK(document_set_cb), NULL);
	g_signal_connect (self, "notify::font", G_CALLBACK(font_set_cb), NULL);
	g_signal_connect (self, "notify::auto-geometry", G_CALLBACK(auto_geometry_set_cb), NULL);

	/* Setup layout manager */
	{
		HexWidgetLayout *layout_manager = HEX_WIDGET_LAYOUT(gtk_widget_get_layout_manager (GTK_WIDGET(self)));

		g_signal_connect_object (layout_manager, "size-allocated", G_CALLBACK(recalc_adjustment), self, G_CONNECT_SWAPPED);
	}

	/* Setup actions and use bindings to determine when they should be enabled/disabled.*/
	{
		GActionEntry document_entries[] = {
			{"undo", undo_action},
			{"redo", redo_action},
		};
		g_autoptr(GSimpleActionGroup) document_actions = g_simple_action_group_new ();
		GAction *action;

		g_action_map_add_action_entries (G_ACTION_MAP(document_actions), document_entries, G_N_ELEMENTS (document_entries), self);

		gtk_widget_insert_action_group (GTK_WIDGET(self), "document", G_ACTION_GROUP(document_actions));

		action = g_action_map_lookup_action (G_ACTION_MAP(document_actions), "undo");
		g_object_bind_property (self, "can-undo", action, "enabled", G_BINDING_SYNC_CREATE);

		action = g_action_map_lookup_action (G_ACTION_MAP(document_actions), "redo");
		g_object_bind_property (self, "can-redo", action, "enabled", G_BINDING_SYNC_CREATE);
	}
	{
		GActionEntry clipboard_entries[] = {
			{"copy", copy_action},
			{"cut", cut_action},
			{"paste", paste_action},
		};
		g_autoptr(GSimpleActionGroup) clipboard_actions = g_simple_action_group_new ();
		GAction *action;

		g_action_map_add_action_entries (G_ACTION_MAP(clipboard_actions), clipboard_entries, G_N_ELEMENTS (clipboard_entries), self);

		gtk_widget_insert_action_group (GTK_WIDGET(self), "clipboard", G_ACTION_GROUP(clipboard_actions));

		action = g_action_map_lookup_action (G_ACTION_MAP(clipboard_actions), "copy");
		g_object_bind_property_full (self, "document", action, "enabled", G_BINDING_SYNC_CREATE, util_have_object_transform_to, NULL, NULL, NULL);

		action = g_action_map_lookup_action (G_ACTION_MAP(clipboard_actions), "cut");
		g_object_bind_property_full (self, "document", action, "enabled", G_BINDING_SYNC_CREATE, util_have_object_transform_to, NULL, NULL, NULL);

		action = g_action_map_lookup_action (G_ACTION_MAP(clipboard_actions), "paste");
		g_object_bind_property_full (self, "document", action, "enabled", G_BINDING_SYNC_CREATE, util_have_object_transform_to, NULL, NULL, NULL);
	}
	{
		GActionEntry find_entries[] = {
			{"next-match", next_match_action},
			{"prev-match", prev_match_action},
			{"clear-matches", clear_matches_action},
		};
		g_autoptr(GSimpleActionGroup) find_actions = g_simple_action_group_new ();
		GAction *action;

		g_action_map_add_action_entries (G_ACTION_MAP(find_actions), find_entries, G_N_ELEMENTS (find_entries), self);

		gtk_widget_insert_action_group (GTK_WIDGET(self), "find", G_ACTION_GROUP(find_actions));

		action = g_action_map_lookup_action (G_ACTION_MAP(find_actions), "next-match");
		g_object_bind_property_full (self, "document", action, "enabled", G_BINDING_SYNC_CREATE, util_have_object_transform_to, NULL, NULL, NULL);

		action = g_action_map_lookup_action (G_ACTION_MAP(find_actions), "prev-match");
		g_object_bind_property_full (self, "document", action, "enabled", G_BINDING_SYNC_CREATE, util_have_object_transform_to, NULL, NULL, NULL);
	}
}

GtkWidget *
hex_widget_new (void)
{
	return g_object_new (HEX_TYPE_WIDGET, NULL);
}
