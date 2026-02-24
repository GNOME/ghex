// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-view"

#include "hex-view.h"
#include "hex-private-common.h"
#include "hex-mark-private.h"
#include "hex-highlight-private.h"

enum
{
	PROP_0,
	PROP_DOCUMENT,
	PROP_CPL,
	PROP_AUTO_GEOMETRY,
	PROP_FONT,
	PROP_BLINK_CURSOR,
	PROP_SELECTION,
	PROP_MARKS,
	PROP_AUTO_HIGHLIGHTS,
	PROP_N_VIS_LINES,
	PROP_INSERT_MODE,

	/* GtkScrollable properties */
	PROP_VADJUSTMENT,
	PROP_HADJUSTMENT,
	PROP_VSCROLL_POLICY,
	PROP_HSCROLL_POLICY,

	N_PROPERTIES = PROP_VADJUSTMENT
};

static GParamSpec *properties[N_PROPERTIES];

typedef struct
{
	HexDocument *document;
	int cpl;
	gboolean auto_geometry;
	char *font;
	gboolean blink_cursor;
	HexSelection *selection;
	GListModel *marks;
	GListModel *auto_highlights;
	gboolean insert_mode;

	/* GtkScrollable interface */

	GtkAdjustment *vadj;
	GtkScrollablePolicy vscroll_policy;

} HexViewPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (HexView, hex_view, GTK_TYPE_WIDGET,
		G_ADD_PRIVATE (HexView)
		G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

/* --- */

#define HEX_TYPE_VIEW_SELECTION (hex_view_selection_get_type ())
G_DECLARE_FINAL_TYPE (HexViewSelection, hex_view_selection, HEX, VIEW_SELECTION, HexSelection)

static gint64 _hex_view_clamp_cursor_pos (HexView *self, gint64 cursor_pos) G_GNUC_WARN_UNUSED_RESULT;
static void hex_view_selection_real_set_cursor_pos (HexSelection *self, gint64 cursor_pos);
static void hex_view_selection_real_set_selection_anchor (HexSelection *self, gint64 selection_anchor);

/* --- */

struct _HexViewSelection
{
	HexSelection parent_instance;

	HexView *parent;
};

G_DEFINE_TYPE (HexViewSelection, hex_view_selection, HEX_TYPE_SELECTION)

/* --- */

static void
hex_view_selection_dispose (GObject *object)
{
	HexViewSelection *self = HEX_VIEW_SELECTION(object);

	g_clear_object (&self->parent);

	/* Chain up */
	G_OBJECT_CLASS(hex_view_selection_parent_class)->dispose (object);
}

static void
hex_view_selection_class_init (HexViewSelectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->dispose = hex_view_selection_dispose;
	HEX_SELECTION_CLASS(klass)->set_cursor_pos = hex_view_selection_real_set_cursor_pos;
	HEX_SELECTION_CLASS(klass)->set_selection_anchor = hex_view_selection_real_set_selection_anchor;
}

static void
hex_view_selection_init (HexViewSelection *self)
{
}

static HexSelection *
hex_view_selection_new (HexView *parent)
{
	HexViewSelection *self = g_object_new (HEX_TYPE_VIEW_SELECTION, NULL);

	self->parent = g_object_ref (parent);

	return HEX_SELECTION(self);
}

static gint64
_hex_view_clamp_cursor_pos (HexView *self, gint64 cursor_pos)
{
	HexViewPrivate *priv = hex_view_get_instance_private (self);
	HexBuffer *buf = hex_document_get_buffer (priv->document);
	const gint64 payload_size = hex_buffer_get_payload_size (buf);
	const gboolean insert_mode = hex_view_get_insert_mode (self);

	if (cursor_pos < 0)
	{
		cursor_pos = 0;

		g_debug ("%s: cursor_pos cannot be below 0. Clamping to 0", __func__);
	}
	/* This logic certainly doesn't need to be this complicated, but for now we
	 * want to be somewhat verbose if the cursor_pos is programmatically set
	 * out of bounds.
	 */
	else if (cursor_pos >= payload_size)
	{
		if (cursor_pos == payload_size && insert_mode)
			;	/* this is fine. */
		else
		{
			cursor_pos = CLAMP (insert_mode ? payload_size : payload_size - 1, 0, INT64_MAX);

			g_debug ("%s: cursor_pos out of bounds. Clamping to: %ld", __func__, cursor_pos);
		}
	}

	return cursor_pos;
}

static void
hex_view_selection_real_set_cursor_pos (HexSelection *selection, gint64 cursor_pos)
{
	HexViewSelection *self = HEX_VIEW_SELECTION(selection);

	g_assert (HEX_IS_VIEW (self->parent));

	cursor_pos = _hex_view_clamp_cursor_pos (self->parent, cursor_pos);

	HEX_SELECTION_CLASS(hex_view_selection_parent_class)->set_cursor_pos (selection, cursor_pos);
}

static void
hex_view_selection_real_set_selection_anchor (HexSelection *selection, gint64 selection_anchor)
{
	HexViewSelection *self = HEX_VIEW_SELECTION(selection);

	g_assert (HEX_IS_VIEW (self->parent));

	selection_anchor = _hex_view_clamp_cursor_pos (self->parent, selection_anchor);

	HEX_SELECTION_CLASS(hex_view_selection_parent_class)->set_selection_anchor (selection, selection_anchor);
}

/* --- */

static void
doc_read_cb (GObject *source_object, GAsyncResult *res, gpointer data)
{
	HexView *self = data;
	HexDocument *doc = (HexDocument *) source_object;
	g_autoptr(GError) error = NULL;
	gboolean retval = FALSE;

	g_assert (HEX_IS_VIEW (self));
	g_assert (HEX_IS_DOCUMENT (doc));

	retval = hex_document_read_finish (HEX_DOCUMENT(source_object), res, &error);

	if (retval)
	{
		g_debug ("%s: document %p read successfully", __func__, doc);
		gtk_widget_queue_draw (GTK_WIDGET(self));
	}
	else
	{
		g_debug ("%s: document read failed", __func__);
	}
}

void
hex_view_set_document (HexView *self, HexDocument *document)
{
	HexViewPrivate *priv;

	g_return_if_fail (HEX_IS_VIEW (self));
	g_return_if_fail (document == NULL || HEX_IS_DOCUMENT (document));

	if (!document)
		g_debug ("%s: NULL passed as document parameter", __func__);

	priv = hex_view_get_instance_private (self);

	g_clear_object (&priv->document);
	priv->document = document ? g_object_ref (document) : hex_document_new ();

	g_signal_connect_object (priv->document, "document-changed", G_CALLBACK(gtk_widget_queue_draw), self, G_CONNECT_SWAPPED);
	g_signal_connect_object (priv->document, "document-changed", G_CALLBACK(gtk_widget_queue_allocate), self, G_CONNECT_SWAPPED);

	g_debug ("%s: document set to: %p", __func__, priv->document);

	// FIXME - should this be necessary??

	if (hex_document_get_file (priv->document) != NULL)
		hex_document_read_async (priv->document, NULL, doc_read_cb, self);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_DOCUMENT]);
}

/* transfer none */

HexDocument *
hex_view_get_document (HexView *self)
{
	HexViewPrivate *priv;

	g_return_val_if_fail (HEX_IS_VIEW (self), NULL);

	priv = hex_view_get_instance_private (self);

	return priv->document;
}

void
hex_view_set_cpl (HexView *self, int cpl)
{
	HexViewPrivate *priv;

	g_return_if_fail (HEX_IS_VIEW (self));

	priv = hex_view_get_instance_private (self);

	priv->cpl = cpl;

	gtk_widget_queue_allocate (GTK_WIDGET(self));

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_CPL]);
}

/* transfer none */

// FIXME - should this always return non-zero?
int
hex_view_get_cpl (HexView *self)
{
	HexViewPrivate *priv;

	g_return_val_if_fail (HEX_IS_VIEW (self), 0);

	priv = hex_view_get_instance_private (self);

	return priv->cpl;
}

void
hex_view_set_auto_geometry (HexView *self, gboolean auto_geometry)
{
	HexViewPrivate *priv;

	g_return_if_fail (HEX_IS_VIEW (self));

	priv = hex_view_get_instance_private (self);

	priv->auto_geometry = auto_geometry;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_AUTO_GEOMETRY]);
}

gboolean
hex_view_get_auto_geometry (HexView *self)
{
	HexViewPrivate *priv;

	g_return_val_if_fail (HEX_IS_VIEW (self), TRUE);

	priv = hex_view_get_instance_private (self);

	return priv->auto_geometry;
}

/* transfer none */
void
hex_view_set_vadjustment (HexView *self, GtkAdjustment *vadj)
{
	HexViewPrivate *priv = hex_view_get_instance_private (self);

	g_clear_object (&priv->vadj);

	if (vadj)
		priv->vadj = g_object_ref (vadj);
}

/* transfer none */
GtkAdjustment *
hex_view_get_vadjustment (HexView *self)
{
	HexViewPrivate *priv = hex_view_get_instance_private (self);

	return priv->vadj;
}

const char *
hex_view_get_font (HexView *self)
{
	HexViewPrivate *priv = hex_view_get_instance_private (self);

	return priv->font;
}

void
hex_view_set_font (HexView *self, const char *font)
{
	HexViewPrivate *priv = hex_view_get_instance_private (self);
	gboolean parse_ret;
	g_autofree char *try_font_tag = NULL;

	g_return_if_fail (self && font);

	/* Sanity check to make sure font is valid */
	try_font_tag = g_strdup_printf ("<span font=\"%s\"></span>", font);
	parse_ret = pango_parse_markup (try_font_tag, -1, 0, NULL, NULL, NULL, NULL);
	g_return_if_fail (parse_ret);

	g_free (priv->font);
	priv->font = g_strdup (font);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_FONT]);
}

void
hex_view_set_blink_cursor (HexView *self, gboolean blink)
{
	HexViewPrivate *priv = hex_view_get_instance_private (self);

	priv->blink_cursor = blink;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_BLINK_CURSOR]);
}

gboolean
hex_view_get_blink_cursor (HexView *self)
{
	HexViewPrivate *priv = hex_view_get_instance_private (self);

	return priv->blink_cursor;
}

/* transfer none */
void
hex_view_set_selection (HexView *self, HexSelection *selection)
{
	HexViewPrivate *priv = hex_view_get_instance_private (self);

	g_clear_object (&priv->selection);

	if (selection)
	{
		g_signal_connect_object (hex_selection_get_highlight (selection), "changed", G_CALLBACK(gtk_widget_queue_draw), self, G_CONNECT_SWAPPED);

		priv->selection =  g_object_ref (selection);
	}

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_SELECTION]);
}

/* transfer none */
HexSelection *
hex_view_get_selection (HexView *self)
{
	HexViewPrivate *priv = hex_view_get_instance_private (self);

	return priv->selection;
}

/* transfer none */
void
hex_view_set_marks (HexView *self, GListModel *marks)
{
	HexViewPrivate *priv = hex_view_get_instance_private (self);

	g_clear_object (&priv->marks);

	if (marks && g_list_model_get_item_type (marks) == HEX_TYPE_MARK)
	{
		priv->marks =  g_object_ref (marks);
	}

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_MARKS]);
}

/* transfer none */
GListModel *
hex_view_get_marks (HexView *self)
{
	HexViewPrivate *priv = hex_view_get_instance_private (self);

	return priv->marks;
}

/* transfer none */
void
hex_view_set_auto_highlights (HexView *self, GListModel *auto_highlights)
{
	HexViewPrivate *priv;
	
	g_return_if_fail (HEX_IS_VIEW (self));
	g_return_if_fail (G_IS_LIST_MODEL (auto_highlights) && g_list_model_get_item_type (auto_highlights) == HEX_TYPE_AUTO_HIGHLIGHT);

	priv = hex_view_get_instance_private (self);

	g_clear_object (&priv->auto_highlights);

	priv->auto_highlights =  g_object_ref (auto_highlights);

	g_signal_connect_object (priv->auto_highlights, "items-changed", G_CALLBACK(gtk_widget_queue_draw), self, G_CONNECT_SWAPPED);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_AUTO_HIGHLIGHTS]);
}

/* transfer none */
GListModel *
hex_view_get_auto_highlights (HexView *self)
{
	HexViewPrivate *priv = hex_view_get_instance_private (self);

	return priv->auto_highlights;
}

int
hex_view_get_n_vis_lines (HexView *self)
{
	HexViewPrivate *priv;
	int retval;

	g_return_val_if_fail (HEX_IS_VIEW (self), 0);

	priv = hex_view_get_instance_private (self);

	retval = gtk_adjustment_get_page_size (priv->vadj) / HEX_ADJ_PIXEL_MULTIPLIER;

	return retval;
}

void
hex_view_set_insert_mode (HexView *self, gboolean insert_mode)
{
	HexViewPrivate *priv;

	g_return_if_fail (HEX_IS_VIEW (self));

	priv = hex_view_get_instance_private (self);

	priv->insert_mode = insert_mode;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_INSERT_MODE]);
}

gboolean
hex_view_get_insert_mode (HexView *self)
{
	HexViewPrivate *priv;

	g_return_val_if_fail (HEX_IS_VIEW (self), FALSE);

	priv = hex_view_get_instance_private (self);

	return priv->insert_mode;
}

/* HexAutoHighlight operations */

static void
refresh_ready_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	HexView *self = user_data;
	HexViewPrivate *priv;
	gboolean retval;

	/* We assume ownership here so we need to transfer ownership or destroy it when done */
	g_autoptr(HexAutoHighlight) auto_highlight = (HexAutoHighlight *) source_object;

	g_assert (HEX_IS_VIEW (self));
	g_assert (HEX_IS_AUTO_HIGHLIGHT (auto_highlight));

	priv = hex_view_get_instance_private (self);

	retval = hex_auto_highlight_refresh_finish (auto_highlight, res);

	if (retval)
	{
		g_list_store_append (G_LIST_STORE(priv->auto_highlights), auto_highlight);

		g_debug ("%s: search finished", __func__);
	}
	else
	{
		g_debug ("%s: search cancelled", __func__);
	}

}

static void
ahl_refresh_complete_cb (HexView *self, HexAutoHighlight *auto_highlight)
{
	HexViewPrivate *priv = hex_view_get_instance_private (self);
	guint ahl_pos;

	/* Emit an ::items-changed signal to signify that an individual item has
	 * changed *within* the list, to force a redraw. Note that this is somewhat
	 * of an abuse of the GListModel API, since TFM states that the
	 * ::items-changed signal is only emitted when an item is added/removed
	 * from the list. Whoopsie-doodle.
	 *
	 * FIXME/TODO: Do we want our own custom listmodel with its own refresh
	 * signal? Need to decide that before finalizing this API.
	 */
	if (g_list_store_find (G_LIST_STORE(priv->auto_highlights), auto_highlight, &ahl_pos))
	{
		g_list_model_items_changed (priv->auto_highlights, ahl_pos, 0, 0);
	}
}

void
hex_view_insert_auto_highlight (HexView *self, HexAutoHighlight *auto_highlight)
{
	HexViewPrivate *priv;

	g_return_if_fail (HEX_IS_VIEW (self));
	g_return_if_fail (HEX_IS_AUTO_HIGHLIGHT (auto_highlight));

	g_signal_connect_object (auto_highlight, "refresh-complete", G_CALLBACK(ahl_refresh_complete_cb), self, G_CONNECT_SWAPPED); 

	hex_auto_highlight_refresh_async (auto_highlight, g_cancellable_new (), refresh_ready_cb, self);
}

gboolean
hex_view_remove_auto_highlight (HexView *self, HexAutoHighlight *auto_highlight)
{
	guint pos;
	HexViewPrivate *priv;

	g_return_val_if_fail (HEX_IS_VIEW (self), FALSE);
	g_return_val_if_fail (HEX_IS_AUTO_HIGHLIGHT (auto_highlight), FALSE);

	priv = hex_view_get_instance_private (self);

	if (! g_list_store_find (G_LIST_STORE(priv->auto_highlights), auto_highlight, &pos))
		return FALSE;

	g_list_store_remove (G_LIST_STORE(priv->auto_highlights), pos);
	return TRUE;
}

void
hex_view_clear_auto_highlights (HexView *self)
{
	HexViewPrivate *priv;

	g_return_if_fail (HEX_IS_VIEW (self));

	priv = hex_view_get_instance_private (self);

	g_list_store_remove_all (G_LIST_STORE(priv->auto_highlights));
}

static void
hex_view_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexView *self = HEX_VIEW(object);
	HexViewPrivate *priv = hex_view_get_instance_private (self);

	switch (property_id)
	{
		case PROP_DOCUMENT:
			hex_view_set_document (self, g_value_get_object (value));
			break;

		case PROP_CPL:
			hex_view_set_cpl (self, g_value_get_int (value));
			break;
			
		case PROP_AUTO_GEOMETRY:
			hex_view_set_auto_geometry (self, g_value_get_boolean (value));
			break;

		case PROP_FONT:
			hex_view_set_font (self, g_value_get_string (value));
			break;

		case PROP_BLINK_CURSOR:
			hex_view_set_blink_cursor (self, g_value_get_boolean (value));
			break;

		case PROP_SELECTION:
			hex_view_set_selection (self, g_value_get_object (value));
			break;

		case PROP_MARKS:
			hex_view_set_marks (self, g_value_get_object (value));
			break;

		case PROP_AUTO_HIGHLIGHTS:
			hex_view_set_auto_highlights (self, g_value_get_object (value));
			break;

		case PROP_INSERT_MODE:
			hex_view_set_insert_mode (self, g_value_get_boolean (value));
			break;

		/* GtkScrollable */

		case PROP_VADJUSTMENT:
			hex_view_set_vadjustment (self, g_value_get_object (value));
			break;

		case PROP_HADJUSTMENT:
			break;

		case PROP_VSCROLL_POLICY:
			priv->vscroll_policy = g_value_get_enum (value);
			break;

		case PROP_HSCROLL_POLICY:
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_view_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexView *self = HEX_VIEW(object);
	HexViewPrivate *priv = hex_view_get_instance_private (self);

	switch (property_id)
	{
		case PROP_DOCUMENT:
			g_value_set_object (value, hex_view_get_document (self));
			break;

		case PROP_CPL:
			g_value_set_int (value, hex_view_get_cpl (self));
			break;
			
		case PROP_AUTO_GEOMETRY:
			g_value_set_boolean (value, hex_view_get_auto_geometry (self));
			break;

		case PROP_FONT:
			g_value_set_string (value, hex_view_get_font (self));
			break;

		case PROP_BLINK_CURSOR:
			g_value_set_boolean (value, hex_view_get_blink_cursor (self));
			break;

		case PROP_SELECTION:
			g_value_set_object (value, hex_view_get_selection (self));
			break;

		case PROP_MARKS:
			g_value_set_object (value, hex_view_get_marks (self));
			break;

		case PROP_AUTO_HIGHLIGHTS:
			g_value_set_object (value, hex_view_get_auto_highlights (self));
			break;

		case PROP_INSERT_MODE:
			g_value_set_boolean (value, hex_view_get_insert_mode (self));
			break;

		case PROP_N_VIS_LINES:
			g_value_set_int (value, hex_view_get_n_vis_lines (self));
			break;

		/* GtkScrollable */

		case PROP_VADJUSTMENT:
			g_value_set_object (value, priv->vadj);
			break;

		case PROP_HADJUSTMENT:
			g_value_set_object (value, NULL);
			break;

		case PROP_VSCROLL_POLICY:
			g_value_set_enum (value, priv->vscroll_policy);
			break;

		case PROP_HSCROLL_POLICY:
			g_value_set_enum (value, 0);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_view_dispose (GObject *object)
{
	HexView *self = HEX_VIEW(object);
	HexViewPrivate *priv = hex_view_get_instance_private (self);

	g_clear_object (&priv->selection);
	g_clear_object (&priv->marks);

	/* Chain up */
	G_OBJECT_CLASS(hex_view_parent_class)->dispose (object);
}

static void
hex_view_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS(hex_view_parent_class)->finalize (object);
}

static void
hex_view_class_init (HexViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose = hex_view_dispose;
	object_class->finalize = hex_view_finalize;
	object_class->set_property = hex_view_set_property;
	object_class->get_property = hex_view_get_property;

	properties[PROP_DOCUMENT] = g_param_spec_object ("document", NULL, NULL,
			HEX_TYPE_DOCUMENT,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	// FIXME - should this default to non-zero?
	properties[PROP_CPL] = g_param_spec_int ("cpl", NULL, NULL,
			0, 10000, 2,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_AUTO_GEOMETRY] = g_param_spec_boolean ("auto-geometry", NULL, NULL,
			TRUE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_FONT] = g_param_spec_string ("font", NULL, NULL,
			"monospace 10",
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_BLINK_CURSOR] = g_param_spec_boolean ("blink-cursor", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_SELECTION] = g_param_spec_object ("selection", NULL, NULL,
			HEX_TYPE_SELECTION,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_MARKS] = g_param_spec_object ("marks", NULL, NULL,
			G_TYPE_LIST_MODEL,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_AUTO_HIGHLIGHTS] = g_param_spec_object ("auto-highlights", NULL, NULL,
			G_TYPE_LIST_MODEL,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_N_VIS_LINES] = g_param_spec_int ("n-vis-lines", NULL, NULL,
			0, INT_MAX, 0,
			default_flags | G_PARAM_READABLE);

	properties[PROP_INSERT_MODE] = g_param_spec_boolean ("insert-mode", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	/* GtkScrollableInterface properties */

	g_object_class_override_property (object_class, PROP_VADJUSTMENT, "vadjustment");
	g_object_class_override_property (object_class, PROP_HADJUSTMENT, "hadjustment");
	g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");
	g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
}

static void
hex_view_init (HexView *self)
{
	/* Set up a dummy selection - this property will almost certainly be subsequently bound to its parent's */
	{
		g_autoptr(HexSelection) selection = hex_view_selection_new (self);

		hex_view_set_selection (self, selection);
	}

	/* Set up a dummy marks model - this property will almost certainly be subsequently bound to its parent's */
	{
		g_autoptr(GListStore) store = g_list_store_new (HEX_TYPE_MARK);
		hex_view_set_marks (self, G_LIST_MODEL(store));
	}

	{
		g_autoptr(GListStore) store = g_list_store_new (HEX_TYPE_AUTO_HIGHLIGHT);
		hex_view_set_auto_highlights (self, G_LIST_MODEL(store));
	}
}

/* <Marks> */

/**
 * hex_view_add_mark:
 * @start: The start offset of the mark
 * @end: The start offset of the mark
 * @color: (nullable): A custom color to set for the mark, or `NULL` to use the
 *   default
 *
 * Add a mark for a `HexView` object at the specified absolute `start` and
 * `end` offsets.
 *
 * Although the mark obtains an index within the widget internally, this index
 * numeral is private and is not retrievable. As a result, it is recommended
 * that applications wishing to manipulate marks retain the pointer returned by
 * this function, and implement their own tracking mechanism for the marks.
 *
 * Returns: (transfer none): A pointer to a [class@Hex.Mark] object, owned by
 * the `HexView`.
 */
HexMark *
hex_view_add_mark (HexView *self, gint64 start, gint64 end, GdkRGBA *color)
{
	HexViewPrivate *priv;
	GListModel *marks;
	g_autoptr(HexMark) mark = NULL;

	g_return_val_if_fail (HEX_IS_VIEW (self), NULL);

	priv = hex_view_get_instance_private (self);

	mark = hex_mark_new ();
	mark->highlight->start_offset = start;
	mark->highlight->end_offset = end;

	if (color)
		hex_mark_set_custom_color (mark, color);

	g_list_store_append (G_LIST_STORE(priv->marks), mark);

	return mark;
}

/**
 * hex_view_delete_mark:
 * @mark: The [class@Hex.Mark] to delete
 *
 * Delete a mark from the internal list held by the `HexView`.
 */
void
hex_view_delete_mark (HexView *self, HexMark *mark)
{
	HexViewPrivate *priv;
	guint pos;

	g_return_if_fail (HEX_IS_VIEW (self));
	g_return_if_fail (HEX_IS_MARK (mark));

	priv = hex_view_get_instance_private (self);

	if (! g_list_store_find (G_LIST_STORE(priv->marks), mark, &pos))
		return;

	g_list_store_remove (G_LIST_STORE(priv->marks), pos);

	gtk_widget_queue_draw (GTK_WIDGET(self));
}

/**
 * hex_view_goto_mark:
 * @mark: The mark to jump to
 *
 * Jump the cursor to the mark in question.
 */
void
hex_view_goto_mark (HexView *self, HexMark *mark)
{
	HexViewPrivate *priv;

	g_return_if_fail (HEX_IS_VIEW (self));
	g_return_if_fail (HEX_IS_MARK (mark));
	
	priv = hex_view_get_instance_private (self);

	hex_selection_collapse (priv->selection, mark->highlight->start_offset);
}

/* </Marks> */
