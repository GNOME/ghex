/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* preferences.c - setting the preferences

   Copyright (C) 1998 - 2001 Free Software Foundation

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

#include <config.h>
#include <gnome.h>
#include <libgnomeprint/gnome-font-dialog.h>

#include "ghex.h"

static void set_prefs(PropertyUI *pui);
static void select_font_cb(GtkWidget *w, GnomePropertyBox *pbox);
static void apply_changes_cb(GnomePropertyBox *pbox, gint page, PropertyUI *pui);
static void max_undo_changed_cb(GtkAdjustment *adj, GnomePropertyBox *pbox);
static void box_size_changed_cb(GtkAdjustment *adj, GnomePropertyBox *pbox);
static void properties_modified_cb(GtkWidget *w, GnomePropertyBox *pbox);
static void offset_cb(GtkWidget *w, PropertyUI *pui);

PropertyUI *prefs_ui = NULL;

GdkFont *def_font = NULL;
gchar *def_font_name = NULL;
gboolean show_offsets_column = TRUE;

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

static GtkWidget *get_font_label(const gchar *name, gdouble size) {
	gchar *caption;
	GtkWidget *label;

	caption = g_strdup_printf("%s %.1f", name, size);
	label = gtk_label_new(caption);
	g_free(caption);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show(label);
	return label;
}

static void font_button_clicked(GtkWidget *button, GnomeFont **font) {
	static GtkWidget *fsd = NULL;
	static GtkWidget *fontsel = NULL;
	const gchar *f_name;
	gdouble f_size;

	if(!fsd) {
		fsd = gnome_font_selection_dialog_new(_("Select font"));
		fontsel = gnome_font_selection_dialog_get_fontsel (GNOME_FONT_SELECTION_DIALOG(fsd));
		gtk_window_set_modal(GTK_WINDOW(fsd), TRUE);
		gnome_dialog_close_hides(GNOME_DIALOG(fsd), TRUE);
	}
	if(*font)
		gnome_font_selection_set_font(GNOME_FONT_SELECTION (fontsel),
									  *font);

	gtk_widget_show(fsd);
	if(gnome_dialog_run_and_close(GNOME_DIALOG(fsd)) == 0) {
		GnomeFont *disp_font;
		disp_font = gnome_font_selection_get_font (GNOME_FONT_SELECTION(fontsel));
		if(*font)
			gtk_object_unref(GTK_OBJECT(*font));
		*font = disp_font;
		f_name = gnome_font_get_name(*font);
		f_size = gnome_font_get_size(*font);
		gtk_container_remove(GTK_CONTAINER(button), GTK_BIN(button)->child);
		gtk_container_add(GTK_CONTAINER(button), get_font_label(f_name, f_size));
	}
}

PropertyUI *create_prefs_dialog() {
	GtkWidget *vbox, *label, *frame, *box, *entry, *fbox, *flabel, *table;
	GtkWidget *menu, *item;
	GtkAdjustment *undo_adj, *box_adj;
	GnomePaperSelector *paper_sel;
	GSList *group;
	PropertyUI *pui;
	const gchar *paper_name;

	int i;

	pui = g_new0(PropertyUI, 1);
	
	pui->pbox = GNOME_PROPERTY_BOX(gnome_property_box_new());
	
	gtk_signal_connect(GTK_OBJECT(pui->pbox), "apply",
					   GTK_SIGNAL_FUNC(apply_changes_cb), pui);
	gnome_dialog_close_hides(GNOME_DIALOG(pui->pbox), TRUE);
	
	/* editing page */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);

	/* max undo levels */
	undo_adj = GTK_ADJUSTMENT(gtk_adjustment_new(MIN(max_undo_depth, MAX_MAX_UNDO_DEPTH),
												 0, MAX_MAX_UNDO_DEPTH, 1, 10, 0));
	gtk_signal_connect(GTK_OBJECT(undo_adj), "value_changed",
					   GTK_SIGNAL_FUNC(max_undo_changed_cb), pui->pbox);

	box = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(box);

	label = gtk_label_new(_("Maximum number of undo levels"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX(box), label, TRUE, TRUE, GNOME_PAD_SMALL);
	gtk_widget_show(label);
						  
	pui->undo_spin = gtk_spin_button_new(undo_adj, 1, 0);
	gtk_box_pack_end (GTK_BOX(box), GTK_WIDGET(pui->undo_spin), FALSE, TRUE, GNOME_PAD);
	gtk_widget_show(pui->undo_spin);

	gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, TRUE, GNOME_PAD_SMALL);

	/* cursor offset format */
	box = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(box);

	label = gtk_label_new(_("Show cursor offset in statusbar as"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX(box), label, TRUE, TRUE, GNOME_PAD);
	gtk_widget_show(label);

	pui->format = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(pui->format), "changed",
					   GTK_SIGNAL_FUNC(properties_modified_cb), pui->pbox);
	gtk_box_pack_start (GTK_BOX(box), pui->format, TRUE, TRUE, GNOME_PAD);
	gtk_widget_show(pui->format);

	pui->offset_menu = gtk_option_menu_new();
	gtk_widget_show(pui->offset_menu);
	menu = gtk_menu_new();
	pui->offset_choice[0] = item = gtk_menu_item_new_with_label(_("Decimal"));
	gtk_signal_connect(GTK_OBJECT(item), "activate",
					   GTK_SIGNAL_FUNC(offset_cb), pui);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	pui->offset_choice[1] = item = gtk_menu_item_new_with_label(_("Hexadecimal"));
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(pui->offset_menu), menu);
	gtk_signal_connect(GTK_OBJECT(item), "activate",
					   GTK_SIGNAL_FUNC(offset_cb), pui);
	pui->offset_choice[2] = item = gtk_menu_item_new_with_label(_("Custom"));
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(pui->offset_menu), menu);
	gtk_signal_connect(GTK_OBJECT(item), "activate",
					   GTK_SIGNAL_FUNC(offset_cb), pui);
	gtk_box_pack_end(GTK_BOX(box), GTK_WIDGET(pui->offset_menu), FALSE, TRUE, GNOME_PAD);

	gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, TRUE, GNOME_PAD_SMALL);

	/* show offsets check button */
	pui->offsets_col = gtk_check_button_new_with_label(_("Show offsets column"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pui->offsets_col), show_offsets_column);
	gtk_box_pack_start(GTK_BOX(vbox), pui->offsets_col, FALSE, TRUE, GNOME_PAD_SMALL);
	gtk_widget_show(pui->offsets_col);

	label = gtk_label_new(_("Editing"));
	gtk_widget_show(label);
	gtk_notebook_append_page (GTK_NOTEBOOK(pui->pbox->notebook), vbox, label);

	/* display page */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	
	/* display font */
	frame = gtk_frame_new(_("Font"));
	gtk_container_border_width(GTK_CONTAINER(frame), GNOME_PAD_SMALL);
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
	
	gtk_container_border_width(GTK_CONTAINER(fbox), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (fbox), entry, 1, 1, GNOME_PAD);
	gtk_box_pack_end (GTK_BOX (fbox), GTK_WIDGET(pui->font_button), 0, 1, GNOME_PAD);
	
	gtk_widget_show(fbox);
	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(fbox));
	
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, GNOME_PAD_SMALL);
	
	/* default group type */
	frame = gtk_frame_new(_("Default Group Type"));
	gtk_container_border_width(GTK_CONTAINER(frame), GNOME_PAD_SMALL);
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
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, GNOME_PAD_SMALL);
	
	label = gtk_label_new(_("Display"));
	gtk_widget_show(label);
	gtk_notebook_append_page (GTK_NOTEBOOK(pui->pbox->notebook), vbox, label);
	
	/* MDI page */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);

	/* mdi modes */
	frame = gtk_frame_new(_("MDI Mode"));
	gtk_container_border_width(GTK_CONTAINER(frame), GNOME_PAD_SMALL);
	gtk_widget_show(frame);

	box = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(box);
	group = NULL;
	for(i = 0; i < NUM_MDI_MODES; i++) {
		pui->mdi_type[i] = GTK_RADIO_BUTTON(gtk_radio_button_new_with_label(group, _(mdi_type_label[i])));
		gtk_widget_show(GTK_WIDGET(pui->mdi_type[i]));
		gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(pui->mdi_type[i]), TRUE, TRUE, GNOME_PAD_SMALL);
		group = gtk_radio_button_group (pui->mdi_type[i]);
	}
	
	gtk_container_add(GTK_CONTAINER(frame), box);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, GNOME_PAD_SMALL);  
	
	label = gtk_label_new(_("MDI"));
	gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(pui->pbox->notebook), vbox, label);
	
	/* printing page */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);

	/* paper selection */
	frame = gtk_frame_new(_("Paper size"));
	gtk_container_border_width(GTK_CONTAINER(frame), GNOME_PAD_SMALL);
	gtk_widget_show(frame);

	pui->paper_sel = gnome_paper_selector_new();
	paper_sel = GNOME_PAPER_SELECTOR(pui->paper_sel);
	paper_name = gnome_paper_name(def_paper);
	gnome_paper_selector_set_name(paper_sel, paper_name);
	gtk_widget_show(pui->paper_sel);
	gtk_container_add(GTK_CONTAINER(frame), pui->paper_sel);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE,
					   GNOME_PAD_SMALL);  

	/* data & header font selection */
	frame = gtk_frame_new(_("Fonts"));
	gtk_container_border_width(GTK_CONTAINER(frame), GNOME_PAD_SMALL);
	gtk_widget_show(frame);

	table = gtk_table_new(2, 2, TRUE);
	gtk_widget_show(table);
	label = gtk_label_new(_("Data font"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
					 GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_FILL,
					 GNOME_PAD_SMALL, GNOME_PAD_SMALL);
	pui->df_button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(pui->df_button),
					  get_font_label(data_font_name, data_font_size));
	gtk_signal_connect(GTK_OBJECT(pui->df_button), "clicked",
					   GTK_SIGNAL_FUNC(font_button_clicked), &pui->data_font);
	gtk_widget_show(pui->df_button);
	gtk_table_attach(GTK_TABLE(table), pui->df_button, 1, 2, 0, 1,
					 GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_FILL,
					 GNOME_PAD_SMALL, GNOME_PAD_SMALL);
	label = gtk_label_new(_("Header font"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
					 GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_FILL,
					 GNOME_PAD_SMALL, GNOME_PAD_SMALL);
	pui->hf_button = gtk_button_new();
	gtk_signal_connect(GTK_OBJECT(pui->hf_button), "clicked",
					   GTK_SIGNAL_FUNC(font_button_clicked), &pui->header_font);
	gtk_widget_show(pui->hf_button);
	gtk_table_attach(GTK_TABLE(table), pui->hf_button, 1, 2, 1, 2,
					 GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_FILL,
					 GNOME_PAD_SMALL, GNOME_PAD_SMALL);
	gtk_container_add(GTK_CONTAINER(frame), table);

	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE,
					   GNOME_PAD_SMALL);  

	/* shaded box entry */
	box_adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 1000, 1, 10, 0));
	gtk_signal_connect(GTK_OBJECT(box_adj), "value_changed",
					   GTK_SIGNAL_FUNC(box_size_changed_cb), pui->pbox);

	box = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(box);

	label = gtk_label_new(_("Print shaded box over"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start (GTK_BOX(box), label, TRUE, TRUE, GNOME_PAD_SMALL);
	gtk_widget_show(label);
						  
	pui->box_size_spin = gtk_spin_button_new(box_adj, 1, 0);
	gtk_box_pack_start (GTK_BOX(box), GTK_WIDGET(pui->box_size_spin), FALSE, TRUE, GNOME_PAD);
	gtk_widget_show(pui->box_size_spin);

	label = gtk_label_new(_("lines (0 for no box)"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX(box), label, FALSE, TRUE, GNOME_PAD_SMALL);
	gtk_widget_show(label);

	gtk_box_pack_start(GTK_BOX(vbox), box, TRUE, TRUE, GNOME_PAD_SMALL);  

	label = gtk_label_new(_("Printing"));
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

	gtk_signal_connect(GTK_OBJECT(pui->offsets_col), "toggled",
					   properties_modified_cb, pui->pbox);

	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(paper_sel->paper)->entry), "changed",
					   properties_modified_cb, pui->pbox);
	gtk_signal_connect(GTK_OBJECT(paper_sel->width), "changed",
					   properties_modified_cb, pui->pbox);
	gtk_signal_connect(GTK_OBJECT(paper_sel->height), "changed",
					   properties_modified_cb, pui->pbox);
	gtk_signal_connect(GTK_OBJECT(pui->df_button), "clicked",
					   properties_modified_cb, pui->pbox);
	gtk_signal_connect(GTK_OBJECT(pui->hf_button), "clicked",
					   properties_modified_cb, pui->pbox);

	return pui;
}

static void set_prefs(PropertyUI *pui) {
	int i;

	for(i = 0; i < 3; i++)
		if(def_group_type == group_type[i]) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pui->group_type[i]), TRUE);
			break;
		}
	
	for(i = 0; i < NUM_MDI_MODES; i++)
		if(mdi_mode == mdi_type[i]) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pui->mdi_type[i]), TRUE);
			break;
		}

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pui->offsets_col), show_offsets_column);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(pui->undo_spin), (gfloat)max_undo_depth);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(pui->box_size_spin), (gfloat)shaded_box_size);

	gtk_widget_set_sensitive(pui->format, FALSE);
	gtk_entry_set_text(GTK_ENTRY(pui->format), offset_fmt);
	if(strcmp(offset_fmt, "%d") == 0)
		gtk_option_menu_set_history(GTK_OPTION_MENU(pui->offset_menu), 0);
	else if(strcmp(offset_fmt, "%X") == 0)
		gtk_option_menu_set_history(GTK_OPTION_MENU(pui->offset_menu), 1);
	else {
		gtk_option_menu_set_history(GTK_OPTION_MENU(pui->offset_menu), 2);
		gtk_widget_set_sensitive(pui->format, TRUE);
	}

	if(def_paper)
		gnome_paper_selector_set_name(GNOME_PAPER_SELECTOR(pui->paper_sel),
									  gnome_paper_name(def_paper));
	if(GTK_BIN(pui->hf_button)->child)
		gtk_container_remove(GTK_CONTAINER(pui->hf_button),
							 GTK_BIN(pui->hf_button)->child);
	gtk_container_add(GTK_CONTAINER(pui->hf_button),
					  get_font_label(header_font_name, header_font_size));
	if(GTK_BIN(pui->df_button)->child)
		gtk_container_remove(GTK_CONTAINER(pui->df_button),
							 GTK_BIN(pui->df_button)->child);
	gtk_container_add(GTK_CONTAINER(pui->df_button),
					  get_font_label(data_font_name, data_font_size));
	if(pui->data_font)
		gtk_object_unref(GTK_OBJECT(pui->data_font));
	pui->data_font = gnome_font_new(data_font_name, data_font_size);
	if(pui->header_font)
		gtk_object_unref(GTK_OBJECT(pui->header_font));
	pui->header_font = gnome_font_new(header_font_name, header_font_size);	
}

/*
 * callbacks for properties dialog and its widgets
 */
static void properties_modified_cb(GtkWidget *w, GnomePropertyBox *pbox) {
	gnome_property_box_changed(pbox);
}

static void max_undo_changed_cb(GtkAdjustment *adj, GnomePropertyBox *pbox) {
	if((guint)adj->value != max_undo_depth)
		gnome_property_box_changed(pbox);
}


static void box_size_changed_cb(GtkAdjustment *adj, GnomePropertyBox *pbox) {
	if((guint)adj->value != shaded_box_size)
		gnome_property_box_changed(pbox);
}

static void apply_changes_cb(GnomePropertyBox *pbox, gint page, PropertyUI *pui) {
	int i, len;
	GList *child, *view;
	GdkFont *new_font;
	guint new_undo_max;
	gboolean show_off, expect_spec;
	gchar *old_offset_fmt;
	gchar *new_paper_name;

	if ( page != -1 ) return; /* Only do something on global apply */
	
	show_off = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pui->offsets_col));
	if(show_off != show_offsets_column) {
		show_offsets_column = show_off;
		child = mdi->children;
			
		while(child) {
			view = GNOME_MDI_CHILD(child->data)->views;
			while(view) {
				gtk_hex_show_offsets(GTK_HEX(view->data), show_off);
				view = g_list_next(view);
			}
			child = g_list_next(child);
		}
	}
		

	for(i = 0; i < 3; i++)
		if(GTK_TOGGLE_BUTTON(pui->group_type[i])->active) {
			def_group_type = group_type[i];
			break;
		}
	
	for(i = 0; i < NUM_MDI_MODES; i++)
		if(GTK_TOGGLE_BUTTON(pui->mdi_type[i])->active) {
			if(mdi_mode != mdi_type[i]) {
				mdi_mode = mdi_type[i];
				gnome_mdi_set_mode(mdi, mdi_mode);
			}
			break;
		}

	new_undo_max = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(pui->undo_spin));
	if(new_undo_max != max_undo_depth) {
		max_undo_depth = new_undo_max;

		child = mdi->children;
		while(child) {
			hex_document_set_max_undo(HEX_DOCUMENT(child->data), max_undo_depth);
			child = child->next;
		}
	}

	shaded_box_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(pui->box_size_spin));

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
				g_free(offset_fmt);
				offset_fmt = old_offset_fmt;
				gtk_entry_select_region(GTK_ENTRY(pui->format), i, i+1);
				gnome_error_dialog_parented(_("The offset format string contains invalid format specifier.\n"
											  "Only 'x', 'X', 'p', 'P', 'd' and 'o' are allowed."),
											GTK_WINDOW(pui->pbox));
				gtk_window_set_focus(GTK_WINDOW(pui->pbox), pui->format);
				break;
			}
		}
	}
	if(offset_fmt != old_offset_fmt)
		g_free(old_offset_fmt);

	if(strcmp(gnome_font_picker_get_font_name(GNOME_FONT_PICKER
											  (pui->font_button)), def_font_name) != 0) {
		if((new_font = gdk_font_load(gnome_font_picker_get_font_name
									 (GNOME_FONT_PICKER(pui->font_button)))) != NULL) {
			child = mdi->children;
			
			while(child) {
				view = GNOME_MDI_CHILD(child->data)->views;
				while(view) {
					gtk_hex_set_font(GTK_HEX(view->data), new_font);
					view = g_list_next(view);
				}
				child = g_list_next(child);
			}
			
			if(def_font)
				gdk_font_unref(def_font);
			
			def_font = new_font;
			
			g_free(def_font_name);
			
			def_font_name = g_strdup(gnome_font_picker_get_font_name
									 (GNOME_FONT_PICKER(pui->font_button)));
		}
		else
			gnome_app_error(mdi->active_window, _("Can not open desired font!"));
	} 

	new_paper_name = gnome_paper_selector_get_name(GNOME_PAPER_SELECTOR(pui->paper_sel));
	def_paper = gnome_paper_with_name(new_paper_name);

	if(pui->data_font) {
		if(data_font_name)
			g_free(data_font_name);
		data_font_name = g_strdup(gnome_font_get_name(GNOME_FONT(pui->data_font)));
		data_font_size = gnome_font_get_size(GNOME_FONT(pui->data_font));
	}
	if(pui->header_font) {
		if(header_font_name)
			g_free(header_font_name);
		header_font_name = g_strdup(gnome_font_get_name(GNOME_FONT(pui->header_font)));
		header_font_size = gnome_font_get_size(GNOME_FONT(pui->header_font));
	}
}

static void select_font_cb(GtkWidget *w, GnomePropertyBox *pbox) {
	gchar *font_desc;
	GtkWidget *peer;

	if (GNOME_IS_FONT_PICKER(w)) {
		font_desc = gnome_font_picker_get_font_name (GNOME_FONT_PICKER(w));
		peer = gtk_object_get_user_data (GTK_OBJECT(w));
		gtk_entry_set_text (GTK_ENTRY(peer), font_desc);
	}
	else {
		font_desc = gtk_entry_get_text (GTK_ENTRY(w));
		peer = gtk_object_get_user_data (GTK_OBJECT(w));
		gnome_font_picker_set_font_name (GNOME_FONT_PICKER(peer), font_desc);
		gnome_property_box_changed(pbox);
	}
}

static void offset_cb(GtkWidget *w, PropertyUI *pui) {
	int i;

	for(i = 0; i < 3; i++)
		if(w == pui->offset_choice[i])
			break;

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

	properties_modified_cb(w, pui->pbox);	
}
