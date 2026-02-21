/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* paste-special.h - Declarations for paste special dialog

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

#ifndef PASTE_SPECIAL_H
#define PASTE_SPECIAL_H

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gtkhex.h"
#include "gtkhex-paste-data.h"
#include "ghex-application-window.h"
#include "common-ui.h"
#include "common-macros.h"

G_BEGIN_DECLS

GtkWidget *	create_paste_special_dialog (GHexApplicationWindow *parent,
		GdkClipboard *clip);

GtkWidget *	create_copy_special_dialog (GHexApplicationWindow *parent,
		GdkClipboard *clip);

G_END_DECLS

#endif /* PASTE_SPECIAL_H */
