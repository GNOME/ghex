/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* print.h - printing related stuff for ghex

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

#ifndef __GHEX_PREFERENCES_H__
#define __GHEX_PREFERENCES_H__

G_BEGIN_DECLS

typedef struct _PropertyUI PropertyUI;
struct _PropertyUI {
	GtkWidget *pbox;
	GtkCheckButton *group_type[3];
	// GONE - GTK4
//	GtkRadioButton *group_type[3];
	GtkWidget *font_button, *undo_spin, *box_size_spin;
	GtkWidget *offset_menu, *offset_choice[3];
	GtkWidget *format, *offsets_col;
	GtkWidget *paper_sel, *print_font_sel;
	GtkWidget *df_button, *hf_button;
	GtkWidget *df_label, *hf_label;
};

extern PropertyUI *prefs_ui;
extern guint group_type[3];
extern gchar *group_type_label[3];

PropertyUI *create_prefs_dialog(void);
void       set_current_prefs(PropertyUI *pui);

G_END_DECLS

#endif /* !__GHEX_PREFERENCES_H__ */
