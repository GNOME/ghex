// vim: ts=4 sw=4 breakindent breakindentopt=shift\:4

/* ghex-clipboard.c - Clipboard dialogs for GHex

   Copyright © 2021-2026 Logan Rathbone <poprocks@gmail.com>
   Copyright © 2025 Dilnavas Roshan <dilnavasroshan@gmail.com>

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

#include "ghex-clipboard.h"

#include "common-ui.h"

#include <glib/gi18n.h>

#include "config.h"

#define GHEX_CLIPBOARD_ERROR (ghex_clipboard_error_quark ())
GQuark ghex_clipboard_error_quark (void);
G_DEFINE_QUARK (ghex-clipboard-error-quark, ghex_clipboard_error)

typedef enum {
	GHEX_CLIPBOARD_ERROR_UNEXPECTED_END,
	GHEX_CLIPBOARD_ERROR_INVALID_DIGIT
} GHexClipboardErrorCode;

typedef enum {
	GHEX_MIME_TYPE_NONE,
	GHEX_MIME_TYPE_PLAINTEXT,
	GHEX_MIME_TYPE_UTF_PLAINTEXT,
	GHEX_MIME_TYPE_LAST
} GHexMimeType;

typedef enum {
	GHEX_MIME_SUB_TYPE_NONE,
	GHEX_MIME_SUB_TYPE_ASCII_PLAINTEXT,
	GHEX_MIME_SUB_TYPE_HEX_PLAINTEXT,
} GHexMimeSubType;

typedef struct {
	char *mime_type;
	char *pretty_name;
	GHexMimeSubType sub_type;
} GHexKnownMimeSubType;

static GHashTable *MIME_HASH;
static GSList *KNOWN_MIME[GHEX_MIME_TYPE_LAST];

/* < GHexClipboardDialog > */

enum
{
	PROP_0,
	PROP_HEX,
	PROP_ACTION_BUTTON_LABEL,
	PROP_ACTION_NAME,
	GHEX_CLIPBOARD_DIALOG_N_PROPERTIES
};

static GParamSpec *ghex_clipboard_dialog_properties[GHEX_CLIPBOARD_DIALOG_N_PROPERTIES];

typedef struct
{
	HexWidget *hex;
	char *action_button_label;

	/* From template */
	GtkListBox *listbox;
	GtkWidget *action_button;
	GtkWidget *cancel_button;
} GHexClipboardDialogPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GHexClipboardDialog, ghex_clipboard_dialog, GTK_TYPE_WINDOW)

/* </ GHexClipboardDialog > */

/* < GHexPasteSpecialDialog > */

struct _GHexPasteSpecialDialog
{
	GHexClipboardDialog parent_instance;
};

G_DEFINE_FINAL_TYPE (GHexPasteSpecialDialog, ghex_paste_special_dialog, GHEX_TYPE_CLIPBOARD_DIALOG)

/* </ GHexPasteSpecialDialog > */

/* < GHexCopySpecialDialog > */

struct _GHexCopySpecialDialog
{
	GHexClipboardDialog parent_instance;
};

G_DEFINE_FINAL_TYPE (GHexCopySpecialDialog, ghex_copy_special_dialog, GHEX_TYPE_CLIPBOARD_DIALOG)

/* </ GHexCopySpecialDialog > */

/* < GHexMimeSubTypeLabel > */

#define GHEX_TYPE_MIME_SUB_TYPE_LABEL (ghex_mime_sub_type_label_get_type ())
G_DECLARE_FINAL_TYPE (GHexMimeSubTypeLabel, ghex_mime_sub_type_label, GHEX, MIME_SUB_TYPE_LABEL,
		GtkWidget)

struct _GHexMimeSubTypeLabel
{
	GtkWidget parent_instance;

	GtkWidget *label;
	GHexKnownMimeSubType *known_sub_type;
};

G_DEFINE_FINAL_TYPE (GHexMimeSubTypeLabel, ghex_mime_sub_type_label, GTK_TYPE_WIDGET)

static void
ghex_mime_sub_type_label_init (GHexMimeSubTypeLabel *self)
{
	self->label = gtk_label_new (NULL);
	gtk_widget_set_hexpand (self->label, TRUE);
	gtk_widget_set_halign (self->label, GTK_ALIGN_START);
	gtk_widget_set_parent (self->label, GTK_WIDGET(self));
}

static void
ghex_mime_sub_type_label_dispose (GObject *object)
{
	GHexMimeSubTypeLabel *self = GHEX_MIME_SUB_TYPE_LABEL (object);

	g_clear_pointer (&self->label, gtk_widget_unparent);

	G_OBJECT_CLASS (ghex_mime_sub_type_label_parent_class)->dispose (object);
}

static void
ghex_mime_sub_type_label_class_init (GHexMimeSubTypeLabelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = ghex_mime_sub_type_label_dispose;

	gtk_widget_class_set_layout_manager_type (widget_class,
			GTK_TYPE_BOX_LAYOUT);
}

static GtkWidget *
ghex_mime_sub_type_label_new (GHexKnownMimeSubType *known_sub_type)
{
	GHexMimeSubTypeLabel *self;

	g_return_val_if_fail (known_sub_type != NULL, NULL);
	g_return_val_if_fail (known_sub_type->pretty_name != NULL, NULL);

	self = g_object_new (GHEX_TYPE_MIME_SUB_TYPE_LABEL, NULL);

	self->known_sub_type = known_sub_type;
	gtk_label_set_text (GTK_LABEL(self->label), known_sub_type->pretty_name);

	return GTK_WIDGET(self);
}

/* </ GHexMimeSubTypeLabel > */

/* Note - I looked into reusing the hex block formatting code from HexWidget,
 * but honestly, it looks like it'd be more trouble than it's worth.
 */
/* Transfer: full */
static char *
hex_paste_data_to_delimited_hex (HexPasteData *hex_paste_data)
{
	g_autoptr(GString) buf = NULL;
	const char *doc_data = NULL;
	int elems;

	g_return_val_if_fail (HEX_IS_PASTE_DATA (hex_paste_data), NULL);

	buf = g_string_new (NULL);
	doc_data = hex_paste_data_get_doc_data (hex_paste_data);
	elems = hex_paste_data_get_elems (hex_paste_data);

	for (int i = 0; i < elems; ++i)
	{
		g_string_append_printf (buf, "%.2X", (guchar)doc_data[i]);

		if (i < elems - 1)
			g_string_append_c (buf, ' ');
	}

	return g_steal_pointer (&buf->str);
}

static int
to_hex_digit_value (gchar digit)
{
	digit = g_ascii_toupper (digit);
	if (g_ascii_isdigit (digit))
		return digit - '0';
	else if (g_ascii_isxdigit (digit))
		return 10 + digit - 'A';
	return -1;
}

static GString *
hex_string_to_bytes (const char   *hex_str,
                     GError      **error)
{
	g_autoptr(GString) byte_values = NULL;
	char value;
	char dig1, dig2;
	int dig1_val, dig2_val;

	byte_values = g_string_new (NULL);
	while (*hex_str)
	{
		/* The space chars can come in between bytes, but they are not
		 * allowed in between hex digits representing a single byte. */

		if (g_unichar_isspace (*hex_str))
		{
			hex_str++;
			continue;
		}

		dig1 = *hex_str;
		hex_str++;

		if (!*hex_str) /* The next char shouldn't be a null char. */
		{
			g_debug ("%s: unexpected end of text", __func__);
			g_set_error (error,
			             GHEX_CLIPBOARD_ERROR,
			             GHEX_CLIPBOARD_ERROR_UNEXPECTED_END,
			             _ ("Failed to paste. "
			                "Unexpected end of text"));
			return NULL;
		}

		dig2 = *hex_str;

		if ((dig1_val = to_hex_digit_value (dig1)) < 0 ||
		    (dig2_val = to_hex_digit_value (dig2)) < 0)
		{
			g_debug ("%s: invalid hex digit", __func__);
			g_set_error (error,
			             GHEX_CLIPBOARD_ERROR,
			             GHEX_CLIPBOARD_ERROR_INVALID_DIGIT,
			             _ ("Failed to paste. "
			                "The pasted string contains an invalid hex digit."));
			return NULL;
		}

		value = (dig1_val << 4) + dig2_val;
		g_string_append_printf (byte_values, "%c", value);

		hex_str++;
	}

	return g_steal_pointer (&byte_values);
}

static void
delimited_paste_received_cb (GObject *source_object,
		GAsyncResult *result,
		gpointer user_data)
{
	GHexClipboardDialog *self = user_data;
	GdkClipboard *clipboard = (GdkClipboard *) source_object;
	GHexClipboardDialogPrivate *priv;
	HexDocument *doc;
	HexSelection *selection;
	g_autofree char *text = NULL;
	g_autoptr(GString) buf = NULL;
	g_autoptr(GError) error = NULL;

	g_assert (GHEX_IS_CLIPBOARD_DIALOG (self));
	g_assert (GDK_IS_CLIPBOARD (clipboard));

	priv = ghex_clipboard_dialog_get_instance_private (self);
	g_assert (HEX_IS_WIDGET (priv->hex));

	/* Get the resulting text of the read operation */
	text = gdk_clipboard_read_text_finish (clipboard,
			result,
			NULL);	/* GError */

	buf = hex_string_to_bytes (text, &error);
	if (! buf)
	{
		GtkWindow *parent = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET(priv->hex)));

		g_debug ("%s: Received invalid delimeter string. Returning.", __func__);

		if (parent)
			ghex_display_dialog (parent, error->message);

		return;
	}

	doc = hex_view_get_document (HEX_VIEW(priv->hex));
	selection = hex_view_get_selection (HEX_VIEW(priv->hex));

	hex_document_set_data (doc,
			hex_selection_get_cursor_pos (selection),
			buf->len,
			0,	/* rep_len (0 to insert w/o replacing; what we want) */
			buf->str,
			TRUE);
}

static void
_ghex_clipboard_dialog_delimited_hex_paste (GHexClipboardDialog *self)
{
	GHexClipboardDialogPrivate *priv;
	GdkClipboard *clipboard;

	g_assert (GHEX_IS_CLIPBOARD_DIALOG (self));

	priv = ghex_clipboard_dialog_get_instance_private (self);

	if (!priv->hex)
		return;

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET(priv->hex));

	gdk_clipboard_read_text_async (clipboard, NULL, delimited_paste_received_cb, self);
}

static void
_ghex_clipboard_dialog_delimited_hex_copy (GHexClipboardDialog *self)
{
	GdkContentProvider *provider = NULL;
	GdkClipboard *clipboard = NULL;
	g_auto(GValue) value = G_VALUE_INIT;
	g_autoptr(GError) error = NULL;
	g_autofree char *hex_str = NULL;
	gboolean got_val = FALSE;
	HexPasteData *hex_paste_data = NULL;
	GHexClipboardDialogPrivate *priv;

	g_assert (GHEX_IS_CLIPBOARD_DIALOG (self));

	priv = ghex_clipboard_dialog_get_instance_private (self);

	if (!priv->hex)
		return;

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET(priv->hex));

	/* Save selection to clipboard as HexPasteData */
	hex_widget_copy_to_clipboard (priv->hex);

	provider = gdk_clipboard_get_content (clipboard);
	g_return_if_fail (GDK_IS_CONTENT_PROVIDER(provider));

	g_value_init (&value, HEX_TYPE_PASTE_DATA);
	got_val = gdk_content_provider_get_value (provider, &value, &error);
	if (!got_val)
	{
		g_critical ("%s: Failed to retrieve value from content provider: %s",
				__func__, error->message);
		return;
	}

	hex_paste_data = HEX_PASTE_DATA(g_value_get_object (&value));

	hex_str = hex_paste_data_to_delimited_hex (hex_paste_data);

	if (hex_str)
		gdk_clipboard_set_text (clipboard, hex_str);
}

static GtkWidget *
create_hex_paste_data_label (void)
{
	GtkWidget *label = gtk_label_new (_("GHex Paste Data"));

	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_hexpand (label, TRUE);

	g_object_set_data (G_OBJECT(label), "is_hex_paste_data_label", GINT_TO_POINTER(TRUE));

	return label;
}

enum {
	CLIPBOARD_OPERATION_COPY,
	CLIPBOARD_OPERATION_PASTE
};

inline static void
clipboard_operation_activate_helper (GHexClipboardDialog *self, int operation)
{
	GHexClipboardDialogPrivate *priv = ghex_clipboard_dialog_get_instance_private (self);
	GtkListBoxRow *row;
	GtkWidget *child;

	if (! priv->hex)
		return;

	row = gtk_list_box_get_selected_row (priv->listbox);
	if (!row)
		return;

	child = gtk_list_box_row_get_child (row);

	if (GHEX_IS_MIME_SUB_TYPE_LABEL (child))
	{
		GHexMimeSubTypeLabel *label = GHEX_MIME_SUB_TYPE_LABEL(child);

		switch (label->known_sub_type->sub_type)
		{
			case GHEX_MIME_SUB_TYPE_NONE:
				if (operation == CLIPBOARD_OPERATION_COPY)
					hex_widget_copy_to_clipboard (priv->hex);
				else if (operation == CLIPBOARD_OPERATION_PASTE)
					hex_widget_paste_from_clipboard (priv->hex);
				else
					g_assert_not_reached ();
				break;

			case GHEX_MIME_SUB_TYPE_ASCII_PLAINTEXT:
				if (operation == CLIPBOARD_OPERATION_COPY)
					hex_widget_copy_to_clipboard (priv->hex);
				else if (operation == CLIPBOARD_OPERATION_PASTE)
					hex_widget_paste_from_clipboard (priv->hex);
				else
					g_assert_not_reached ();
				break;

			case GHEX_MIME_SUB_TYPE_HEX_PLAINTEXT:
				if (operation == CLIPBOARD_OPERATION_COPY)
					_ghex_clipboard_dialog_delimited_hex_copy (self);
				else if (operation == CLIPBOARD_OPERATION_PASTE)
					_ghex_clipboard_dialog_delimited_hex_paste (self);
				else
					g_assert_not_reached ();
				break;

			default:
				g_assert_not_reached ();
		}
	}
	else if (g_object_get_data (G_OBJECT(child), "is_hex_paste_data_label"))
	{
		if (operation == CLIPBOARD_OPERATION_COPY)
			hex_widget_copy_to_clipboard (priv->hex);
		else if (operation == CLIPBOARD_OPERATION_PASTE)
			hex_widget_paste_from_clipboard (priv->hex);
		else
			g_assert_not_reached ();
	}
	else {
		g_assert_not_reached ();
	}

	gtk_window_close (GTK_WINDOW(self));
}

static GHexKnownMimeSubType *
ghex_known_mime_sub_type_new (const char *mime_type, const char *pretty_name, GHexMimeSubType sub_type)
{
	GHexKnownMimeSubType *new_subtype = g_new0 (GHexKnownMimeSubType, 1);

	new_subtype->mime_type = g_strdup (mime_type);
	new_subtype->pretty_name = g_strdup (pretty_name);
	new_subtype->sub_type = sub_type;

	return new_subtype;
}

static void
ghex_known_mime_sub_type_destroy (GHexKnownMimeSubType *known_sub_type)
{
	g_free (known_sub_type->mime_type);
	g_free (known_sub_type->pretty_name);
	g_free (known_sub_type);
}

static void
ghex_known_mime_list_destroy (gpointer data)
{
	GSList *list = data;

	g_slist_free_full (list, (GDestroyNotify) ghex_known_mime_sub_type_destroy);
}

static guint
mime_hash_func (gconstpointer key)
{
	char *str = g_strdup ((const char *)key);
	guint hash = GHEX_MIME_TYPE_NONE;
	char *cp;

	/* strip off parameters. */
	cp = strtok(str, ";");

	if (g_ascii_strcasecmp (str, "text/plain") == 0)
	{
		const char *utf_str = "charset=utf";

		hash = GHEX_MIME_TYPE_PLAINTEXT;
		cp = strtok (NULL, ";");

		if (cp && g_ascii_strncasecmp (cp, utf_str, strlen(utf_str)) == 0)
			hash = GHEX_MIME_TYPE_UTF_PLAINTEXT;
	}
	g_free (str);
	return hash;
}

static gboolean
mime_hash_equal (gconstpointer a,
               gconstpointer b)
{
	char *str1 = g_strdup ((const char *)a);
	char *str2 = g_strdup ((const char *)b);
	char *cp1, *cp2;
	gboolean retval = FALSE;

	if (g_ascii_strcasecmp(str1, str2) == 0)
		retval = TRUE;

	g_free (str1);
	g_free (str2);

	return retval;
}

static void
init_global_mime_hash (void)
{
	g_assert (MIME_HASH == NULL);

	g_debug ("GHexClipboardDialogClass: initializing MIME_HASH");

	MIME_HASH = g_hash_table_new_full (mime_hash_func,
			mime_hash_equal,
			g_free,
			ghex_known_mime_list_destroy);

#define LIST KNOWN_MIME[GHEX_MIME_TYPE_PLAINTEXT]
#define MIME "text/plain"

	/* _destroy will already handle the freeing, but we need to set it to NULL
	 * due to the way GSList works. */
	if (LIST)
		LIST = NULL;

	LIST = g_slist_append (LIST, ghex_known_mime_sub_type_new (MIME,
				_("Plain text (as ASCII)"),
				GHEX_MIME_SUB_TYPE_ASCII_PLAINTEXT));

	g_hash_table_insert (MIME_HASH,
			g_strdup (MIME),
			LIST);
#undef LIST
#undef MIME

#define LIST KNOWN_MIME[GHEX_MIME_TYPE_UTF_PLAINTEXT]
#define MIME "text/plain;charset=utf-8"

	if (LIST)
		LIST = NULL;

	LIST = g_slist_append (LIST, ghex_known_mime_sub_type_new (MIME,
				_("Plain text (Unicode)"),
				GHEX_MIME_SUB_TYPE_ASCII_PLAINTEXT));

	LIST = g_slist_append (LIST, ghex_known_mime_sub_type_new (MIME,
				_("Plain text (as hex string representing bytes)"),
				GHEX_MIME_SUB_TYPE_HEX_PLAINTEXT));

	g_hash_table_insert (MIME_HASH,
			g_strdup (MIME),
			LIST);

#undef LIST
#undef MIME
}

static void
populate_paste_special_listbox (GHexClipboardDialog *self)
{
	GHexClipboardDialogPrivate *priv = ghex_clipboard_dialog_get_instance_private (self);
	GdkClipboard *clipboard;
	GdkContentProvider *provider;
	GdkContentFormats *formats;
	GValue value = G_VALUE_INIT;
	g_autoptr(GError) error = NULL;
	gboolean have_hex_paste_data = FALSE;
	const char * const * mime_types;

	g_assert (MIME_HASH != NULL);

	if (! priv->hex)
		return;
	if (! gtk_widget_get_visible (GTK_WIDGET(self)))
		return;

	gtk_list_box_remove_all (priv->listbox);

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET(priv->hex));

	/* Note: this will return NULL if the contents are NOT owned by
	 * the local process, as opposed to _get_formats */
	provider = gdk_clipboard_get_content (clipboard);

	/* Get all available formats (this will include both known GTypes (such
	 * as our HexPasteData) _and_ other MIME types. */
	formats = gdk_clipboard_get_formats (clipboard);

	g_debug("%s: formats: %s",
			__func__,
			gdk_content_formats_to_string (formats));

	mime_types = gdk_content_formats_get_mime_types (formats, NULL);
	g_assert (mime_types != NULL);

	g_value_init (&value, HEX_TYPE_PASTE_DATA);
	have_hex_paste_data = GDK_IS_CONTENT_PROVIDER (provider) &&
		gdk_content_provider_get_value (provider, &value, &error);

	if (have_hex_paste_data)
	{
		GtkWidget *hex_paste_data_label = create_hex_paste_data_label ();

		gtk_list_box_append (priv->listbox, hex_paste_data_label);
	}

	for (int i = 0; mime_types[i] != NULL; ++i)
	{
		GSList *tracer = NULL;

		g_debug ("%s: checking mime_types[%d]: %s",
				__func__, i, mime_types[i]);

		for (tracer = g_hash_table_lookup (MIME_HASH, mime_types[i]);
				tracer != NULL;
				tracer = tracer->next)
		{
			GtkWidget *label;
			GHexKnownMimeSubType *known_sub_type = tracer->data;

			g_debug ("%s: MATCH - known_sub_type->pretty_name: %s",
					__func__, known_sub_type->pretty_name);

			label = ghex_mime_sub_type_label_new (known_sub_type);
			gtk_list_box_append (priv->listbox, label);
		}
	}

	gtk_list_box_select_row (priv->listbox, gtk_list_box_get_row_at_index (priv->listbox, 0));
}

static void
populate_copy_special_listbox (GHexClipboardDialog *self)
{
	GHexClipboardDialogPrivate *priv = ghex_clipboard_dialog_get_instance_private (self);
	GtkWidget *hex_paste_data_label;

	if (! priv->hex)
		return;
	if (! gtk_widget_get_visible (GTK_WIDGET(self)))
		return;

	gtk_list_box_remove_all (priv->listbox);

	/* We always give the option to copy to HexPasteData. */

	hex_paste_data_label = create_hex_paste_data_label ();
	gtk_list_box_append (priv->listbox, hex_paste_data_label);

	/* Add a listbox entry for each known mime type. */
	for (int i = 0; i < GHEX_MIME_TYPE_LAST; ++i)
	{
		GSList *tracer;
		
		for (tracer = KNOWN_MIME[i];
				tracer != NULL;
				tracer = tracer->next)
		{
			GHexKnownMimeSubType *known_sub_type = tracer->data;
			GtkWidget *label = ghex_mime_sub_type_label_new (known_sub_type);

			gtk_list_box_append (priv->listbox, label);
		}
	}

	gtk_list_box_select_row (priv->listbox, gtk_list_box_get_row_at_index (priv->listbox, 0));
}

/* < GHexClipboardDialog > */

static const char *
_ghex_clipboard_dialog_get_action_button_label (GHexClipboardDialog *self)
{
	GHexClipboardDialogPrivate *priv;

	g_return_val_if_fail (GHEX_IS_CLIPBOARD_DIALOG (self), NULL);

	priv = ghex_clipboard_dialog_get_instance_private (self);

	return gtk_button_get_label (GTK_BUTTON(priv->action_button));
}

static void
_ghex_clipboard_dialog_set_action_button_label (GHexClipboardDialog *self, const char *action_button_label)
{
	GHexClipboardDialogPrivate *priv;

	g_return_if_fail (GHEX_IS_CLIPBOARD_DIALOG (self));

	priv = ghex_clipboard_dialog_get_instance_private (self);

	if (g_strcmp0 (action_button_label, _ghex_clipboard_dialog_get_action_button_label (self)) != 0)
	{
		gtk_button_set_label (GTK_BUTTON(priv->action_button), action_button_label);

		g_object_notify_by_pspec (G_OBJECT(self), ghex_clipboard_dialog_properties[PROP_ACTION_BUTTON_LABEL]);
	}
}

static void
_ghex_clipboard_dialog_set_action_name (GHexClipboardDialog *self, const char *action_name)
{
	GHexClipboardDialogPrivate *priv;

	g_return_if_fail (GHEX_IS_CLIPBOARD_DIALOG (self));

	priv = ghex_clipboard_dialog_get_instance_private (self);

	gtk_actionable_set_action_name (GTK_ACTIONABLE(priv->action_button), action_name);

	g_object_notify_by_pspec (G_OBJECT(self), ghex_clipboard_dialog_properties[PROP_ACTION_NAME]);
}

static const char *
_ghex_clipboard_dialog_get_action_name (GHexClipboardDialog *self)
{
	GHexClipboardDialogPrivate *priv;

	g_return_val_if_fail (GHEX_IS_CLIPBOARD_DIALOG (self), NULL);

	priv = ghex_clipboard_dialog_get_instance_private (self);

	return gtk_actionable_get_action_name (GTK_ACTIONABLE(priv->action_button));
}

void
ghex_clipboard_dialog_set_hex (GHexClipboardDialog *self, HexWidget *hex)
{
	GHexClipboardDialogPrivate *priv;

	g_return_if_fail (GHEX_IS_CLIPBOARD_DIALOG (self));
	g_return_if_fail (hex == NULL || HEX_IS_WIDGET (hex));

	priv = ghex_clipboard_dialog_get_instance_private (self);

	g_clear_object (&priv->hex);

	if (hex)
		priv->hex = g_object_ref (hex);

	g_object_notify_by_pspec (G_OBJECT(self), ghex_clipboard_dialog_properties[PROP_HEX]);
}

HexWidget *
ghex_clipboard_dialog_get_hex (GHexClipboardDialog *self)
{
	GHexClipboardDialogPrivate *priv;

	g_return_val_if_fail (GHEX_IS_CLIPBOARD_DIALOG (self), NULL);

	priv = ghex_clipboard_dialog_get_instance_private (self);

	return priv->hex;
}

static void
ghex_clipboard_dialog_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GHexClipboardDialog *self = GHEX_CLIPBOARD_DIALOG (object);

	switch (property_id)
	{
		case PROP_HEX:
			ghex_clipboard_dialog_set_hex (self, g_value_get_object (value));
			break;

		case PROP_ACTION_NAME:
			_ghex_clipboard_dialog_set_action_name (self, g_value_get_string (value));
			break;

		case PROP_ACTION_BUTTON_LABEL:
			_ghex_clipboard_dialog_set_action_button_label (self, g_value_get_string (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_clipboard_dialog_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	GHexClipboardDialog *self = GHEX_CLIPBOARD_DIALOG (object);

	switch (property_id)
	{
		case PROP_HEX:
			g_value_set_object (value, ghex_clipboard_dialog_get_hex (self));
			break;

		case PROP_ACTION_NAME:
			g_value_set_string (value, _ghex_clipboard_dialog_get_action_name (self));
			break;

		case PROP_ACTION_BUTTON_LABEL:
			g_value_set_string (value, _ghex_clipboard_dialog_get_action_button_label (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static gboolean
esc_key_press_cb (GtkEventControllerKey *key,
		guint keyval,
		guint keycode,
		GdkModifierType state,
		GHexClipboardDialog *self)
{
	if (keyval == GDK_KEY_Escape)
	{
		gtk_window_close (GTK_WINDOW(self));
		return GDK_EVENT_STOP;
	}
	return GDK_EVENT_PROPAGATE;
}

static void
_ghex_clipboard_dialog_activate_action (GHexClipboardDialog *self)
{
	GHexClipboardDialogPrivate *priv = ghex_clipboard_dialog_get_instance_private (self);
	const char *action = _ghex_clipboard_dialog_get_action_name (self);

	gtk_widget_activate_action (GTK_WIDGET(self), action, NULL);
}

static void
ghex_clipboard_dialog_init (GHexClipboardDialog *self)
{
	GHexClipboardDialogPrivate *priv = ghex_clipboard_dialog_get_instance_private (self);

	gtk_widget_init_template (GTK_WIDGET(self));

	/* Setup signals */

	g_signal_connect_object (priv->action_button, "clicked", G_CALLBACK(_ghex_clipboard_dialog_activate_action), self, G_CONNECT_SWAPPED);

	g_signal_connect_object (priv->listbox, "row-activated", G_CALLBACK(_ghex_clipboard_dialog_activate_action), self, G_CONNECT_SWAPPED);

	g_signal_connect_object (priv->cancel_button, "clicked", G_CALLBACK(gtk_window_close), self, G_CONNECT_SWAPPED);

	/* Make ESC close the dialog */
	{
		GtkEventController *key = gtk_event_controller_key_new ();

		gtk_widget_add_controller (GTK_WIDGET(self), key);
		g_signal_connect_object (key, "key-pressed", G_CALLBACK(esc_key_press_cb), self, G_CONNECT_DEFAULT);
	}
}

static void
ghex_clipboard_dialog_dispose (GObject *object)
{
	GHexClipboardDialog *self = GHEX_CLIPBOARD_DIALOG(object);
	GHexClipboardDialogPrivate *priv = ghex_clipboard_dialog_get_instance_private (self);

	g_clear_object (&priv->hex);

	G_OBJECT_CLASS(ghex_clipboard_dialog_parent_class)->dispose (object);
}

static void
ghex_clipboard_dialog_class_init (GHexClipboardDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose = ghex_clipboard_dialog_dispose;
	object_class->set_property = ghex_clipboard_dialog_set_property;
	object_class->get_property = ghex_clipboard_dialog_get_property;
	
	if (! MIME_HASH)
		init_global_mime_hash ();

	ghex_clipboard_dialog_properties[PROP_HEX] = g_param_spec_object ("hex", NULL, NULL,
			HEX_TYPE_WIDGET,
			default_flags | G_PARAM_READWRITE);

	ghex_clipboard_dialog_properties[PROP_ACTION_NAME] = g_param_spec_string ("action-name", NULL, NULL,
			NULL,
			default_flags | G_PARAM_READWRITE);

	ghex_clipboard_dialog_properties[PROP_ACTION_BUTTON_LABEL] = g_param_spec_string ("action-button-label", NULL, NULL,
			NULL,
			default_flags | G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, GHEX_CLIPBOARD_DIALOG_N_PROPERTIES, ghex_clipboard_dialog_properties);

	gtk_widget_class_set_template_from_resource (widget_class, RESOURCE_BASE_PATH "/ghex-clipboard-dialog.ui");

	gtk_widget_class_bind_template_child_private (widget_class, GHexClipboardDialog, listbox);
	gtk_widget_class_bind_template_child_private (widget_class, GHexClipboardDialog, action_button);
	gtk_widget_class_bind_template_child_private (widget_class, GHexClipboardDialog, cancel_button);
}

/* < GHexPasteSpecialDialog > */

static void
ghex_paste_special_dialog_action (GtkWidget *widget, const char *action_name, GVariant *parameter)
{
	gpointer self = widget;

	g_assert (GHEX_IS_PASTE_SPECIAL_DIALOG (self));

	clipboard_operation_activate_helper (self, CLIPBOARD_OPERATION_PASTE);
}

static void
ghex_paste_special_dialog_init (GHexPasteSpecialDialog *self)
{
	gtk_widget_init_template (GTK_WIDGET(self));

	g_signal_connect (self, "notify::visible", G_CALLBACK(populate_paste_special_listbox), NULL);
}

static void
ghex_paste_special_dialog_class_init (GHexPasteSpecialDialogClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_install_action (widget_class, "clipboard.paste-special", NULL, ghex_paste_special_dialog_action);

	gtk_widget_class_set_template_from_resource (widget_class, RESOURCE_BASE_PATH "/ghex-paste-special-dialog.ui");
}

GtkWidget *
ghex_paste_special_dialog_new (GtkWindow *parent)
{
	return g_object_new (GHEX_TYPE_PASTE_SPECIAL_DIALOG,
			"transient-for", parent,
			NULL);
}

/* </ GHexPasteSpecialDialog > */

/* < GHexCopySpecialDialog > */

static void
ghex_copy_special_dialog_action (GtkWidget *widget, const char *action_name, GVariant *parameter)
{
	gpointer self = widget;

	g_assert (GHEX_IS_COPY_SPECIAL_DIALOG (self));

	clipboard_operation_activate_helper (self, CLIPBOARD_OPERATION_COPY);
}

static void
ghex_copy_special_dialog_init (GHexCopySpecialDialog *self)
{
	gtk_widget_init_template (GTK_WIDGET(self));

	g_signal_connect (self, "notify::visible", G_CALLBACK(populate_copy_special_listbox), NULL);
}

static void
ghex_copy_special_dialog_class_init (GHexCopySpecialDialogClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_install_action (widget_class, "clipboard.copy-special", NULL, ghex_copy_special_dialog_action);

	gtk_widget_class_set_template_from_resource (widget_class, RESOURCE_BASE_PATH "/ghex-copy-special-dialog.ui");
}

GtkWidget *
ghex_copy_special_dialog_new (GtkWindow *parent)
{
	g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);

	return g_object_new (GHEX_TYPE_COPY_SPECIAL_DIALOG,
			"transient-for", parent,
			NULL);
}

/* </ GHexCopySpecialDialog > */
