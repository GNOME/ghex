/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex.h - definition of a HexWidget widget, modified for use with GnomeMDI

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

#define GHEX_TYPE_CONVERTER (ghex_converter_get_type ())
G_DECLARE_FINAL_TYPE (GHexConverter, ghex_converter, GHEX, CONVERTER, GtkWindow)

GtkWidget * ghex_converter_new (GtkWindow *parent_win);
void ghex_converter_set_hex (GHexConverter *self, HexView *hex);
HexView * ghex_converter_get_hex (GHexConverter *self);

G_END_DECLS
