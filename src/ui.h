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
   If not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifndef __GHEX_UI_H__
#define __GHEX_UI_H__

#include <gtk/gtk.h>

#include <libbonoboui.h>

#include "ghex-window.h"
#include "preferences.h"

G_BEGIN_DECLS

/* command verbs */
extern BonoboUIVerb ghex_verbs[];

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

void find_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void advanced_find_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void replace_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void jump_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void set_byte_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void set_word_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void set_long_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void undo_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void redo_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void add_view_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void remove_view_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void insert_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void quit_app_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void file_list_activated_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);

G_END_DECLS

#endif /* !__GHEX_UI_H__ */
