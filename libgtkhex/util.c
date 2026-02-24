// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-util"

#include "util.h"
#include "hex-selection.h"

/* <UtilTuple> */

UtilTuple *
util_tuple_new (void)
{
	UtilTuple *self = g_new0 (UtilTuple, 1);
	return self;
}

void
util_tuple_destroy (UtilTuple *self)
{
	for (guint i = 0; i < G_N_ELEMENTS (self->data); ++i)
	{
		if (self->data[i] && self->destroy[i]) {
			g_clear_pointer (&self->data[i], self->destroy[i]);
		}
	}

	g_free (self);
}

/* </UtilTuple> */

void
util_warn_invalid_enum (const char *desc, GType enum_type, int enum_value)
{
	g_autofree char *enum_prettyname = g_enum_to_string (enum_type, enum_value);
	g_autofree char *warning_msg = g_strdup_printf ("Invalid %s: %s",
			desc ? desc : "parameter",
			enum_prettyname);

	g_warning ("%s", warning_msg);
}

/* These 2 functions are 'generic' enough to delete or zero a selection for
 * any object that has a :document and :selection property.
 *
 * FIXME/TODO: Pretty sure this is no longer necessary since both are now
 * properties of the HexView abstract class.
 */
gboolean
util_delete_selection_in_doc (gpointer source_object)
{
	g_autoptr(HexDocument) doc = NULL;
	g_autoptr(HexSelection) selection = NULL;
	g_autoptr(HexHighlight) highlight = NULL;
	gint64 cursor_pos = -1;
	gint64 start_offset;
	gsize len;

	g_return_val_if_fail (G_IS_OBJECT (source_object), FALSE);

	g_object_get (source_object,
			"document", &doc,
			"selection", &selection,
			NULL);

	g_return_val_if_fail (HEX_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (HEX_IS_SELECTION (selection), FALSE);

	g_object_get (selection,
			"highlight", &highlight,
			"cursor-pos", &cursor_pos,
			NULL);

	g_return_val_if_fail (HEX_IS_HIGHLIGHT (highlight), FALSE);
	g_return_val_if_fail (cursor_pos != -1, FALSE);

	len = hex_highlight_get_n_selected (highlight);
	g_return_val_if_fail (len, FALSE);

	/* --- */

	start_offset = hex_selection_get_start_offset (selection);

	hex_document_delete_data (doc, start_offset, len, TRUE);
	hex_selection_collapse (selection, start_offset);

	return TRUE;
}

// FIXME - move to HexDocument
static char *zeros;

gboolean
util_zero_selection_in_doc (gpointer source_object)
{
	g_autoptr(HexDocument) doc = NULL;
	g_autoptr(HexSelection) selection = NULL;
	g_autoptr(HexHighlight) highlight = NULL;
	gint64 cursor_pos = -1;
	gsize len = 0;
	gsize written = 0;
	gint64 i;

	g_return_val_if_fail (G_IS_OBJECT (source_object), FALSE);

	g_object_get (source_object,
			"document", &doc,
			"selection", &selection,
			NULL);

	g_return_val_if_fail (HEX_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (HEX_IS_SELECTION (selection), FALSE);

	g_object_get (selection,
			"highlight", &highlight,
			"cursor-pos", &cursor_pos,
			NULL);

	g_return_val_if_fail (HEX_IS_HIGHLIGHT (highlight), FALSE);
	g_return_val_if_fail (cursor_pos != -1, FALSE);

	len = hex_highlight_get_n_selected (highlight);
	g_return_val_if_fail (len, FALSE);

	/* --- */

	if (! zeros)
		zeros = g_malloc0 (512);

	i = hex_selection_get_start_offset (selection);

	while (written < len)
	{
		size_t batch_size = len-written < 512 ? len-written : 512;

		hex_document_set_data (doc,
				i, batch_size, batch_size,
				zeros, TRUE);

		i += batch_size;
		written += batch_size;
	}

	hex_selection_collapse (selection, cursor_pos);

	return TRUE;
}

/* All-purpose GObject tranform_to function that converts the non-NULL value of
 * an object to 'TRUE'.
 */
gboolean
util_have_object_transform_to (GBinding *binding,
		const GValue *from_value /* source = GObject */,
		GValue *to_value /* target = gboolean */,
		gpointer data) /* ignored */
{
	GObject *from = g_value_get_object (from_value);
	g_value_set_boolean (to_value, from ? TRUE : FALSE);
	return TRUE;
}

/* Transform a GFile to a document title string */
gboolean
file_title_transform_to (GBinding *binding, const GValue *from_value, GValue *to_value, gpointer data)
{
	GFile *file = g_value_get_object (from_value);
	g_autofree char *str = NULL;

	if (file)
		str = g_file_get_basename (file);
	else
		str = g_strdup (_("Untitled"));

	g_value_set_string (to_value, str);

	return TRUE;
}

/* transfer: full */
char *
util_gdk_rgba_to_hex_color (const GdkRGBA *color)
{
	char *retval = NULL;

	g_assert (color != NULL);

	retval = g_strdup_printf ("#%02X%02X%02X",
			(int) (color->red	* 255),
			(int) (color->green	* 255),
			(int) (color->blue	* 255)
			);

	return retval;
}
