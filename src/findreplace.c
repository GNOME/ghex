/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* findreplace.c - finding & replacing data

   Copyright (C) 1998, 1999, 2000 Free Software Foundation

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

static void find_next_cb(GtkWidget *);
static void find_prev_cb(GtkWidget *);
static void replace_next_cb(GtkWidget *);
static void replace_one_cb(GtkWidget *);
static void replace_all_cb(GtkWidget *);
static void set_find_type_cb(GtkWidget *, gint);
static void set_replace_type_cb(GtkWidget *, gint);
static void goto_byte_cb(GtkWidget *);
static gint get_search_string(gchar *, gchar *, gint);

FindDialog *find_dialog = NULL;
ReplaceDialog *replace_dialog = NULL;
JumpDialog *jump_dialog = NULL;

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
					   GTK_SIGNAL_FUNC(delete_event_cb), dialog->window);
	
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
	
	dialog->f_next = create_button(dialog->window, GNOME_STOCK_PIXMAP_FORWARD, _("Find Next"));
	gtk_signal_connect (GTK_OBJECT (dialog->f_next),
						"clicked", GTK_SIGNAL_FUNC(find_next_cb),
						dialog->f_string);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->f_next,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->f_next, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_next);
	dialog->f_prev = create_button(dialog->window, GNOME_STOCK_PIXMAP_BACK, _("Find Previous"));
	gtk_signal_connect (GTK_OBJECT (dialog->f_prev),
						"clicked", GTK_SIGNAL_FUNC(find_prev_cb),
						dialog->f_string);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->f_prev,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->f_prev, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_prev);
	dialog->f_close = gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);
	gtk_signal_connect (GTK_OBJECT (dialog->f_close),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						dialog->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->f_close,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->f_close, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->f_close);
	
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dialog->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), 2);

	return dialog;
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
	
	dialog->next = create_button(dialog->window, GNOME_STOCK_PIXMAP_FORWARD, _("Find next"));
	gtk_signal_connect (GTK_OBJECT (dialog->next),
						"clicked", GTK_SIGNAL_FUNC(replace_next_cb),
						dialog->f_string);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->next,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->next, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->next);
	dialog->replace = gtk_button_new_with_label(_("Replace"));
	gtk_signal_connect (GTK_OBJECT (dialog->replace),
						"clicked", GTK_SIGNAL_FUNC(replace_one_cb),
						NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->replace,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->replace, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->replace);
	dialog->replace_all= gtk_button_new_with_label(_("Replace All"));
	gtk_signal_connect (GTK_OBJECT (dialog->replace_all),
						"clicked", GTK_SIGNAL_FUNC(replace_all_cb),
						NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->replace_all,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->replace_all, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->replace_all);
	dialog->close = gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);
	gtk_signal_connect (GTK_OBJECT (dialog->close),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						dialog->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->close,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->close, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->close);
	
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dialog->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), 2);

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
	
	dialog->ok = gnome_stock_button(GNOME_STOCK_BUTTON_OK);
	gtk_signal_connect (GTK_OBJECT (dialog->ok),
						"clicked", GTK_SIGNAL_FUNC(goto_byte_cb),
						dialog->int_entry);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->ok,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->ok, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->ok);
	dialog->cancel = gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);
	gtk_signal_connect (GTK_OBJECT (dialog->cancel),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						dialog->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->cancel,
					   TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(dialog->cancel, GTK_CAN_DEFAULT);
	gtk_widget_show(dialog->cancel);
	
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dialog->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), 2);

	return dialog;
}

static gint get_search_string(gchar *str, gchar *buf, gint type)
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

static void set_find_type_cb(GtkWidget *w, gint type)
{
	find_dialog->search_type = type;
}

static void set_replace_type_cb(GtkWidget *w, gint type)
{
	replace_dialog->search_type = type;
}

static void find_next_cb(GtkWidget *w)
{
	GtkHex *gh;
	guint offset, str_len;
	gchar str[256];
	
	if(mdi->active_child == NULL) {
		gnome_app_error(mdi->active_window, _("There is no active buffer to search!"));
		return;
	}
	
	gh = GTK_HEX(mdi->active_view);
	
	if((str_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(find_dialog->f_string)), str,
									find_dialog->search_type)) == 0) {
		gnome_app_error(mdi->active_window, _("There seems to be no string to search for!"));
		return;
	}
   	if(hex_document_find_forward(HEX_DOCUMENT(mdi->active_child),
								 gh->cursor_pos+1, str, str_len, &offset))
		gtk_hex_set_cursor(gh, offset);
	else
		gnome_app_flash(mdi->active_window, _("End Of File reached"));
}

static void find_prev_cb(GtkWidget *w)
{
	GtkHex *gh;
	guint offset, str_len;
	gchar str[256];
		
	if(mdi->active_child == NULL) {
		gnome_app_error(mdi->active_window, _("There is no active buffer to search!"));
		return;
	}
	
	gh = GTK_HEX(mdi->active_view);
	
	if((str_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(find_dialog->f_string)), str,
									find_dialog->search_type)) == 0) {
		gnome_app_error(mdi->active_window, _("There seems to be no string to search for!"));
		return;
	}

	if(hex_document_find_backward(HEX_DOCUMENT(mdi->active_child),
								  gh->cursor_pos, str, str_len, &offset))
		gtk_hex_set_cursor(gh, offset);
	else
		gnome_app_flash(mdi->active_window, _("Beginning Of File reached"));
}

static void goto_byte_cb(GtkWidget *w)
{
	guint byte = 2;
	gchar *byte_str = gtk_entry_get_text(GTK_ENTRY(jump_dialog->int_entry));
	
	if(mdi->active_child == NULL) {
		gnome_app_error(mdi->active_window, _("There is no active buffer to move the cursor in!"));
		return;
	}

	if(strlen(byte_str) == 0) {
		gnome_app_error(mdi->active_window, _("No offset has been specified!"));
		return;
	}
	
	if((sscanf(byte_str, "0x%x", &byte) == 1) ||
	   (sscanf(byte_str, "%d", &byte) == 1)) {
		if(byte >= HEX_DOCUMENT(mdi->active_child)->file_size)
			gnome_app_error(mdi->active_window,_("Can not position cursor beyond the End Of File!"));
		else
			gtk_hex_set_cursor(GTK_HEX(mdi->active_view), byte);
	}
	else
		gnome_app_error(mdi->active_window, _("The offset must be a positive integer value!"));
}

static void replace_next_cb(GtkWidget *w)
{
	GtkHex *gh;
	guint offset, str_len;
	gchar str[256];
		
	if(mdi->active_child == NULL) {
		gnome_app_error(mdi->active_window, _("There is no active buffer to search!"));
		return;
	}
	
	gh = GTK_HEX(mdi->active_view);

	if((str_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(replace_dialog->f_string)), str,
									replace_dialog->search_type)) == 0) {
		gnome_app_error(mdi->active_window, _("There seems to be no string to search for!"));
		return;
	}

	if(hex_document_find_forward(HEX_DOCUMENT(mdi->active_child),
								 gh->cursor_pos+1, str, str_len, &offset))
		gtk_hex_set_cursor(gh, offset);
	else
		gnome_app_flash(mdi->active_window, _("End Of File reached"));
}

static void replace_one_cb(GtkWidget *w)
{
	gchar find_str[256], rep_str[256];
	guint find_len, rep_len, offset;
	GtkHex *gh;
	HexDocument *doc;
	
	if(mdi->active_child == NULL) {
		gnome_app_error(mdi->active_window, _("There is no active buffer to replace data in!"));
		return;
	}
	
	gh = GTK_HEX(mdi->active_view);
	doc = HEX_DOCUMENT(mdi->active_child);
	
	if( ((find_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(replace_dialog->f_string)), find_str,
									   replace_dialog->search_type)) == 0) ||
		((rep_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(replace_dialog->r_string)), rep_str,
									  replace_dialog->search_type)) == 0)) {
		gnome_app_error(mdi->active_window, _("Strange find or replace string!"));
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
	else
		gnome_app_flash(mdi->active_window, _("End Of File reached!"));
}

static void replace_all_cb(GtkWidget *w)
{
	gchar find_str[256], rep_str[256], *flash;
	guint find_len, rep_len, offset, count;
	GtkHex *gh;
	HexDocument *doc;
	
	if(mdi->active_child == NULL) {
		gnome_app_error(mdi->active_window, _("There is no active buffer to replace data in!"));
		return;
	}
	
	gh = GTK_HEX(mdi->active_view);
	doc = HEX_DOCUMENT(mdi->active_child);
	
	if( ((find_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(replace_dialog->f_string)), find_str,
									   replace_dialog->search_type)) == 0) ||
		((rep_len = get_search_string(gtk_entry_get_text(GTK_ENTRY(replace_dialog->r_string)), rep_str,
									  replace_dialog->search_type)) == 0)) {
		gnome_app_error(mdi->active_window, _("Strange find or replace string!"));
		return;
	}
	
	if(find_len > doc->file_size - gh->cursor_pos)
		return;
	
	count = 0;
	while(hex_document_find_forward(doc, gh->cursor_pos, find_str, find_len,
									&offset)) {
		hex_document_set_data(doc, offset, rep_len, find_len, rep_str, TRUE);
		count++;
	}
	
	gtk_hex_set_cursor(gh, offset);  
	
	flash = g_strdup_printf(_("Replaced %d occurencies."), count);
	gnome_app_flash(mdi->active_window, flash);
	g_free(flash);
}
