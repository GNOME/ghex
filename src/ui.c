/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ui.c - GHex user interface

   Copyright (C) 1997, 1998 Free Software Foundation

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

   Author: Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
*/

#include <config.h>
#include <gnome.h>
#include "gtkhex.h"
#include "gnome-support.h"
#include "callbacks.h"
#include "ghex.h"

GnomeUIInfo file_menu[] = {
	{ GNOME_APP_UI_ITEM, N_("_Open"), N_("Open a file"), open_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW, 'O', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_ITEM, N_("_Save"), N_("Save buffer "), save_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE, 'S', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_ITEM, N_("Save _as..."), N_("Save buffer under a different file name"), save_as_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE_AS, 0, 0, NULL },
	{ GNOME_APP_UI_ITEM, N_("_Revert"), N_("Revert buffer"), revert_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_REVERT, 'R', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_ITEM, N_("_Close"), N_("Close file"), close_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CLOSE, 0, 0, NULL },
	{ GNOME_APP_UI_SEPARATOR, NULL, NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_ITEM, N_("Open _Converter..."), N_("Open base conversion dialog"), converter_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_SEPARATOR, NULL, NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_ITEM, N_("_Preferences"), N_("Set user preferences"), prefs_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PREF, 'P', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_SEPARATOR, NULL, NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_ITEM, N_("_Exit"), N_("Exit GHex"), quit_app_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_EXIT, 'Q', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_ENDOFINFO }
};

GnomeUIInfo empty_menu[] = {
	{ GNOME_APP_UI_ENDOFINFO },
};

GnomeUIInfo view_menu[] = {
	{ GNOME_APP_UI_ITEM, N_("_Add view"), N_("Add a new view of the buffer"), add_view_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0,
	  0, NULL },
	{ GNOME_APP_UI_ITEM, N_("_Remove view"), N_("Remove the current view of the buffer"), remove_view_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_ENDOFINFO }
};

GnomeUIInfo help_menu[] = {
	{ GNOME_APP_UI_HELP, NULL, NULL, "ghex", NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_SEPARATOR, NULL, NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_ITEM, N_("_About..."), N_("About GHex"), about_cb, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ABOUT, 0, 0, NULL },
	{ GNOME_APP_UI_ENDOFINFO }
};

GnomeUIInfo main_menu[] = {
	{ GNOME_APP_UI_SUBTREE, N_("_File"), NULL, file_menu, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_SUBTREE, N_("_View"), NULL, view_menu, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_SUBTREE, N_("File_s"), NULL, empty_menu, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_SUBTREE, N_("_Help"), NULL, help_menu, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_ENDOFINFO }
};

guint group_type[3] = {
	GROUP_BYTE,
	GROUP_WORD,
	GROUP_LONG,
};

gchar *group_type_label[3] = {
	N_("Bytes"),
	N_("Words"),
	N_("Longwords"),
};

static void set_prefs(PropertyUI *);

GtkWidget *file_sel = NULL;

FindDialog find_dialog = { NULL };
ReplaceDialog replace_dialog = { NULL };
JumpDialog jump_dialog = { NULL };
Converter converter = { NULL };
PropertyUI prefs_ui = { NULL };

GdkFont *def_font = NULL;
gchar *def_font_name = NULL;

guint mdi_type[NUM_MDI_MODES] = {
	GNOME_MDI_DEFAULT_MODE,
	GNOME_MDI_NOTEBOOK,
	GNOME_MDI_TOPLEVEL,
	GNOME_MDI_MODAL
};

gchar *mdi_type_label[NUM_MDI_MODES] = {
	N_("Default"),
	N_("Notebook"),
	N_("Toplevel"),
	N_("Modal"),
};

guint search_type = 0;
gchar *search_type_label[] = {
	N_("hex data"),
	N_("ASCII data"),
};

void show_message(gchar *msg) {
	GtkWidget *message_box;
	
	message_box = gnome_message_box_new(msg, GNOME_MESSAGE_BOX_INFO, GNOME_STOCK_BUTTON_OK, NULL);
	gnome_message_box_set_modal(GNOME_MESSAGE_BOX(message_box));
	gtk_window_position (GTK_WINDOW (message_box), GTK_WIN_POS_MOUSE);
	gtk_widget_show(message_box);
}

void report_error(gchar *msg) {
	GtkWidget *message_box;
	
	message_box = gnome_message_box_new(msg, GNOME_MESSAGE_BOX_ERROR, GNOME_STOCK_BUTTON_OK, NULL);
	gnome_message_box_set_modal(GNOME_MESSAGE_BOX(message_box));
	gtk_window_position (GTK_WINDOW (message_box), GTK_WIN_POS_MOUSE);
	gtk_widget_show(message_box);
}

static void question_click_cb(GtkWidget *w, gint button, gint *reply) {
	*reply = button;
}  

static void question_destroy_cb(GtkObject *obj) {
	gtk_main_quit();
}

gint ask_user(GnomeMessageBox *message_box) {
	gint reply;
	
	gnome_dialog_set_close(GNOME_DIALOG(message_box), TRUE);
	gtk_window_set_modal(GTK_WINDOW(message_box), TRUE);
	gtk_signal_connect(GTK_OBJECT(message_box), "clicked",
					   GTK_SIGNAL_FUNC(question_click_cb), &reply);
	gtk_signal_connect(GTK_OBJECT(message_box), "destroy",
					   GTK_SIGNAL_FUNC(question_destroy_cb), NULL);
	gtk_window_position (GTK_WINDOW (message_box), GTK_WIN_POS_MOUSE);
	gtk_widget_show(GTK_WIDGET(message_box));
	/* I hope increasing main_level is the proper way to stop ghex until
	   user had replied to this question... */
	gtk_main();
	
	return reply;
}

GtkWidget *create_button(GtkWidget *window, gchar *type, gchar *text) {
	GtkWidget *button, *pixmap, *label, *hbox;
	
	hbox = gtk_hbox_new(FALSE, 2);
	
	label = gtk_label_new(text);
	pixmap = gnome_stock_pixmap_widget(window, type);
	
	gtk_box_pack_start(GTK_BOX(hbox), pixmap, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 1);
	
	button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(button), hbox);
	
	gtk_widget_show(label);
	gtk_widget_show(pixmap);
	gtk_widget_show(hbox);
	
	return button;
}

void create_find_dialog(FindDialog *dialog) {
	gint i;
	GSList *group;
	gchar type_label[256];
	
	dialog->window = gtk_dialog_new();
	gtk_signal_connect(GTK_OBJECT(dialog->window), "delete_event",
					   GTK_SIGNAL_FUNC(delete_event_cb), &dialog->window);
	
	gtk_window_set_title(GTK_WINDOW(dialog->window), _("GHex: Find Data"));
	
	dialog->f_string = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), dialog->f_string,
					   TRUE, TRUE, 0);
	gtk_widget_show(dialog->f_string);
	
	for(i = 0, group = NULL; i < 2;
		i++, group = gtk_radio_button_group(GTK_RADIO_BUTTON(dialog->type_button[i-1]))) {
		sprintf(type_label, _("Search for %s"), _(search_type_label[i]));
		
		dialog->type_button[i] = gtk_radio_button_new_with_label(group, type_label);
		
		if(find_dialog.search_type == i)
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(dialog->type_button[i]), TRUE);
		
		gtk_signal_connect(GTK_OBJECT(dialog->type_button[i]), "clicked",
						   GTK_SIGNAL_FUNC(set_find_type_cb), (gpointer)i);
		
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
	gtk_widget_show(dialog->f_next);
	dialog->f_prev = create_button(dialog->window, GNOME_STOCK_PIXMAP_BACK, _("Find Previous"));
	gtk_signal_connect (GTK_OBJECT (dialog->f_prev),
						"clicked", GTK_SIGNAL_FUNC(find_prev_cb),
						dialog->f_string);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->f_prev,
					   TRUE, TRUE, 0);
	gtk_widget_show(dialog->f_prev);
	dialog->f_close = gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);
	gtk_signal_connect (GTK_OBJECT (dialog->f_close),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						&dialog->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->f_close,
					   TRUE, TRUE, 0);
	gtk_widget_show(dialog->f_close);
	
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dialog->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), 2);
}

void create_replace_dialog(ReplaceDialog *dialog) {
	gint i;
	GSList *group;
	gchar type_label[256];
	
	dialog->window = gtk_dialog_new();
	gtk_signal_connect(GTK_OBJECT(dialog->window), "delete_event",
					   GTK_SIGNAL_FUNC(delete_event_cb), &dialog->window);
	
	gtk_window_set_title(GTK_WINDOW(dialog->window), _("GHex: Find & Replace Data"));
	
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
		sprintf(type_label, _("Replace %s"), _(search_type_label[i]));
		
		dialog->type_button[i] = gtk_radio_button_new_with_label(group, type_label);
		
		if(replace_dialog.search_type == i)
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(dialog->type_button[i]), TRUE);
		
		gtk_signal_connect(GTK_OBJECT(dialog->type_button[i]), "clicked",
						   GTK_SIGNAL_FUNC(set_replace_type_cb), (gpointer)i);
		
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
	gtk_widget_show(dialog->next);
	dialog->replace = gtk_button_new_with_label(_("Replace"));
	gtk_signal_connect (GTK_OBJECT (dialog->replace),
						"clicked", GTK_SIGNAL_FUNC(replace_one_cb),
						NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->replace,
					   TRUE, TRUE, 0);
	gtk_widget_show(dialog->replace);
	dialog->replace_all= gtk_button_new_with_label(_("Replace All"));
	gtk_signal_connect (GTK_OBJECT (dialog->replace_all),
						"clicked", GTK_SIGNAL_FUNC(replace_all_cb),
						NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->replace_all,
					   TRUE, TRUE, 0);
	gtk_widget_show(dialog->replace_all);
	dialog->close = gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);
	gtk_signal_connect (GTK_OBJECT (dialog->close),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						&dialog->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->close,
					   TRUE, TRUE, 0);
	gtk_widget_show(dialog->close);
	
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dialog->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), 2);
}

void create_jump_dialog(JumpDialog *dialog) {
	dialog->window = gtk_dialog_new();
	gtk_signal_connect(GTK_OBJECT(dialog->window), "delete_event",
					   GTK_SIGNAL_FUNC(delete_event_cb), &dialog->window);
	
	gtk_window_set_title(GTK_WINDOW(dialog->window), _("GHex: Jump To Byte"));
	
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
	gtk_widget_show(dialog->ok);
	dialog->cancel = gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);
	gtk_signal_connect (GTK_OBJECT (dialog->cancel),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						&dialog->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->window)->action_area), dialog->cancel,
					   TRUE, TRUE, 0);
	gtk_widget_show(dialog->cancel);
	
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dialog->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog->window)->vbox), 2);
}

void create_converter(Converter *conv) {
	GtkWidget *table, *label, *close;
	
	conv->window = gtk_dialog_new();
	gtk_signal_connect(GTK_OBJECT(conv->window), "delete_event",
					   GTK_SIGNAL_FUNC(delete_event_cb), &conv->window);
	
	gtk_window_set_title(GTK_WINDOW(conv->window), _("GHex: Converter"));
	
	table = gtk_table_new(4, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(conv->window)->vbox), table,
					   TRUE, TRUE, 0);
	gtk_widget_show(table);
	
	label = gtk_label_new(_("Binary"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	gtk_widget_show(label);
	conv->entry[0] = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(conv->entry[0]), "activate",
					   GTK_SIGNAL_FUNC(conv_entry_cb), (gpointer)2);
	gtk_table_attach_defaults(GTK_TABLE(table), conv->entry[0], 1, 2, 0, 1);
	gtk_widget_show(conv->entry[0]);
	
	label = gtk_label_new(_("Decimal"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
	gtk_widget_show(label);
	conv->entry[1] = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(conv->entry[1]), "activate",
					   GTK_SIGNAL_FUNC(conv_entry_cb), (gpointer)10);
	gtk_table_attach_defaults(GTK_TABLE(table), conv->entry[1], 1, 2, 1, 2);
	gtk_widget_show(conv->entry[1]);
	
	label = gtk_label_new(_("Hex"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
	gtk_widget_show(label);
	conv->entry[2] = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(conv->entry[2]), "activate",
					   GTK_SIGNAL_FUNC(conv_entry_cb), (gpointer)16);
	gtk_table_attach_defaults(GTK_TABLE(table), conv->entry[2], 1, 2, 2, 3);
	gtk_widget_show(conv->entry[2]);
	
	label = gtk_label_new(_("ASCII"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);
	gtk_widget_show(label);
	conv->entry[3] = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(conv->entry[3]), "activate",
					   GTK_SIGNAL_FUNC(conv_entry_cb), (gpointer)0);
	gtk_table_attach_defaults(GTK_TABLE(table), conv->entry[3], 1, 2, 3, 4);
	gtk_widget_show(conv->entry[3]);
	
	close = gtk_button_new_with_label(_("Close"));
	gtk_signal_connect (GTK_OBJECT (close),
						"clicked", GTK_SIGNAL_FUNC(cancel_cb),
						&conv->window);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(conv->window)->action_area), close,
					   TRUE, TRUE, 0);
	gtk_widget_show(close);
	
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(conv->window)->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(conv->window)->vbox), 2);
}

static void set_prefs(PropertyUI *pui) {
	int i;
	
	for(i = 0; i < 3; i++)
		if(def_group_type == group_type[i]) {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(pui->group_type[i]), TRUE);
			break;
		}
	
	for(i = 0; i < NUM_MDI_MODES; i++)
		if(mdi_mode == mdi_type[i]) {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(pui->mdi_type[i]), TRUE);
			break;
		}
}

void create_prefs_dialog(PropertyUI *pui) {
	GtkWidget *vbox, *label, *frame, *box, *entry, *fbox, *flabel;
	GSList *group;
	
	int i;
	
	pui->pbox = GNOME_PROPERTY_BOX(gnome_property_box_new());
	
	gtk_signal_connect(GTK_OBJECT(pui->pbox), "destroy",
					   GTK_SIGNAL_FUNC(prop_destroy_cb), pui);
	gtk_signal_connect(GTK_OBJECT(pui->pbox), "apply",
					   GTK_SIGNAL_FUNC(apply_changes_cb), pui);
	
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	
	frame = gtk_frame_new(_("Font"));
	gtk_container_border_width(GTK_CONTAINER(frame), 4);
	gtk_widget_show(frame);
	
	fbox = gtk_hbox_new(0, 0);
	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), def_font_name);
	gtk_signal_connect (GTK_OBJECT (entry), "changed",
						select_font_cb, pui->pbox);
	pui->font_button = gnome_font_picker_new();
	gnome_font_picker_set_font_name(GNOME_FONT_PICKER(pui->font_button),
									def_font_name);
	gnome_font_picker_set_mode(GNOME_FONT_PICKER(pui->font_button),
							   GNOME_FONT_PICKER_MODE_USER_WIDGET);
	gtk_signal_connect (GTK_OBJECT (pui->font_button), "font_set",
						GTK_SIGNAL_FUNC (select_font_cb), pui->pbox);
	flabel = gtk_label_new (_("Browse..."));
	gnome_font_picker_uw_set_widget(GNOME_FONT_PICKER(pui->font_button), GTK_WIDGET(flabel));
	
	gtk_object_set_user_data(GTK_OBJECT(entry), GTK_OBJECT(pui->font_button)); 
	gtk_object_set_user_data (GTK_OBJECT(pui->font_button), GTK_OBJECT(entry)); 
	
	gtk_widget_show(flabel);
	gtk_widget_show(GTK_WIDGET(pui->font_button));
	gtk_widget_show(entry);
	
	gtk_container_border_width(GTK_CONTAINER(fbox), 4);
	gtk_box_pack_start (GTK_BOX (fbox), entry, 1, 1, GNOME_PAD);
	gtk_box_pack_end (GTK_BOX (fbox), GTK_WIDGET(pui->font_button), 0, 1, GNOME_PAD);
	
	gtk_widget_show(fbox);
	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(fbox));
	
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 2);
	
	frame = gtk_frame_new(_("Default Group Type"));
	gtk_container_border_width(GTK_CONTAINER(frame), 4);
	gtk_widget_show(frame);
	
	box = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(box);
	group = NULL;
	for(i = 0; i < 3; i++) {
		pui->group_type[i] = GTK_RADIO_BUTTON(gtk_radio_button_new_with_label(group, _(group_type_label[i])));
		gtk_widget_show(GTK_WIDGET(pui->group_type[i]));
		gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(pui->group_type[i]), TRUE, TRUE, 2);
		group = gtk_radio_button_group (pui->group_type[i]);
	}
	gtk_container_add(GTK_CONTAINER(frame), box);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 2);
	
	label = gtk_label_new(_("Display"));
	gtk_widget_show(label);
	gtk_notebook_append_page (GTK_NOTEBOOK(pui->pbox->notebook), vbox, label);
	
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	
	frame = gtk_frame_new(_("MDI Mode"));
	gtk_container_border_width(GTK_CONTAINER(frame), 4);
	gtk_widget_show(frame);
	
	box = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(box);
	group = NULL;
	for(i = 0; i < NUM_MDI_MODES; i++) {
		pui->mdi_type[i] = GTK_RADIO_BUTTON(gtk_radio_button_new_with_label(group, _(mdi_type_label[i])));
		gtk_widget_show(GTK_WIDGET(pui->mdi_type[i]));
		gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(pui->mdi_type[i]), TRUE, TRUE, 2);
		group = gtk_radio_button_group (pui->mdi_type[i]);
	}
	
	gtk_container_add(GTK_CONTAINER(frame), box);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 2);  
	
	label = gtk_label_new(_("MDI"));
	gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(pui->pbox->notebook), vbox, label);
	
	set_prefs(pui);
	
	/* signals have to be connected after set_prefs(), otherwise
	   a gnome_property_box_changed() is called */
	
	for(i = 0; i < NUM_MDI_MODES; i++)
		gtk_signal_connect(GTK_OBJECT(pui->mdi_type[i]), "clicked",
						   properties_modified_cb, pui->pbox);
	for(i = 0; i < 3; i++)
		gtk_signal_connect(GTK_OBJECT(pui->group_type[i]), "clicked",
						   properties_modified_cb, pui->pbox);
}
