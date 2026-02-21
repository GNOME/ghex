/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* chartable.h - character table dialog

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

#ifndef GHEX_CHARTABLE_H
#define GHEX_CHARTABLE_H

#include <stdlib.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gtkhex.h"
#include "common-ui.h"

G_BEGIN_DECLS

GtkWidget *create_char_table (GtkWindow *parent_win, /* can-NULL */
		HexWidget *gh);

G_END_DECLS

#endif /* GHEX_CHARTABLE_H */
