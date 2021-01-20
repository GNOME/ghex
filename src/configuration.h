/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* configuration.h - constants and declarations for GSettings

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
   If not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifndef GHEX_CONFIGURATION_H
#define GHEX_CONFIGURATION_H

#include <gtk/gtk.h>

#include "preferences.h"
#include "configuration.h"

G_BEGIN_DECLS

/* GSettings keys */
#define GHEX_PREF_FONT               "font"
#define GHEX_PREF_GROUP              "group-data-by"
#define GHEX_PREF_DATA_FONT          "print-font-data"
#define GHEX_PREF_HEADER_FONT        "print-font-header"
#define GHEX_PREF_BOX_SIZE           "print-shaded-rows"
#define GHEX_PREF_OFFSETS_COLUMN     "show-offsets"

/* Our preferred settings; as only one copy of them is required,
 * we'll make them global vars, though this is a bit ugly.
 */
extern char			*def_font_name;
extern char			*data_font_name, *header_font_name;
extern char			*offset_fmt;
extern gboolean		show_offsets_column;

extern guint		shaded_box_size;
extern int			def_group_type;

extern GSettings	*settings;

/* Initializes the gsettings client */
void ghex_init_configuration (void);

G_END_DECLS

#endif /* GHEX_CONFIGURATION_H */
