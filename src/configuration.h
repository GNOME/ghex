/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* configuration.h - constants and declarations for GSettings

   Copyright (C) 1998 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021 Logan Rathbone <poprocks@gmail.com>

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

   Original Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifndef GHEX_CONFIGURATION_H
#define GHEX_CONFIGURATION_H

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

/* GSettings keys */
#define GHEX_PREF_FONT				"font"
#define GHEX_PREF_GROUP				"group-data-by"
#define GHEX_PREF_SB_OFFSET_FORMAT	"statusbar-offset-format"
#define GHEX_PREF_DATA_FONT			"print-font-data"
#define GHEX_PREF_HEADER_FONT		"print-font-header"
#define GHEX_PREF_BOX_SIZE			"print-shaded-rows"
#define GHEX_PREF_OFFSETS_COLUMN	"show-offsets"
#define GHEX_PREF_DARK_MODE			"dark-mode"
#define GHEX_PREF_CONTROL_CHARS		"display-control-characters"

enum dark_mode {
	DARK_MODE_OFF,
	DARK_MODE_ON,
	DARK_MODE_SYSTEM
};

/* Our preferred settings; as only one copy of them is required,
 * we'll make them global vars, though this is a bit ugly.
 */
extern char			*def_font_name;
extern char			*data_font_name, *header_font_name;
extern char			*offset_fmt;
extern gboolean		show_offsets_column;
extern guint		shaded_box_size;
extern int			def_group_type;
extern int        def_sb_offset_format;
extern int			def_dark_mode;
extern gboolean		def_display_control_characters;

extern GSettings	*settings;
extern GtkCssProvider *global_provider;

/* Initializes the gsettings client */
void ghex_init_configuration (void);

G_END_DECLS

#endif /* GHEX_CONFIGURATION_H */
