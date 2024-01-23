/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* findreplace.h - types related to find and replace dialogs

   Copyright (C) 2004 Free Software Foundation

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

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifndef FINDREPLACE_H
#define FINDREPLACE_H 

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gtkhex.h"
#include "configuration.h"
#include "common-ui.h"

G_BEGIN_DECLS

#define PANE_TYPE_DIALOG pane_dialog_get_type ()
G_DECLARE_DERIVABLE_TYPE (PaneDialog, pane_dialog, PANE, DIALOG, GtkWidget)

struct _PaneDialogClass
{
	GtkWidgetClass parent_class;

	void (*closed) (PaneDialog  *self);

	/* Padding to allow adding up to 12 new virtual functions without
	 * breaking ABI. */
	gpointer padding[12];
};

#define FIND_TYPE_DIALOG (find_dialog_get_type ())
G_DECLARE_DERIVABLE_TYPE (FindDialog, find_dialog, FIND, DIALOG, PaneDialog)

struct _FindDialogClass
{
	PaneDialogClass parent_class;

	/* Padding to allow adding up to 12 new virtual functions without
	 * breaking ABI. */
	gpointer padding[12];
};

#define REPLACE_TYPE_DIALOG (replace_dialog_get_type ())
G_DECLARE_FINAL_TYPE (ReplaceDialog, replace_dialog, REPLACE, DIALOG,
		FindDialog)

#define JUMP_TYPE_DIALOG (jump_dialog_get_type ())
G_DECLARE_FINAL_TYPE (JumpDialog, jump_dialog, JUMP, DIALOG, PaneDialog)

/* PUBLIC METHOD DECLARATIONS */

/* PaneDialog (generic) */
void pane_dialog_set_hex (PaneDialog *self, HexWidget *gh);
HexWidget *pane_dialog_get_hex (PaneDialog *self);
void pane_dialog_close (PaneDialog *self);

/* FindDialog */
GtkWidget *find_dialog_new (void);

/* ReplaceDialog */
GtkWidget *replace_dialog_new (void);

/* JumpDialog */
GtkWidget *jump_dialog_new (void);

G_END_DECLS

#endif /* FINDREPLACE_H */
