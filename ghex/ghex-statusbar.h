/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * ghex-statusbar.h: declaration of GHexStatusbar widget
 *
 * Copyright © 2022-2026 Logan Rathbone <poprocks@gmail.com>
 *
 *  GHex is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  GHex is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with GHex; see the file COPYING.
 *  If not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <gtk/gtk.h>
#include "hex-selection.h"

/**
 * GHexStatusbarOffsetFormat:
 * @GHEX_STATUSBAR_OFFSET_HEX: hexadecimal
 * @GHEX_STATUSBAR_OFFSET_DEC: decimal
 * @GHEX_STATUSBAR_OFFSET_BOTH: both (equivalent to
 *   `HEX_WIDGET_STATUS_BAR_OFFSET_HEX|HEX_WIDGET_STATUS_BAR_OFFSET_DEC`) 
 *
 * Specifies the format of the offset shown on the status bar.
 */
typedef enum
{
   GHEX_STATUSBAR_OFFSET_HEX  =    1,
   GHEX_STATUSBAR_OFFSET_DEC  =    2,
   GHEX_STATUSBAR_OFFSET_BOTH = (GHEX_STATUSBAR_OFFSET_HEX | GHEX_STATUSBAR_OFFSET_DEC),
} GHexStatusbarOffsetFormat;

#define GHEX_TYPE_STATUSBAR (ghex_statusbar_get_type ())
G_DECLARE_FINAL_TYPE (GHexStatusbar, ghex_statusbar, GHEX, STATUSBAR, GtkWidget)

GtkWidget * ghex_statusbar_new (void);
void ghex_statusbar_set_selection (GHexStatusbar *self, HexSelection *selection);
HexSelection * ghex_statusbar_get_selection (GHexStatusbar *self);
void ghex_statusbar_set_offset_format (GHexStatusbar *self, GHexStatusbarOffsetFormat offset_format);
GHexStatusbarOffsetFormat ghex_statusbar_get_offset_format (GHexStatusbar *self);
