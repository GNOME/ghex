/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ghex-application-window.c - GHex main application window

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

   Original GHex Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifndef GHEX_NOTEBOOK_TAB_H
#define GHEX_NOTEBOOK_TAB_H

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gtkhex.h"

G_BEGIN_DECLS

#define GHEX_TYPE_NOTEBOOK_TAB (ghex_notebook_tab_get_type ())
G_DECLARE_FINAL_TYPE (GHexNotebookTab, ghex_notebook_tab, GHEX, NOTEBOOK_TAB,
				GtkWidget)

/* Method Declarations */

GtkWidget * 	ghex_notebook_tab_new (void);
void 			ghex_notebook_tab_add_hex (GHexNotebookTab *self, GtkHex *gh);
const char * 	ghex_notebook_tab_get_filename (GHexNotebookTab *self);
GtkHex * 		ghex_notebook_tab_get_hex (GHexNotebookTab *self);

G_END_DECLS

#endif
