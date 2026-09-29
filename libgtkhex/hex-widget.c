// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-widget"

#include "hex-widget.h"

#include "hex-private-common.h"
#include "gtkhex-paste-data.h"
#include "hex-highlight-private.h"
#include "hex-auto-highlight-private.h"
#include "hex-text-offsets.h"
#include "util.h"

#include "libgtkhex-enums.h"

static gboolean hex_widget_get_can_undo (HexWidget *self);
static gboolean hex_widget_get_can_redo (HexWidget *self);

enum
{
	PROP_0,
	PROP_CAN_UNDO,
	PROP_CAN_REDO,
	PROP_SHOW_OFFSETS,
	PROP_SHOW_HEX,
	PROP_SHOW_ASCII,
	PROP_HEX_CPL,
	PROP_AUTO_GEOMETRY_CPL,
	PROP_GROUP_TYPE,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _HexWidget
{
	HexView parent_instance;

	GBinding *auto_geometry_binding;
	gboolean can_undo;
	gboolean can_redo;
	int hex_cpl;
	int auto_geo_cpl;
	HexGroupType group_type;
	
	/* From template */
	GtkWidget *offsets;
	GtkWidget *xdisp;
	GtkWidget *adisp;
};

G_DEFINE_FINAL_TYPE (HexWidget, hex_widget, HEX_TYPE_VIEW)

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
	GListModel *change_list;
	guint n_items;
	g_autoptr(HexChangeData) last_change = NULL;

	g_assert (HEX_IS_DOCUMENT (document));
	g_assert (hex_document_get_can_undo (document));

	change_list = hex_document_get_undo_data (document);
	if ((n_items = g_list_model_get_n_items (change_list)) > 0)
		last_change = g_list_model_get_item (change_list, n_items - 1);

	hex_document_undo (document);

	hex_selection_collapse (selection, hex_change_data_get_start_offset (last_change));
}

static void
redo_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexWidget *self = user_data;
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	GListModel *change_list;
	guint n_items;
	g_autoptr(HexChangeData) last_change = NULL;

	g_assert (HEX_IS_DOCUMENT (document));
	g_assert (hex_document_get_can_redo (document));

	hex_document_redo (document);

	change_list = hex_document_get_undo_data (document);
	if ((n_items = g_list_model_get_n_items (change_list)) > 0)
		last_change = g_list_model_get_item (change_list, n_items - 1);

	hex_selection_collapse (selection, hex_change_data_get_start_offset (last_change));
}

/* --- */

static void
document_set_cb (HexWidget *self, GParamSpec *pspec, gpointer user_data)
{
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));

	g_object_bind_property (document, "can-undo", self, "can-undo", G_BINDING_SYNC_CREATE);
	g_object_bind_property (document, "can-redo", self, "can-redo", G_BINDING_SYNC_CREATE);
}

static void
auto_geometry_set_cb (HexWidget *self, GParamSpec *pspec, gpointer user_data)
{
	gboolean auto_geometry = hex_view_get_auto_geometry (HEX_VIEW(self));

	g_clear_object (&self->auto_geometry_binding);

	if (auto_geometry)
	{
		self->auto_geometry_binding = g_object_bind_property (self, "auto-geometry-cpl", self, "cpl", G_BINDING_SYNC_CREATE);
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

void
hex_widget_set_show_offsets (HexWidget *self, gboolean show)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	if (show != hex_widget_get_show_offsets (self))
		gtk_widget_set_visible (self->offsets, show);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_SHOW_OFFSETS]);
}

gboolean
hex_widget_get_show_offsets (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), TRUE);

	return gtk_widget_get_visible (self->offsets);
}

void
hex_widget_set_show_hex (HexWidget *self, gboolean show)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	if (show != hex_widget_get_show_hex (self))
		gtk_widget_set_visible (self->xdisp, show);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_SHOW_HEX]);
}

gboolean
hex_widget_get_show_hex (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), TRUE);

	return gtk_widget_get_visible (self->xdisp);
}

void
hex_widget_set_show_ascii (HexWidget *self, gboolean show)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	if (show != hex_widget_get_show_ascii (self))
		gtk_widget_set_visible (self->adisp, show);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_SHOW_ASCII]);
}

gboolean
hex_widget_get_show_ascii (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), TRUE);

	return gtk_widget_get_visible (self->adisp);
}

int
hex_widget_get_hex_cpl (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), 20);

	return self->hex_cpl;
}

int
hex_widget_get_auto_geometry_cpl (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), 20);

	return self->auto_geo_cpl;
}

void
hex_widget_set_group_type (HexWidget *self, HexGroupType group_type)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	self->group_type = group_type;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_GROUP_TYPE]);
}

HexGroupType
hex_widget_get_group_type (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), HEX_GROUP_BYTE);

	return self->group_type;
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

		case PROP_SHOW_OFFSETS:
			hex_widget_set_show_offsets (self, g_value_get_boolean (value));
			break;

		case PROP_SHOW_HEX:
			hex_widget_set_show_hex (self, g_value_get_boolean (value));
			break;

		case PROP_SHOW_ASCII:
			hex_widget_set_show_ascii (self, g_value_get_boolean (value));
			break;

		case PROP_GROUP_TYPE:
			hex_widget_set_group_type (self, g_value_get_enum (value));
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
		case PROP_CAN_UNDO:
			g_value_set_boolean (value, hex_widget_get_can_undo (self));
			break;

		case PROP_CAN_REDO:
			g_value_set_boolean (value, hex_widget_get_can_redo (self));
			break;

		case PROP_SHOW_OFFSETS:
			g_value_set_boolean (value, hex_widget_get_show_offsets (self));
			break;

		case PROP_SHOW_HEX:
			g_value_set_boolean (value, hex_widget_get_show_hex (self));
			break;

		case PROP_SHOW_ASCII:
			g_value_set_boolean (value, hex_widget_get_show_ascii (self));
			break;

		case PROP_HEX_CPL:
			g_value_set_int (value, hex_widget_get_hex_cpl (self));
			break;

		case PROP_AUTO_GEOMETRY_CPL:
			g_value_set_int (value, hex_widget_get_auto_geometry_cpl (self));
			break;

		case PROP_GROUP_TYPE:
			g_value_set_enum (value, hex_widget_get_group_type (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static gboolean
hex_widget_focus (GtkWidget *widget, GtkDirectionType dir)
{
	HexWidget *self = HEX_WIDGET(widget);

	switch (dir)
	{
		case GTK_DIR_TAB_BACKWARD:
			if (gtk_widget_is_focus (self->xdisp))
				return FALSE;
			else if (gtk_widget_is_focus (self->adisp))
				return gtk_widget_grab_focus (self->xdisp);
			break;

		case GTK_DIR_TAB_FORWARD:
			if (gtk_widget_is_focus (self->adisp))
				return FALSE;
			else if (gtk_widget_is_focus (self->xdisp))
				return gtk_widget_grab_focus (self->adisp);
			break;

		default:
			break;
	}

	return GTK_WIDGET_CLASS (hex_widget_parent_class)->focus (widget, dir);
}

static gboolean
hex_widget_grab_focus (GtkWidget *widget)
{
	HexWidget *self = HEX_WIDGET(widget);

	if (hex_widget_get_show_hex (self) && gtk_widget_grab_focus (self->xdisp))
		return TRUE;
	else if (hex_widget_get_show_ascii (self) && gtk_widget_grab_focus (self->adisp))
		return TRUE;

	return GTK_WIDGET_CLASS (hex_widget_parent_class)->grab_focus (widget);
}

static void
hex_widget_measure (GtkWidget *widget, GtkOrientation orientation, int for_size, int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
	GtkWidget *child;
	int minimum_size = 0;
	int natural_size = 0;

	for (child = gtk_widget_get_first_child (widget);
			child != NULL;
			child = gtk_widget_get_next_sibling (child))
	{
		int child_min = 0, child_nat = 0;

		if (!gtk_widget_should_layout (child))
			continue;

		gtk_widget_measure (child, orientation,
				/* for-size: */ -1,		/* == unknown. */
				&child_min, &child_nat,
				NULL, NULL);
		minimum_size = MAX (minimum_size, child_min);
		natural_size = MAX (natural_size, child_nat);
	}

	if (minimum != NULL)
		*minimum = minimum_size;
	if (natural != NULL)
		*natural = natural_size;
}

#define BASE_ALLOC (GtkAllocation){.x = 0, .y = 0, .width = 0, .height = full_height}

static void
hex_widget_size_allocate (GtkWidget *widget, int full_width, int full_height, int baseline)
{
	HexWidget *self = HEX_WIDGET(widget);
	GtkWidget *child;
	GtkAllocation off_alloc = BASE_ALLOC;
	GtkAllocation hex_alloc = BASE_ALLOC;
	GtkAllocation asc_alloc = BASE_ALLOC;
	GtkWidget *hex = NULL, *ascii = NULL, *offsets = NULL;
	int tmp_auto_cpl = 0;
	const int char_width = hex_view_get_char_width (HEX_VIEW(self));

	g_return_if_fail (char_width != 0);

	for (child = gtk_widget_get_first_child (widget);
			child != NULL;
			child = gtk_widget_get_next_sibling (child))
	{
		if (! gtk_widget_should_layout (child))
			continue;

		/* Setup allocation depending on what column we're in.
		 * This loop is run through again once we obtain some initial values. */

		if (HEX_IS_TEXT_OFFSETS (child))
			offsets = child;
		else if (HEX_IS_TEXT_HEX (child))
			hex = child;
		else if (HEX_IS_TEXT_ASCII (child))
			ascii = child;
		else
		{
			GtkRequisition child_req = {0};
			GtkAllocation alloc = BASE_ALLOC;

			g_debug ("%s: unexpected child: %s - %p", __func__, G_OBJECT_TYPE_NAME (child), (void *) child);

			/* just position the widget in the centre at its preferred
			 * size. TODO: check v/halign and v/hexand
			 */
			gtk_widget_get_preferred_size (child, &child_req, NULL);
			alloc.width = child_req.width;
			alloc.height = child_req.height;
			alloc.x = (full_width / 2) - (alloc.width / 2);
			alloc.y = (full_height / 2) - (alloc.height / 2);
			gtk_widget_size_allocate (child, &alloc, -1);
		}
	}

	/* Order doesn't really matter for the offsets column since
	 * it's essentially fixed, so let's do that first.
	 */
	if (offsets)
	{
		int min, nat;

		gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1, &min, &nat, NULL, NULL);

		g_debug ("%s: min: %d - nat: %d", __func__, min, nat);

		/* offsets always goes at x coordinate 0 so just leave it as it's
		 * zeroed out anyway. */

		off_alloc.width = min;
	}

	/* Let's measure ascii next, as hex's width is essentially locked to it, if
	 * it's visible. Since hex and ascii are both drawing areas, they both
	 * default to preferring a width of 0, so we have to measure completely.
	 */
	if (ascii)
	{
		int ascii_max_width;

		/* Use width of offsets (if any) as baseline for x posn of ascii and
		 * go from there.
		 */
		asc_alloc.x = off_alloc.width;

		ascii_max_width = asc_alloc.width = full_width - off_alloc.width;

		tmp_auto_cpl = asc_alloc.width / char_width - 1;
		self->hex_cpl = 0;

		if (hex)
		{
			const int max_width_left = ascii_max_width;
			int tot_cpl, hex_cpl, ascii_cpl;

			tot_cpl = max_width_left / char_width;

			/* Calculate how many hex vs. ascii characters can be stuffed
			 * on one line.
			 */
			ascii_cpl = 0;
			do {
				if (ascii_cpl % self->group_type == 0 &&
						tot_cpl < self->group_type * 3)
					break;
		
				++ascii_cpl;
				tot_cpl -= 3;   /* 2 for hex disp, 1 for ascii disp */
		
				if (ascii_cpl % self->group_type == 0) /* just ended a group */
					tot_cpl--;
			}
			while (tot_cpl > 0);

			hex_cpl = util_hex_cpl_from_ascii_cpl (ascii_cpl, self->group_type);

			asc_alloc.width = ascii_cpl * char_width;
			hex_alloc.width = max_width_left - asc_alloc.width;

			hex_alloc.x = off_alloc.width;
			asc_alloc.x = hex_alloc.x + hex_alloc.width;

			self->hex_cpl = hex_cpl;
			tmp_auto_cpl = ascii_cpl;
		}
	}

	/* Already determined what to do if have ascii and hex together */
	if (hex && !ascii)
	{
		hex_alloc.x = off_alloc.width;

		hex_alloc.width = full_width - off_alloc.width;

		self->hex_cpl = hex_alloc.width / char_width;

		tmp_auto_cpl = (hex_alloc.width / char_width) / 3;

		self->hex_cpl = util_hex_cpl_from_ascii_cpl (tmp_auto_cpl, self->group_type);
	}

	if (offsets)
		gtk_widget_size_allocate (offsets, &off_alloc, -1);
	if (hex)
		gtk_widget_size_allocate (hex, &hex_alloc, -1);
	if (ascii)
		gtk_widget_size_allocate (ascii, &asc_alloc, -1);

	self->auto_geo_cpl = tmp_auto_cpl;
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_AUTO_GEOMETRY_CPL]);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_HEX_CPL]);

	GTK_WIDGET_CLASS(hex_widget_parent_class)->size_allocate (widget, full_width, full_height, baseline);
}
#undef BASE_ALLOC

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

	/* We need the object fully constructed before we bind the font properties so
	 * it can't be done in the ui file.
	 */
	g_object_bind_property (self, "font", self->adisp, "font", G_BINDING_SYNC_CREATE);
	g_object_bind_property (self, "font", self->xdisp, "font", G_BINDING_SYNC_CREATE);
	g_object_bind_property (self, "font", self->offsets, "font", G_BINDING_SYNC_CREATE);

	// FIXME - not sure if we still need this here or if it can be in the .ui file
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

	widget_class->focus = hex_widget_focus;
	widget_class->grab_focus = hex_widget_grab_focus;
	widget_class->size_allocate = hex_widget_size_allocate;
	widget_class->measure = hex_widget_measure;

	/* TEMPLATE */

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libgtkhex/ui/hex-widget.ui");

	gtk_widget_class_bind_template_child (widget_class, HexWidget, offsets);
	gtk_widget_class_bind_template_child (widget_class, HexWidget, xdisp);
	gtk_widget_class_bind_template_child (widget_class, HexWidget, adisp);

	/* PROPERTIES */

	properties[PROP_CAN_UNDO] = g_param_spec_boolean ("can-undo", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_CAN_REDO] = g_param_spec_boolean ("can-redo", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_SHOW_OFFSETS] = g_param_spec_boolean ("show-offsets", NULL, NULL,
			TRUE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_SHOW_HEX] = g_param_spec_boolean ("show-hex", NULL, NULL,
			TRUE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_SHOW_ASCII] = g_param_spec_boolean ("show-ascii", NULL, NULL,
			TRUE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_HEX_CPL] = g_param_spec_int ("hex-cpl", NULL, NULL,
			0, 10000, 20,
			default_flags | G_PARAM_READABLE);

	properties[PROP_AUTO_GEOMETRY_CPL] = g_param_spec_int ("auto-geometry-cpl", NULL, NULL,
			0, 10000, 20,
			default_flags | G_PARAM_READABLE);

	properties[PROP_GROUP_TYPE] = g_param_spec_enum ("group-type", NULL, NULL,
			HEX_TYPE_GROUP_TYPE,
			HEX_GROUP_BYTE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

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

	gtk_widget_set_focusable (GTK_WIDGET(self), TRUE);

	g_signal_connect (self, "notify::document", G_CALLBACK(document_set_cb), NULL);
	g_signal_connect (self, "notify::auto-geometry", G_CALLBACK(auto_geometry_set_cb), NULL);

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
}

GtkWidget *
hex_widget_new (void)
{
	return g_object_new (HEX_TYPE_WIDGET, NULL);
}

HexTextHex *
hex_widget_get_hex_display (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), NULL);

	return HEX_TEXT_HEX(self->xdisp);
}

HexTextAscii *
hex_widget_get_ascii_display (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), NULL);

	return HEX_TEXT_ASCII(self->adisp);
}
