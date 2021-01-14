/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* findreplace.c - finding & replacing data

   Copyright © 1998 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021 Logan Rathbone

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "findreplace.h"
//#include "ui.h"
#include "gtkhex.h"
#include "configuration.h"

/* DEFINES */
#define JUMP_DIALOG_CSS_NAME "jumpdialog"

/* GOBJECT DEFINITIONS */

struct _JumpDialog {
	GtkWidget parent_instance;

	GtkHex *gh;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *int_entry;
	GtkWidget *ok, *cancel;
};

G_DEFINE_TYPE (JumpDialog, jump_dialog, GTK_TYPE_WIDGET)

struct _FindDialog {
	GtkWidget parent_instance;

	GtkHex *gh;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	HexDocument *f_doc;
	GtkWidget *f_gh;
	GtkWidget *f_next, *f_prev, *f_close;
	
	GtkHex_AutoHighlight *auto_highlight;
};

G_DEFINE_TYPE (FindDialog, find_dialog, GTK_TYPE_WIDGET)

struct _ReplaceDialog {
	GtkWidget parent_instance;

	GtkHex *gh;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *f_frame, *r_frame;
	GtkWidget *f_gh, *r_gh;
	HexDocument *f_doc, *r_doc;
	GtkWidget *replace, *replace_all, *next, *close;
	
	GtkHex_AutoHighlight *auto_highlight;
}; 

G_DEFINE_TYPE (ReplaceDialog, replace_dialog, GTK_TYPE_WIDGET)

/* PRIVATE FUNCTION DECLARATIONS */

//static gint find_delete_event_cb(GtkWidget *w, GdkEventAny *e,
//		FindDialog *dialog);
static void find_cancel_cb(GtkButton *button, gpointer user_data);
static void cancel_cb (GtkButton *button, gpointer user_data);
static void find_next_cb(GtkButton *button, gpointer user_data);
static void find_prev_cb(GtkButton *button, gpointer user_data);
static void replace_next_cb(GtkButton *button, gpointer user_data);
static void replace_one_cb(GtkButton *button, gpointer user_data);
static void replace_all_cb(GtkButton *button, gpointer user_data);
static void goto_byte_cb(GtkButton *button, gpointer user_data);
static gint get_search_string(HexDocument *doc, gchar **str);


static GtkWidget *
create_hex_view(HexDocument *doc)
{
    GtkWidget *gh = hex_document_add_view(doc);

	// TEST
	gtk_hex_set_group_type(GTK_HEX(gh), GROUP_BYTE);
//	gtk_hex_set_group_type(GTK_HEX(gh), def_group_type);

	// FIXME - JUST DELETE?
#if 0
    if (def_metrics && def_font_desc) {
	    gtk_hex_set_font(GTK_HEX(gh), def_metrics, def_font_desc);
    }
#endif

    gtk_hex_set_insert_mode(GTK_HEX(gh), TRUE);
    gtk_hex_set_geometry(GTK_HEX(gh), 16, 4);

    return gh;
}

static gint get_search_string(HexDocument *doc, gchar **str)
{
	guint size = doc->file_size;
	
	if(size > 0)
		*str = (gchar *)hex_document_get_data(doc, 0, size);
	else
		*str = NULL;

	return size;
}

#if 0
/* find and advanced find need special close dialogs, since they
 * need to do stuff with the highlights
 */
static gint find_delete_event_cb(GtkWidget *w, GdkEventAny *e, FindDialog *dialog)
{
	GHexWindow *win = ghex_window_get_active();
	GtkHex *gh = win->gh;
	
	if (dialog->auto_highlight) gtk_hex_delete_autohighlight(gh, dialog->auto_highlight);
	dialog->auto_highlight = NULL;
	gtk_widget_hide(w);

	return TRUE;
}
#endif

// FIXME - this is kind of a crappy hack to get this to build
// Revisit.
static void
cancel_cb (GtkButton *button, gpointer user_data)
{
	GtkWidget *widget = GTK_WIDGET(user_data);

	g_return_if_fail (GTK_IS_WIDGET(widget));

	(void)button;	/* unused */

	gtk_widget_hide (widget);
}

static void
find_cancel_cb(GtkButton *button, gpointer user_data)
{
	FindDialog *self = FIND_DIALOG(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	
	g_return_if_fail (FIND_IS_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX(self->gh));

	(void)button;	/* unused */

	if (self->auto_highlight)
		gtk_hex_delete_autohighlight(self->gh, self->auto_highlight);

	self->auto_highlight = NULL;

	gtk_widget_hide(self);
}

static void
find_next_cb(GtkButton *button, gpointer user_data)
{
	FindDialog *self = FIND_DIALOG(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	HexDocument *doc;
	guint cursor_pos;
	guint offset, str_len;
	gchar *str;
	
	g_return_if_fail (FIND_IS_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX(self->gh));

	(void)button;	/* unused */

	doc = gtk_hex_get_document(self->gh);
	cursor_pos = gtk_hex_get_cursor(self->gh);

	if ((str_len = get_search_string(self->f_doc, &str)) == 0) {
		g_debug(_("There is no string to search for!"));
		// FIXME
//		display_error_dialog (self, _("There is no string to search for!"));
		return;
	}

	if (self->auto_highlight)
		gtk_hex_delete_autohighlight(self->gh, self->auto_highlight);

	self->auto_highlight = NULL;
	self->auto_highlight = gtk_hex_insert_autohighlight(self->gh,
			str, str_len);
	// FIXME - oh dang maybe I shouldn't gave got rid of this - maybe
	// there's a way to replicate this with states. :(
//			, "red");

	if (hex_document_find_forward(doc,
								 cursor_pos + 1,
								 str, str_len, &offset))
	{
		gtk_hex_set_cursor(self->gh, offset);
	}
	else {
		// FIXME - NOT IMPLEMENTED
//		ghex_window_flash(win, _("End Of File reached"));
//		display_info_dialog(self, _("String was not found!\n"));
	}

	if (str)
		g_free(str);
}

static void
find_prev_cb(GtkButton *button, gpointer user_data)
{
	FindDialog *self = FIND_DIALOG(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	HexDocument *doc;
	guint cursor_pos;
	guint offset, str_len;
	gchar *str;
	
	g_return_if_fail (FIND_IS_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX(self->gh));

	(void)button;	/* unused */

	doc = gtk_hex_get_document(self->gh);
	cursor_pos = gtk_hex_get_cursor(self->gh);

	if ((str_len = get_search_string(self->f_doc, &str)) == 0) {
		// FIXME
//		display_error_dialog (self, _("There is no string to search for!"));
		return;
	}

	if (self->auto_highlight)
		gtk_hex_delete_autohighlight(self->gh, self->auto_highlight);

	self->auto_highlight = NULL;
	self->auto_highlight = gtk_hex_insert_autohighlight(self->gh,
			str, str_len);
	// FIXME - restore our purdy colours
			//, "red");

	if (hex_document_find_backward(doc,
				cursor_pos, str, str_len, &offset))
	{
		gtk_hex_set_cursor(self->gh, offset);
	}
	else {
		// FIXME - NOT IMPLEMENTED
//		ghex_window_flash(win, _("Beginning Of File reached"));
//		display_info_dialog(self, _("String was not found!\n"));
	}
	if (str)
		g_free(str);
}

static void
goto_byte_cb(GtkButton *button, gpointer user_data)
{
	JumpDialog *self = JUMP_DIALOG(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	HexDocument *doc;
	guint cursor_pos;
	GtkEntry *entry;
	GtkEntryBuffer *buffer;
	guint byte = 2, len, i;
	gint is_relative = 0;
	gboolean is_hex;
	const gchar *byte_str;
	
	g_return_if_fail (JUMP_IS_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX(self->gh));

	(void)button;	/* unused */

	doc = gtk_hex_get_document(self->gh);
	cursor_pos = gtk_hex_get_cursor(self->gh);

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
		// FIXME
//		display_error_dialog (self, _("No offset has been specified!"));
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
				// FIXME
#if 0
				display_error_dialog(self,
								 _("The specified offset is beyond the "
								" file boundaries!"));
#endif
				return;
			}
			byte = byte * is_relative + cursor_pos;
		}
		if (byte >= doc->file_size) {
			// FIXME
#if 0
			display_error_dialog(self,
								 _("Can not position cursor beyond the "
								   "End Of File!"));
#endif
		} else {
			gtk_hex_set_cursor(self->gh, byte);
		}
	}
	else {
		// FIXME
#if 0
		display_error_dialog(self,
				_("You may only give the offset as:\n"
					"  - a positive decimal number, or\n"
					"  - a hex number, beginning with '0x', or\n"
					"  - a '+' or '-' sign, followed by a relative offset"));
#endif
	}
}

static void
replace_next_cb (GtkButton *button, gpointer user_data)
{
	ReplaceDialog *self = REPLACE_DIALOG(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	HexDocument *doc;
	guint cursor_pos;
	guint offset, str_len;
	gchar *str = NULL;

	g_return_if_fail (REPLACE_IS_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX(self->gh));

	doc = gtk_hex_get_document (self->gh);
	cursor_pos = gtk_hex_get_cursor (self->gh);

	if ((str_len = get_search_string(self->f_doc, &str)) == 0) {
		// FIXME
//		display_error_dialog (self, _("There is no string to search for!"));
		return;
	}

	if (hex_document_find_forward(doc,
								 cursor_pos + 1, str, str_len, &offset))
	{
		gtk_hex_set_cursor(self->gh, offset);
	}
	else {
		// FIXME - NOT IMPLEMENTED
//		display_info_dialog (self, _("String was not found!\n"));
//		ghex_window_flash(win, _("End Of File reached"));
	}

	if (str)
		g_free(str);
}

static void
replace_one_cb(GtkButton *button, gpointer user_data)
{
	ReplaceDialog *self = REPLACE_DIALOG(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	HexDocument *doc;
	guint cursor_pos;
	gchar *find_str = NULL, *rep_str = NULL;
	guint find_len, rep_len, offset;

	g_return_if_fail (REPLACE_IS_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX(self->gh));

	(void)button;	/* unused */

	doc = gtk_hex_get_document (self->gh);
	cursor_pos = gtk_hex_get_cursor (self->gh);
	
	if ((find_len = get_search_string(self->f_doc, &find_str)) == 0) {
		// FIXME
//		display_error_dialog (self, _("There is no string to search for!"));
		return;
	}
	rep_len = get_search_string(self->r_doc, &rep_str);
	
	if (find_len > doc->file_size - cursor_pos)
		goto clean_up;
	
	if (hex_document_compare_data(doc, find_str, cursor_pos, find_len) == 0)
	{
		hex_document_set_data(doc, cursor_pos,
							  rep_len, find_len, rep_str, TRUE);
	}
	
	if (hex_document_find_forward(doc, cursor_pos + rep_len,
				find_str, find_len, &offset))
	{
		gtk_hex_set_cursor(self->gh, offset);
	}
	else {
		// FIXME - NOT IMPLEMENTED
//		display_info_dialog(self, _("End Of File reached!"));
//		ghex_window_flash(win, _("End Of File reached!"));
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
	HexDocument *doc;
	guint cursor_pos;
	gchar *find_str = NULL, *rep_str = NULL, *flash;
	guint find_len, rep_len, offset, count;

	g_return_if_fail (REPLACE_IS_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX(self->gh));

	(void)button;	/* unused */

	doc = gtk_hex_get_document (self->gh);
	cursor_pos = gtk_hex_get_cursor (self->gh);

	if ((find_len = get_search_string(self->f_doc, &find_str)) == 0) {
		// FIXME
//		display_error_dialog (self, _("There is no string to search for!"));
		return;
	}
	rep_len = get_search_string(self->r_doc, &rep_str);

	if (find_len > doc->file_size - cursor_pos)
		goto clean_up;
	
	count = 0;
	cursor_pos = 0;  

	while(hex_document_find_forward(doc, cursor_pos, find_str, find_len,
									&offset)) {
		hex_document_set_data (doc, offset, rep_len, find_len, rep_str, TRUE);
		cursor_pos = offset + rep_len;
		count++;
	}
	
	gtk_hex_set_cursor(self->gh, MIN(offset, doc->file_size));  

	if(count == 0) {
		// FIXME
//		display_info_dialog (self, _("No occurrences were found."));
	}
	
	flash = g_strdup_printf(ngettext("Replaced %d occurrence.",
									 "Replaced %d occurrences.",
									 count), count);
	// FIXME - NOT IMPLEMENTED
//	ghex_window_flash(win, flash);
	g_free(flash);

clean_up:
	if (find_str)
		g_free(find_str);

	if (rep_str)
		g_free(rep_str);
}

/* PRIVATE METHOD DEFINITIONS */

/* FindDialog */

static void
find_dialog_init (FindDialog *self)
{
	/* Yes, we could use these newfangled layout managers, but I'm a bit lazy
	 * right now.
	 */
	self->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_parent (self->vbox, GTK_WIDGET(self));

	self->frame = gtk_frame_new(_("Find String"));
	self->f_doc = hex_document_new();
	self->f_gh = create_hex_view(self->f_doc);
	gtk_frame_set_child (GTK_FRAME(self->frame), self->f_gh);
	gtk_box_append (GTK_BOX(self->vbox), self->frame);
	
	self->hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_append (GTK_BOX(self->vbox), self->hbox);

	self->f_next = gtk_button_new_with_mnemonic (_("Find _Next"));
	g_signal_connect (G_OBJECT (self->f_next), "clicked",
					  G_CALLBACK(find_next_cb), self);
	gtk_widget_set_receives_default (self->f_next, TRUE);
	gtk_box_append (GTK_BOX(self->hbox), self->f_next);

	self->f_prev = gtk_button_new_with_mnemonic (_("Find _Previous"));
	g_signal_connect (G_OBJECT (self->f_prev), "clicked",
					  G_CALLBACK(find_prev_cb), self);
	gtk_box_append (GTK_BOX(self->hbox), self->f_prev);

	self->f_close = gtk_button_new_with_mnemonic (_("_Close"));
	g_signal_connect (G_OBJECT (self->f_close), "clicked",
			G_CALLBACK(find_cancel_cb), self);
	gtk_box_append (GTK_BOX(self->hbox), self->f_close);

	/* FIXME / TODO - just keeping these strings alive for adaptation into
	 * the new accessibility framework if possible, since they have likely
	 * already been translated.
	 */
	g_debug("%s: The following strings are just being preserved for "
			"adaptation into the new accessibility framework. Ignore.",
			__func__);

	g_debug(_("Find Data"));
	g_debug(_("Enter the hex data or ASCII data to search for"));

	g_debug(_("Find Next"));
	g_debug(_("Finds the next occurrence of the search string"));

	g_debug(_("Find previous"));
	g_debug(_("Finds the previous occurrence of the search string "));

	g_debug(_("Cancel"));
	g_debug(_("Closes find data window"));
}


// TODO - see: find_delete_event_cb for some possible destructor hints here?
static void
find_dialog_dispose(GObject *object)
{
	FindDialog *self = FIND_DIALOG(object);

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
find_dialog_class_init(FindDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	/* <boilerplate> */
	object_class->dispose = find_dialog_dispose;
	object_class->finalize = find_dialog_finalize;
	/* </boilerplate> */

	/* set the box-type layout manager for this Find dialog widget.
	 */
	gtk_widget_class_set_layout_manager_type (widget_class,
			GTK_TYPE_BOX_LAYOUT);
}

GtkWidget *
find_dialog_new (void)
{
	return g_object_new(FIND_TYPE_DIALOG, NULL);
}

void
find_dialog_set_hex (FindDialog *self, GtkHex *gh)
{
	g_return_if_fail (FIND_IS_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX(gh));

	g_debug("%s: setting GtkHex of FindDialog to: %p",
			__func__, (void *)gh);

	self->gh = gh;
}

/* ReplaceDialog */

static void
replace_dialog_init (ReplaceDialog *self)
{
	self->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_parent (self->vbox, GTK_WIDGET(self));

	self->f_doc = hex_document_new ();
	self->f_gh = create_hex_view (self->f_doc);

	self->f_frame = gtk_frame_new (_("Find String"));
	gtk_frame_set_child (GTK_FRAME(self->f_frame), self->f_gh); 
	gtk_box_append (GTK_BOX(self->vbox), self->f_frame);
	
	self->r_doc = hex_document_new();
	self->r_gh = create_hex_view (self->r_doc);
	self->r_frame = gtk_frame_new(_("Replace With"));
	gtk_frame_set_child (GTK_FRAME(self->r_frame), self->r_gh);
	gtk_box_append (GTK_BOX(self->vbox), self->r_frame);

	self->hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_append (GTK_BOX(self->vbox), self->hbox);

	self->next = gtk_button_new_with_mnemonic (_("Find _Next"));
	g_signal_connect (G_OBJECT (self->next),
					  "clicked", G_CALLBACK(replace_next_cb),
					  self);
	gtk_box_append (GTK_BOX(self->hbox), self->next);
	gtk_widget_set_receives_default (self->next, TRUE);

	self->replace = gtk_button_new_with_mnemonic (_("_Replace"));
	g_signal_connect (G_OBJECT (self->replace),
					  "clicked", G_CALLBACK(replace_one_cb),
					  self);
	gtk_box_append (GTK_BOX(self->hbox), self->replace);

	self->replace_all= gtk_button_new_with_mnemonic (_("Replace _All"));
	g_signal_connect (G_OBJECT (self->replace_all),
					  "clicked", G_CALLBACK(replace_all_cb),
					  self);
	gtk_box_append (GTK_BOX(self->hbox), self->replace_all);

	self->close = gtk_button_new_with_mnemonic (_("_Close"));
	g_signal_connect (G_OBJECT (self->close),
					  "clicked", G_CALLBACK(cancel_cb),
					  self);
	gtk_box_append (GTK_BOX(self->hbox), self->close);

	/* FIXME/TODO - preserve translated strings for a11y */

	g_debug("%s: preserved strings for a11y - Ignore.", __func__);

	g_debug(_("Find Data"));
	g_debug(_("Enter the hex data or ASCII data to search for"));

	g_debug(_("Replace Data"));
	g_debug(_("Enter the hex data or ASCII data to replace with"));

	g_debug(_("Find next"));
	g_debug(_("Finds the next occurrence of the search string"));

	g_debug(_("Replace"));
	g_debug(_("Replaces the search string with the replace string"));

	g_debug(_("Replace All"));
	g_debug(_("Replaces all occurrences of the search string with the replace string"));

	g_debug(_("Cancel"));
	g_debug(_("Closes find and replace data window"));
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
replace_dialog_class_init(ReplaceDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	/* <boilerplate> */
	object_class->dispose = replace_dialog_dispose;
	object_class->finalize = replace_dialog_finalize;
	/* </boilerplate> */

	/* set the box-type layout manager for this Find dialog widget.
	 */
	gtk_widget_class_set_layout_manager_type (widget_class,
			GTK_TYPE_BOX_LAYOUT);
}

GtkWidget *
replace_dialog_new(void)
{
	return g_object_new(REPLACE_TYPE_DIALOG, NULL);
}

void
replace_dialog_set_hex (ReplaceDialog *self, GtkHex *gh)
{
	g_return_if_fail (REPLACE_IS_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX(gh));

	g_debug("%s: setting GtkHex of ReplaceDialog to: %p",
			__func__, (void *)gh);

	self->gh = gh;
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

	self->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_parent (self->box, widget);
	
	/* FIXME/TODO - this is not very intuitive. */
	self->label = gtk_label_new (_("Jump to byte (enter offset):"));
	self->int_entry = gtk_entry_new();

	gtk_box_append (GTK_BOX(self->box), self->label);
	gtk_box_append (GTK_BOX(self->box), self->int_entry);

	self->ok = gtk_button_new_with_mnemonic (_("_Jump"));
	g_signal_connect (G_OBJECT (self->ok),
					  "clicked", G_CALLBACK(goto_byte_cb),
					  self);
	gtk_box_append (GTK_BOX(self->box), self->ok);
	gtk_widget_set_receives_default (self->ok, TRUE);

	self->cancel = gtk_button_new_with_mnemonic (_("_Close"));
	g_signal_connect (G_OBJECT (self->cancel),
					  "clicked", G_CALLBACK(cancel_cb),
					  self);
	gtk_box_append (GTK_BOX(self->box), self->cancel);

	/* FIXME/TODO - preserve strings for a11y */
	
	g_debug("%s: preserving strings for a11y. Safely ignore.", __func__);

	g_debug(_("Jump to byte"));
	g_debug(_("Enter the byte to jump to"));

	g_debug(_("Jump"));
	g_debug(_("Jumps to the specified byte"));

	g_debug(_("Close"));
	g_debug(_("Closes jump to byte window"));
}

static void
jump_dialog_dispose(GObject *object)
{
	JumpDialog *self = JUMP_DIALOG(object);

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(jump_dialog_parent_class)->dispose(object);
}

static void
jump_dialog_finalize(GObject *gobject)
{
	/* here, you would free stuff. I've got nuthin' for ya. */

	/* --- */

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(jump_dialog_parent_class)->finalize(gobject);
}

static void
jump_dialog_class_init(JumpDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	/* <boilerplate> */
	object_class->dispose = jump_dialog_dispose;
	object_class->finalize = jump_dialog_finalize;
	/* </boilerplate> */

	/* CSS */
	gtk_widget_class_set_css_name (widget_class, JUMP_DIALOG_CSS_NAME);

	/* set the box-type layout manager for this Find dialog widget.
	 */
	gtk_widget_class_set_layout_manager_type (widget_class,
			GTK_TYPE_BOX_LAYOUT);
}

GtkWidget *
jump_dialog_new(void)
{
	return g_object_new(JUMP_TYPE_DIALOG, NULL);
}

void
jump_dialog_set_hex (JumpDialog *self, GtkHex *gh)
{
	g_return_if_fail (JUMP_IS_DIALOG(self));
	g_return_if_fail (GTK_IS_HEX(gh));

	g_debug("%s: setting GtkHex of JumpDialog to: %p",
			__func__, (void *)gh);

	self->gh = gh;
}
