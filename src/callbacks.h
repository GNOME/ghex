/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* callbacks.h - forward decls of callbacks.c

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

#ifndef CALLBACKS_H
#define CALLBACKS_H

/* the tale of two entries ;) */
typedef struct _ReplaceCBData ReplaceCBData;

struct _ReplaceCBData {
  GtkEntry *find;
  GtkEntry *replace;
};

void select_buffer_cb();
void quit_app_cb();
void open_cb();
void close_cb();
void save_cb();
void save_as_cb();
void revert_cb();
void properties_modified_cb();
void cancel_cb();
gint delete_event_cb();
void prop_destroy_cb();
void select_font_cb();
void apply_changes_cb();
void add_view_cb();
void remove_view_cb();

gint remove_doc_cb();
void view_changed_cb();
void cleanup_cb();

void prefs_cb();
void converter_cb();
void find_next_cb();
void find_prev_cb();
void replace_next_cb();
void replace_one_cb();
void replace_all_cb();
void goto_byte_cb();
void set_find_type_cb();
void set_replace_type_cb();
void conv_entry_cb();

void open_selected_file();
void save_selected_file();

void about_cb();
void show_help_cb();

#endif
