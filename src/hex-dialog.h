/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copright (c) David Hammerton 2003
 * David Hammerton <crazney@crazney.net>
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

#ifndef __HEX_DIALOG_H__
#define __HEX_DIALOG_H__

#include <glib-object.h>

#define HEX_DIALOG_TYPE           (hex_dialog_get_type())
#define HEX_DIALOG(obj)           G_TYPE_CHECK_INSTANCE_CAST (obj, hex_dialog_get_type (), HexDialog)
#define HEX_DIALOG_CLASS(klass)   G_TYPE_CHECK_CLASS_CAST (klass, hex_dialog_get_type (), HexDialogClass)
#define IS_HEX_DIALOG(obj)        G_TYPE_CHECK_INSTANCE_TYPE (obj, hex_dialog_get_type ())


typedef struct _HexDialog       HexDialog;
typedef struct _HexDialogClass  HexDialogClass;

typedef enum
{
    S8 = 0,
    US8,
    S16,
    US16,
    S32,
    US32,
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

struct _HexDialog
{
    GObject gobject;

    GtkWidget *entry[ENTRY_MAX];
    GtkWidget *config_endian;
    GtkWidget *config_hex;
    HexConversionProperties properties;
    HexDialogVal64 val;
};

struct _HexDialogClass
{
    GObjectClass parent_class;
};

GType        hex_dialog_get_type(void);
HexDialog    *hex_dialog_new(void);
GtkWidget    *hex_dialog_getview(HexDialog *);
void         hex_dialog_updateview(HexDialog *dialog, HexDialogVal64 *val);


#endif /* __HEX_DIALOG_H__ */

