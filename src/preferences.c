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

/* Changed to libgnomeprintui from libgnomeprint -- SnM */
#include <libgnomeprintui/gnome-font-dialog.h>

#include "ghex.h"

static void set_prefs(PropertyUI *pui);
static void select_font_cb(GtkWidget *w, GnomePropertyBox *pbox);
static void apply_changes_cb(GnomePropertyBox *pbox, gint page, PropertyUI *pui);
static void max_undo_changed_cb(GtkAdjustment *adj, GnomePropertyBox *pbox);
static void box_size_changed_cb(GtkAdjustment *adj, GnomePropertyBox *pbox);
static void properties_modified_cb(GtkWidget *w, GnomePropertyBox *pbox);
static void offset_cb(GtkWidget *w, PropertyUI *pui);

PropertyUI *prefs_ui = NULL;

PangoFontMetrics *def_metrics = NULL; /* Changes for Gnome 2.0 */
PangoFontDescription *def_font_desc = NULL;

gchar *def_font_name = NULL;
gboolean show_offsets_column = TRUE;

guint mdi_type[NUM_MDI_MODES] = {
	BONOBO_MDI_DEFAULT_MODE,
	BONOBO_MDI_NOTEBOOK,
	BONOBO_MDI_TOPLEVEL,
	BONOBO_MDI_MODAL
};

gchar *mdi_type_label[NUM_MDI_MODES] = {
	N_("Default"),
	N_("Notebook"),
	N_("Toplevel"),
	N_("Modal"),
};

static char* get_font_name (const gchar *name, int size) {
	gchar *caption;

	g_warning ("Size in get_font_name is %d", size);
	caption = g_strdup_printf("%s %d", name, size);
	return caption;
}

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

#if 0
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
#endif

}

/*
 * Now we shall have a preferences dialog similar to what gedit2 does.
 * GnomePropertyBox has been deprecated completely.
 * We dont need this function. Alteast for now -- SnM
 * Look at dialogs/ghex-preferences-dialog.[ch]
 */

PropertyUI *create_prefs_dialog() {

#ifdef SNM
	static GnomeHelpMenuEntry help_entry = { "ghex", "prefs.html" };
#endif

	GtkWidget *vbox, *label, *frame, *box, *entry, *fbox, *flabel, *table;
	GtkWidget *menu, *item;
	GtkAdjustment *undo_adj, *box_adj;
#ifdef SNM
	GnomePaperSelector *paper_sel;
#endif

	GSList *group;
	PropertyUI *pui;
	const gchar *paper_name;

	int i;

	pui = g_new0(PropertyUI, 1);
	
	pui->pbox = GNOME_PROPERTY_BOX(gnome_property_box_new());
	
	gtk_signal_connect(GTK_OBJECT(pui->pbox), "apply",
					   GTK_SIGNAL_FUNC(apply_changes_cb), pui);

#ifdef SNM
	gtk_signal_connect (GTK_OBJECT (pui->pbox), "help",
					GTK_SIGNAL_FUNC(gnome_help_pbox_goto), &help_entry);
	gnome_dialog_close_hides(GNOME_DIALOG(pui->pbox), TRUE);
#endif
	

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
	
	fbox = gtk_hbox_new(0, 5);
#if 0
	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), def_font_name);
	gtk_signal_connect (GTK_OBJECT (entry), "changed",
						GTK_SIGNAL_FUNC(select_font_cb), pui->pbox);
#endif
	pui->font_button = gnome_font_picker_new();
	gnome_font_picker_set_font_name(GNOME_FONT_PICKER(pui->font_button),
									def_font_name);
	gnome_font_picker_set_mode(GNOME_FONT_PICKER(pui->font_button),
							   GNOME_FONT_PICKER_MODE_FONT_INFO);
	gnome_font_picker_fi_set_use_font_in_label (GNOME_FONT_PICKER (pui->font_button), TRUE, 14);
	gnome_font_picker_fi_set_show_size (GNOME_FONT_PICKER (pui->font_button), TRUE);
	
	gtk_signal_connect (GTK_OBJECT (pui->font_button), "font_set",
						GTK_SIGNAL_FUNC (select_font_cb), pui->pbox);
#if 0
	flabel = gtk_label_new (_("Browse..."));
	gnome_font_picker_uw_set_widget(GNOME_FONT_PICKER(pui->font_button), GTK_WIDGET(flabel));
#endif
	flabel = gtk_label_new("");
	gtk_label_set_mnemonic_widget (GTK_LABEL (flabel), pui->font_button);
#if 0
	
	gtk_object_set_user_data(GTK_OBJECT(entry), GTK_OBJECT(pui->font_button)); 
	gtk_object_set_user_data (GTK_OBJECT(pui->font_button), GTK_OBJECT(entry)); 
	
#endif
	gtk_widget_show(flabel);
	gtk_widget_show(GTK_WIDGET(pui->font_button));
#if 0
	gtk_widget_show(entry);
#endif
	gtk_container_border_width(GTK_CONTAINER(fbox), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (fbox), GTK_WIDGET(pui->font_button), FALSE, TRUE, GNOME_PAD_BIG);
#if 0
	gtk_box_pack_start (GTK_BOX (fbox), entry, 1, 1, GNOME_PAD);
	gtk_box_pack_end (GTK_BOX (fbox), GTK_WIDGET(pui->font_button), 0, 1, GNOME_PAD);
#endif
	
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

#ifdef SNM
	pui->paper_sel = gnome_paper_selector_new();
	paper_sel = GNOME_PAPER_SELECTOR(pui->paper_sel);
	paper_name = gnome_paper_name(def_paper);
	gnome_paper_selector_set_name(paper_sel, paper_name);
	gtk_widget_show(pui->paper_sel);
	gtk_container_add(GTK_CONTAINER(frame), pui->paper_sel);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE,
					   GNOME_PAD_SMALL);  
#endif

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

	pui->df_button = gnome_font_picker_new();
	gnome_font_picker_set_mode (GNOME_FONT_PICKER (pui->df_button),
				GNOME_FONT_PICKER_MODE_FONT_INFO);
	gnome_font_picker_fi_set_use_font_in_label (GNOME_FONT_PICKER (pui->df_button), TRUE, 14);
	gnome_font_picker_fi_set_show_size (GNOME_FONT_PICKER (pui->df_button), TRUE);
	pui->df_label = gtk_label_new("");
	gtk_label_set_mnemonic_widget (GTK_LABEL (pui->df_label), pui->df_button);
	
	
#if 0
	pui->df_button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(pui->df_button),
					  get_font_label(data_font_name, data_font_size));
	gtk_signal_connect(GTK_OBJECT(pui->df_button), "clicked",
					   GTK_SIGNAL_FUNC(font_button_clicked), &pui->data_font);
#endif
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
#if 0
	pui->hf_button = gtk_button_new();
#endif
	pui->hf_button = gnome_font_picker_new();
	gnome_font_picker_set_mode (GNOME_FONT_PICKER (pui->hf_button),
				GNOME_FONT_PICKER_MODE_FONT_INFO);
	gnome_font_picker_fi_set_use_font_in_label (GNOME_FONT_PICKER (pui->hf_button), TRUE, 14);
	gnome_font_picker_fi_set_show_size (GNOME_FONT_PICKER (pui->hf_button), TRUE);
	pui->hf_label = gtk_label_new("");
	gtk_label_set_mnemonic_widget (GTK_LABEL (pui->hf_label), pui->hf_button);

#if 0	
	gtk_signal_connect(GTK_OBJECT(pui->hf_button), "clicked",
					   GTK_SIGNAL_FUNC(font_button_clicked), &pui->header_font);
#endif
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
						   GTK_SIGNAL_FUNC(properties_modified_cb), pui->pbox);

	for(i = 0; i < 3; i++)
		gtk_signal_connect(GTK_OBJECT(pui->group_type[i]), "clicked",
						   GTK_SIGNAL_FUNC(properties_modified_cb), pui->pbox);

	gtk_signal_connect(GTK_OBJECT(pui->offsets_col), "toggled",
					   GTK_SIGNAL_FUNC(properties_modified_cb), pui->pbox);

#ifdef SNM
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(paper_sel->paper)->entry), "changed",
					   GTK_SIGNAL_FUNC(properties_modified_cb), pui->pbox);
	gtk_signal_connect(GTK_OBJECT(paper_sel->width), "changed",
					   GTK_SIGNAL_FUNC(properties_modified_cb), pui->pbox);
	gtk_signal_connect(GTK_OBJECT(paper_sel->height), "changed",
					   GTK_SIGNAL_FUNC(properties_modified_cb), pui->pbox);
	gtk_signal_connect(GTK_OBJECT(pui->df_button), "clicked",
					   GTK_SIGNAL_FUNC(properties_modified_cb), pui->pbox);
#endif

	gtk_signal_connect(GTK_OBJECT(pui->hf_button), "clicked",
					   GTK_SIGNAL_FUNC(properties_modified_cb), pui->pbox);

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

	gnome_font_picker_set_font_name(GNOME_FONT_PICKER(pui->hf_button),
					header_font_name);

	gnome_font_picker_set_font_name(GNOME_FONT_PICKER(pui->df_button),
					data_font_name);
	

#ifdef SNM
	if(def_paper)
		gnome_paper_selector_set_name(GNOME_PAPER_SELECTOR(pui->paper_sel),
									  gnome_paper_name(def_paper));
#endif

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
	PangoFontMetrics *new_metrics; /* Changes for Gnome 2.0 */
	PangoFontDescription *new_desc;

	guint new_undo_max;
	gboolean show_off, expect_spec;
	gchar *old_offset_fmt;
	gchar *new_paper_name;

#ifdef SNM
	if ( page != -1 ) return; /* Only do something on global apply */
#endif
	
	show_off = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pui->offsets_col));
	if(show_off != show_offsets_column) {
		show_offsets_column = show_off;
		child = bonobo_mdi_get_children (BONOBO_MDI (mdi));
			
		while(child) {
			view = bonobo_mdi_child_get_views (BONOBO_MDI_CHILD(child->data));
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
			child = bonobo_mdi_get_children (BONOBO_MDI (mdi));
			
			while(child) {
				view = bonobo_mdi_child_get_views (BONOBO_MDI_CHILD(child->data));
				while(view) {
					gtk_hex_set_group_type(GTK_HEX(view->data), def_group_type);
					view = g_list_next(view);
				}
				child = g_list_next(child);
			}
			break;
		}
	
	for(i = 0; i < NUM_MDI_MODES; i++)
		if(GTK_TOGGLE_BUTTON(pui->mdi_type[i])->active) {
			if(mdi_mode != mdi_type[i]) {
				mdi_mode = mdi_type[i];
				bonobo_mdi_set_mode( BONOBO_MDI(mdi), mdi_mode);
			}
			break;
		}

	new_undo_max = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(pui->undo_spin));
	if(new_undo_max != max_undo_depth) {
		max_undo_depth = new_undo_max;

		child = bonobo_mdi_get_children (BONOBO_MDI (mdi));
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
				gtk_entry_set_text(GTK_ENTRY(pui->format), old_offset_fmt);
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
		if((new_metrics = gtk_hex_load_font(gnome_font_picker_get_font_name
									 (GNOME_FONT_PICKER(pui->font_button)))) != NULL) {
			new_desc = pango_font_description_from_string (gnome_font_picker_get_font_name (GNOME_FONT_PICKER (pui->font_button)));
			child = bonobo_mdi_get_children (BONOBO_MDI (mdi));
			
			while(child) {
				view = bonobo_mdi_child_get_views (BONOBO_MDI_CHILD(child->data));
				while(view) {
					gtk_hex_set_font(GTK_HEX(view->data), new_metrics, new_desc);
					view = g_list_next(view);
				}
				child = g_list_next(child);
			}
		
			if (def_metrics)
				pango_font_metrics_unref (def_metrics);

			if (def_font_desc)
				pango_font_description_free (def_font_desc);

			def_metrics = new_metrics;
			
			g_free(def_font_name);
			pango_font_description_free (new_desc);
			
			def_font_name = g_strdup(gnome_font_picker_get_font_name
									 (GNOME_FONT_PICKER(pui->font_button)));
			def_font_desc = pango_font_description_from_string (def_font_name);
		}
		else
			display_error_dialog (bonobo_mdi_get_active_window (BONOBO_MDI (mdi)), _("Can not open desired font!"));
	} 


	data_font_name = g_strdup (gnome_font_picker_get_font_name (GNOME_FONT_PICKER (pui->df_button)));
	header_font_name = g_strdup (gnome_font_picker_get_font_name (GNOME_FONT_PICKER (pui->hf_button)));
#ifdef SNM /* Have to find a replacement for this */
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
#endif
}

static void select_font_cb(GtkWidget *w, GnomePropertyBox *pbox) {
	const gchar *font_desc;
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
