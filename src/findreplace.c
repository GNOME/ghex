/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* findreplace.c - finding & replacing data

   Copyright (C) 1998 - 2004 Free Software Foundation

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
   If not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "findreplace.h"
#include "ui.h"
#include "gtkhex.h"
#include "configuration.h"

static gint find_delete_event_cb(GtkWidget *w, GdkEventAny *e,
								 FindDialog *dialog);
static void find_cancel_cb(GtkWidget *w, FindDialog *dialog);
static gint advanced_find_delete_event_cb(GtkWidget *w, GdkEventAny *e,
										  AdvancedFindDialog *dialog);
static void advanced_find_close_cb(GtkWidget *w, AdvancedFindDialog *dialog);

static void find_next_cb(GtkButton *button, FindDialog *);
static void find_prev_cb(GtkButton *button, FindDialog *);
static void replace_next_cb(GtkButton *button, gpointer);
static void replace_one_cb(GtkButton *button, gpointer);
static void replace_all_cb(GtkButton *button, gpointer);
static void goto_byte_cb(GtkButton *button, GtkWidget *);
static gint get_search_string(HexDocument *doc, gchar **str);

static void advanced_find_add_add_cb(GtkButton *button,
									 AdvancedFind_AddDialog *dialog);
static void advanced_find_add_cb(GtkButton *button, AdvancedFindDialog *);
static void advanced_find_delete_cb(GtkButton *button, AdvancedFindDialog *dialog);
static void advanced_find_next_cb(GtkButton *button, AdvancedFindDialog *dialog);
static void advanced_find_prev_cb(GtkButton *button, AdvancedFindDialog *dialog);


FindDialog *find_dialog = NULL;
ReplaceDialog *replace_dialog = NULL;
JumpDialog *jump_dialog = NULL;

/* basic structure to hold private information to be stored in the
 * gtk list.
 */
typedef struct
{
	gchar *str;
	gint str_len;
	GtkHex_AutoHighlight *auto_highlight;
} AdvancedFind_ListData;

static GtkWidget *create_hex_view(HexDocument *doc)
{
    GtkWidget *gh = hex_document_add_view(doc);

	gtk_hex_set_group_type(GTK_HEX(gh), def_group_type);
	if (def_metrics && def_font_desc) {
		gtk_hex_set_font(GTK_HEX(gh), def_metrics, def_font_desc);
	}
	gtk_hex_set_insert_mode(GTK_HEX(gh), TRUE);
	gtk_hex_set_geometry(GTK_HEX(gh), 16, 4);
    return gh;
}

FindDialog *create_find_dialog()
{
	gint i;
	GSList *group;
	FindDialog *dialog;
	GtkWidget *frame;

	dialog = g_new0(FindDialog, 1);

	dialog->window = gtk_dialog_new();
	g_signal_connect(G_OBJECT(dialog->window), "delete_event",
					 G_CALLBACK(find_delete_event_cb), dialog);
	
	create_dialog_title(dialog->window, _("GHex (%s): Find Data"));
	
	dialog->f_doc = hex_document_new();
	dialog->f_gh = create_hex_view(dialog->f_doc);
	frame = gtk_frame_new(_("Find String"));
	gtk_container_add(GTK_CONTAINER(frame), dialog->f_gh);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), frame,
					   TRUE, TRUE, 0);
	gtk_widget_show(frame);
	gtk_widget_show(dialog->f_gh);
	
	dialog->f_next = create_button(dialog->window, GTK_STOCK_GO_FORWARD, _("Find _Next"));
	g_signal_connect (G_OBJECT (dialog->f_next), "clicked",
					  G_CALLBACK(find_next_cb), dialog);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->f_next,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->f_next, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_next);
	dialog->f_prev = create_button(dialog->window, GTK_STOCK_GO_BACK, _("Find _Previous"));
	g_signal_connect (G_OBJECT (dialog->f_prev), "clicked",
					  G_CALLBACK(find_prev_cb), dialog);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->f_prev,
					   TRUE, TRUE, 0);

	GTK_WIDGET_SET_FLAGS(dialog->f_prev, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_prev);

	dialog->f_close = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	g_signal_connect (G_OBJECT (dialog->f_close),
					  "clicked", G_CALLBACK(find_cancel_cb),
					  dialog);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->f_close,
					   TRUE, TRUE, 0);

	GTK_WIDGET_SET_FLAGS(dialog->f_close, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_close);

	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), 2);

	if (GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (dialog->f_gh))) {
		add_atk_namedesc (dialog->f_gh, _("Find Data"), _("Enter the hex data or ASCII data to search for"));
		add_atk_namedesc (dialog->f_next, _("Find Next"), _("Finds the next occurrence of the search string"));
		add_atk_namedesc (dialog->f_prev, _("Find previous"), _("Finds the previous occurrence of the search string "));
		add_atk_namedesc (dialog->f_close, _("Cancel"), _("Closes find data window"));
	}

	return dialog;
}

AdvancedFind_AddDialog *create_advanced_find_add_dialog(AdvancedFindDialog *parent)
{
	AdvancedFind_AddDialog *dialog = g_new0(AdvancedFind_AddDialog, 1);
	GtkWidget *button, *frame, *sep;

	dialog->window = gtk_dialog_new();
	gtk_widget_hide(dialog->window);
	g_signal_connect(G_OBJECT(dialog->window), "delete_event",
					 G_CALLBACK(delete_event_cb), dialog->window);

	create_dialog_title(dialog->window, _("GHex (%s): Find Data: Add search"));

	dialog->f_doc = hex_document_new();
	dialog->f_gh = create_hex_view(dialog->f_doc);
	frame = gtk_frame_new(_("Find String"));
	gtk_container_add(GTK_CONTAINER(frame), dialog->f_gh);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), frame,
					   TRUE, TRUE, 0);
	gtk_widget_show(frame);
	gtk_widget_show(dialog->f_gh);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), sep,
					   FALSE, FALSE, 0);

	dialog->colour = gtk_color_selection_new();
	gtk_color_selection_set_has_opacity_control(GTK_COLOR_SELECTION(dialog->colour),
												FALSE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox),
					   dialog->colour, FALSE, FALSE, 0);
	gtk_widget_show(dialog->colour);

	button = create_button(dialog->window, GTK_STOCK_ADD, _("Add"));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), button,
					   TRUE, TRUE, 0);
	g_signal_connect (G_OBJECT (button),
					  "clicked", G_CALLBACK(advanced_find_add_add_cb),
					  dialog);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_show(button);

	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	g_signal_connect (G_OBJECT (button),
					  "clicked", G_CALLBACK(cancel_cb),
					  dialog->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), button,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_show(button);

	return dialog;
}

AdvancedFindDialog *create_advanced_find_dialog(GHexWindow *parent)
{
	AdvancedFindDialog *dialog;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	GtkWidget *sep;

	dialog = g_new0(AdvancedFindDialog, 1);

	dialog->parent = parent;

	dialog->addDialog = create_advanced_find_add_dialog(dialog);

	dialog->window = gtk_dialog_new();
	g_signal_connect(G_OBJECT(dialog->window), "delete_event",
					 G_CALLBACK(advanced_find_delete_event_cb), dialog);

	gtk_window_set_default_size(GTK_WINDOW(dialog->window), 300, 350);

	create_dialog_title(dialog->window, _("GHex (%s): Find Data"));

	dialog->hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox),
					   dialog->hbox, TRUE, TRUE, GNOME_PAD_SMALL);
	gtk_widget_show(dialog->hbox);

	dialog->list = gtk_list_store_new(3,
									  G_TYPE_STRING, G_TYPE_STRING,
			                          G_TYPE_POINTER, G_TYPE_POINTER);
	dialog->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (dialog->list));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Search String"),
													   renderer,
													   "text", 0,
													   "foreground", 1,
													   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->tree), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Highlight Colour"),
													   renderer,
													   "background", 1,
													   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->tree), column);

	gtk_box_pack_start(GTK_BOX(dialog->hbox), dialog->tree,
					   TRUE, TRUE, GNOME_PAD_SMALL);
	gtk_widget_show (dialog->tree);

	dialog->vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(dialog->hbox), dialog->vbox,
					   FALSE, FALSE, GNOME_PAD_SMALL);
	gtk_widget_show(dialog->vbox);

	dialog->f_next = create_button(dialog->window, GTK_STOCK_GO_FORWARD, _("Find _Next"));
	gtk_box_pack_start(GTK_BOX(dialog->vbox), dialog->f_next,
					   FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (dialog->f_next),
					  "clicked", G_CALLBACK(advanced_find_next_cb),
					  dialog);
	GTK_WIDGET_SET_FLAGS(dialog->f_next, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_next);

	dialog->f_prev = create_button(dialog->window, GTK_STOCK_GO_BACK, _("Find _Previous"));
	gtk_box_pack_start(GTK_BOX(dialog->vbox), dialog->f_prev,
					   FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (dialog->f_prev),
					  "clicked", G_CALLBACK(advanced_find_prev_cb),
					  dialog);
	GTK_WIDGET_SET_FLAGS(dialog->f_prev, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_prev);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(dialog->vbox), sep, FALSE, FALSE, GNOME_PAD_SMALL);
	gtk_widget_show(sep);

	dialog->f_new = create_button(dialog->window, GTK_STOCK_ADD, _("_Add New"));
	gtk_box_pack_start(GTK_BOX(dialog->vbox), dialog->f_new,
					   FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (dialog->f_new),
					  "clicked", G_CALLBACK(advanced_find_add_cb),
					  dialog);
	GTK_WIDGET_SET_FLAGS(dialog->f_new, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_new);

	dialog->f_remove = create_button(dialog->window, GTK_STOCK_REMOVE, _("_Remove Selected"));
	gtk_box_pack_start(GTK_BOX(dialog->vbox), dialog->f_remove,
					   FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (dialog->f_remove),
					  "clicked", G_CALLBACK(advanced_find_delete_cb),
					  dialog);
	GTK_WIDGET_SET_FLAGS(dialog->f_remove, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_remove);

	dialog->f_close = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	g_signal_connect(G_OBJECT(dialog->f_close),
					 "clicked", G_CALLBACK(advanced_find_close_cb),
					 dialog);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area),
					   dialog->f_close, TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->f_close, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_close);

	if (GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (dialog->f_close)))
	{
		add_atk_namedesc(dialog->f_close, _("Close"), _("Closes advanced find window"));
	}

	return dialog;
}

static void delete_advanced_find_add_dialog(AdvancedFind_AddDialog *dialog)
{
	gtk_widget_destroy(GTK_WIDGET(dialog->window));
	g_free(dialog);
}

gboolean advanced_find_foreachfunc_cb(GtkTreeModel *model, GtkTreePath *path,
									  GtkTreeIter *iter, gpointer data)
{
	AdvancedFind_ListData *udata;
	GtkHex *gh = (GtkHex *)data;
	gtk_tree_model_get(model, iter, 2, &udata, -1);
	gtk_hex_delete_autohighlight(gh, udata->auto_highlight);
	if(NULL != udata->str)
		g_free(udata->str);
	g_free(udata);
	return FALSE;
}

void delete_advanced_find_dialog(AdvancedFindDialog *dialog)
{
	delete_advanced_find_add_dialog(dialog->addDialog);
	gtk_tree_model_foreach(GTK_TREE_MODEL(dialog->list),
						   advanced_find_foreachfunc_cb, (gpointer *)dialog->parent->gh);
	g_free(dialog);
}

ReplaceDialog *create_replace_dialog()
{
	ReplaceDialog *dialog;
	GtkWidget *frame;

	dialog = g_new0(ReplaceDialog, 1);

	dialog->window = gtk_dialog_new();
	g_signal_connect(G_OBJECT(dialog->window), "delete_event",
					 G_CALLBACK(delete_event_cb), dialog->window);
	
	create_dialog_title(dialog->window, _("GHex (%s): Find & Replace Data"));
	
	dialog->f_doc = hex_document_new();
	dialog->f_gh = create_hex_view(dialog->f_doc);
	frame = gtk_frame_new(_("Find String"));
	gtk_container_add(GTK_CONTAINER(frame), dialog->f_gh);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), frame,
					   TRUE, TRUE, 0);
	gtk_widget_show(frame);
	gtk_widget_show(dialog->f_gh);
	
	dialog->r_doc = hex_document_new();
	dialog->r_gh = create_hex_view(dialog->r_doc);
	frame = gtk_frame_new(_("Replace With"));
	gtk_container_add(GTK_CONTAINER(frame), dialog->r_gh);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), frame,
					   TRUE, TRUE, 0);
	gtk_widget_show(frame);
	gtk_widget_show(dialog->r_gh);
	
	dialog->next = create_button(dialog->window, GTK_STOCK_GO_FORWARD, _("Find _next"));
	g_signal_connect (G_OBJECT (dialog->next),
					  "clicked", G_CALLBACK(replace_next_cb),
					  NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->next,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->next, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->next);
	dialog->replace = gtk_button_new_with_mnemonic(_("_Replace"));
	g_signal_connect (G_OBJECT (dialog->replace),
					  "clicked", G_CALLBACK(replace_one_cb),
					  NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->replace,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->replace, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->replace);
	dialog->replace_all= gtk_button_new_with_mnemonic(_("Replace _All"));
	g_signal_connect (G_OBJECT (dialog->replace_all),
					  "clicked", G_CALLBACK(replace_all_cb),
					  NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->replace_all,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->replace_all, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->replace_all);

	dialog->close = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	g_signal_connect (G_OBJECT (dialog->close),
					  "clicked", G_CALLBACK(cancel_cb),
					  dialog->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->close,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->close, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->close);
	
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), 2);

	if (GTK_IS_ACCESSIBLE(gtk_widget_get_accessible(dialog->f_gh))) {
		add_atk_namedesc (dialog->f_gh, _("Find Data"), _("Enter the hex data or ASCII data to search for"));
		add_atk_namedesc (dialog->r_gh, _("Replace Data"), _("Enter the hex data or ASCII data to replace with"));
		add_atk_namedesc (dialog->next, _("Find next"), _("Finds the next occurrence of the search string"));
		add_atk_namedesc (dialog->replace, _("Replace"), _("Replaces the search string with the replace string"));
		add_atk_namedesc (dialog->replace_all, _("Replace All"), _("Replaces all occurrences of the search string with the replace string"));
		add_atk_namedesc (dialog->close, _("Cancel"), _("Closes find and replace data window"));
	}

	return dialog;
}

JumpDialog *create_jump_dialog()
{
	JumpDialog *dialog;

	dialog = g_new0(JumpDialog, 1);

	dialog->window = gtk_dialog_new();
	g_signal_connect(G_OBJECT(dialog->window), "delete_event",
					 G_CALLBACK(delete_event_cb), dialog->window);
	
	create_dialog_title(dialog->window, _("GHex (%s): Jump To Byte"));
	
	dialog->int_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), dialog->int_entry,
					   TRUE, TRUE, 0);
	gtk_widget_show(dialog->int_entry);

	dialog->ok = gtk_button_new_from_stock (GTK_STOCK_OK);
	g_signal_connect (G_OBJECT (dialog->ok),
					  "clicked", G_CALLBACK(goto_byte_cb),
					  dialog->int_entry);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->ok,
					   TRUE, TRUE, 0);

	GTK_WIDGET_SET_FLAGS(dialog->ok, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->ok);
	dialog->cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	g_signal_connect (G_OBJECT (dialog->cancel),
					  "clicked", G_CALLBACK(cancel_cb),
					  dialog->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->cancel,
					   TRUE, TRUE, 0);

	GTK_WIDGET_SET_FLAGS(dialog->cancel, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->cancel);
	
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), 2);

	if (GTK_IS_ACCESSIBLE (gtk_widget_get_accessible(dialog->int_entry))) {
		add_atk_namedesc (dialog->int_entry, _("Jump to byte"), _("Enter the byte to jump to"));
		add_atk_namedesc (dialog->ok, _("OK"), _("Jumps to the specified byte"));
		add_atk_namedesc (dialog->cancel, _("Cancel"), _("Closes jump to byte window"));
	}

	return dialog;
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

static void find_cancel_cb(GtkWidget *w, FindDialog *dialog)
{
	GHexWindow *win = ghex_window_get_active();
	GtkHex *gh = win->gh;
	
	if (dialog->auto_highlight) gtk_hex_delete_autohighlight(gh, dialog->auto_highlight);
	dialog->auto_highlight = NULL;
	gtk_widget_hide(dialog->window);
}

static gint advanced_find_delete_event_cb(GtkWidget *w, GdkEventAny *e,
										  AdvancedFindDialog *dialog)
{
	gtk_widget_hide(w);

	return TRUE;
}

static void advanced_find_close_cb(GtkWidget *w, AdvancedFindDialog *dialog)
{
	gtk_widget_hide(dialog->window);
}

static void find_next_cb(GtkButton *button, FindDialog *dialog)
{
	GtkHex *gh;
	guint offset, str_len;
	gchar *str;
	GHexWindow *win = ghex_window_get_active();

	if(win == NULL || win->gh == NULL) {
		display_error_dialog (win, _("There is no active document to search!"));
		return;
	}
	
	gh = win->gh;
	
	if((str_len = get_search_string(find_dialog->f_doc, &str)) == 0) {
		display_error_dialog (win, _("There is no string to search for!"));
		return;
	}
	if (dialog->auto_highlight) gtk_hex_delete_autohighlight(gh, dialog->auto_highlight);
	dialog->auto_highlight = NULL;
	dialog->auto_highlight = gtk_hex_insert_autohighlight(gh, str, str_len, "red");
	if(hex_document_find_forward(gh->document,
								 gh->cursor_pos+1, str, str_len, &offset))
	{
		gtk_hex_set_cursor(gh, offset);
	}
	else {
		ghex_window_flash(win, _("End Of File reached"));
		display_info_dialog(win, _("String was not found!\n"));
	}
	if(NULL != str)
		g_free(str);
}

static void find_prev_cb(GtkButton *button, FindDialog *dialog)
{
	GtkHex *gh;
	guint offset, str_len;
	gchar *str;
	GHexWindow *win = ghex_window_get_active();
		
	if(win == NULL || win->gh == NULL) {
		display_error_dialog (win, _("There is no active document to search!"));
		return;
	}
	
	gh = win->gh;
	
	if((str_len = get_search_string(find_dialog->f_doc, &str)) == 0) {
		display_error_dialog (win, _("There is no string to search for!"));
		return;
	}

	if (dialog->auto_highlight) gtk_hex_delete_autohighlight(gh, dialog->auto_highlight);
	dialog->auto_highlight = NULL;
	dialog->auto_highlight = gtk_hex_insert_autohighlight(gh, str, str_len, "red");
	if(hex_document_find_backward(gh->document,
								  gh->cursor_pos, str, str_len, &offset))
		gtk_hex_set_cursor(gh, offset);
	else {
		ghex_window_flash(win, _("Beginning Of File reached"));
		display_info_dialog(win, _("String was not found!\n"));
	}
	if(NULL != str)
		g_free(str);
}

static void goto_byte_cb(GtkButton *button, GtkWidget *w)
{
	guint byte = 2, len, i;
	gboolean is_hex;
	const gchar *byte_str = gtk_entry_get_text(GTK_ENTRY(jump_dialog->int_entry));
	GHexWindow *win = ghex_window_get_active();
	
	if(win == NULL || win->gh == NULL) {
		display_error_dialog (win,
							  _("There is no active document to move the "
								"cursor in!"));
		return;
	}

	if((len = strlen(byte_str)) == 0) {
		display_error_dialog (win, _("No offset has been specified!"));
		return;
	}

	is_hex = ((len > 2) && (byte_str[0] == '0') && (byte_str[1] == 'x'));

	if(!is_hex) {
		for(i = 0; i < len; i++)
			if(!(byte_str[i] >= '0' && byte_str[i] <= '9')) 
				break;
	}
	else {
		for(i = 2; i < len; i++)
			if(!((byte_str[i] >= '0' && byte_str[i] <= '9') ||
				 (byte_str[i] >= 'A' && byte_str[i] <= 'F') ||
				 (byte_str[i] >= 'a' && byte_str[i] <= 'f')))
				break;
	}

	if((i == len) &&
	   ((sscanf(byte_str, "0x%x", &byte) == 1) ||
		(sscanf(byte_str, "%d", &byte) == 1))) {
		if(byte >= win->gh->document->file_size)
			display_error_dialog(win,
								 _("Can not position cursor beyond the "
								   "End Of File!"));
		else
			gtk_hex_set_cursor(win->gh, byte);
	}
	else
		display_error_dialog(win,
							 _("You may only give the offset as:\n"
							   "  - a positive decimal number, or\n"
							   "  - a hex number, beginning with '0x'"));
}

static void replace_next_cb(GtkButton *button, gpointer unused)
{
	GtkHex *gh;
	guint offset, str_len;
	gchar *str = NULL;
	GHexWindow *win = ghex_window_get_active();
		
	if(win == NULL || win->gh == NULL) {
		display_error_dialog (win, _("There is no active document to search!"));
		return;
	}
	
	gh = win->gh;

	if((str_len = get_search_string(replace_dialog->f_doc, &str)) == 0) {
		display_error_dialog (win, _("There is no string to search for!"));
		return;
	}

	if(hex_document_find_forward(gh->document,
								 gh->cursor_pos+1, str, str_len, &offset))
		gtk_hex_set_cursor(gh, offset);
	else {
		display_info_dialog(win, _("String was not found!\n"));
		ghex_window_flash(win, _("End Of File reached"));
	}

	if(NULL != str)
		g_free(str);
}

static void replace_one_cb(GtkButton *button, gpointer unused)
{
	gchar *find_str = NULL, *rep_str = NULL;
	guint find_len, rep_len, offset;
	GtkHex *gh;
	HexDocument *doc;
	GHexWindow *win = ghex_window_get_active();
	
	if(win == NULL || win->gh == NULL) {
		display_error_dialog (win, _("There is no active buffer to replace data in!"));
		return;
	}
	
	gh = win->gh;

	doc = win->gh->document;
	
	if((find_len = get_search_string(replace_dialog->f_doc, &find_str)) == 0) {
		display_error_dialog (win, _("There is no string to search for!"));
		return;
	}
	rep_len = get_search_string(replace_dialog->r_doc, &rep_str);
	
	if(find_len > doc->file_size - gh->cursor_pos)
		goto clean_up;
	
	if(hex_document_compare_data(doc, find_str, gh->cursor_pos, find_len) == 0)
		hex_document_set_data(doc, gh->cursor_pos,
							  rep_len, find_len, rep_str, TRUE);
	
	if(hex_document_find_forward(doc, gh->cursor_pos + rep_len, find_str, find_len,
								 &offset))
		gtk_hex_set_cursor(gh, offset);
	else {
		display_info_dialog(win, _("End Of File reached!"));
		ghex_window_flash(win, _("End Of File reached!"));
	}

 clean_up:
	if(NULL != find_str)
		g_free(find_str);
	if(NULL != rep_str)
		g_free(rep_str);
}

static void replace_all_cb(GtkButton *button, gpointer unused)
{
	gchar *find_str = NULL, *rep_str = NULL, *flash;
	guint find_len, rep_len, offset, count, cursor_pos;
	GtkHex *gh;
	HexDocument *doc;
	GHexWindow *win = ghex_window_get_active();
	
	if(win == NULL || win->gh == NULL) {
		display_error_dialog (win, _("There is no active document to replace data in!"));
		return;
	}
	
	gh = win->gh;

	doc = gh->document;
	
	if((find_len = get_search_string(replace_dialog->f_doc, &find_str)) == 0) {
		display_error_dialog (win, _("There is no string to search for!"));
		return;
	}
	rep_len = get_search_string(replace_dialog->r_doc, &rep_str);

	if(find_len > doc->file_size - gh->cursor_pos)
		goto clean_up;
	
	count = 0;
	cursor_pos = 0;  

	while(hex_document_find_forward(doc, cursor_pos, find_str, find_len,
									&offset)) {
		hex_document_set_data(doc, offset, rep_len, find_len, rep_str, TRUE);
		cursor_pos = offset + rep_len;
		count++;
	}
	
	gtk_hex_set_cursor(gh, MIN(offset, doc->file_size));  

	if(count == 0) {
		display_info_dialog(win, _("No occurrences were found."));
	}
	
	flash = g_strdup_printf(_("Replaced %d occurrences."), count);
	ghex_window_flash(win, flash);
	g_free(flash);

 clean_up:
	if(NULL != find_str)
		g_free(find_str);
	if(NULL != rep_str)
		g_free(rep_str);
}

static void advanced_find_add_add_cb(GtkButton *button,
									 AdvancedFind_AddDialog *dialog)
{
	gtk_dialog_response(GTK_DIALOG(dialog->window), GTK_RESPONSE_OK);
}

static void advanced_find_add_cb(GtkButton *button, AdvancedFindDialog *dialog)
{
	gint ret;
	if(!GTK_WIDGET_VISIBLE(dialog->addDialog->window)) {
		gtk_window_set_position (GTK_WINDOW(dialog->addDialog->window), GTK_WIN_POS_MOUSE);
		gtk_widget_show(dialog->addDialog->window);
	}

	ret = gtk_dialog_run(GTK_DIALOG(dialog->addDialog->window));
	gtk_widget_hide(dialog->addDialog->window);
	if (ret != GTK_RESPONSE_NONE)
	{
		gchar *colour;
		GdkColor gcol;
		AdvancedFind_ListData *data = g_new0(AdvancedFind_ListData, 1);
		GtkHex *gh = dialog->parent->gh;
		GtkTreeIter iter;
		
		g_return_if_fail (gh != NULL);
		
		if((data->str_len = get_search_string(dialog->addDialog->f_doc, &data->str)) == 0) {
			display_error_dialog (dialog->parent, _("No string to search for!"));
			return;
		}
		gtk_color_selection_get_current_color
			(GTK_COLOR_SELECTION(dialog->addDialog->colour), &gcol);
		colour = g_strdup_printf("#%02x%02x%02x",
								 gcol.red, gcol.green, gcol.blue);
		data->auto_highlight = gtk_hex_insert_autohighlight(gh, data->str, data->str_len, colour);
		gtk_list_store_append(dialog->list, &iter);
		gtk_list_store_set(dialog->list, &iter,
						   0, data->str,
						   1, colour,
						   2, data,
						   -1);
		g_free(colour);
	}
}

static void advanced_find_delete_cb(GtkButton *button, AdvancedFindDialog *dialog)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tree));
	GtkTreeIter iter;
	GtkTreeModel *model;
	AdvancedFind_ListData *data;
	GtkHex *gh = dialog->parent->gh;

	if (gtk_tree_selection_get_selected(selection, &model, &iter) != TRUE)
		return;
	
	gtk_tree_model_get(model, &iter, 2, &data, -1);
	gtk_hex_delete_autohighlight(gh, data->auto_highlight);
	if(NULL != data->str)
		g_free(data->str);
	g_free(data);
	gtk_list_store_remove(dialog->list, &iter);
}

static void advanced_find_next_cb(GtkButton *button, AdvancedFindDialog *dialog)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tree));
	GtkTreeIter iter;
	GtkTreeModel *model;
	AdvancedFind_ListData *data;
	GtkHex *gh = dialog->parent->gh;
	guint offset;
	GHexWindow *win = ghex_window_get_active();

	if (gtk_tree_selection_get_selected(selection, &model, &iter) != TRUE)
		return;
	
	gtk_tree_model_get(model, &iter, 2, &data, -1);
	if(hex_document_find_forward(gh->document,
								 gh->cursor_pos+1, data->str, data->str_len, &offset))
	{
		gtk_hex_set_cursor(gh, offset);
	}
	else {
		ghex_window_flash(win, _("End Of File reached"));
		display_info_dialog(win, _("String was not found!\n"));
	}
}

static void advanced_find_prev_cb(GtkButton *button, AdvancedFindDialog *dialog)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tree));
	GtkTreeIter iter;
	GtkTreeModel *model;
	AdvancedFind_ListData *data;
	GtkHex *gh = dialog->parent->gh;
	guint offset;
	GHexWindow *win = ghex_window_get_active();

	if (gtk_tree_selection_get_selected(selection, &model, &iter) != TRUE)
		return;
	
	gtk_tree_model_get(model, &iter, 2, &data, -1);
	if(hex_document_find_backward(gh->document,
								  gh->cursor_pos, data->str, data->str_len, &offset))
		gtk_hex_set_cursor(gh, offset);
	else {
		ghex_window_flash(win, _("Beginning Of File reached"));
		display_info_dialog(win, _("String was not found!\n"));
	}
}

