/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* chartable.h - character table dialog

   Copyright (C) 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021-2026 Logan Rathbone <poprocks@gmail.com>

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

   Original Author: Jaka Mocnik <jaka@gnu.org>
*/

#pragma once

#include "gtkhex.h"

G_BEGIN_DECLS

#define GHEX_TYPE_CHAR_TABLE (ghex_char_table_get_type ())
G_DECLARE_FINAL_TYPE (GHexCharTable, ghex_char_table, GHEX, CHAR_TABLE, GtkWindow)

GtkWidget * ghex_char_table_new (GtkWindow *parent_win);
void ghex_char_table_set_hex (GHexCharTable *self, HexView *hex);
HexView *ghex_char_table_get_hex (GHexCharTable *self);

G_END_DECLS
