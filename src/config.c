/* -*- mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* config.c - configuration loading/saving via gnome-config routines

   Copyright (C) 1997 - 2001 Free Software Foundation

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

#include <string.h>

#include "ghex.h"

gint def_group_type = GROUP_BYTE;
guint max_undo_depth;
gchar *offset_fmt = NULL;
gboolean show_offsets_column;

void save_configuration() {
	if(def_font)
		gnome_config_set_string("/ghex/Display/Font", def_font_name);
	else
		gnome_config_clean_key("/ghex/Display/Font");
	
	gnome_config_set_int("/ghex/Display/Group", def_group_type);
	
	gnome_config_set_int("/ghex/MDI/Mode", mdi_mode);
	
	gnome_config_set_int("/ghex/Editing/MaxUndoDepth", max_undo_depth);

	gnome_config_set_string("/ghex/Editing/OffsetFormat", offset_fmt);

	gnome_config_set_bool("/ghex/Editing/OffsetsColumn", show_offsets_column);

	gnome_config_set_string("/ghex/Printing/Paper", gnome_paper_name(def_paper));

	gnome_config_set_int("/ghex/Printing/BoxSize", shaded_box_size);

	gnome_config_set_string("/ghex/Printing/DataFont", data_font_name);

	gnome_config_set_float("/ghex/Printing/DataFontSize", data_font_size);

	gnome_config_set_string("/ghex/Printing/HeaderFont", header_font_name);

	gnome_config_set_float("/ghex/Printing/HeaderFontSize", header_font_size);

	gnome_config_sync();
}

void load_configuration() {
	gchar *font_desc;
	GdkFont *new_font;
	GnomeFont *print_font;
	gchar *def_paper_name;

	if((font_desc = gnome_config_get_string("/ghex/Display/Font=" DEFAULT_FONT)) != NULL) {
		if((new_font = gdk_font_load(font_desc)) != NULL) {
			if(def_font)
				gdk_font_unref(def_font);
			def_font = new_font;
			if(def_font_name)
				g_free(def_font_name);
			
			def_font_name = g_strdup(font_desc);
		}
	}
	
	def_group_type = gnome_config_get_int("/ghex/Display/Group=1");

	max_undo_depth = gnome_config_get_int("/ghex/Editing/MaxUndoDepth=100");

	if(offset_fmt)
		g_free(offset_fmt);

	offset_fmt = gnome_config_get_string("/ghex/Editing/OffsetFormat=%X");

	mdi_mode = gnome_config_get_int("/ghex/MDI/Mode=2");

	show_offsets_column = gnome_config_get_bool("/ghex/Editing/OffsetsColumn=true");

	def_paper_name = gnome_config_get_string("/ghex/Printing/Paper=a4");
	def_paper = gnome_paper_with_name(def_paper_name);
	g_free(def_paper_name);
	if(!def_paper)
		def_paper = gnome_paper_with_name(gnome_paper_name_default());

 	shaded_box_size = gnome_config_get_int("/ghex/Printing/BoxSize=0");

	data_font_name = gnome_config_get_string("/ghex/Printing/DataFont=Courier");
	data_font_size = gnome_config_get_float("/ghex/Printing/DataFontSize=10");
	header_font_name = gnome_config_get_string("/ghex/Printing/HeaderFont=Helvetica");
	header_font_size = gnome_config_get_float("/ghex/Printing/HeaderFontSize=12");
	print_font = gnome_font_new(data_font_name, data_font_size);
	if(!print_font) {
		data_font_name = g_strdup("Courier");
		data_font_size = 10.0;
	}
	else
		gtk_object_unref(GTK_OBJECT(print_font));
	print_font = gnome_font_new(header_font_name, header_font_size);
	if(!print_font) {
	    header_font_name = g_strdup("Helvetica");
		header_font_size = 12.0;
	}
	else
		gtk_object_unref(GTK_OBJECT(print_font));
}
