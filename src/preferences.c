/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* preferences.c - setting the preferences

   Copyright (C) 1998, 1999 Free Software Foundation

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
#include "ghex.h"

static void set_prefs(PropertyUI *pui);
static void select_font_cb(GtkWidget *w, GnomePropertyBox *pbox);
static void apply_changes_cb(GnomePropertyBox *pbox, gint page, PropertyUI *pui);
static void max_undo_changed_cb(GtkAdjustment *adj, GnomePropertyBox *pbox);
static void properties_modified_cb(GtkWidget *w, GnomePropertyBox *pbox);
static void prop_destroy_cb(GtkWidget *w, PropertyUI *data);

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

void create_prefs_dialog(PropertyUI *pui) {
	GtkWidget *vbox, *label, *frame, *box, *entry, *fbox, *flabel;
	GtkAdjustment *undo_adj;
	GSList *group;
	
	int i;
	
	pui->pbox = GNOME_PROPERTY_BOX(gnome_property_box_new());
	
	gtk_signal_connect(GTK_OBJECT(pui->pbox), "destroy",
					   GTK_SIGNAL_FUNC(prop_destroy_cb), pui);
	gtk_signal_connect(GTK_OBJECT(pui->pbox), "apply",
					   GTK_SIGNAL_FUNC(apply_changes_cb), pui);
	
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);

	undo_adj = GTK_ADJUSTMENT(gtk_adjustment_new(MIN(max_undo_depth, MAX_MAX_UNDO_DEPTH),
												 0, MAX_MAX_UNDO_DEPTH, 1, 10, 0));
	gtk_signal_connect(GTK_OBJECT(undo_adj), "value_changed",
					   GTK_SIGNAL_FUNC(max_undo_changed_cb), pui->pbox);

	box = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(box);

	label = gtk_label_new(_("Maximum number of undo levels"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX(box), label, TRUE, TRUE, GNOME_PAD);
	gtk_widget_show(label);
						  
	pui->spin = gtk_spin_button_new(undo_adj, 1, 0);
	gtk_box_pack_end (GTK_BOX(box), GTK_WIDGET(pui->spin), FALSE, TRUE, GNOME_PAD);
	gtk_widget_show(pui->spin);

	gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, TRUE, 2);

	label = gtk_label_new(_("Editing"));
	gtk_widget_show(label);
	gtk_notebook_append_page (GTK_NOTEBOOK(pui->pbox->notebook), vbox, label);
	
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
}

/*
 * callbacks for properties dialog and its widgets
 */
static void properties_modified_cb(GtkWidget *w, GnomePropertyBox *pbox) {
	gnome_property_box_changed(pbox);
}

static void prop_destroy_cb(GtkWidget *w, PropertyUI *data) {
	data->pbox = NULL;
}

static void max_undo_changed_cb(GtkAdjustment *adj, GnomePropertyBox *pbox) {
	if((guint)adj->value != max_undo_depth)
		gnome_property_box_changed(pbox);
}

static void apply_changes_cb(GnomePropertyBox *pbox, gint page, PropertyUI *pui) {
	int i;
	GList *child, *view;
	GdkFont *new_font;
	guint new_undo_max;

	if ( page != -1 ) return; /* Only do something on global apply */
	
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

	new_undo_max = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(pui->spin));
	if(new_undo_max != max_undo_depth) {
		max_undo_depth = new_undo_max;

		child = mdi->children;
		while(child) {
			hex_document_set_max_undo(HEX_DOCUMENT(child->data), max_undo_depth);
			child = child->next;
		}
	}

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
}

static void select_font_cb(GtkWidget *w, GnomePropertyBox *pbox) {
	gchar *font_desc;
	GtkWidget *peer;
	if (GNOME_IS_FONT_PICKER(w)) {
		font_desc = gnome_font_picker_get_font_name (GNOME_FONT_PICKER(w));
		peer = gtk_object_get_user_data (GTK_OBJECT(w));
		gtk_entry_set_text (GTK_ENTRY(peer), font_desc);
	} else {
		font_desc = gtk_entry_get_text (GTK_ENTRY(w));
		peer = gtk_object_get_user_data (GTK_OBJECT(w));
		gnome_font_picker_set_font_name (GNOME_FONT_PICKER(peer), font_desc);
		gnome_property_box_changed(pbox);
	}
}
