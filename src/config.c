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

/* We shall be using gconf for Gnome 2.0 -- SnM */
#include <gconf/gconf-client.h>

#define GHEX_BASE_KEY                "/apps/ghex2"
#define GHEX_PREF_FONT               "/font"
#define GHEX_PREF_GROUP              "/group"
#define GHEX_PREF_MAX_UNDO_DEPTH     "/maxundodepth"   
#define GHEX_PREF_OFFSET_FORMAT      "/offsetformat"
#define GHEX_PREF_OFFSETS_COLUMN     "/offsetscolumn"
#define GHEX_PREF_PAPER              "/paper"
#define GHEX_PREF_BOX_SIZE           "/boxsize"
#define GHEX_PREF_DATA_FONT          "/datafont"
#define GHEX_PREF_DATA_FONT_SIZE     "/datafontsize"
#define GHEX_PREF_HEADER_FONT        "/headerfont"
#define GHEX_PREF_HEADER_FONT_SIZE   "/headerfontsize"

static GConfClient *gconf_client = NULL;

gint def_group_type = GROUP_BYTE;
guint max_undo_depth;
gchar *offset_fmt = NULL;
gboolean show_offsets_column;

void save_configuration () {

	/* Set the default font name -- SnM */
	if (def_metrics) {
		gconf_client_set_string (gconf_client,
								 GHEX_BASE_KEY GHEX_PREF_FONT,
								 def_font_name,
								 NULL);
	}

	/* Set group type -- SnM */
	gconf_client_set_int (gconf_client,
						  GHEX_BASE_KEY GHEX_PREF_GROUP,
						  def_group_type,
						  NULL);

	/* Set the max undo depth -- SnM */
	gconf_client_set_int (gconf_client,
						  GHEX_BASE_KEY GHEX_PREF_MAX_UNDO_DEPTH,
						  max_undo_depth,
						  NULL);

	/* Set the offset format -- SnM */
	gconf_client_set_string (gconf_client,
							 GHEX_BASE_KEY GHEX_PREF_OFFSET_FORMAT,
							 offset_fmt,
							 NULL);

	/* Set show offsets column -- SnM */
	gconf_client_set_bool (gconf_client,
						   GHEX_BASE_KEY GHEX_PREF_OFFSETS_COLUMN,
						   show_offsets_column,
						   NULL);	
#ifdef SNM
	/* Set the printing paper -- SnM */
	gconf_client_set_string (gconf_client,
							 GHEX_BASE_KEY GHEX_PREF_PAPER,
							 gnome_paper_name (def_paper),
							 NULL);
#endif

	/* Set the box size  -- SnM */
	gconf_client_set_int (gconf_client,
						  GHEX_BASE_KEY GHEX_PREF_BOX_SIZE,
						  shaded_box_size,
						  NULL);

	/* Set the data font -- SnM */
	gconf_client_set_string (gconf_client,
							 GHEX_BASE_KEY GHEX_PREF_DATA_FONT,
							 data_font_name,
							 NULL);

	/* Set the data font size -- SnM */
	gconf_client_set_float (gconf_client,
							GHEX_BASE_KEY GHEX_PREF_DATA_FONT_SIZE,
							data_font_size,
							NULL);

	/* Set the header font -- SnM */
	gconf_client_set_string (gconf_client,
							 GHEX_BASE_KEY GHEX_PREF_HEADER_FONT,
							 header_font_name,
							 NULL);

	/* Set the header font size -- SnM */
	gconf_client_set_float (gconf_client,
							GHEX_BASE_KEY GHEX_PREF_HEADER_FONT_SIZE,
							header_font_size,
							NULL);

	gconf_client_suggest_sync (gconf_client, NULL);
} 

void load_configuration () {
	gchar *font_name;
#if 0
	GdkFont *new_font;
#endif
	PangoFontMetrics *new_metrics;

	GnomeFont *print_font;
	gchar *def_paper_name;

	/* Get the default font metrics */
	font_name = gconf_client_get_string (gconf_client,
										 GHEX_BASE_KEY GHEX_PREF_FONT,
										 NULL);

	if (NULL == font_name)
		font_name = g_strdup (DEFAULT_FONT);

	if (def_metrics)
		pango_font_metrics_unref (def_metrics);

	if (def_font_desc)
		pango_font_description_free (def_font_desc);

	new_metrics = gtk_hex_load_font (font_name);

	if (!new_metrics) {
		def_metrics = gtk_hex_load_font (DEFAULT_FONT);
		def_font_desc = pango_font_description_from_string (font_name);
		def_font_name = g_strdup (DEFAULT_FONT);
	}
	else {
		def_metrics = new_metrics;
		def_font_desc = pango_font_description_from_string (font_name);
		def_font_name = g_strdup (font_name);
	}

	g_free (font_name);

	/* Get the default group type -- SnM */
	def_group_type = gconf_client_get_int (gconf_client,
										   GHEX_BASE_KEY GHEX_PREF_GROUP,
										   NULL);

	/* Sanity check for group type */
	if (def_group_type <= 0 )
		def_group_type = GROUP_BYTE;

	/* Get the max undo depth -- SnM */
	max_undo_depth = gconf_client_get_int (gconf_client,
										   GHEX_BASE_KEY GHEX_PREF_MAX_UNDO_DEPTH,
										   NULL);


	/* Get the offset format -- SnM */ 

	if (offset_fmt)
		g_free (offset_fmt);

	offset_fmt = gconf_client_get_string (gconf_client,
										  GHEX_BASE_KEY GHEX_PREF_OFFSET_FORMAT,
										  NULL);

	/* Check if offset_fmt is NULL. Shouldnt happen if we get the default
	 * value from the gconf client -- SnM
	 */

	if (NULL==offset_fmt) {
		offset_fmt = g_strdup("%X"); 
	}

	/* Get the show offsets column value -- SnM */
	show_offsets_column = gconf_client_get_bool (gconf_client,
												 GHEX_BASE_KEY GHEX_PREF_OFFSETS_COLUMN,
												 NULL);

#ifdef SNM
	/* Get the default paper name -- SnM */
	def_paper_name = gconf_client_get_string (gconf_client,
											  GHEX_BASE_KEY GHEX_PREF_PAPER,
											  NULL);
	def_paper = gnome_paper_with_name (def_paper_name);
	g_free (def_paper_name);
	if (!def_paper)
		def_paper = gnome_paper_with_name (gnome_paper_name_default ());
#endif

	/* Get the shaded box size -- SnM */
	shaded_box_size = gconf_client_get_int (gconf_client,
											GHEX_BASE_KEY GHEX_PREF_BOX_SIZE,
											NULL);

	/* Get the data font name -- SnM */
	data_font_name = gconf_client_get_string (gconf_client,
											  GHEX_BASE_KEY GHEX_PREF_DATA_FONT,
											  NULL);
	
	/* Check if data_font_name is NULL. Should not happen if we get the
	 * default value from the gconf client -- SnM
	 */
	if (NULL == data_font_name) {
		data_font_name = g_strdup ("Courier 10");
	}

	/* Get the data font size -- SnM */
	data_font_size = gconf_client_get_float (gconf_client,
											 GHEX_BASE_KEY GHEX_PREF_DATA_FONT_SIZE,
											 NULL);

	/* Check if the data_font_size is 0.0. Should not happen if we get the
	 * default value from the gconf client -- SnM
	 */
	if (0.0 == data_font_size) {
		data_font_size = 10.0;
	}

	/* Get the header font name -- SnM */
	header_font_name = gconf_client_get_string (gconf_client,
												GHEX_BASE_KEY GHEX_PREF_HEADER_FONT,
												NULL);

	/* Check if the header_font_name is NULL. Should not happen if we get
	 * the default value from the gconf client -- SnM
	 */

	if(NULL == header_font_name) {
		header_font_name = g_strdup ("Helvetica 12");
	}

	/* Get the header font size -- SnM */
	header_font_size = gconf_client_get_float (gconf_client,
											   GHEX_BASE_KEY GHEX_PREF_HEADER_FONT_SIZE,
											   NULL);

	/* Check if the header_font_size is 0.0. Should not happen if we get
	 * the default value from the gconf client -- SnM
	 */

	if (0.0 == header_font_size) {
		header_font_size = 12.0;
	}	
	
#ifdef SNM
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
#endif
}

static void ghex_prefs_notify_cb (GConfClient *client,
								  guint cnxn_id,
								  GConfEntry *entry,
								  gpointer user_data)
{
	/* Doing nothing for now -- SnM */
}

void ghex_prefs_init ()
{
	gconf_client = gconf_client_get_default ();

	g_return_if_fail (gconf_client != NULL);

	gconf_client_add_dir (gconf_client,
						  GHEX_BASE_KEY,
						  GCONF_CLIENT_PRELOAD_RECURSIVE,
						  NULL);

	gconf_client_notify_add (gconf_client,
							 GHEX_BASE_KEY,
							 ghex_prefs_notify_cb,
							 NULL, NULL, NULL);
}
