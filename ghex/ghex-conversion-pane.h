// vim: linebreak breakindent breakindentopt=shift\:4
/*
 * Copright (c) David Hammerton 2003 <crazney@crazney.net>
 *
 * Copyright © 2004-2020 Various individual contributors, including
 * but not limited to: Jonathon Jongsma, Kalev Lember, who continued to
 * maintain the source code under the licensing terms described
 * herein and below.
 *
 * Copyright © 2021-2026 Logan Rathbone <poprocks@gmail.com>
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

typedef struct
{
    guchar v[8];
} GHexConversionVal64;

#define GHEX_TYPE_CONVERSION_PANE (ghex_conversion_pane_get_type ())
G_DECLARE_FINAL_TYPE (GHexConversionPane, ghex_conversion_pane, GHEX, CONVERSION_PANE, GtkWidget)

/* PUBLIC METHOD DECLARATIONS */

GHexConversionPane    *ghex_conversion_pane_new (void);
void         ghex_conversion_pane_update (GHexConversionPane *dialog, GHexConversionVal64 *val);
