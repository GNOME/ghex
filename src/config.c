/* -*- mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* config.c - configuration loading/saving via gnome-config routines

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

#include <string.h>

#include "ghex.h"

gint def_group_type = GROUP_BYTE;

void save_configuration() {
	if(def_font)
		gnome_config_set_string("/ghex/Display/Font", def_font_name);
	else
		gnome_config_clean_key("/ghex/Display/Font");
	
	gnome_config_set_int("/ghex/Display/Group", def_group_type);
	
	gnome_config_set_int("/ghex/MDI/Mode", mdi_mode);
	
	gnome_config_sync();
}

void load_configuration() {
	gchar *font_desc;
	gint group;
	GdkFont *new_font;
	
	if((font_desc = gnome_config_get_string("/ghex/Display/Font=" DEFAULT_FONT)) != NULL) {
		if((new_font = gdk_font_load(font_desc)) != NULL) {
			if(def_font)
				gdk_font_unref(def_font);
			def_font = new_font;
			if(def_font_name)
				free(def_font_name);
			
			def_font_name = strdup(font_desc);
		}
	}
	
	def_group_type = gnome_config_get_int("/ghex/Display/Group=1");
	
	mdi_mode = gnome_config_get_int("/ghex/MDI/Mode=2");
}
