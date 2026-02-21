/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copright (c) David Hammerton 2003
 * David Hammerton <crazney@crazney.net>
 *
 * Copyright © 2004-2020 Various individual contributors, including
 * but not limited to: Jonathon Jongsma, Kalev Lember, who continued to
 * maintain the source code under the licensing terms described
 * herein and below.
 *
 * Copyright © 2021 Logan Rathbone <poprocks@gmail.com>
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

#ifndef HEX_DIALOG_H
#define HEX_DIALOG_H

#include <gtk/gtk.h>

typedef enum
{
    S8 = 0,
    US8,
    S16,
    US16,
    S32,
    US32,
    S64,
    US64,
    FLOAT32,
    FLOAT64,
    HEX,
    OCT,
    BIN,
    ENTRY_MAX
} HexDialogEntryTypes;

typedef struct
{
    guchar v[8];
} HexDialogVal64;

typedef enum
{
    LITTLE,
    BIG
} HexEndian;

typedef struct
{
    HexEndian endian;
    gboolean hexHint;        /* only some functions use the Hint parameter */
    guchar streamBitsHint;
} HexConversionProperties;

#define HEX_TYPE_DIALOG (hex_dialog_get_type ())
G_DECLARE_FINAL_TYPE (HexDialog, hex_dialog, HEX, DIALOG, GObject)


/* PUBLIC METHOD DECLARATIONS */

HexDialog    *hex_dialog_new (void);
GtkWidget    *hex_dialog_getview (HexDialog *);
void         hex_dialog_updateview (HexDialog *dialog, HexDialogVal64 *val);


#endif /* HEX_DIALOG_H */
