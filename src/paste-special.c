/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* paste-special.c - `Paste special' dialog for GHex

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

#include "paste-special.h"

/* DEFINES */

#define PASTE_SPECIAL_RESOURCE "/org/gnome/GHex/paste-special.ui"

/* MACROS */

#define GET_WIDGET(X) \
	X = GTK_WIDGET(gtk_builder_get_object (builder, #X)); \
	g_assert (GTK_IS_WIDGET (X))

/* ENUMS AND DATATYPES */

#define HEX_PASTE_ERROR hex_paste_error_quark ()
G_DEFINE_QUARK (hex-paste-error-quark, hex_paste_error)

typedef enum operations {
	COPY_OPERATION,
	PASTE_OPERATION

} PasteSpecialOperation;

typedef enum {
	HEX_PASTE_ERROR_INVALID

} HexPasteError;

enum mime_types {
	NO_MIME,
	PLAINTEXT_MIME,
	UTF_PLAINTEXT_MIME,
	LAST_MIME
};

typedef enum {
	NO_SUBTYPE,
	ASCII_PLAINTEXT,
	HEX_PLAINTEXT
} MimeSubType;

typedef struct {
	char *mime_type;
	char *pretty_name;
	MimeSubType sub_type;
} KnownMimeType;

G_DECLARE_FINAL_TYPE (MimeSubTypeLabel, mime_sub_type_label, MIME_SUB_TYPE, LABEL,
		GtkWidget)

struct _MimeSubTypeLabel {
	GtkWidget parent_instance;

	GtkWidget *label;
	KnownMimeType *known_type;
};

G_DEFINE_TYPE (MimeSubTypeLabel, mime_sub_type_label, GTK_TYPE_WIDGET)

/* PRIVATE FORWARD DECLARATIONS */
static void dialog_response_cb (GtkDialog *dialog, int response_id,
		gpointer user_data);
static gboolean destroy_paste_special_dialog (void);

/* STATIC GLOBALS */

static GtkBuilder *builder;
static GHexApplicationWindow *app_window;
static GdkClipboard *clipboard;
static GtkHexPasteData *hex_paste_data;
static GHashTable *mime_hash;
static GSList *known_mime[LAST_MIME];
static GtkWidget *hex_paste_data_label;

/* use GET_WIDGET(X) macro to set these from builder, where
 * X == var name == builder id name. Don't include quotation marks. */

static GtkWidget *paste_special_dialog;

static GtkWidget *paste_button;
static GtkWidget *close_button;
static GtkWidget *paste_special_listbox;

/* MimeSubTypeLabel - Constructors and Destructors */

static void
mime_sub_type_label_init (MimeSubTypeLabel *self)
{
	self->label = gtk_label_new (NULL);
	gtk_widget_set_hexpand (self->label, TRUE);
	gtk_widget_set_halign (self->label, GTK_ALIGN_START);
	gtk_widget_set_parent (self->label, GTK_WIDGET(self));
}

static void
mime_sub_type_label_dispose (GObject *object)
{
	MimeSubTypeLabel *self = MIME_SUB_TYPE_LABEL (object);

	g_clear_pointer (&self->label, gtk_widget_unparent);

	G_OBJECT_CLASS (mime_sub_type_label_parent_class)->dispose (object);
}

static void
mime_sub_type_label_class_init (MimeSubTypeLabelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = mime_sub_type_label_dispose;

	gtk_widget_class_set_layout_manager_type (widget_class,
			GTK_TYPE_BOX_LAYOUT);
}

GtkWidget *
mime_sub_type_label_new (KnownMimeType *known_type)
{
	MimeSubTypeLabel *self = g_object_new (mime_sub_type_label_get_type (),
			NULL);

	g_return_val_if_fail (known_type->pretty_name, NULL);

	self->known_type = known_type;
	gtk_label_set_text (GTK_LABEL(self->label), known_type->pretty_name);

	return GTK_WIDGET(self);
}

/* PRIVATE FUNCTIONS */

/* Note - I looked into reusing the hex block formatting code from GtkHex,
 * but honestly, it looks like it'd be more trouble than it's worth.
 */
static char *
hex_paste_data_to_delimited_hex (void)
{
	GString *buf;
	char *doc_data = NULL;
	int elems;
	char *ret_str;

	g_return_val_if_fail (GTK_IS_HEX_PASTE_DATA (hex_paste_data), NULL);

	buf = g_string_new (NULL);
	doc_data = gtk_hex_paste_data_get_doc_data (hex_paste_data);
	elems = gtk_hex_paste_data_get_elems (hex_paste_data);

	for (int i = 0; i < elems; ++i)
	{
		g_string_append_printf (buf, "%.2X", doc_data[i]);

		if (i < elems - 1)
			g_string_append_c (buf, ' ');
	}
	ret_str = g_string_free (buf,
			FALSE);		/* Free the GString container but grab the C-string. */

	return ret_str;
}

static GString *
delimited_hex_to_gstring (const char *hex_str, GError **err)
{
	char *copy;
	char *cp;
	GString *buf;

	g_return_val_if_fail (hex_str, NULL);

	buf = g_string_new (NULL);
	copy = g_strdup (hex_str);

	cp = strtok(copy, " ");
	do {
		guint hex_char;
		int scan_ct;

		if (strlen(cp) != 2) goto fail;

		scan_ct = sscanf(cp, "%x", &hex_char);

		if (scan_ct != 1) goto fail;
		if (hex_char > 255) goto fail;

		g_string_append_printf (buf, "%c", hex_char);

	} while ((cp = strtok(NULL, " ")) != NULL);

	g_free (copy);

	g_debug ("%s: buf->str: %s", __func__, buf->str);
	return buf;

fail:
	g_debug ("%s: Invalid hex format detected", __func__);
	g_set_error (err,
			HEX_PASTE_ERROR,
			HEX_PASTE_ERROR_INVALID,
			"Paste failed; invalid hex format.\n\n"
			"The string must be in the format of space-delineated "
			"hex byte pairs.\n\n"
			"For example: \"FF 3D 99 0A\"");
	g_free (copy);
	return NULL;
}

static void
delimited_paste_received_cb (GObject *source_object,
		GAsyncResult *result,
		gpointer user_data)
{
	GtkHex *gh;
	HexDocument *doc;
	char *text;
	GString *buf = NULL;
	GError *err = NULL; 

	/* Get the resulting text of the read operation */
	text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD(source_object),
			result,
			NULL);	/* GError */

	buf = delimited_hex_to_gstring (text, &err);
	if (! buf)
	{
		g_debug ("%s: Received invalid delimeter string. Returning.",
				__func__);
		if (err) {
			display_error_dialog (GTK_WINDOW(app_window), err->message);
			g_error_free (err);
		}
		return;
	}

	gh = ghex_application_window_get_hex (app_window);
	g_return_if_fail (GTK_IS_HEX (gh));

	doc = gtk_hex_get_document (gh);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	hex_document_set_data (doc,
			gtk_hex_get_cursor (gh),
			buf->len,
			0,	/* rep_len (0 to insert w/o replacing; what we want) */
			buf->str,
			TRUE);
}

static void
delimited_hex_paste (void)
{
	gdk_clipboard_read_text_async (clipboard,
			NULL,	/* GCancellable *cancellable */
			delimited_paste_received_cb,
			NULL);	/* user_data */
}

static void
delimited_hex_copy (void)
{
	GdkContentProvider *provider;
	GValue value = G_VALUE_INIT;
	GError *error = NULL;
	gboolean have_hex_paste_data = FALSE;
	char *hex_str = NULL;

	/* Save selection to clipboard as HexPasteData */
	gtk_hex_copy_to_clipboard (ghex_application_window_get_hex(app_window));

	provider = gdk_clipboard_get_content (clipboard);
	g_return_if_fail (GDK_IS_CONTENT_PROVIDER(provider));

	g_value_init (&value, GTK_TYPE_HEX_PASTE_DATA);
	gdk_content_provider_get_value (provider, &value, &error);
	hex_paste_data = GTK_HEX_PASTE_DATA(g_value_get_object (&value));

	hex_str = hex_paste_data_to_delimited_hex ();

	/* FIXME - Maybe this should be set up as a union? Not sure what would
	 * best satisfy the principle of least astonishment (POLA).
	 */
	if (hex_str)
		gdk_clipboard_set_text (clipboard, hex_str);
}

static void
create_hex_paste_data_label (void)
{
	hex_paste_data_label = gtk_label_new (_("GHex Paste Data"));
	gtk_widget_set_halign (hex_paste_data_label, GTK_ALIGN_START);
	gtk_widget_set_hexpand (hex_paste_data_label, TRUE);
}

static void
row_activated_cb (GtkListBox *box,
		GtkListBoxRow *row,
		gpointer user_data)
{
#define STANDARD_PASTE \
	gtk_hex_paste_from_clipboard \
		(ghex_application_window_get_hex(app_window));

#define STANDARD_COPY \
	gtk_hex_copy_to_clipboard \
		(ghex_application_window_get_hex(app_window));

	GtkWidget *child = gtk_list_box_row_get_child (row);
	PasteSpecialOperation operation = GPOINTER_TO_INT (user_data);

	g_return_if_fail (GTK_IS_WIDGET(child));

	if (MIME_SUB_TYPE_IS_LABEL(child))
	{
		MimeSubTypeLabel *label = MIME_SUB_TYPE_LABEL(child);

		switch (label->known_type->sub_type)
		{
			case NO_SUBTYPE:
				if (operation == COPY_OPERATION) {
					STANDARD_COPY
				} else {
					STANDARD_PASTE
				}
				break;

			case ASCII_PLAINTEXT:
				if (operation == COPY_OPERATION) {
					STANDARD_COPY
				} else {
					STANDARD_PASTE
				}
				break;

			case HEX_PLAINTEXT:
				if (operation == COPY_OPERATION) {
					delimited_hex_copy ();
				} else {
					delimited_hex_paste ();
				}
				break;

			default:
				g_error ("%s: ERROR - INVALID MIME TYPE. "
						"THIS SHOULDN'T HAPPEN!",
						__func__);
				break;
		}
	}
	else	/* HexPasteData */
	{
		if (operation == COPY_OPERATION) {
			STANDARD_COPY
		} else {
			STANDARD_PASTE
		}
	}
}
#undef STANDARD_PASTE
#undef STANDARD_COPY

static void
common_init_widgets (void)
{
	GET_WIDGET (paste_special_dialog);
	GET_WIDGET (paste_button);
	GET_WIDGET (close_button);
	GET_WIDGET (paste_special_listbox);

	gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX(paste_special_listbox),
			FALSE);
}

static void
paste_special_init_widgets (void)
{
	common_init_widgets ();
}

static void
copy_special_init_widgets (void)
{
	common_init_widgets ();
	gtk_button_set_label (GTK_BUTTON(paste_button), _("_Copy"));
	gtk_window_set_title (GTK_WINDOW(paste_special_dialog),
			_("Copy Special"));
}

static KnownMimeType *
create_known_mime_sub_type (const char *mime_type, const char *pretty_name,
		MimeSubType sub_type)
{
	KnownMimeType *new_subtype = g_new0 (KnownMimeType, 1);

	new_subtype->mime_type = g_strdup (mime_type);
	new_subtype->pretty_name = g_strdup (pretty_name);
	new_subtype->sub_type = sub_type;

	return new_subtype;
}

static void
known_mime_sub_type_destroy (KnownMimeType *known_type)
{
	if (known_type->mime_type)
		g_free (known_type->mime_type);

	if (known_type->pretty_name)
		g_free (known_type->pretty_name);

	g_free (known_type);
}

static void
known_mime_list_destroy (gpointer data)
{
	GSList *list = data;

	g_slist_free_full (list,
			(GDestroyNotify)known_mime_sub_type_destroy);
}

static guint
mime_hash_func (gconstpointer key)
{
	char *str = g_strdup ((const char *)key);
	guint hash = NO_MIME;
	char *cp;

	g_debug ("%s: str: %s", __func__, str);

	/* strip off parameters. */
	cp = strtok(str, ";");

	g_debug ("%s: cp: %s", __func__, cp);

	if (g_ascii_strcasecmp (str, "text/plain") == 0)
	{
		char *utf_str = "charset=utf";

		hash = PLAINTEXT_MIME;

		cp = strtok (NULL, ";");

		g_debug ("%s: cp after 2nd strtok: %s", __func__, cp);

		if (cp && g_ascii_strncasecmp (cp, utf_str, strlen(utf_str)) == 0)
			hash = UTF_PLAINTEXT_MIME;
	}
	g_free (str);
	g_debug ("%s: returning: %u", __func__, hash);
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

	g_debug ("%s: str1: %s - str2: %s", __func__, str1, str2);

	if (g_ascii_strcasecmp(str1, str2) == 0)
		retval = TRUE;

	g_free (str1);
	g_free (str2);

	g_debug ("%s: returning: %d", __func__, retval);
	return retval;
}

static void
init_mime_hash (void)
{
	/* Create mime_hash, clearing any existing */

	if (mime_hash) {
		g_clear_pointer (&mime_hash, g_hash_table_destroy);
	}
	g_assert (mime_hash == NULL);

	mime_hash = g_hash_table_new_full (mime_hash_func,
			mime_hash_equal,
			g_free,
			known_mime_list_destroy);
	g_assert (mime_hash);

	/* PLAINTEXT_MIME */
#define LIST known_mime[PLAINTEXT_MIME]
#define MIME "text/plain"

	/* _destroy will already handle the freeing, but we need to set it to NULL
	 * due to the way GSList works. */
	if (LIST)
		LIST = NULL;

	LIST = g_slist_append (LIST, create_known_mime_sub_type (MIME,
				_("Plain text (as ASCII)"),
				ASCII_PLAINTEXT));

	LIST = g_slist_append (LIST, create_known_mime_sub_type (MIME,
				_("Plain text (as space-delimited hex pairs)"),
				HEX_PLAINTEXT));

	g_hash_table_insert (mime_hash,
			g_strdup (MIME),
			LIST);
#undef LIST
#undef MIME

	/* UTF_PLAINTEXT_MIME */
#define LIST known_mime[UTF_PLAINTEXT_MIME]
#define MIME "text/plain;charset=utf-8"

	if (LIST)
		LIST = NULL;

	LIST = g_slist_append (LIST, create_known_mime_sub_type (MIME,
				_("Plain text (Unicode)"),
				NO_SUBTYPE));

	g_hash_table_insert (mime_hash,
			g_strdup (MIME),
			LIST);

#undef LIST
#undef MIME
}

static void
common_setup_signals (void)
{
	g_return_if_fail (GTK_IS_DIALOG (paste_special_dialog));

	g_signal_connect (paste_special_dialog, "response",
			G_CALLBACK(dialog_response_cb), NULL);
}

static void
paste_special_setup_signals (void)
{
	common_setup_signals ();

	g_signal_connect (paste_special_listbox, "row-activated",
			G_CALLBACK(row_activated_cb), GINT_TO_POINTER (PASTE_OPERATION));
}

static void
copy_special_setup_signals (void)
{
	common_setup_signals ();

	g_signal_connect (paste_special_listbox, "row-activated",
			G_CALLBACK(row_activated_cb), GINT_TO_POINTER (COPY_OPERATION));
}

static void
paste_special_populate_listbox (void)
{
	GdkContentProvider *provider;
	GdkContentFormats *formats;
	GValue value = G_VALUE_INIT;
	GError *error = NULL;
	gboolean have_hex_paste_data = FALSE;
	const char * const * mime_types;

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

	g_value_init (&value, GTK_TYPE_HEX_PASTE_DATA);
	have_hex_paste_data = GDK_IS_CONTENT_PROVIDER (provider) &&
		gdk_content_provider_get_value (provider, &value, &error);

	if (have_hex_paste_data)
	{
		hex_paste_data = GTK_HEX_PASTE_DATA(g_value_get_object (&value));

		create_hex_paste_data_label ();

		gtk_list_box_append (GTK_LIST_BOX(paste_special_listbox),
				hex_paste_data_label);
	}

	for (int i = 0; mime_types[i] != NULL; ++i)
	{
		GSList *tracer = NULL;

		g_debug ("%s: checking mime_types[%d]: %s",
				__func__, i, mime_types[i]);

		for (tracer = g_hash_table_lookup (mime_hash, mime_types[i]);
				tracer != NULL;
				tracer = tracer->next)
		{
			GtkWidget *label;
			KnownMimeType *type = tracer->data;

			g_debug ("%s: MATCH - type->pretty_name: %s",
					__func__, type->pretty_name);

			label = mime_sub_type_label_new (type);
			gtk_list_box_append (GTK_LIST_BOX(paste_special_listbox), label);
		}
	}
}

static void
copy_special_populate_listbox (void)
{
	/* We always give the option to copy to HexPasteData. */
	create_hex_paste_data_label ();

	gtk_list_box_append (GTK_LIST_BOX(paste_special_listbox),
			hex_paste_data_label);

	/* Add a listbox entry for each known mime type. */
	for (int i = 0; i < LAST_MIME; ++i)
	{
		GSList *tracer;
		
		for (tracer = known_mime[i];
				tracer != NULL;
				tracer = tracer->next)
		{
			GtkWidget *label;
			KnownMimeType *type = tracer->data;

			label = mime_sub_type_label_new (type);
			gtk_list_box_append (GTK_LIST_BOX(paste_special_listbox), label);
		}
	}
}

static gboolean
destroy_paste_special_dialog (void)
{
	g_debug ("%s: START", __func__);

	g_return_val_if_fail (GTK_IS_WINDOW (paste_special_dialog),
			GDK_EVENT_PROPAGATE);

	if (builder)
		g_clear_object (&builder);

	if (mime_hash)
		g_clear_pointer (&mime_hash, g_hash_table_destroy);

	/* This is not refcounted and is just a pure pointer */
	if (hex_paste_data)
		hex_paste_data = NULL;

	gtk_window_destroy (GTK_WINDOW(paste_special_dialog));

	return GDK_EVENT_STOP;
}

static void
dialog_response_cb (GtkDialog *dialog,
		int        response_id,
		gpointer   user_data)
{
	GtkListBoxRow *row;

	g_debug ("%s: START -- response_id: %d",
			__func__, response_id);

	switch (response_id)
	{
		case GTK_RESPONSE_ACCEPT:
			row = gtk_list_box_get_selected_row
				(GTK_LIST_BOX(paste_special_listbox));
			g_signal_emit_by_name (row, "activate");
			break;

		/* This should cover both delete events and close. */
		default:
			destroy_paste_special_dialog ();
			break;
	}
}

/* PUBLIC FUNCTIONS */

GtkWidget *
create_paste_special_dialog (GHexApplicationWindow *parent,
		GdkClipboard *clip)
{
	g_return_val_if_fail (GDK_IS_CLIPBOARD (clip), NULL);
	g_return_val_if_fail (GHEX_IS_APPLICATION_WINDOW (parent), NULL);

	clipboard = clip;
	app_window = parent;
	builder = gtk_builder_new_from_resource (PASTE_SPECIAL_RESOURCE);

	init_mime_hash ();
	paste_special_init_widgets ();
	paste_special_populate_listbox ();
	paste_special_setup_signals ();

	gtk_window_set_transient_for (GTK_WINDOW(paste_special_dialog),
			GTK_WINDOW(parent));

	return paste_special_dialog;
}

GtkWidget *
create_copy_special_dialog (GHexApplicationWindow *parent,
		GdkClipboard *clip)
{
	g_return_val_if_fail (GDK_IS_CLIPBOARD (clip), NULL);
	g_return_val_if_fail (GHEX_IS_APPLICATION_WINDOW (parent), NULL);

	clipboard = clip;
	app_window = parent;
	builder = gtk_builder_new_from_resource (PASTE_SPECIAL_RESOURCE);

	init_mime_hash ();
	copy_special_init_widgets ();
	copy_special_populate_listbox ();
	copy_special_setup_signals ();

	gtk_window_set_transient_for (GTK_WINDOW(paste_special_dialog),
			GTK_WINDOW(parent));

	return paste_special_dialog;
}
