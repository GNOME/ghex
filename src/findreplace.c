/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* findreplace.c - finding & replacing data

   Copyright © 1998 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021-2022 Logan Rathbone <poprocks@gmail.com>

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

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#include "findreplace.h"

#include <config.h>

/* DEFINES */

#define JUMP_DIALOG_CSS_NAME "jumpdialog"

/* GOBJECT DEFINITIONS */

enum signal_types {
	CLOSED,
	LAST_SIGNAL
};

enum FindDirection {
	FIND_FORWARD,
	FIND_BACKWARD
};

static guint signals[LAST_SIGNAL];

typedef struct
{
	HexWidget *gh;
	HexWidgetAutoHighlight *auto_highlight;

} PaneDialogPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (PaneDialog, pane_dialog, GTK_TYPE_WIDGET)

typedef struct {
	HexDocument *f_doc;
	GtkWidget *f_gh;
	GtkWidget *find_frame;
	GtkWidget *find_string_label;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *f_next, *f_prev, *f_clear;
	GtkWidget *close;
	GtkWidget *options_btn;
	GtkWidget *options_popover;
	GtkWidget *options_regex;
	GtkWidget *options_ignore_case;
	GtkWidget *options_show_pane;
	gboolean found;
	GCancellable *cancellable;

} FindDialogPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (FindDialog, find_dialog, PANE_TYPE_DIALOG)

struct _ReplaceDialog {
	FindDialog parent_instance;

	GtkWidget *r_gh;
	HexDocument *r_doc;

	GtkWidget *r_frame;
	GtkWidget *replace, *replace_all;
}; 

G_DEFINE_TYPE (ReplaceDialog, replace_dialog, FIND_TYPE_DIALOG)

struct _JumpDialog {
	PaneDialog parent_instance;

	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *int_entry;
	GtkWidget *ok, *cancel;
	GtkEventController *focus_controller;
};

G_DEFINE_TYPE (JumpDialog, jump_dialog, PANE_TYPE_DIALOG)


/* PRIVATE FUNCTION DECLARATIONS */

static void common_cancel_cb (GtkButton *button, gpointer user_data);
static void find_next_cb (GtkButton *button, gpointer user_data);
static void find_prev_cb (GtkButton *button, gpointer user_data);
static void find_clear_cb (GtkButton *button, gpointer user_data);
static void replace_one_cb (GtkButton *button, gpointer user_data);
static void replace_all_cb (GtkButton *button, gpointer user_data);
static void replace_clear_cb (GtkButton *button, gpointer user_data);
static void goto_byte_cb (GtkButton *button, gpointer user_data);
static gint64 get_search_string (HexDocument *doc, gchar **str);
static void pane_dialog_update_busy_state (PaneDialog *self);
static void mark_gh_busy (HexWidget *gh, gboolean busy);

static GtkWidget *
create_hex_view (HexDocument *doc)
{
	GtkWidget *gh;

	gh = hex_widget_new (doc);

	gtk_widget_set_hexpand (gh, TRUE);
	hex_widget_set_group_type (HEX_WIDGET(gh), def_group_type);
	common_set_gtkhex_font_from_settings (HEX_WIDGET(gh));
    hex_widget_set_insert_mode (HEX_WIDGET(gh), TRUE);

    return gh;
}

static gint64
get_search_string (HexDocument *doc, char **str)
{
	gint64 size = hex_buffer_get_payload_size (hex_document_get_buffer (doc));
	
	if (size > 0)
		*str = hex_buffer_get_data (hex_document_get_buffer (doc), 0, size);
	else
		*str = NULL;

	return size;
}

static void
pane_dialog_real_close (PaneDialog *self)
{
	PaneDialogPrivate *priv = pane_dialog_get_instance_private (self);

	if (priv->auto_highlight && priv->gh) {
		hex_widget_delete_autohighlight (priv->gh, priv->auto_highlight);
	}
	priv->auto_highlight = NULL;
}

static void
find_options_show_pane_changed_cb (AdwToggleGroup *tg,
				GParamSpec *pspec,
				gpointer user_data)
{
	FindDialog *self = FIND_DIALOG(user_data);
	FindDialogPrivate *f_priv = find_dialog_get_instance_private (self);
	const char *active_id = adw_toggle_group_get_active_name (tg);
	gboolean show_ascii, show_hex;

	if (g_strcmp0 (active_id, "ascii") == 0)
	{
		show_ascii = TRUE;
		show_hex = FALSE;
	}
	else if (g_strcmp0 (active_id, "hex") == 0)
	{
		show_ascii = FALSE;
		show_hex = TRUE;
	}
	else	/* both */
	{
		show_ascii = TRUE;
		show_hex = TRUE;
	}

	hex_widget_show_hex_column (HEX_WIDGET(f_priv->f_gh), show_hex);
	hex_widget_show_ascii_column (HEX_WIDGET(f_priv->f_gh), show_ascii);

	if (REPLACE_IS_DIALOG (self))
	{
		hex_widget_show_hex_column (HEX_WIDGET(REPLACE_DIALOG(self)->r_gh),
				show_hex);
		hex_widget_show_ascii_column (HEX_WIDGET(REPLACE_DIALOG(self)->r_gh),
				show_ascii);
	}
}

static void
find_cancel_cb (GtkButton *button, gpointer user_data)
{
	FindDialog *self = FIND_DIALOG(user_data);
	FindDialogPrivate *f_priv = find_dialog_get_instance_private (self);

	g_cancellable_cancel (f_priv->cancellable);
}

static void
common_cancel_cb (GtkButton *button, gpointer user_data)
{
	PaneDialog *self = PANE_DIALOG(user_data);

	g_signal_emit(self, signals[CLOSED], 0);
}

/* Small helper function. */
static void
no_string_dialog (GtkWindow *parent)
{
	display_dialog (parent, _("No string provided."));
}

static void
set_watch_cursor (GtkWidget *widget, gboolean enabled)
{
	if (enabled)
	{
		gtk_widget_set_cursor_from_name (widget, "watch");
	}
	else
	{
		gtk_widget_set_cursor (widget, NULL);
	}
}

static void
mark_gh_busy (HexWidget *gh, gboolean busy)
{
	set_watch_cursor (GTK_WIDGET(gh), busy);
	g_object_set_data (G_OBJECT(gh), "busy", GINT_TO_POINTER(busy));
}

static gboolean
gh_is_busy (HexWidget *gh)
{
	gpointer data = g_object_get_data (G_OBJECT(gh), "busy");

	if (data)
		return GPOINTER_TO_INT (data);
	else
		return FALSE;
}

/* Helper to grab search flags from the GUI */
static inline HexSearchFlags
search_flags_from_checkboxes (const FindDialogPrivate *f_priv)
{
	HexSearchFlags flags = HEX_SEARCH_NONE;

	if (gtk_check_button_get_active (GTK_CHECK_BUTTON(f_priv->options_regex)))
		flags |= HEX_SEARCH_REGEX;
	if (gtk_check_button_get_active (GTK_CHECK_BUTTON(f_priv->options_ignore_case)))
		flags |= HEX_SEARCH_IGNORE_CASE;

	return flags;
}

static void
find_ready_cb (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
	HexDocument *doc = HEX_DOCUMENT (source_object);
	FindDialog *self = FIND_DIALOG (user_data);
	HexDocumentFindData *find_data;
	GTask *task = G_TASK(res);
	PaneDialogPrivate *priv = pane_dialog_get_instance_private (PANE_DIALOG(self));
	FindDialogPrivate *f_priv = find_dialog_get_instance_private (self);
	GtkWindow *parent = GTK_WINDOW(gtk_widget_get_native (GTK_WIDGET(self)));
	HexSearchFlags flags;

	find_data = hex_document_find_finish (doc, res);

	/* Typically this will be due to a cancellation - could theoretically be
	 * an error, but not much we can do to report a search error anyway.
	 */
	if (! find_data)
		goto out;

	flags = search_flags_from_checkboxes (f_priv);

	if (find_data->found)
	{
		f_priv->found = TRUE;

		hex_widget_set_cursor (priv->gh, find_data->offset);

		/* If string found, insert auto-highlights of search string */

		if (priv->auto_highlight)
			hex_widget_delete_autohighlight (priv->gh, priv->auto_highlight);

		priv->auto_highlight = NULL;
		priv->auto_highlight = hex_widget_insert_autohighlight_full (priv->gh,
				find_data->what, find_data->len, flags);

		gtk_widget_grab_focus (GTK_WIDGET(priv->gh));
	}
	else
	{
		display_dialog (parent,
			f_priv->found ? find_data->found_msg : find_data->not_found_msg);

		f_priv->found = FALSE;
	}
	mark_gh_busy (priv->gh, FALSE);
	pane_dialog_update_busy_state (PANE_DIALOG(self));
out:
	g_object_unref (task);
}

static void
find_common (FindDialog *self, enum FindDirection direction,
		const char *found_msg, const char *not_found_msg)
{
	GtkWidget *widget = GTK_WIDGET(self);
	PaneDialogPrivate *priv;
	FindDialogPrivate *f_priv;
	GtkWindow *parent;
	HexDocument *doc;
	gint64 cursor_pos;
	gint64 str_len;
	char *str;
	HexDocumentFindData *find_data = NULL;
	
	priv = pane_dialog_get_instance_private (PANE_DIALOG(self));
	f_priv = find_dialog_get_instance_private (self);

	parent = GTK_WINDOW(gtk_widget_get_native (widget));

	doc = hex_widget_get_document (priv->gh);
	cursor_pos = hex_widget_get_cursor (priv->gh);

	str_len = get_search_string (f_priv->f_doc, &str);
	if (str_len == 0)
	{
		no_string_dialog (parent);
		return;
	}

	/* Search for requested string */

	find_data = hex_document_find_data_new ();
	find_data->what = str;
	find_data->len = str_len;
	find_data->found_msg = found_msg;
	find_data->not_found_msg = not_found_msg;
	find_data->flags = search_flags_from_checkboxes (f_priv);
	
	g_cancellable_reset (f_priv->cancellable);

	if (direction == FIND_FORWARD)
	{
		find_data->start = f_priv->found == FALSE ?
								cursor_pos :
								cursor_pos + 1;

		hex_document_find_forward_full_async (doc,
				find_data,
				f_priv->cancellable,
				find_ready_cb,
				self);
	}
	else	/* FIND_BACKWARD */
	{
		find_data->start = cursor_pos;

		hex_document_find_backward_full_async (doc,
				find_data,
				f_priv->cancellable,
				find_ready_cb,
				self);
	}
	mark_gh_busy (priv->gh, TRUE);
	pane_dialog_update_busy_state (PANE_DIALOG(self));
}

static void
find_prev_cb (GtkButton *button, gpointer user_data)
{
	FindDialog *self = FIND_DIALOG(user_data);

	find_common (self, FIND_BACKWARD,
			_("Beginning of file reached.\n\n"
				"No further matches found."),
			_("Beginning of file reached.\n\n"
				"No occurrences found from cursor."));
}

static void
find_next_cb (GtkButton *button, gpointer user_data)
{
	FindDialog *self = FIND_DIALOG(user_data);

	find_common (self, FIND_FORWARD,
			_("End of file reached.\n\n"
				"No further matches found."),
			_("End of file reached.\n\n"
				"No occurrences found from cursor."));
}

/* CROSSREF: replace_clear_cb */
static void
find_clear_cb (GtkButton *button, gpointer user_data)
{
	FindDialog *self = FIND_DIALOG(user_data);
	FindDialogPrivate *f_priv = find_dialog_get_instance_private (self);
	GtkWidget *new_gh;
	HexDocument *new_doc;

	new_doc = hex_document_new ();
	new_gh = create_hex_view (new_doc);

	gtk_box_remove (GTK_BOX(f_priv->find_frame),
			gtk_widget_get_last_child (f_priv->find_frame));
	gtk_box_append (GTK_BOX(f_priv->find_frame), new_gh);

	f_priv->f_doc = new_doc;
	f_priv->f_gh = new_gh;

	find_options_show_pane_changed_cb (ADW_TOGGLE_GROUP(f_priv->options_show_pane), NULL, self);

	gtk_widget_grab_focus (GTK_WIDGET(self));
}

static void
goto_byte_cb (GtkButton *button, gpointer user_data)
{
	JumpDialog *self = JUMP_DIALOG(user_data);
	PaneDialogPrivate *priv = pane_dialog_get_instance_private (PANE_DIALOG(self));
	GtkWindow *parent;
	HexDocument *doc;
	gint64 cursor_pos;
	GtkEntry *entry;
	GtkEntryBuffer *buffer;
	gint64 byte = 2;
	int len, i;
	int is_relative = 0;
	gboolean is_hex;
	const gchar *byte_str;
	gint64 payload;
	
	parent = GTK_WINDOW(gtk_widget_get_native (GTK_WIDGET(self)));
	if (! GTK_IS_WINDOW(parent))
		parent = NULL;

	doc = hex_widget_get_document (priv->gh);
	cursor_pos = hex_widget_get_cursor (priv->gh);
	payload = hex_buffer_get_payload_size (hex_document_get_buffer (doc));

	entry = GTK_ENTRY(self->int_entry);
	buffer = gtk_entry_get_buffer (entry);
	byte_str = gtk_entry_buffer_get_text (buffer);

	len = strlen(byte_str);
	
	if (len > 1 && byte_str[0] == '+') {
		is_relative = 1;
		byte_str++;
		len--;
	}
	else if (len > 1 && byte_str[0] == '-') {
		is_relative = -1;
		byte_str++;
		len--;
	}
	
	if (len == 0) {
		display_dialog (parent, _("No offset has been specified."));
		return;
	}

	is_hex = ((len > 2) && (byte_str[0] == '0') && (byte_str[1] == 'x'));

	if (!is_hex) {
		for (i = 0; i < len; i++)
			if (!(byte_str[i] >= '0' && byte_str[i] <= '9')) 
				break;
	}
	else {
		for (i = 2; i < len; i++)
			if(!((byte_str[i] >= '0' && byte_str[i] <= '9') ||
				 (byte_str[i] >= 'A' && byte_str[i] <= 'F') ||
				 (byte_str[i] >= 'a' && byte_str[i] <= 'f')))
				break;
	}

	if((i == len) &&
	   ((sscanf(byte_str, "0x%lx", &byte) == 1) ||
		(sscanf(byte_str, "%ld", &byte) == 1))) {
		if(is_relative) {
			if(is_relative == -1 && byte > cursor_pos) {
				display_dialog (parent,
								 _("The specified offset is beyond the "
								" file boundaries."));
				return;
			}
			byte = byte * is_relative + cursor_pos;
		}
		if (byte >= payload) {
			display_dialog (parent,
								 _("Can not position cursor beyond the "
								   "end of file."));
			return;
		} else {
			/* SUCCESS */
			hex_widget_set_cursor (priv->gh, byte);
			gtk_widget_grab_focus (GTK_WIDGET(priv->gh));
		}
	}
	else {
		display_dialog (parent,
				_("You may only give the offset as:\n"
					"  - a positive decimal number, or\n"
					"  - a hex number, beginning with '0x', or\n"
					"  - a '+' or '-' sign, followed by a relative offset"));
		return;
	}
	g_signal_emit(self, signals[CLOSED], 0);
}

static void
replace_one_cb (GtkButton *button, gpointer user_data)
{
	ReplaceDialog *self = REPLACE_DIALOG(user_data);
	PaneDialogPrivate *priv;
	FindDialogPrivate *f_priv;
	GtkWindow *parent;
	HexDocument *doc;
	gint64 cursor_pos;
	char *find_str = NULL, *rep_str = NULL;
	size_t find_len, rep_str_len;
	gint64 payload;
	HexDocumentFindData *find_data = NULL;

	priv = pane_dialog_get_instance_private (PANE_DIALOG(self));
	f_priv = find_dialog_get_instance_private (FIND_DIALOG(self));

	parent = GTK_WINDOW(gtk_widget_get_native (GTK_WIDGET(self)));
	if (! GTK_IS_WINDOW(parent))
		parent = NULL;

	doc = hex_widget_get_document (priv->gh);
	cursor_pos = hex_widget_get_cursor (priv->gh);
	payload = hex_buffer_get_payload_size (hex_document_get_buffer (doc));
	
	if ((find_len = get_search_string (f_priv->f_doc, &find_str)) == 0	||
		(rep_str_len = get_search_string (self->r_doc, &rep_str)) == 0)
	{
		no_string_dialog (parent);
		goto clean_up;
	}
	
	if (find_len > payload - cursor_pos)
		goto clean_up;
	
	find_data = hex_document_find_data_new ();
	find_data->start = cursor_pos;
	find_data->what = find_str;
	find_data->len = find_len;
	find_data->flags = search_flags_from_checkboxes (f_priv);

	if (hex_document_find_forward_full (doc, find_data))
	{
		hex_widget_set_cursor (priv->gh, find_data->offset);
		cursor_pos = hex_widget_get_cursor (priv->gh);
		hex_document_set_data (doc,
				cursor_pos, rep_str_len, find_data->found_len, rep_str, TRUE);
	}
	else
	{
		display_dialog (parent, _("String was not found."));
	}

clean_up:
	g_free (find_str);
	g_free (rep_str);
	g_free (find_data);
}

static void
replace_all_cb (GtkButton *button, gpointer user_data)
{
	ReplaceDialog *self = REPLACE_DIALOG(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	PaneDialogPrivate *priv;
	FindDialogPrivate *f_priv;
	GtkWindow *parent;
	HexDocument *doc;
	gint64 cursor_pos;
	char *find_str = NULL, *rep_str = NULL;
	size_t find_len, rep_str_len;
	gint64 payload;
	HexDocumentFindData *find_data = NULL;
	int count;

	priv = pane_dialog_get_instance_private (PANE_DIALOG(self));
	f_priv = find_dialog_get_instance_private (FIND_DIALOG(self));

	parent = GTK_WINDOW(gtk_widget_get_native (GTK_WIDGET(self)));
	if (! GTK_IS_WINDOW(parent))
		parent = NULL;

	doc = hex_widget_get_document (priv->gh);
	cursor_pos = hex_widget_get_cursor (priv->gh);
	payload = hex_buffer_get_payload_size (hex_document_get_buffer (doc));

	if ((find_len = get_search_string (f_priv->f_doc, &find_str)) == 0	||
		(rep_str_len = get_search_string (self->r_doc, &rep_str)) == 0)
	{
		no_string_dialog (parent);
		goto clean_up;
	}

	if (find_len > payload - cursor_pos)
		goto clean_up;
	
	find_data = hex_document_find_data_new ();
	find_data->start = 0;
	find_data->what = find_str;
	find_data->len = find_len;
	find_data->flags = search_flags_from_checkboxes (f_priv);

	count = 0;
	while (hex_document_find_forward_full (doc, find_data))
	{
		hex_document_set_data (doc,
				find_data->offset, rep_str_len, find_data->found_len, rep_str,
				TRUE);
		count++;
	}
	
	if (count)
	{
		char *msg;
		msg = g_strdup_printf (_("Search complete: %d replacements made."),
					count);
		display_dialog (parent, msg);
		g_free (msg);
	}
	else
	{
		display_dialog (parent, _("No occurrences were found."));
	}
	
clean_up:
	g_free (find_str);
	g_free (rep_str);
	g_free (find_data);
}

/* CROSSREF: find_clear_cb */
static void
replace_clear_cb (GtkButton *button, gpointer user_data)
{
	ReplaceDialog *self = REPLACE_DIALOG(user_data);
	FindDialogPrivate *f_priv = find_dialog_get_instance_private (FIND_DIALOG(self));
	GtkWidget *new_r_gh;
	HexDocument *new_r_doc;

	new_r_doc = hex_document_new ();
	new_r_gh = create_hex_view (new_r_doc);

	gtk_box_remove (GTK_BOX(self->r_frame),
			gtk_widget_get_last_child (self->r_frame));
	gtk_box_append (GTK_BOX(self->r_frame), new_r_gh);

	self->r_doc = new_r_doc;
	self->r_gh = new_r_gh;

	find_options_show_pane_changed_cb (ADW_TOGGLE_GROUP(f_priv->options_show_pane), NULL, self);

	gtk_widget_grab_focus (GTK_WIDGET(self));
}

static gboolean
pane_dialog_key_press_cb (GtkEventControllerKey *controller,
               guint                  keyval,
               guint                  keycode,
               GdkModifierType        state,
               gpointer               user_data)
{
	PaneDialog *self = PANE_DIALOG(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);

	switch (keyval)
	{
		case GDK_KEY_Escape:
			g_signal_emit(self,
					signals[CLOSED],
					0);
			return TRUE;
			break;

		default:
			break;
	}
	return FALSE;
}


/* PRIVATE METHOD DEFINITIONS */

/* PaneDialog (generic base) */
static void
pane_dialog_init (PaneDialog *self)
{
	GtkEventController *controller;

	controller = gtk_event_controller_key_new ();

	g_signal_connect(controller, "key-pressed",
			G_CALLBACK(pane_dialog_key_press_cb),
			self);

	gtk_widget_add_controller (GTK_WIDGET(self),
			GTK_EVENT_CONTROLLER(controller));
}

static void
pane_dialog_dispose (GObject *object)
{
	PaneDialog *self = PANE_DIALOG(object);

	/* Chain up */
	G_OBJECT_CLASS(pane_dialog_parent_class)->dispose(object);
}


static void
pane_dialog_finalize (GObject *gobject)
{
	/* Chain up */
	G_OBJECT_CLASS(pane_dialog_parent_class)->finalize(gobject);
}


static void
pane_dialog_class_init (PaneDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	object_class->dispose = pane_dialog_dispose;
	object_class->finalize = pane_dialog_finalize;

	klass->closed = pane_dialog_real_close;

	gtk_widget_class_set_layout_manager_type (widget_class,
			GTK_TYPE_BOX_LAYOUT);

	/* SIGNALS */

	signals[CLOSED] = 
		g_signal_new ("closed",
					  G_OBJECT_CLASS_TYPE(object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (PaneDialogClass, closed),
					  NULL,
					  NULL,
					  NULL,
					  G_TYPE_NONE, 0);
}

/* Helper */
static void
pane_dialog_update_busy_state (PaneDialog *self)
{
	PaneDialogPrivate *priv = pane_dialog_get_instance_private (self);

	if (FIND_IS_DIALOG (self))
	{
		FindDialogPrivate *f_priv;
		gboolean busy;

		f_priv = find_dialog_get_instance_private (FIND_DIALOG(self));
		busy = gh_is_busy (priv->gh);
		gtk_widget_set_sensitive (f_priv->f_next, !busy);
		gtk_widget_set_sensitive (f_priv->f_prev, !busy);
	}
}

void
pane_dialog_set_hex (PaneDialog *self, HexWidget *gh)
{
	PaneDialogPrivate *priv;

	g_return_if_fail (PANE_IS_DIALOG (self));
	g_return_if_fail (HEX_IS_WIDGET (gh));

	priv = pane_dialog_get_instance_private (PANE_DIALOG(self));

	/* Clear auto-highlight if any.
	 */
	if (priv->auto_highlight)
		hex_widget_delete_autohighlight (priv->gh, priv->auto_highlight);
	priv->auto_highlight = NULL;

	priv->gh = gh;

	pane_dialog_update_busy_state (self);
}

/* (transfer none) */
HexWidget *
pane_dialog_get_hex (PaneDialog *self)
{
	PaneDialogPrivate *priv;

	g_return_val_if_fail (PANE_IS_DIALOG (self), NULL);

	priv = pane_dialog_get_instance_private (self);

	return priv->gh;
}

void
pane_dialog_close (PaneDialog *self)
{
	g_return_if_fail (PANE_IS_DIALOG (self));

	g_signal_emit(self, signals[CLOSED], 0);
}

/* FindDialog */

static void
find_dialog_init (FindDialog *self)
{
	FindDialogPrivate *f_priv = find_dialog_get_instance_private (self);
	GtkBuilder *builder;

	gtk_widget_init_template (GTK_WIDGET(self));

	f_priv->cancellable = g_cancellable_new ();

	/* Setup HexWidget and make child of our frame */
	f_priv->f_doc = hex_document_new ();
	f_priv->f_gh = create_hex_view (f_priv->f_doc);
	gtk_box_append (GTK_BOX(f_priv->find_frame), f_priv->f_gh);
	
	/* Setup find options popover as child of our options menubutton */
	builder = gtk_builder_new_from_resource (RESOURCE_BASE_PATH "/find-options.ui");
	f_priv->options_popover = GTK_WIDGET(
			gtk_builder_get_object (builder, "find_options_popover"));
	f_priv->options_regex = GTK_WIDGET(
			gtk_builder_get_object (builder, "find_options_regex"));
	f_priv->options_ignore_case = GTK_WIDGET(
			gtk_builder_get_object (builder, "find_options_ignore_case"));
	f_priv->options_show_pane = GTK_WIDGET(
			gtk_builder_get_object (builder, "find_options_show_pane"));

	gtk_menu_button_set_popover (GTK_MENU_BUTTON(f_priv->options_btn),
			f_priv->options_popover);

	g_object_unref (builder);

	/* Setup signals */
	g_signal_connect (f_priv->f_next, "clicked", G_CALLBACK(find_next_cb), self);
	g_signal_connect (f_priv->f_prev, "clicked", G_CALLBACK(find_prev_cb), self);
	g_signal_connect (f_priv->f_clear, "clicked", G_CALLBACK(find_clear_cb), self);
	g_signal_connect (f_priv->close, "clicked", G_CALLBACK(common_cancel_cb), self);
	g_signal_connect (f_priv->close, "clicked", G_CALLBACK(find_cancel_cb), self);
	g_signal_connect (f_priv->options_show_pane, "notify::active",
			G_CALLBACK(find_options_show_pane_changed_cb), self);
}

static gboolean 
find_dialog_grab_focus (GtkWidget *widget)
{
	FindDialog *self = FIND_DIALOG(widget);
	FindDialogPrivate *f_priv = find_dialog_get_instance_private (self);
	GtkWindow *parent = NULL;

	parent = GTK_WINDOW(gtk_widget_get_root (GTK_WIDGET(self)));
	if (GTK_IS_WINDOW(parent))
		gtk_window_set_default_widget (parent, f_priv->f_next);

	return gtk_widget_grab_focus (f_priv->f_gh);
}

static void
find_dialog_dispose (GObject *object)
{
	FindDialog *self = FIND_DIALOG(object);
	FindDialogPrivate *f_priv = find_dialog_get_instance_private (self);

	g_clear_object (&f_priv->cancellable);
	g_clear_pointer (&f_priv->vbox, gtk_widget_unparent);

	/* Chain up */
	G_OBJECT_CLASS(find_dialog_parent_class)->dispose(object);
}

static void
find_dialog_finalize(GObject *gobject)
{
	/* Chain up */
	G_OBJECT_CLASS(find_dialog_parent_class)->finalize(gobject);
}

static void
find_dialog_class_init (FindDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	object_class->dispose = find_dialog_dispose;
	object_class->finalize = find_dialog_finalize;

	widget_class->grab_focus = find_dialog_grab_focus;

	gtk_widget_class_set_template_from_resource (widget_class,
			RESOURCE_BASE_PATH "/find-dialog.ui");
	gtk_widget_class_bind_template_child_private (widget_class, FindDialog,
			vbox);
	gtk_widget_class_bind_template_child_private (widget_class, FindDialog,
			find_frame);
	gtk_widget_class_bind_template_child_private (widget_class, FindDialog,
			find_string_label);
	gtk_widget_class_bind_template_child_private (widget_class, FindDialog,
			hbox);
	gtk_widget_class_bind_template_child_private (widget_class, FindDialog,
			f_next);
	gtk_widget_class_bind_template_child_private (widget_class, FindDialog,
			f_prev);
	gtk_widget_class_bind_template_child_private (widget_class, FindDialog,
			f_clear);
	gtk_widget_class_bind_template_child_private (widget_class, FindDialog,
			options_btn);
	gtk_widget_class_bind_template_child_private (widget_class, FindDialog,
			close);
}

GtkWidget *
find_dialog_new (void)
{
	return g_object_new (FIND_TYPE_DIALOG, NULL);
}


/* ReplaceDialog */

static void
replace_dialog_init (ReplaceDialog *self)
{
	FindDialogPrivate *f_priv = find_dialog_get_instance_private (FIND_DIALOG(self));
	GtkBuilder *builder;

	self->r_doc = hex_document_new ();
	self->r_gh = create_hex_view (self->r_doc);

	/* Instantiate ReplaceDialog-specific widgets and plug them into the
	 * FindDialog in the right places.
	 */
	builder = gtk_builder_new_from_resource (RESOURCE_BASE_PATH "/replace-dialog.ui");

	gtk_widget_set_visible (f_priv->find_string_label, TRUE);

	self->r_frame = GTK_WIDGET(gtk_builder_get_object (builder, "r_frame"));
	gtk_box_append (GTK_BOX(self->r_frame), self->r_gh);
	gtk_box_insert_child_after (GTK_BOX(f_priv->vbox), self->r_frame, f_priv->find_frame);

	self->replace = GTK_WIDGET(gtk_builder_get_object (builder, "replace"));
	gtk_box_insert_child_after (GTK_BOX(f_priv->hbox), self->replace, f_priv->f_prev);

	self->replace_all = GTK_WIDGET(gtk_builder_get_object (builder, "replace_all"));
	gtk_box_insert_child_after (GTK_BOX(f_priv->hbox), self->replace_all, self->replace);

	g_object_unref (builder);

	/* Setup signals */
	g_signal_connect (self->replace, "clicked", G_CALLBACK(replace_one_cb), self);
	g_signal_connect (self->replace_all, "clicked", G_CALLBACK(replace_all_cb), self);
	g_signal_connect (f_priv->f_clear, "clicked", G_CALLBACK(replace_clear_cb), self);
}

static void
replace_dialog_dispose(GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS(replace_dialog_parent_class)->dispose(object);
}

static void
replace_dialog_finalize(GObject *gobject)
{
	/* Chain up */
	G_OBJECT_CLASS(replace_dialog_parent_class)->finalize(gobject);
}

static void
replace_dialog_class_init (ReplaceDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	object_class->dispose = replace_dialog_dispose;
	object_class->finalize = replace_dialog_finalize;
}

GtkWidget *
replace_dialog_new(void)
{
	return g_object_new(REPLACE_TYPE_DIALOG, NULL);
}

/* JumpDialog */

static void
jump_dialog_init (JumpDialog *self)
{
	GtkWidget *widget = GTK_WIDGET(self);

	/* Widget - template, signals, etc. */

	gtk_widget_init_template (widget);

	/* In GTK4, you can't expect GtkEntry itself to report correctly whether
	 * it has focus, (has-focus will == FALSE even though it looks focused...
	 * so we add an event controller manually and track that. Thanks to mclasen
	 */
	self->focus_controller = gtk_event_controller_focus_new ();
	gtk_widget_add_controller (self->int_entry, self->focus_controller);
	
	g_signal_connect (self->ok, "clicked", G_CALLBACK(goto_byte_cb), self);
	g_signal_connect (self->cancel, "clicked", G_CALLBACK(common_cancel_cb), self);
}

static gboolean 
jump_dialog_grab_focus (GtkWidget *widget)
{
	JumpDialog *self = JUMP_DIALOG(widget);
	gboolean retval;
	GtkWindow *parent = NULL;

	parent = GTK_WINDOW(gtk_widget_get_root (GTK_WIDGET(self)));
	if (GTK_IS_WINDOW(parent))
		gtk_window_set_default_widget (parent, self->ok);

	if (gtk_event_controller_focus_contains_focus (
				GTK_EVENT_CONTROLLER_FOCUS(self->focus_controller)))
		retval = FALSE;
	else
		retval = gtk_widget_grab_focus (self->int_entry);

	return retval;
}

static void
jump_dialog_dispose (GObject *object)
{
	JumpDialog *self = JUMP_DIALOG(object);

	g_clear_pointer (&self->box, gtk_widget_unparent);

	/* Chain up */
	G_OBJECT_CLASS(jump_dialog_parent_class)->dispose(object);
}

static void
jump_dialog_finalize (GObject *gobject)
{
	/* Chain up */
	G_OBJECT_CLASS(jump_dialog_parent_class)->finalize(gobject);
}

static void
jump_dialog_class_init (JumpDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	object_class->dispose = jump_dialog_dispose;
	object_class->finalize = jump_dialog_finalize;

	widget_class->grab_focus = jump_dialog_grab_focus;

	gtk_widget_class_set_css_name (widget_class, JUMP_DIALOG_CSS_NAME);

	gtk_widget_class_set_template_from_resource (widget_class,
			RESOURCE_BASE_PATH "/jump-dialog.ui");
	gtk_widget_class_bind_template_child (widget_class, JumpDialog, box);
	gtk_widget_class_bind_template_child (widget_class, JumpDialog, label);
	gtk_widget_class_bind_template_child (widget_class, JumpDialog, int_entry);
	gtk_widget_class_bind_template_child (widget_class, JumpDialog, ok);
	gtk_widget_class_bind_template_child (widget_class, JumpDialog, cancel);
}

GtkWidget *
jump_dialog_new(void)
{
	return g_object_new(JUMP_TYPE_DIALOG, NULL);
}
