/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ui.h - main application user interface & ui utility functions

   Copyright (C) 2004 Free Software Foundation

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

#ifndef __GHEX_UI_H__
#define __GHEX_UI_H__

#include <gtk/gtk.h>

#include "ghex-window.h"
#include "preferences.h"

G_BEGIN_DECLS

/* various ui convenience functions */
void create_dialog_title   (GtkWidget *, gchar *);
gint ask_user              (GtkMessageDialog *);
void display_error_dialog (GHexWindow *win, const gchar *msg);
void display_info_dialog (GHexWindow *win, const gchar *msg, ...);
void raise_and_focus_widget(GtkWidget *);
void set_doc_menu_sensitivity(HexDocument *doc);
void update_dialog_titles (void);
GtkWidget *create_button   (GtkWidget *, const gchar *, gchar *);

/* hiding widgets on cancel or delete_event */
gint delete_event_cb(GtkWidget *, GdkEventAny *, GtkWindow *);
void cancel_cb      (GtkWidget *, GtkWidget *);

/* File menu */
void open_cb (GtkAction *action, gpointer user_data);
void save_cb (GtkAction *action, gpointer user_data);
void save_as_cb (GtkAction *action, gpointer user_data);
void export_html_cb (GtkAction *action, gpointer user_data);
void revert_cb (GtkAction *action, gpointer user_data);
void print_cb (GtkAction *action, gpointer user_data);
void print_preview_cb (GtkAction *action, gpointer user_data);
void close_cb (GtkAction *action, gpointer user_data);
void quit_app_cb (GtkAction *action, gpointer user_data);

/* Edit menu */
void undo_cb (GtkAction *action, gpointer user_data);
void redo_cb (GtkAction *action, gpointer user_data);
void copy_cb (GtkAction *action, gpointer user_data);
void cut_cb (GtkAction *action, gpointer user_data);
void paste_cb (GtkAction *action, gpointer user_data);
void find_cb (GtkAction *action, gpointer user_data);
void advanced_find_cb (GtkAction *action, gpointer user_data);
void replace_cb (GtkAction *action, gpointer user_data);
void jump_cb (GtkAction *action, gpointer user_data);
void insert_mode_cb (GtkAction *action, gpointer user_data);
void prefs_cb (GtkAction *action, gpointer user_data);

/* View menu */
void add_view_cb (GtkAction *action, gpointer user_data);
void remove_view_cb (GtkAction *action, gpointer user_data);
void base_data_cb (GtkAction *action, GtkRadioAction *current, gpointer user_data);
void group_data_cb (GtkAction *action, GtkRadioAction *current, gpointer user_data);
void set_byte_cb (GtkAction *action, gpointer user_data);
void set_word_cb (GtkAction *action, gpointer user_data);
void set_long_cb (GtkAction *action, gpointer user_data);

/* Windows menu */
void character_table_cb (GtkAction *action, gpointer user_data);
void converter_cb (GtkAction *action, gpointer user_data);
void type_dialog_cb (GtkAction *action, gpointer user_data);

/* Help menu */
void help_cb (GtkAction *action, GHexWindow *window);
void about_cb (GtkAction *action, GHexWindow *window);

void file_list_activated_cb (GtkAction *action, gpointer user_data);

G_END_DECLS

#endif /* !__GHEX_UI_H__ */
