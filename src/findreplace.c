/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* findreplace.c - finding & replacing data

   Copyright © 1998 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

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
	GtkHex *gh;
	GtkHex_AutoHighlight *auto_highlight;

} PaneDialogPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (PaneDialog, pane_dialog, GTK_TYPE_WIDGET)

typedef struct {
	HexDocument *f_doc;
	GtkWidget *f_gh;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *f_next, *f_prev, *f_clear;
	GtkWidget *close;

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
static size_t get_search_string (HexDocument *doc, gchar **str);


static GtkWidget *
create_hex_view (HexDocument *doc)
{
	GtkWidget *gh;

	gh = gtk_hex_new (doc);
	g_object_ref (gh);

	gtk_widget_set_hexpand (gh, TRUE);
	gtk_hex_set_group_type (GTK_HEX(gh), def_group_type);
	common_set_gtkhex_font_from_settings (GTK_HEX(gh));
    gtk_hex_set_insert_mode (GTK_HEX(gh), TRUE);
    gtk_hex_set_geometry (GTK_HEX(gh), 16, 4);

    return gh;
}

static size_t
get_search_string (HexDocument *doc, char **str)
{
	size_t size = hex_buffer_get_payload_size (hex_document_get_buffer (doc));
	
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
		gtk_hex_delete_autohighlight (priv->gh, priv->auto_highlight);
	}
	priv->auto_highlight = NULL;

	gtk_widget_hide (GTK_WIDGET(self));
}

static void
common_cancel_cb (GtkButton *button, gpointer user_data)
{
	PaneDialog *self = PANE_DIALOG(user_data);

	(void)button;	/* unused */
	g_return_if_fail (PANE_IS_DIALOG (self));

	g_signal_emit(self,
			signals[CLOSED],
			0);	/* GQuark detail (just set to 0 if unknown) */
}

/* Small helper function. */
static void
no_string_dialog (GtkWindow *parent)
{
	display_error_dialog (parent,
			_("No string provided."));
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
	int cursor_pos;
	int str_len;
	size_t offset;
	char *str;
	static gboolean found = FALSE;
	
	g_return_if_fail (FIND_IS_DIALOG(self));

	priv = pane_dialog_get_instance_private (PANE_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX (priv->gh));
	f_priv = find_dialog_get_instance_private (self);
	g_return_if_fail (HEX_IS_DOCUMENT (f_priv->f_doc));

	parent = GTK_WINDOW(gtk_widget_get_native (widget));
	if (! GTK_IS_WINDOW(parent))
		parent = NULL;

	doc = gtk_hex_get_document (priv->gh);
	cursor_pos = gtk_hex_get_cursor (priv->gh);

	if ((str_len = get_search_string (f_priv->f_doc, &str)) == 0)
	{
		no_string_dialog (parent);
		return;
	}

	/* Insert auto-highlights of search string */

	if (priv->auto_highlight)
		gtk_hex_delete_autohighlight (priv->gh, priv->auto_highlight);

	priv->auto_highlight = NULL;
	priv->auto_highlight = gtk_hex_insert_autohighlight(priv->gh,
			str, str_len);

	/* Search for requested string */
	
	if (direction == FIND_FORWARD 
		?
			hex_document_find_forward (doc,
				found == FALSE ? cursor_pos : cursor_pos + 1,
				str, str_len, &offset)
			
		:
			hex_document_find_backward (doc,
				cursor_pos, str, str_len, &offset)
			)
	{
		found = TRUE;
		gtk_hex_set_cursor (priv->gh, offset);
		gtk_widget_grab_focus (GTK_WIDGET(priv->gh));
	}
	else
	{
		display_info_dialog (parent, found ? found_msg : not_found_msg);

		found = FALSE;
	}
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

static void
find_clear_cb (GtkButton *button, gpointer user_data)
{
	FindDialog *self = FIND_DIALOG(user_data);
	FindDialogPrivate *f_priv;
	GtkWidget *widget = GTK_WIDGET(user_data);
	GtkWidget *new_gh;
	HexDocument *new_doc;

	(void)user_data;

	g_return_if_fail (FIND_IS_DIALOG (self));

	f_priv = find_dialog_get_instance_private (self);
	g_return_if_fail (GTK_IS_HEX (f_priv->f_gh));
	g_return_if_fail (HEX_IS_DOCUMENT (f_priv->f_doc));

	new_doc = hex_document_new ();
	new_gh = create_hex_view (new_doc);

	gtk_frame_set_child (GTK_FRAME(f_priv->frame), new_gh);

	f_priv->f_doc = new_doc;
	f_priv->f_gh = new_gh;

	gtk_widget_grab_focus (widget);
}

static void
goto_byte_cb (GtkButton *button, gpointer user_data)
{
	JumpDialog *self = JUMP_DIALOG(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	PaneDialogPrivate *priv;
	GtkWindow *parent;
	HexDocument *doc;
	int cursor_pos;
	GtkEntry *entry;
	GtkEntryBuffer *buffer;
	int byte = 2, len, i;
	int is_relative = 0;
	gboolean is_hex;
	const gchar *byte_str;
	size_t payload;
	
	(void)button;	/* unused */

	g_return_if_fail (JUMP_IS_DIALOG(self));

	priv = pane_dialog_get_instance_private (PANE_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX(priv->gh));
	
	parent = GTK_WINDOW(gtk_widget_get_native (widget));
	if (! GTK_IS_WINDOW(parent))
		parent = NULL;

	doc = gtk_hex_get_document(priv->gh);
	cursor_pos = gtk_hex_get_cursor(priv->gh);
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
		display_error_dialog (parent, _("No offset has been specified."));
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
	   ((sscanf(byte_str, "0x%x", &byte) == 1) ||
		(sscanf(byte_str, "%d", &byte) == 1))) {
		if(is_relative) {
			if(is_relative == -1 && byte > cursor_pos) {
				display_error_dialog(parent,
								 _("The specified offset is beyond the "
								" file boundaries."));
				return;
			}
			byte = byte * is_relative + cursor_pos;
		}
		if (byte >= payload) {
			display_error_dialog(parent,
								 _("Can not position cursor beyond the "
								   "end of file."));
		} else {
			/* SUCCESS */
			gtk_hex_set_cursor (priv->gh, byte);
			gtk_widget_grab_focus (GTK_WIDGET(priv->gh));
		}
	}
	else {
		display_error_dialog(parent,
				_("You may only give the offset as:\n"
					"  - a positive decimal number, or\n"
					"  - a hex number, beginning with '0x', or\n"
					"  - a '+' or '-' sign, followed by a relative offset"));
	}
}

static void
replace_one_cb (GtkButton *button, gpointer user_data)
{
	ReplaceDialog *self = REPLACE_DIALOG(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	PaneDialogPrivate *priv;
	FindDialogPrivate *f_priv;
	GtkWindow *parent;
	HexDocument *doc;
	int cursor_pos;
	char *find_str = NULL, *rep_str = NULL;
	int find_len, rep_len;
	size_t offset;
	size_t payload;

	(void)button;	/* unused */
	g_return_if_fail (REPLACE_IS_DIALOG(self));

	priv = pane_dialog_get_instance_private (PANE_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX (priv->gh));

	f_priv = find_dialog_get_instance_private (FIND_DIALOG(self));
	g_return_if_fail (HEX_IS_DOCUMENT (f_priv->f_doc));
	g_return_if_fail (HEX_IS_DOCUMENT (self->r_doc));

	parent = GTK_WINDOW(gtk_widget_get_native (widget));
	if (! GTK_IS_WINDOW(parent))
		parent = NULL;

	doc = gtk_hex_get_document (priv->gh);
	cursor_pos = gtk_hex_get_cursor (priv->gh);
	payload = hex_buffer_get_payload_size (hex_document_get_buffer (doc));
	
	if ((find_len = get_search_string(f_priv->f_doc, &find_str)) == 0)
	{
		no_string_dialog (parent);
		return;
	}
	rep_len = get_search_string(self->r_doc, &rep_str);
	
	if (find_len > payload - cursor_pos)
		goto clean_up;
	
	if (hex_document_compare_data(doc, find_str, cursor_pos, find_len) == 0)
	{
		hex_document_set_data(doc, cursor_pos,
							  rep_len, find_len, rep_str, TRUE);
	}
	
	if (hex_document_find_forward(doc, cursor_pos + rep_len,
				find_str, find_len, &offset))
	{
		gtk_hex_set_cursor(priv->gh, offset);
	}
	else {
		display_info_dialog (parent, _("String was not found."));
	}

clean_up:
	if (find_str)
		g_free(find_str);

	if(rep_str)
		g_free(rep_str);
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
	int cursor_pos;
	char *find_str = NULL, *rep_str = NULL;
	size_t find_len, rep_len, count;
	size_t offset;
	size_t payload;

	(void)button;	/* unused */
	g_return_if_fail (REPLACE_IS_DIALOG (self));

	priv = pane_dialog_get_instance_private (PANE_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX (priv->gh));
	f_priv = find_dialog_get_instance_private (FIND_DIALOG(self));
	g_return_if_fail (HEX_IS_DOCUMENT (f_priv->f_doc));
	g_return_if_fail (HEX_IS_DOCUMENT (self->r_doc));

	parent = GTK_WINDOW(gtk_widget_get_native (widget));
	if (! GTK_IS_WINDOW(parent))
		parent = NULL;

	doc = gtk_hex_get_document (priv->gh);
	cursor_pos = gtk_hex_get_cursor (priv->gh);
	payload = hex_buffer_get_payload_size (hex_document_get_buffer (doc));

	if ((find_len = get_search_string(f_priv->f_doc, &find_str)) == 0)
	{
		no_string_dialog (parent);
		return;
	}
	rep_len = get_search_string(self->r_doc, &rep_str);

	if (find_len > payload - (unsigned)cursor_pos)
		goto clean_up;
	
	count = 0;
	cursor_pos = 0;  

	while(hex_document_find_forward(doc, cursor_pos, find_str, find_len,
									&offset)) {
		hex_document_set_data (doc, offset, rep_len, find_len, rep_str, TRUE);
		cursor_pos = offset + rep_len;
		count++;
	}
	
	gtk_hex_set_cursor(priv->gh, MIN(offset, payload));  

	if (count == 0) {
		display_info_dialog (parent, _("No occurrences were found."));
	}
	
clean_up:
	if (find_str)
		g_free(find_str);

	if (rep_str)
		g_free(rep_str);
}

static void
replace_clear_cb (GtkButton *button, gpointer user_data)
{
	ReplaceDialog *self = REPLACE_DIALOG(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	GtkWidget *new_r_gh;
	HexDocument *new_r_doc;

	g_return_if_fail (GTK_IS_HEX (self->r_gh));
	g_return_if_fail (HEX_IS_DOCUMENT (self->r_doc));

	new_r_doc = hex_document_new ();
	new_r_gh = create_hex_view (new_r_doc);

	gtk_frame_set_child (GTK_FRAME(self->r_frame), new_r_gh);

	self->r_doc = new_r_doc;
	self->r_gh = new_r_gh;

	gtk_widget_grab_focus (widget);
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

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(pane_dialog_parent_class)->dispose(object);
}


static void
pane_dialog_finalize (GObject *gobject)
{
	/* here, you would free stuff. I've got nuthin' for ya. */

	/* --- */

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(pane_dialog_parent_class)->finalize(gobject);
}


static void
pane_dialog_class_init (PaneDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	/* <boilerplate> */
	object_class->dispose = pane_dialog_dispose;
	object_class->finalize = pane_dialog_finalize;
	/* </boilerplate> */

	klass->closed = pane_dialog_real_close;

	/* set the box-type layout manager for this Pane dialog widget.
	 */
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

void
pane_dialog_set_hex (PaneDialog *self, GtkHex *gh)
{
	PaneDialogPrivate *priv;

	g_return_if_fail (PANE_IS_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX(gh));

	priv = pane_dialog_get_instance_private (PANE_DIALOG(self));

	g_debug("%s: setting GtkHex of PaneDialog %p to: %p",
			__func__, (void *)self, (void *)gh);

	/* Clear auto-highlight if any.
	 */
	if (priv->auto_highlight)
		gtk_hex_delete_autohighlight (priv->gh, priv->auto_highlight);
	priv->auto_highlight = NULL;

	priv->gh = gh;
}

void
pane_dialog_close (PaneDialog *self)
{
	g_return_if_fail (PANE_IS_DIALOG (self));

	g_signal_emit(self,
			signals[CLOSED],
			0);	/* GQuark detail (just set to 0 if unknown) */
}

/* FindDialog */

static void
find_dialog_init (FindDialog *self)
{
	FindDialogPrivate *f_priv = find_dialog_get_instance_private (self);

	/* Setup our root container. */
	f_priv->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_parent (f_priv->vbox, GTK_WIDGET(self));

	f_priv->frame = gtk_frame_new(_("Find String"));
	f_priv->f_doc = hex_document_new();
	f_priv->f_gh = create_hex_view(f_priv->f_doc);
	gtk_frame_set_child (GTK_FRAME(f_priv->frame), f_priv->f_gh);
	gtk_box_append (GTK_BOX(f_priv->vbox), f_priv->frame);
	gtk_accessible_update_property (GTK_ACCESSIBLE(f_priv->frame),
			GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
			_("Enter the hex data or ASCII data to search for"),
			-1);
	
	f_priv->hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_append (GTK_BOX(f_priv->vbox), f_priv->hbox);

	f_priv->f_next = gtk_button_new_with_mnemonic (_("Find _Next"));
	g_signal_connect (G_OBJECT (f_priv->f_next), "clicked",
					  G_CALLBACK(find_next_cb), self);
	gtk_widget_set_receives_default (f_priv->f_next, TRUE);
	gtk_box_append (GTK_BOX(f_priv->hbox), f_priv->f_next);
	gtk_accessible_update_property (GTK_ACCESSIBLE(f_priv->f_next),
			GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
			_("Finds the next occurrence of the search string"),
			-1);

	f_priv->f_prev = gtk_button_new_with_mnemonic (_("Find _Previous"));
	g_signal_connect (G_OBJECT (f_priv->f_prev), "clicked",
					  G_CALLBACK(find_prev_cb), self);
	gtk_box_append (GTK_BOX(f_priv->hbox), f_priv->f_prev);
	gtk_accessible_update_property (GTK_ACCESSIBLE(f_priv->f_prev),
			GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
			_("Finds the previous occurrence of the search string"),
			-1);

	f_priv->f_clear = gtk_button_new_with_mnemonic (_("_Clear"));
	g_signal_connect (G_OBJECT (f_priv->f_clear), "clicked",
					  G_CALLBACK(find_clear_cb), self);
	gtk_box_append (GTK_BOX(f_priv->hbox), f_priv->f_clear);
	gtk_accessible_update_property (GTK_ACCESSIBLE(f_priv->f_clear),
			GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
			_("Clears the data you are searching for"),
			-1);

	f_priv->close = gtk_button_new_from_icon_name ("window-close-symbolic");
	gtk_button_set_has_frame (GTK_BUTTON(f_priv->close), FALSE);
	gtk_widget_set_hexpand (f_priv->close, TRUE);
	gtk_widget_set_halign (f_priv->close, GTK_ALIGN_END);
	g_signal_connect (G_OBJECT (f_priv->close), "clicked",
			G_CALLBACK(common_cancel_cb), self);
	gtk_box_append (GTK_BOX(f_priv->hbox), f_priv->close);
	gtk_accessible_update_property (GTK_ACCESSIBLE(f_priv->close),
			GTK_ACCESSIBLE_PROPERTY_LABEL,
			_("Close"),
			-1);
	gtk_accessible_update_property (GTK_ACCESSIBLE(f_priv->close),
			GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
			_("Closes the find pane"),
			-1);
}

static gboolean 
find_dialog_grab_focus (GtkWidget *widget)
{
	FindDialog *self = FIND_DIALOG(widget);
	FindDialogPrivate *f_priv = find_dialog_get_instance_private (self);

	if (gtk_widget_has_focus (f_priv->f_gh))
		return FALSE;

	return gtk_widget_grab_focus (f_priv->f_gh);
}

static void
find_dialog_dispose(GObject *object)
{
	FindDialog *self = FIND_DIALOG(object);
	FindDialogPrivate *f_priv = find_dialog_get_instance_private (self);

	g_clear_pointer (&f_priv->vbox, gtk_widget_unparent);

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(find_dialog_parent_class)->dispose(object);
}

static void
find_dialog_finalize(GObject *gobject)
{
	/* here, you would free stuff. I've got nuthin' for ya. */

	/* --- */

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(find_dialog_parent_class)->finalize(gobject);
}

static void
find_dialog_class_init (FindDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	/* <boilerplate> */
	object_class->dispose = find_dialog_dispose;
	object_class->finalize = find_dialog_finalize;
	/* </boilerplate> */

	widget_class->grab_focus = find_dialog_grab_focus;
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
	FindDialogPrivate *f_priv =
		find_dialog_get_instance_private (FIND_DIALOG(self));

	self->r_doc = hex_document_new();
	self->r_gh = create_hex_view (self->r_doc);

	self->r_frame = gtk_frame_new(_("Replace With"));
	gtk_frame_set_child (GTK_FRAME(self->r_frame), self->r_gh);
	gtk_box_insert_child_after (GTK_BOX(f_priv->vbox),
			self->r_frame, f_priv->frame);
	gtk_accessible_update_property (GTK_ACCESSIBLE(self->r_frame),
			GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
			("Enter the hex data or ASCII data to replace with"),
			-1);

	self->replace = gtk_button_new_with_mnemonic (_("_Replace"));
	g_signal_connect (G_OBJECT (self->replace),
					  "clicked", G_CALLBACK(replace_one_cb),
					  self);
	gtk_box_insert_child_after (GTK_BOX(f_priv->hbox),
			self->replace, f_priv->f_prev);
	gtk_accessible_update_property (GTK_ACCESSIBLE(self->replace),
			GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
			("Replaces the search string with the replace string"),
			-1);

	self->replace_all = gtk_button_new_with_mnemonic (_("Replace _All"));
	g_signal_connect (G_OBJECT (self->replace_all),
					  "clicked", G_CALLBACK(replace_all_cb),
					  self);
	gtk_box_insert_child_after (GTK_BOX(f_priv->hbox),
			self->replace_all, self->replace);
	gtk_accessible_update_property (GTK_ACCESSIBLE(self->replace_all),
			GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
			_("Replaces all occurrences of the search string with the replace string"),
			-1);

	g_signal_connect (G_OBJECT (f_priv->f_clear),
					  "clicked", G_CALLBACK(replace_clear_cb),
					  self);
}

static void
replace_dialog_dispose(GObject *object)
{
	ReplaceDialog *self = REPLACE_DIALOG(object);

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(replace_dialog_parent_class)->dispose(object);
}

static void
replace_dialog_finalize(GObject *gobject)
{
	/* here, you would free stuff. I've got nuthin' for ya. */

	/* --- */

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(replace_dialog_parent_class)->finalize(gobject);
}

static void
replace_dialog_class_init (ReplaceDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	/* <boilerplate> */
	object_class->dispose = replace_dialog_dispose;
	object_class->finalize = replace_dialog_finalize;
	/* </boilerplate> */
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
	GtkStyleContext *context;
	GtkCssProvider *provider;

	/* CSS */

	context = gtk_widget_get_style_context (widget);
	provider = gtk_css_provider_new ();

	gtk_css_provider_load_from_data (provider,
									 JUMP_DIALOG_CSS_NAME " {\n"
									 "   padding-left: 20px;\n"
									 "   padding-top: 6px;\n"
									 "   padding-bottom: 6px;\n"
									 "}\n", -1);

	/* add the provider to our widget's style context. */
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);

	/* Widget */

	self->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_parent (self->box, widget);
	
	self->label = gtk_label_new (_("Jump to byte (enter offset):"));
	self->int_entry = gtk_entry_new ();
	gtk_accessible_update_property (GTK_ACCESSIBLE(self->int_entry),
		GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
		_("Enter the offset byte to jump to. The default is decimal format, but "
			"other format strings are supported such as hexidecimal format, if "
			"C-style notation using the '0x' prefix is used. If your string is "
			"not recognized, a dialog will be presented explaining the valid "
			"formats of strings accepted."),
			-1);

	/* In GTK4, you can't expect GtkEntry itself to report correctly whether
	 * it has focus, (has-focus will == FALSE even though it looks focused...
	 * so we add an event controller manually and track that. Thanks to mclasen
	 */
	self->focus_controller = gtk_event_controller_focus_new ();
	gtk_widget_add_controller (self->int_entry, self->focus_controller);
	
	gtk_box_append (GTK_BOX(self->box), self->label);
	gtk_box_append (GTK_BOX(self->box), self->int_entry);

	self->ok = gtk_button_new_with_mnemonic (_("_Jump"));
	g_signal_connect (G_OBJECT (self->ok),
					  "clicked", G_CALLBACK(goto_byte_cb),
					  self);
	gtk_box_append (GTK_BOX(self->box), self->ok);
	gtk_widget_set_receives_default (self->ok, TRUE);
	gtk_accessible_update_property (GTK_ACCESSIBLE(self->ok),
			GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
			_("Jumps to the specified byte"),
			-1);

	self->cancel = gtk_button_new_from_icon_name ("window-close-symbolic");
	gtk_widget_set_hexpand (self->cancel, TRUE);
	gtk_widget_set_halign (self->cancel, GTK_ALIGN_END);
	gtk_button_set_has_frame (GTK_BUTTON(self->cancel), FALSE);
	g_signal_connect (G_OBJECT (self->cancel),
					  "clicked", G_CALLBACK(common_cancel_cb),
					  self);
	gtk_box_append (GTK_BOX(self->box), self->cancel);

	gtk_accessible_update_property (GTK_ACCESSIBLE(self->cancel),
			GTK_ACCESSIBLE_PROPERTY_LABEL,
			_("Close"),
			-1);
	gtk_accessible_update_property (GTK_ACCESSIBLE(self->cancel),
			GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
			_("Closes the jump-to-byte pane"),
			-1);
}

static gboolean 
jump_dialog_grab_focus (GtkWidget *widget)
{
	JumpDialog *self = JUMP_DIALOG(widget);
	gboolean retval;

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

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(jump_dialog_parent_class)->dispose(object);
}

static void
jump_dialog_finalize (GObject *gobject)
{
	/* here, you would free stuff. I've got nuthin' for ya. */

	/* --- */

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(jump_dialog_parent_class)->finalize(gobject);
}

static void
jump_dialog_class_init (JumpDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	/* <boilerplate> */
	object_class->dispose = jump_dialog_dispose;
	object_class->finalize = jump_dialog_finalize;
	/* </boilerplate> */

	widget_class->grab_focus = jump_dialog_grab_focus;

	/* CSS */
	gtk_widget_class_set_css_name (widget_class, JUMP_DIALOG_CSS_NAME);
}

GtkWidget *
jump_dialog_new(void)
{
	return g_object_new(JUMP_TYPE_DIALOG, NULL);
}
