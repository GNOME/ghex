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

#include "ghex.h"

void quit_app_cb(GtkWidget *);
void open_cb(GtkWidget *);
void close_cb(GtkWidget *);
void save_cb(GtkWidget *);
void save_as_cb(GtkWidget *);
void revert_cb(GtkWidget *);
void properties_modified_cb(GtkWidget *, GnomePropertyBox *);
void cancel_cb(GtkWidget *, GtkWidget **);
gint delete_event_cb(GtkWidget *, gpointer, GtkWidget **);
void prop_destroy_cb(GtkWidget *, PropertyUI *);
void select_font_cb(GtkWidget *, GnomePropertyBox *);
void apply_changes_cb(GnomePropertyBox *, gint, PropertyUI *);
void add_view_cb(GtkWidget *);
void remove_view_cb(GtkWidget *);

gint remove_doc_cb(GnomeMDI *, HexDocument *);
void view_changed_cb(GnomeMDI *, GtkHex *);
void child_changed_cb(GnomeMDI *, HexDocument *);
void customize_app_cb(GnomeMDI *, GnomeApp *);
void cleanup_cb(GnomeMDI *);

void prefs_cb(GtkWidget *);
void converter_cb(GtkWidget *);
void find_next_cb(GtkWidget *);
void find_prev_cb(GtkWidget *);
void replace_next_cb(GtkWidget *);
void replace_one_cb(GtkWidget *);
void replace_all_cb(GtkWidget *);
void goto_byte_cb(GtkWidget *);
void set_find_type_cb(GtkWidget *, gint);
void set_replace_type_cb(GtkWidget *, gint);
void conv_entry_cb(GtkEntry *, gint);

void open_selected_file(GtkWidget *);
void save_selected_file(GtkWidget *);

void about_cb(GtkWidget *);
void show_help_cb(GtkWidget *);

#endif
