/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* findreplace.h - types related to find and replace dialogs

   Copyright (C) 2004 Free Software Foundation
   Copyright (C) 2005-2020 FIXME
   Copyright © 2021 Logan Rathbone

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

#ifndef FINDREPLACE_H
#define FINDREPLACE_H 

#include <gtk/gtk.h>

#include "gtkhex.h"
#include "findreplace.h"

G_BEGIN_DECLS

#define JUMP_TYPE_DIALOG (jump_dialog_get_type ())
G_DECLARE_FINAL_TYPE (JumpDialog, jump_dialog, JUMP, DIALOG, GtkWidget)

#define FIND_TYPE_DIALOG (find_dialog_get_type ())
G_DECLARE_FINAL_TYPE (FindDialog, find_dialog, FIND, DIALOG, GtkWidget)

#define REPLACE_TYPE_DIALOG (replace_dialog_get_type ())
G_DECLARE_FINAL_TYPE (ReplaceDialog, replace_dialog, REPLACE, DIALOG, GtkWidget)

/* PUBLIC METHOD DECLARATIONS */

/* FindDialog */
GtkWidget *find_dialog_new(void);
void find_dialog_set_hex(FindDialog *self, GtkHex *gh);

/* ReplaceDialog */
GtkWidget *replace_dialog_new(void);
void replace_dialog_set_hex(ReplaceDialog *self, GtkHex *gh);

/* JumpDialog */
GtkWidget *jump_dialog_new(void);
void jump_dialog_set_hex(JumpDialog *self, GtkHex *gh);

G_END_DECLS

#endif /* FINDREPLACE_H */
