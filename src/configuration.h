/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* print.h - printing related stuff for ghex

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

#ifndef __GHEX_CONFIGURATION_H__
#define __GHEX_CONFIGURATION_H__

#include <gconf/gconf-client.h>

#include <gtk/gtk.h>

#include "preferences.h"
#include "configuration.h"

G_BEGIN_DECLS

/* GConf keys */
#define GHEX_BASE_KEY                "/apps/ghex2"
#define GHEX_PREF_FONT               "/font"
#define GHEX_PREF_GROUP              "/group"
#define GHEX_PREF_MAX_UNDO_DEPTH     "/maxundodepth"   
#define GHEX_PREF_OFFSET_FORMAT      "/offsetformat"
#define GHEX_PREF_OFFSETS_COLUMN     "/offsetscolumn"
#define GHEX_PREF_PAPER              "/paper"
#define GHEX_PREF_BOX_SIZE           "/boxsize"
#define GHEX_PREF_DATA_FONT          "/datafont"
#define GHEX_PREF_HEADER_FONT        "/headerfont"

/* our preferred settings; as only one copy of them is required,
   we'll make them global vars, although this is a bit ugly */
extern PangoFontMetrics *def_metrics;
extern PangoFontDescription *def_font_desc;

extern gchar      *def_font_name;
extern gchar      *data_font_name, *header_font_name;
extern gdouble    data_font_size, header_font_size;    
extern guint      max_undo_depth;
extern gchar      *offset_fmt;
extern gboolean   show_offsets_column;

extern gint       shaded_box_size;
extern gint       def_group_type;

extern GConfClient *gconf_client;

/* Initializes the gconf client */
void ghex_init_configuration (void);

/* config stuff */
void ghex_load_configuration(void);

G_END_DECLS

#endif /* !__GHEX_CONFIGURATION_H__ */
