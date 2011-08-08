/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* preferences.c - setting the preferences

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
#endif /* HAVE_CONFIG_H */

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "preferences.h"
#include "configuration.h"
#include "ghex-window.h"
#include "ui.h"

#define MAX_MAX_UNDO_DEPTH 100000

static void select_font_cb(GtkWidget *w, PropertyUI *pui);
static void select_display_font_cb(GtkWidget *w, PropertyUI *pui);
static void max_undo_changed_cb(GtkAdjustment *adj, PropertyUI *pui);
static void box_size_changed_cb(GtkAdjustment *adj, PropertyUI *pui);
static void offset_cb(GtkWidget *w, PropertyUI *pui);
static void prefs_response_cb(GtkDialog *dlg, gint response, PropertyUI *pui);
static void offsets_col_cb(GtkToggleButton *tb, PropertyUI *pui);
static void group_type_cb(GtkRadioButton *rd, PropertyUI *pui);
static void format_activated_cb(GtkEntry *entry, PropertyUI *pui);
static gboolean format_focus_out_event_cb(GtkEntry *entry, GdkEventFocus *event,
										  PropertyUI *pui);

PropertyUI *prefs_ui = NULL;

PropertyUI *
create_prefs_dialog()
{
	GtkWidget *vbox, *label, *frame, *box, *fbox, *flabel, *table;
	GtkAdjustment *undo_adj, *box_adj;
	GtkWidget *notebook;

	GSList *group;
	PropertyUI *pui;

	int i;

	gboolean gail_up;

	pui = g_new0(PropertyUI, 1);
	
	pui->pbox = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(pui->pbox), _("GHex Preferences"));
	
	gtk_dialog_add_button (GTK_DIALOG (pui->pbox),
						   GTK_STOCK_CLOSE,
						   GTK_RESPONSE_CLOSE);

	gtk_dialog_add_button (GTK_DIALOG (pui->pbox),
						   GTK_STOCK_HELP,
						   GTK_RESPONSE_HELP);

	g_signal_connect(G_OBJECT(pui->pbox), "response",
					 G_CALLBACK(prefs_response_cb), pui);

	g_signal_connect(G_OBJECT(pui->pbox), "delete-event",
					 G_CALLBACK (gtk_widget_hide_on_delete),
					 NULL);

	notebook = gtk_notebook_new();
	gtk_widget_show(notebook);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(pui->pbox))), notebook);

	/* editing page */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);

	/* max undo levels */
	undo_adj = GTK_ADJUSTMENT(gtk_adjustment_new(MIN(max_undo_depth, MAX_MAX_UNDO_DEPTH),
												 0, MAX_MAX_UNDO_DEPTH, 1, 10, 0));

	box = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(box);

	label = gtk_label_new_with_mnemonic(_("_Maximum number of undo levels:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX(box), label, TRUE, TRUE, 8);
	gtk_widget_show(label);
						  
	pui->undo_spin = gtk_spin_button_new(undo_adj, 1, 0);
	gtk_box_pack_end (GTK_BOX(box), GTK_WIDGET(pui->undo_spin), FALSE, TRUE, 8);
	gtk_widget_show(pui->undo_spin);

	gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, TRUE, 8);

	/* cursor offset format */
	box = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(box);

	gtk_label_set_mnemonic_widget(GTK_LABEL(label), pui->undo_spin);

	gail_up = GTK_IS_ACCESSIBLE(gtk_widget_get_accessible(label)) ;

	if (gail_up) {
		add_atk_namedesc (pui->undo_spin, _("Undo levels"), _("Select maximum number of undo levels"));
		add_atk_relation (pui->undo_spin, label, ATK_RELATION_LABELLED_BY);
	}

	label = gtk_label_new_with_mnemonic(_("_Show cursor offset in statusbar as:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX(box), label, TRUE, TRUE, 8);
	gtk_widget_show(label);

	pui->format = gtk_entry_new();
	g_signal_connect(G_OBJECT(pui->format), "activate",
					 G_CALLBACK(format_activated_cb), pui);
	g_signal_connect(G_OBJECT(pui->format), "focus_out_event",
					 G_CALLBACK(format_focus_out_event_cb), pui);
	gtk_box_pack_start (GTK_BOX(box), pui->format, TRUE, TRUE, 8);
	gtk_widget_show(pui->format);

	pui->offset_menu = gtk_combo_box_text_new();
	gtk_label_set_mnemonic_widget (GTK_LABEL(label), pui->offset_menu);
	gtk_widget_show(pui->offset_menu);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(pui->offset_menu),
								   _("Decimal"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(pui->offset_menu),
								   _("Hexadecimal"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(pui->offset_menu),
								   _("Custom"));
	g_signal_connect(G_OBJECT(pui->offset_menu), "changed",
					 G_CALLBACK(offset_cb), pui);
	gtk_box_pack_end(GTK_BOX(box), GTK_WIDGET(pui->offset_menu),
					 FALSE, TRUE, 8);

	gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, TRUE, 4);

	if (gail_up) {
		add_atk_namedesc (pui->format, "format_entry", _("Enter the cursor offset format"));
		add_atk_namedesc (pui->offset_menu, "format_combobox", _("Select the cursor offset format"));
		add_atk_relation (label, pui->format, ATK_RELATION_LABEL_FOR);
		add_atk_relation (pui->format, label, ATK_RELATION_LABELLED_BY);
		add_atk_relation (label, pui->offset_menu, ATK_RELATION_LABEL_FOR);
		add_atk_relation (pui->offset_menu, label, ATK_RELATION_LABELLED_BY);
		add_atk_relation (pui->format, pui->offset_menu, ATK_RELATION_CONTROLLED_BY);
		add_atk_relation (pui->offset_menu, pui->format, ATK_RELATION_CONTROLLER_FOR);
	}

	/* show offsets check button */
	pui->offsets_col = gtk_check_button_new_with_mnemonic(_("Sh_ow offsets column"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pui->offsets_col), show_offsets_column);
	gtk_box_pack_start(GTK_BOX(vbox), pui->offsets_col, FALSE, TRUE, 4);
	gtk_widget_show(pui->offsets_col);

	label = gtk_label_new(_("Editing"));
	gtk_widget_show(label);
	gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, label);

	/* display page */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	
	/* display font */
	frame = gtk_frame_new(_("Font"));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
	gtk_widget_show(frame);
	
	fbox = gtk_hbox_new(0, 5);
	pui->font_button = gtk_font_button_new();
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(pui->font_button),
								  def_font_name);
	g_signal_connect (pui->font_button, "font-set",
	                  G_CALLBACK (select_display_font_cb), pui);
	flabel = gtk_label_new("");
	gtk_label_set_mnemonic_widget (GTK_LABEL (flabel), pui->font_button);
	gtk_widget_show(flabel);
	gtk_widget_show(GTK_WIDGET(pui->font_button));
	gtk_container_set_border_width(GTK_CONTAINER(fbox), 4);
	gtk_box_pack_start (GTK_BOX (fbox), GTK_WIDGET(pui->font_button), FALSE, TRUE, 12);
	
	gtk_widget_show(fbox);
	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(fbox));
	
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 4);
	
	/* default group type */
	frame = gtk_frame_new(_("Default Group Type"));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
	gtk_widget_show(frame);

	box = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(box);
	group = NULL;
	for(i = 0; i < 3; i++) {
		pui->group_type[i] = GTK_RADIO_BUTTON(gtk_radio_button_new_with_mnemonic(group, _(group_type_label[i])));
		gtk_widget_show(GTK_WIDGET(pui->group_type[i]));
		gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(pui->group_type[i]), TRUE, TRUE, 2);
		group = gtk_radio_button_get_group(pui->group_type[i]);
	}
	gtk_container_add(GTK_CONTAINER(frame), box);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 4);
	
	label = gtk_label_new(_("Display"));
	gtk_widget_show(label);
	gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, label);
	
	/* printing page */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);

	/* paper selection */
	frame = gtk_frame_new(_("Paper size"));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
	gtk_widget_show(frame);

	/* data & header font selection */
	frame = gtk_frame_new(_("Fonts"));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
	gtk_widget_show(frame);

	table = gtk_table_new(2, 3, TRUE);
	gtk_widget_show(table);
	label = gtk_label_new_with_mnemonic(_("_Data font:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
					 GTK_FILL, GTK_FILL,
					 4, 4);

	pui->df_button = gtk_font_button_new_with_font(def_font_name);
	g_signal_connect (G_OBJECT (pui->df_button), "font_set",
					  G_CALLBACK (select_font_cb), pui);
	pui->df_label = gtk_label_new("");
	gtk_label_set_mnemonic_widget (GTK_LABEL (pui->df_label), pui->df_button);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), pui->df_button);

	if (gail_up) {
		add_atk_namedesc (pui->df_button, _("Data font"), _("Select the data font"));
		add_atk_relation (pui->df_button, label, ATK_RELATION_LABELLED_BY);
	}	
	
	gtk_widget_show(pui->df_button);
	gtk_table_attach(GTK_TABLE(table), pui->df_button, 1, 2, 0, 1,
					 GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_FILL,
					 4, 4);
	label = gtk_label_new("");
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1,
					 GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_FILL,
					 4, 4);

	label = gtk_label_new_with_mnemonic(_("Header fo_nt:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
					 GTK_FILL, GTK_FILL,
					 4, 4);
	pui->hf_button = gtk_font_button_new_with_font(def_font_name);
	g_signal_connect (G_OBJECT (pui->hf_button), "font_set",
					  G_CALLBACK (select_font_cb), pui);
	pui->hf_label = gtk_label_new("");
	gtk_label_set_mnemonic_widget (GTK_LABEL (pui->hf_label), pui->hf_button);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), pui->hf_button);

	if (gail_up) {
		add_atk_namedesc (pui->hf_button, _("Header font"), _("Select the header font"));
		add_atk_relation (pui->hf_button, label, ATK_RELATION_LABELLED_BY);
	}

	gtk_widget_show(pui->hf_button);
	gtk_table_attach(GTK_TABLE(table), pui->hf_button, 1, 2, 1, 2,
					 GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_FILL,
					 4, 4);

	label = gtk_label_new("");
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 2, 3, 1, 2,
					 GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_FILL,
					 4, 4);

	gtk_container_add(GTK_CONTAINER(frame), table);

	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE,
					   4);

	/* shaded box entry */
	box_adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 1000, 1, 10, 0));

	box = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(box);

	label = gtk_label_new_with_mnemonic(_("_Print shaded box over:"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start (GTK_BOX(box), label, TRUE, TRUE, 4);
	gtk_widget_show(label);
						  
	pui->box_size_spin = gtk_spin_button_new(box_adj, 1, 0);
	gtk_box_pack_start (GTK_BOX(box), GTK_WIDGET(pui->box_size_spin), FALSE, TRUE, 8);
	gtk_widget_show(pui->box_size_spin);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), pui->box_size_spin);

	if (gail_up) {
		add_atk_namedesc (pui->box_size_spin, _("Box size"), _("Select size of box (in number of lines)"));
		add_atk_relation (pui->box_size_spin, label, ATK_RELATION_LABELLED_BY);
	}

	label = gtk_label_new(_("lines (0 for no box)"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX(box), label, FALSE, TRUE, 4);
	gtk_widget_show(label);

	gtk_box_pack_start(GTK_BOX(vbox), box, TRUE, TRUE, 4);

	label = gtk_label_new(_("Printing"));
	gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

	for(i = 0; i < 3; i++)
		g_signal_connect(G_OBJECT(pui->group_type[i]), "clicked",
						 G_CALLBACK(group_type_cb), pui);
	g_signal_connect(G_OBJECT(pui->offsets_col), "toggled",
					 G_CALLBACK(offsets_col_cb), pui);
	g_signal_connect(G_OBJECT(undo_adj), "value_changed",
					 G_CALLBACK(max_undo_changed_cb), pui);
	g_signal_connect(G_OBJECT(box_adj), "value_changed",
					 G_CALLBACK(box_size_changed_cb), pui);

	return pui;
}


void set_current_prefs(PropertyUI *pui) {
	int i;

	for(i = 0; i < 3; i++)
		if(def_group_type == group_type[i]) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pui->group_type[i]), TRUE);
			break;
		}
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pui->offsets_col), show_offsets_column);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(pui->undo_spin), (gfloat)max_undo_depth);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(pui->box_size_spin), (gfloat)shaded_box_size);

	gtk_widget_set_sensitive(pui->format, FALSE);
	gtk_entry_set_text(GTK_ENTRY(pui->format), offset_fmt);
	if(strcmp(offset_fmt, "%d") == 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(pui->offset_menu), 0);
	else if(strcmp(offset_fmt, "%X") == 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(pui->offset_menu), 1);
	else {
		gtk_combo_box_set_active(GTK_COMBO_BOX(pui->offset_menu), 2);
		gtk_widget_set_sensitive(pui->format, TRUE);
	}

	if(header_font_name)
		gtk_font_button_set_font_name(GTK_FONT_BUTTON(pui->hf_button),
			 header_font_name);
	if(data_font_name)
		gtk_font_button_set_font_name(GTK_FONT_BUTTON(pui->df_button),
			 data_font_name);
	if(def_font_name)
		gtk_font_button_set_font_name(GTK_FONT_BUTTON(pui->font_button),
									  def_font_name);

	gtk_dialog_set_default_response(GTK_DIALOG(pui->pbox), GTK_RESPONSE_CLOSE);
}

static void
max_undo_changed_cb(GtkAdjustment *adj, PropertyUI *pui)
{
	if((guint)gtk_adjustment_get_value(adj) != max_undo_depth) {
		max_undo_depth = gtk_spin_button_get_value_as_int
			(GTK_SPIN_BUTTON(pui->undo_spin));
		g_settings_set (settings,
		                GHEX_PREF_MAX_UNDO_DEPTH,
		                "u",
		                max_undo_depth);
	}
}

static void
box_size_changed_cb(GtkAdjustment *adj, PropertyUI *pui)
{
	if((guint)gtk_adjustment_get_value(adj) != shaded_box_size) {
		shaded_box_size = gtk_spin_button_get_value_as_int
			(GTK_SPIN_BUTTON(pui->box_size_spin));
		g_settings_set (settings,
		                GHEX_PREF_BOX_SIZE,
		                "u",
		                shaded_box_size);
	}
}

static void
offsets_col_cb(GtkToggleButton *tb, PropertyUI *pui)
{
	show_offsets_column = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(pui->offsets_col));
	g_settings_set_boolean (settings,
	                        GHEX_PREF_OFFSETS_COLUMN,
	                        show_offsets_column);
}

static void
group_type_cb(GtkRadioButton *rd, PropertyUI *pui)
{
	int i;

	for(i = 0; i < 3; i++)
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pui->group_type[i]))) {
			def_group_type = group_type[i];
			break;
		}
	g_settings_set_enum (settings,
	                     GHEX_PREF_GROUP,
	                     def_group_type);
}

static void
prefs_response_cb(GtkDialog *dlg, gint response, PropertyUI *pui)
{
	GError *error = NULL;

	switch(response) {
	case GTK_RESPONSE_HELP:
		gtk_show_uri (NULL, "ghelp:ghex?ghex-prefs",  gtk_get_current_event_time (), &error);
		if(NULL != error) {
			GtkWidget *dialog;
			dialog = gtk_message_dialog_new
				(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				 _("There was an error displaying help: \n%s"),
				 error->message);
			g_signal_connect(G_OBJECT (dialog), "response",
							 G_CALLBACK (gtk_widget_destroy),
							 NULL);
			gtk_window_set_resizable(GTK_WINDOW (dialog), FALSE);
			gtk_window_present (GTK_WINDOW (dialog));

			g_error_free(error);
		}
		break;
	case GTK_RESPONSE_CLOSE:
		gtk_widget_hide(pui->pbox);
		break;
	default:
		gtk_widget_hide(pui->pbox);
		break;
	}
}

static void
select_display_font_cb(GtkWidget *w, PropertyUI *pui)
{
	PangoFontMetrics *new_metrics;
	PangoFontDescription *new_desc;

	if(strcmp(gtk_font_button_get_font_name
			  (GTK_FONT_BUTTON(pui->font_button)),
			  def_font_name) != 0) {
		if((new_metrics = gtk_hex_load_font
			(gtk_font_button_get_font_name
			 (GTK_FONT_BUTTON(pui->font_button)))) != NULL) {
			new_desc = pango_font_description_from_string
				(gtk_font_button_get_font_name
				 (GTK_FONT_BUTTON (pui->font_button)));
			if (def_metrics)
				pango_font_metrics_unref (def_metrics);
			if (def_font_desc)
				pango_font_description_free (def_font_desc);
			def_metrics = new_metrics;
			if(def_font_name)
				g_free(def_font_name);
			def_font_name = g_strdup
				(gtk_font_button_get_font_name
				 (GTK_FONT_BUTTON(pui->font_button)));
			def_font_desc = new_desc;
			g_settings_set_string (settings,
			                       GHEX_PREF_FONT,
			                       def_font_name);
		}
		else
			display_error_dialog (ghex_window_get_active(),
								  _("Can not open desired font!"));
	}
}

static void
select_font_cb(GtkWidget *w, PropertyUI *pui)
{
	if(w == pui->df_button) {
		if(data_font_name)
			g_free(data_font_name);
		data_font_name = g_strdup(gtk_font_button_get_font_name
								  (GTK_FONT_BUTTON (pui->df_button)));
		g_settings_set_string (settings,
		                       GHEX_PREF_DATA_FONT,
		                       data_font_name);
	}
	else if(w == pui->hf_button) {
		if(header_font_name)
			g_free(header_font_name);
		header_font_name = g_strdup(gtk_font_button_get_font_name
									(GTK_FONT_BUTTON (pui->hf_button)));
		g_settings_set_string (settings,
		                       GHEX_PREF_HEADER_FONT,
		                       header_font_name);
	}
}

static void
update_offset_fmt_from_entry(GtkEntry *entry, PropertyUI *pui)
{
	int i, len;
	gchar *old_offset_fmt;
	gboolean expect_spec;
	const GList *win_list;

	old_offset_fmt = offset_fmt;
	offset_fmt = g_strdup(gtk_entry_get_text(GTK_ENTRY(pui->format)));
	/* check for a valid format string */
	len = strlen(offset_fmt);
	expect_spec = FALSE;
	for(i = 0; i < len; i++) {
		if(offset_fmt[i] == '%')
			expect_spec = TRUE;
		if( expect_spec &&
			( (offset_fmt[i] >= 'a' && offset_fmt[i] <= 'z') ||
			  (offset_fmt[i] >= 'A' && offset_fmt[i] <= 'Z') ) ) {
			expect_spec = FALSE;
			if(offset_fmt[i] != 'x' && offset_fmt[i] != 'd' &&
			   offset_fmt[i] != 'o' && offset_fmt[i] != 'X' &&
			   offset_fmt[i] != 'P' && offset_fmt[i] != 'p') {
				GtkWidget *msg_dialog;

				g_free(offset_fmt);
				offset_fmt = old_offset_fmt;
				gtk_entry_set_text(GTK_ENTRY(pui->format), old_offset_fmt);
				msg_dialog =
					gtk_message_dialog_new(GTK_WINDOW(pui->pbox),
										   GTK_DIALOG_MODAL|
										   GTK_DIALOG_DESTROY_WITH_PARENT,
										   GTK_MESSAGE_ERROR,
										   GTK_BUTTONS_OK,
										   _("The offset format string contains invalid format specifier.\n"
											 "Only 'x', 'X', 'p', 'P', 'd' and 'o' are allowed."));
				gtk_dialog_run(GTK_DIALOG(msg_dialog));
				gtk_widget_destroy(msg_dialog);
				gtk_widget_grab_focus(pui->format);
				break;
			}
		}
	}
	if(offset_fmt != old_offset_fmt)
		g_free(old_offset_fmt);
	g_settings_set_string (settings,
	                       GHEX_PREF_OFFSET_FORMAT,
	                       offset_fmt);
	win_list = ghex_window_get_list();
	while(NULL != win_list) {
		ghex_window_update_status_message((GHexWindow *)win_list->data);
		win_list = win_list->next;
	}
}

static gboolean
format_focus_out_event_cb(GtkEntry *entry, GdkEventFocus *event, PropertyUI *pui)
{
	update_offset_fmt_from_entry(entry, pui);
	return FALSE;
}

static void
format_activated_cb(GtkEntry *entry, PropertyUI *pui)
{
	update_offset_fmt_from_entry(entry, pui);
}

static void
offset_cb(GtkWidget *w, PropertyUI *pui)
{
	int i = gtk_combo_box_get_active(GTK_COMBO_BOX(w)); 

	switch(i) {
	case 0:
		gtk_entry_set_text(GTK_ENTRY(pui->format), "%d");
		gtk_widget_set_sensitive(pui->format, FALSE);
		break;
	case 1:
		gtk_entry_set_text(GTK_ENTRY(pui->format), "%X");
		gtk_widget_set_sensitive(pui->format, FALSE);
		break;
	case 2:
		gtk_widget_set_sensitive(pui->format, TRUE);
		break;
	}
	update_offset_fmt_from_entry(GTK_ENTRY(pui->format), pui);
}
