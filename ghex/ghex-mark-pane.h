/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* mark-dialog.h - types related to the mark dialog

   Copyright © 2023 Logan Rathbone <poprocks@gmail.com>

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

   Original GHex Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifndef MARKDIALOG_H
#define MARKDIALOG_H

#include "gtkhex.h"
#include "findreplace.h"
#include "common-ui.h"

G_BEGIN_DECLS

#define MARK_TYPE_DIALOG (mark_dialog_get_type ())
G_DECLARE_FINAL_TYPE (MarkDialog, mark_dialog, MARK, DIALOG, PaneDialog)

/* PUBLIC METHOD DECLARATIONS */

/* MarkDialog */
GtkWidget *mark_dialog_new (void);
void mark_dialog_refresh (MarkDialog *self);
void mark_dialog_activate_mark_num (MarkDialog *self, int mark_num);

G_END_DECLS

#endif /* MARKDIALOG_H */
