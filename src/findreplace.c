/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* findreplace.c - finding & replacing data

   Copyright (C) 1998 - 2002 Free Software Foundation

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
#include <config.h>
#endif

#include <gnome.h>
#include "ghex.h"

static gint find_delete_event_cb(GtkWidget *w, GdkEventAny *e,
								 FindDialog *dialog);
static void find_cancel_cb(GtkWidget *w, FindDialog *dialog);
static gint advanced_find_delete_event_cb(GtkWidget *w, GdkEventAny *e,
										  AdvancedFindDialog *dialog);
static void advanced_find_close_cb(GtkWidget *w, AdvancedFindDialog *dialog);

static void find_next_cb(GtkButton *button, FindDialog *);
static void find_prev_cb(GtkButton *button, FindDialog *);
static void replace_next_cb(GtkButton *button, GtkWidget *);
static void replace_one_cb(GtkButton *button, GtkWidget *);
static void replace_all_cb(GtkButton *button, GtkWidget *);
static void set_find_type_cb(GtkWidget *, gint);
static void set_replace_type_cb(GtkWidget *, gint);
static void goto_byte_cb(GtkButton *button, GtkWidget *);
static gint get_search_string(const gchar *, gchar *, gint);

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
	gchar str[256];
	gint str_len;
	GtkHex_AutoHighlight *auto_highlight;
} AdvancedFind_ListData;

#define TYPE_LABEL_LEN 256

FindDialog *create_find_dialog()
{
	gint i;
	GSList *group;
	gchar type_label[TYPE_LABEL_LEN + 1];
	FindDialog *dialog;

	dialog = g_new0(FindDialog, 1);

	dialog->window = gtk_dialog_new();
	gtk_signal_connect(GTK_OBJECT(dialog->window), "delete_event",
					   GTK_SIGNAL_FUNC(find_delete_event_cb), dialog);
	
	create_dialog_title(dialog->window, _("GHex (%s): Find Data"));
	
	dialog->f_string = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), dialog->f_string,
					   TRUE, TRUE, 0);
	gtk_widget_show(dialog->f_string);
	
	for(i = 0, group = NULL; i < 2;
		i++, group = gtk_radio_button_group(GTK_RADIO_BUTTON(dialog->type_button[i-1]))) {
		g_snprintf(type_label, TYPE_LABEL_LEN, _("Search for %s"),
				   _(search_type_label[i]));
		
		dialog->type_button[i] = gtk_radio_button_new_with_label(group, type_label);
		
		if(dialog->search_type == i)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->type_button[i]), TRUE);
		
		gtk_signal_connect(GTK_OBJECT(dialog->type_button[i]), "clicked",
						   GTK_SIGNAL_FUNC(set_find_type_cb), GINT_TO_POINTER(i));
		
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), dialog->type_button[i],
						   TRUE, TRUE, 0);
		
		gtk_widget_show(dialog->type_button[i]);
	}
	
	dialog->f_next = create_button(dialog->window, GTK_STOCK_GO_FORWARD, _("Find _Next"));
	gtk_signal_connect (GTK_OBJECT (dialog->f_next),
						"clicked", GTK_SIGNAL_FUNC(find_next_cb),
						dialog);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->f_next,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->f_next, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_next);
	dialog->f_prev = create_button(dialog->window, GTK_STOCK_GO_BACK, _("Find _Previous"));
	gtk_signal_connect (GTK_OBJECT (dialog->f_prev),
						"clicked", GTK_SIGNAL_FUNC(find_prev_cb),
						dialog);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->f_prev,
					   TRUE, TRUE, 0);

	GTK_WIDGET_SET_FLAGS(dialog->f_prev, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_prev);

	dialog->f_close = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_signal_connect (GTK_OBJECT (dialog->f_close),
						"clicked", GTK_SIGNAL_FUNC(find_cancel_cb),
						dialog);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->f_close,
					   TRUE, TRUE, 0);

	GTK_WIDGET_SET_FLAGS(dialog->f_close, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_close);

	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dialog->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), 2);

	if (GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (dialog->f_string))) {
		add_atk_namedesc (dialog->f_string, _("Find Data"), _("Enter the hex data or ASCII data to search for"));
		add_atk_namedesc (dialog->f_next, _("Find Next"), _("Finds the next occurrence of the search string"));
		add_atk_namedesc (dialog->f_prev, _("Find previous"), _("Finds the previous occurrence of the search string "));
		add_atk_namedesc (dialog->f_close, _("Cancel"), _("Closes find data window"));
	}

	return dialog;
}

AdvancedFind_AddDialog *create_advanced_find_add_dialog(AdvancedFindDialog *parent)
{
	AdvancedFind_AddDialog *dialog = g_new0(AdvancedFind_AddDialog, 1);
	GtkWidget *button;
	int i;
	GSList *group;
	gchar type_label[256];

	dialog->window = gtk_dialog_new();
	gtk_widget_hide(dialog->window);
	gtk_signal_connect(GTK_OBJECT(dialog->window), "delete_event",
					   GTK_SIGNAL_FUNC(delete_event_cb), dialog->window);

	create_dialog_title(dialog->window, _("GHex (%s): Find Data: Add search"));

	dialog->f_string = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), dialog->f_string,
					   TRUE, TRUE, 0);
	gtk_widget_show(dialog->f_string);

	for(i = 0, group = NULL; i < 2;
		i++, group = gtk_radio_button_group(GTK_RADIO_BUTTON(dialog->type_button[i-1]))) {
		g_snprintf(type_label, TYPE_LABEL_LEN, _("Search for %s"),
				   _(search_type_label[i]));
		
		dialog->type_button[i] = gtk_radio_button_new_with_label(group, type_label);
		
		if(0 == i)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->type_button[i]), TRUE);
		
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), dialog->type_button[i],
						   TRUE, TRUE, 0);
		
		gtk_widget_show(dialog->type_button[i]);
	}

	button = create_button(dialog->window, GTK_STOCK_ADD, _("Add"));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), button,
					   TRUE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (button),
						"clicked", GTK_SIGNAL_FUNC(advanced_find_add_add_cb),
						dialog);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_show(button);

	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_signal_connect (GTK_OBJECT (button),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						dialog->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), button,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_show(button);

	dialog->colour = gnome_color_picker_new();
	gnome_color_picker_set_use_alpha(GNOME_COLOR_PICKER(dialog->colour), FALSE);
	gnome_color_picker_set_title(GNOME_COLOR_PICKER(dialog->colour), _("Highlight Colour"));
	gnome_color_picker_set_i8(GNOME_COLOR_PICKER(dialog->colour), 255, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area),
					   dialog->colour, TRUE, TRUE, 0);
	gtk_widget_show(dialog->colour);

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
	gtk_signal_connect(GTK_OBJECT(dialog->window), "delete_event",
					   GTK_SIGNAL_FUNC(advanced_find_delete_event_cb), dialog);

	gtk_window_set_default_size(GTK_WINDOW(dialog->window), 300, 350);

	create_dialog_title(dialog->window, _("GHex (%s): Find Data"));

	dialog->hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox),
					   dialog->hbox, TRUE, TRUE, GNOME_PAD_SMALL);
	gtk_widget_show(dialog->hbox);

	dialog->list = gtk_list_store_new(3, G_TYPE_STRING, GTK_TYPE_STRING,
			                          G_TYPE_POINTER, GTK_TYPE_POINTER);
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
	gtk_signal_connect (GTK_OBJECT (dialog->f_next),
						"clicked", GTK_SIGNAL_FUNC(advanced_find_next_cb),
						dialog);
	GTK_WIDGET_SET_FLAGS(dialog->f_next, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_next);

	dialog->f_prev = create_button(dialog->window, GTK_STOCK_GO_BACK, _("Find _Previous"));
	gtk_box_pack_start(GTK_BOX(dialog->vbox), dialog->f_prev,
					   FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (dialog->f_prev),
						"clicked", GTK_SIGNAL_FUNC(advanced_find_prev_cb),
						dialog);
	GTK_WIDGET_SET_FLAGS(dialog->f_prev, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_prev);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(dialog->vbox), sep, FALSE, FALSE, GNOME_PAD_SMALL);
	gtk_widget_show(sep);

	dialog->f_new = create_button(dialog->window, GTK_STOCK_ADD, _("_Add New"));
	gtk_box_pack_start(GTK_BOX(dialog->vbox), dialog->f_new,
					   FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (dialog->f_new),
						"clicked", GTK_SIGNAL_FUNC(advanced_find_add_cb),
						dialog);
	GTK_WIDGET_SET_FLAGS(dialog->f_new, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_new);

	dialog->f_remove = create_button(dialog->window, GTK_STOCK_REMOVE, _("_Remove Selected"));
	gtk_box_pack_start(GTK_BOX(dialog->vbox), dialog->f_remove,
					   FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (dialog->f_remove),
						"clicked", GTK_SIGNAL_FUNC(advanced_find_delete_cb),
						dialog);
	GTK_WIDGET_SET_FLAGS(dialog->f_remove, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_remove);


	dialog->f_close = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_signal_connect(GTK_OBJECT(dialog->f_close),
					   "clicked", GTK_SIGNAL_FUNC(advanced_find_close_cb),
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
	gint i;
	GSList *group;
	gchar type_label[256];
	ReplaceDialog *dialog;

	dialog = g_new0(ReplaceDialog, 1);

	dialog->window = gtk_dialog_new();
	gtk_signal_connect(GTK_OBJECT(dialog->window), "delete_event",
					   GTK_SIGNAL_FUNC(delete_event_cb), dialog->window);
	
	create_dialog_title(dialog->window, _("GHex (%s): Find & Replace Data"));
	
	dialog->f_string = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), dialog->f_string,
					   TRUE, TRUE, 0);
	gtk_widget_show(dialog->f_string);
	
	dialog->r_string = gtk_entry_new();

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), dialog->r_string,
					   TRUE, TRUE, 0);
	gtk_widget_show(dialog->r_string);
	
	for(i = 0, group = NULL; i < 2;
		i++, group = gtk_radio_button_group(GTK_RADIO_BUTTON(dialog->type_button[i-1]))) {
		g_snprintf(type_label, TYPE_LABEL_LEN, _("Replace %s"),
				   _(search_type_label[i]));
		
		dialog->type_button[i] = gtk_radio_button_new_with_label(group, type_label);
		
		if(dialog->search_type == i)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->type_button[i]), TRUE);
		
		gtk_signal_connect(GTK_OBJECT(dialog->type_button[i]), "clicked",
						   GTK_SIGNAL_FUNC(set_replace_type_cb), GINT_TO_POINTER(i));
		
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), dialog->type_button[i],
						   TRUE, TRUE, 0);
		
		gtk_widget_show(dialog->type_button[i]);
	}
	
	dialog->next = create_button(dialog->window, GTK_STOCK_GO_FORWARD, _("Find _next"));
	gtk_signal_connect (GTK_OBJECT (dialog->next),
						"clicked", GTK_SIGNAL_FUNC(replace_next_cb),
						dialog->f_string);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->next,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->next, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->next);
	dialog->replace = gtk_button_new_with_mnemonic(_("_Replace"));
	gtk_signal_connect (GTK_OBJECT (dialog->replace),
						"clicked", GTK_SIGNAL_FUNC(replace_one_cb),
						NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->replace,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->replace, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->replace);
	dialog->replace_all= gtk_button_new_with_mnemonic(_("Replace _All"));
	gtk_signal_connect (GTK_OBJECT (dialog->replace_all),
						"clicked", GTK_SIGNAL_FUNC(replace_all_cb),
						NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->replace_all,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->replace_all, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->replace_all);

	dialog->close = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_signal_connect (GTK_OBJECT (dialog->close),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						dialog->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->close,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->close, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->close);
	
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dialog->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), 2);

	if (GTK_IS_ACCESSIBLE(gtk_widget_get_accessible(dialog->f_string))) {
		add_atk_namedesc (dialog->f_string, _("Find Data"), _("Enter the hex data or ASCII data to search for"));
		add_atk_namedesc (dialog->r_string, _("Replace Data"), _("Enter the hex data or ASCII data to replace with"));
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
	gtk_signal_connect(GTK_OBJECT(dialog->window), "delete_event",
					   GTK_SIGNAL_FUNC(delete_event_cb), dialog->window);
	
	create_dialog_title(dialog->window, _("GHex (%s): Jump To Byte"));
	
	dialog->int_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), dialog->int_entry,
					   TRUE, TRUE, 0);
	gtk_widget_show(dialog->int_entry);

	dialog->ok = gtk_button_new_from_stock (GTK_STOCK_OK);
	gtk_signal_connect (GTK_OBJECT (dialog->ok),
						"clicked", GTK_SIGNAL_FUNC(goto_byte_cb),
						dialog->int_entry);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->ok,
					   TRUE, TRUE, 0);

	GTK_WIDGET_SET_FLAGS(dialog->ok, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->ok);
	dialog->cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_signal_connect (GTK_OBJECT (dialog->cancel),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						dialog->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->cancel,
					   TRUE, TRUE, 0);

	GTK_WIDGET_SET_FLAGS(dialog->cancel, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->cancel);
	
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dialog->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), 2);

	if (GTK_IS_ACCESSIBLE (gtk_widget_get_accessible(dialog->int_entry))) {
		add_atk_namedesc (dialog->int_entry, _("Jump to byte"), _("Enter the byte to jump to"));
		add_atk_namedesc (dialog->ok, _("OK"), _("Jumps to the specified byte"));
		add_atk_namedesc (dialog->cancel, _("Cancel"), _("Closes jump to byte window"));
	}

	return dialog;
}

static gint get_search_string(const gchar *str, gchar *buf, gint type)
{
	gint len = strlen(str), shift;
	
	if(len > 0) {
		if(type == DATA_TYPE_HEX) {
			/* we convert the string from hex */
			if(len % 2 != 0)
				return 0;  /* the number of hex digits must be EVEN */
			len = 0;     /* we'll store the returned string length in len */
			shift = 4;
			*buf = '\0';
			while(*str != 0) {
				if((*str >= '0') && (*str <= '9'))
					*buf |= (*str - '0') << shift;
				else if((*str >= 'A') && (*str <= 'F'))
					*buf |= (*str - 'A' + 10) << shift;
				else if((*str >= 'a') && (*str <= 'f'))
					*buf |= (*str - 'a' + 10) << shift;
				else
					return 0;
				
				if(shift > 0)
					shift = 0;
				else {
					shift = 4;
					buf++;
					len++;
					*buf = '\0';
				}
				
				str++;
			}
		}
		else if(type == DATA_TYPE_ASCII)
			strcpy(buf, str);
		
		return len;
	}
	return 0;
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

static void set_find_type_cb(GtkWidget *w, gint type)
{
	find_dialog->search_type = type;
}

static void set_replace_type_cb(GtkWidget *w, gint type)
{
	replace_dialog->search_type = type;
}

static void find_next_cb(GtkButton *button, FindDialog *dialog)
{
	GtkHex *gh;
	guint offset, str_len;
	gchar str[256];
	GHexWindow *win = ghex_window_get_active();

	if(win == NULL || win->gh == NULL) {
		display_error_dialog (win, _("There is no active document to search!"));
		return;
	}
	
	gh = win->gh;
	
	if((str_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(find_dialog->f_string)), str,
									find_dialog->search_type)) == 0) {
		display_error_dialog (win, _("The string is not appropriate for the selected data type!"));
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
}

static void find_prev_cb(GtkButton *button, FindDialog *dialog)
{
	GtkHex *gh;
	guint offset, str_len;
	gchar str[256];
	GHexWindow *win = ghex_window_get_active();
		
	if(win == NULL || win->gh == NULL) {
		display_error_dialog (win, _("There is no active document to search!"));
		return;
	}
	
	gh = win->gh;
	
	if((str_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(find_dialog->f_string)), str,
									find_dialog->search_type)) == 0) {
		display_error_dialog (win, _("The string is not appropriate for the selected data type!"));
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
}

static void goto_byte_cb(GtkButton *button, GtkWidget *w)
{
	guint byte = 2;
	const gchar *byte_str = gtk_entry_get_text(GTK_ENTRY(jump_dialog->int_entry));
	GHexWindow *win = ghex_window_get_active();
	
	if(win == NULL || win->gh == NULL) {
		display_error_dialog (win, _("There is no active document to move the cursor in!"));
		return;
	}

	if(strlen(byte_str) == 0) {
		display_error_dialog (win, _("No offset has been specified!"));
		return;
	}
	
	if((sscanf(byte_str, "0x%x", &byte) == 1) ||
	   (sscanf(byte_str, "%d", &byte) == 1)) {
		if(byte >= win->gh->document->file_size)
			display_error_dialog (win, _("Can not position cursor beyond the End Of File!"));
		else
			gtk_hex_set_cursor(win->gh, byte);
	}
	else
		display_error_dialog (win, _("The offset must be a positive integer value!"));
}

static void replace_next_cb(GtkButton *button, GtkWidget *w)
{
	GtkHex *gh;
	guint offset, str_len;
	gchar str[256];
	GHexWindow *win = ghex_window_get_active();
		
	if(win == NULL || win->gh == NULL) {
		display_error_dialog (win, _("There is no active document to search!"));
		return;
	}
	
	gh = win->gh;

	if((str_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(replace_dialog->f_string)), str,
									replace_dialog->search_type)) == 0) {
		display_error_dialog (win, _("There seems to be no string to search for!"));
		return;
	}

	if(hex_document_find_forward(gh->document,
								 gh->cursor_pos+1, str, str_len, &offset))
		gtk_hex_set_cursor(gh, offset);
	else {
		display_info_dialog(win, _("String was not found!\n"));
		ghex_window_flash(win, _("End Of File reached"));
	}
}

static void replace_one_cb(GtkButton *button, GtkWidget *w)
{
	gchar find_str[256], rep_str[256];
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
	
	if( ((find_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(replace_dialog->f_string)), find_str,
									   replace_dialog->search_type)) == 0) ||
		((rep_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(replace_dialog->r_string)), rep_str,
									  replace_dialog->search_type)) == 0)) {
		display_error_dialog (win, _("Strings are not appropriate for the selected data type!"));
		return;
	}
	
	if(find_len > doc->file_size - gh->cursor_pos)
		return;
	
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
}

static void replace_all_cb(GtkButton *button, GtkWidget *w)
{
	gchar find_str[256], rep_str[256], *flash;
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
	
	if( ((find_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(replace_dialog->f_string)), find_str,
									   replace_dialog->search_type)) == 0) ||
		((rep_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(replace_dialog->r_string)), rep_str,
									  replace_dialog->search_type)) == 0)) {
		display_error_dialog (win, _("Strings are not appropriate for the selected data type!"));
		return;
	}
	
	if(find_len > doc->file_size - gh->cursor_pos)
		return;
	
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
		display_info_dialog(win, _("No occurences were found."));
	}
	
	flash = g_strdup_printf(_("Replaced %d occurencies."), count);
	ghex_window_flash(win, flash);
	g_free(flash);
}

static void advanced_find_add_add_cb(GtkButton *button,
									 AdvancedFind_AddDialog *dialog)
{
	gint search_type;
	for (search_type = 0; search_type < 2; search_type++)
	{
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->type_button[search_type])))
		{
			gtk_dialog_response(GTK_DIALOG(dialog->window), search_type);
			return;
		}
	}
	gtk_dialog_response(GTK_DIALOG(dialog->window), GTK_RESPONSE_NONE);
}

static void advanced_find_add_cb(GtkButton *button, AdvancedFindDialog *dialog)
{
	gint ret;
	if(!GTK_WIDGET_VISIBLE(dialog->addDialog->window)) {
		gtk_window_position (GTK_WINDOW(dialog->addDialog->window), GTK_WIN_POS_MOUSE);
		gtk_widget_show(dialog->addDialog->window);
	}

	ret = gtk_dialog_run(GTK_DIALOG(dialog->addDialog->window));
	gtk_widget_hide(dialog->addDialog->window);
	if (ret >= 0)
	{
		gchar *colour;
		guint8 colour_parts[4];
		AdvancedFind_ListData *data = g_new0(AdvancedFind_ListData, 1);
		GtkHex *gh = dialog->parent->gh;
		const gchar *findstr = gtk_entry_get_text(GTK_ENTRY(dialog->addDialog->f_string));
		GtkTreeIter iter;

		if((data->str_len = get_search_string(findstr, data->str, ret)) == 0) {
			display_error_dialog (dialog->parent, _("The string is not appropriate for the selected data type!"));
			return;
		}
		gnome_color_picker_get_i8(GNOME_COLOR_PICKER(dialog->addDialog->colour),
								  &colour_parts[0],
								  &colour_parts[1],
								  &colour_parts[2],
								  &colour_parts[3]);
		colour = g_strdup_printf("#%02x%02x%02x", colour_parts[0], colour_parts[1], colour_parts[2]);
		data->auto_highlight = gtk_hex_insert_autohighlight(gh, data->str, data->str_len, colour);
		gtk_list_store_append(dialog->list, &iter);
		gtk_list_store_set(dialog->list, &iter,
						   0, findstr,
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

