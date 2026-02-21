/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copright © David Hammerton 2003
 * David Hammerton <crazney@crazney.net>
 *
 * Copyright © 2004-2020 Various individual contributors, including
 * but not limited to: Jonathon Jongsma, Kalev Lember, who continued to
 * maintain the source code under the licensing terms described
 * herein and below.
 *
 * Copyright © 2021-2024 Logan Rathbone <poprocks@gmail.com>
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

#include "hex-dialog.h"

#include <glib/gi18n.h>

#include <stdio.h>
#include <string.h>

#include <config.h>

struct _HexDialog
{
    GObject gobject;

    GtkWidget *entry[ENTRY_MAX];
    GtkWidget *config_endian;
    GtkWidget *config_hex;
    HexConversionProperties properties;
    HexDialogVal64 val;
};

/* conversion functions */
static char *HexConvert_S8(HexDialogVal64 *val, HexConversionProperties *prop);
static char *HexConvert_US8(HexDialogVal64 *val, HexConversionProperties *prop);
static char *HexConvert_S16(HexDialogVal64 *val, HexConversionProperties *prop);
static char *HexConvert_US16(HexDialogVal64 *val, HexConversionProperties *prop);
static char *HexConvert_S32(HexDialogVal64 *val, HexConversionProperties *prop);
static char *HexConvert_US32(HexDialogVal64 *val, HexConversionProperties *prop);
static char *HexConvert_S64(HexDialogVal64 *val, HexConversionProperties *prop);
static char *HexConvert_US64(HexDialogVal64 *val, HexConversionProperties *prop);
static char *HexConvert_32float(HexDialogVal64 *val, HexConversionProperties *prop);
static char *HexConvert_64float(HexDialogVal64 *val, HexConversionProperties *prop);
static char *HexConvert_hex(HexDialogVal64 *val, HexConversionProperties *prop);
static char *HexConvert_oct(HexDialogVal64 *val, HexConversionProperties *prop);
static char *HexConvert_bin(HexDialogVal64 *val, HexConversionProperties *prop);

static struct {
    char *name;
    char *(*conv_function)(HexDialogVal64 *val, HexConversionProperties *prop);
} HexDialogEntries[ENTRY_MAX] = {
    { N_("Signed 8 bit:"), HexConvert_S8 },
    { N_("Unsigned 8 bit:"), HexConvert_US8 },
    { N_("Signed 16 bit:"), HexConvert_S16 },
    { N_("Unsigned 16 bit:"), HexConvert_US16 },
    { N_("Signed 32 bit:"), HexConvert_S32 },
    { N_("Unsigned 32 bit:"), HexConvert_US32 },
    { N_("Signed 64 bit:"), HexConvert_S64 },
    { N_("Unsigned 64 bit:"), HexConvert_US64 },
    { N_("Float 32 bit:"), HexConvert_32float },
    { N_("Float 64 bit:"), HexConvert_64float },
    { N_("Hexadecimal:"), HexConvert_hex },
    { N_("Octal:"), HexConvert_oct }, 
    { N_("Binary:"), HexConvert_bin }
};

G_DEFINE_TYPE (HexDialog, hex_dialog, G_TYPE_OBJECT)

static void
hex_dialog_init (HexDialog *dialog)
{
    int i;

    for (i = 0; i < ENTRY_MAX; i++)
        dialog->entry[i] = NULL;

    dialog->config_endian = NULL;
    dialog->config_hex = NULL;
    dialog->properties.endian = LITTLE;
    dialog->properties.hexHint = FALSE;
    dialog->properties.streamBitsHint = 8;

    for (i = 0; i < 8; i++)
        dialog->val.v[i] = 0;
}

static void
hex_dialog_class_init (HexDialogClass *klass)
{
}

HexDialog *
hex_dialog_new (void)
{
    HexDialog *dialog;

    dialog = HEX_DIALOG(g_object_new(HEX_TYPE_DIALOG, NULL));
    g_return_val_if_fail (dialog != NULL, NULL);

    return dialog;
}

static void
create_dialog_prop (HexDialogEntryTypes type,
                               HexDialog *dialog,
                               GtkWidget *grid,
                               gint xpos, gint ypos)
{
    GtkWidget *label;
	char *markup;

    label = gtk_label_new (NULL);
	markup = g_strdup_printf ("<small>%s</small>", _(HexDialogEntries[type].name));
	gtk_label_set_markup (GTK_LABEL(label), markup);
	g_free (markup);
	gtk_widget_set_halign (label, GTK_ALIGN_END);
    gtk_grid_attach (GTK_GRID (grid), label,
                     xpos, ypos, 1, 1);

    dialog->entry[type] = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(dialog->entry[type]), FALSE);
    gtk_widget_set_hexpand (dialog->entry[type], TRUE);
    gtk_grid_attach (GTK_GRID (grid), dialog->entry[type],
                     xpos+1, ypos, 1, 1);
}

static void
config_toggled_cb(GtkToggleButton *togglebutton, gpointer user_data)
{
    HexDialog *dialog = HEX_DIALOG(user_data);
    dialog->properties.endian = gtk_check_button_get_active(GTK_CHECK_BUTTON(dialog->config_endian)) ?
                                LITTLE : BIG;
    dialog->properties.hexHint = gtk_check_button_get_active(GTK_CHECK_BUTTON(dialog->config_hex));
    hex_dialog_updateview(dialog, NULL);
}

static void
config_spinchange_cb(GtkSpinButton *spinbutton, gpointer user_data)
{
    HexDialog *dialog = HEX_DIALOG(user_data);
    dialog->properties.streamBitsHint = (guchar)gtk_spin_button_get_value(spinbutton);
    hex_dialog_updateview(dialog, NULL);
}

GtkWidget *hex_dialog_getview(HexDialog *dialog)
{
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *grid;
    GtkWidget *label;
    GtkAdjustment *adjuster;
    GtkWidget *spin;
	char *stream_length_str;

    grid = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID(grid), 12);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_vexpand (GTK_WIDGET (vbox), FALSE);
    gtk_box_append (GTK_BOX (vbox), grid);

    create_dialog_prop (S8, dialog, grid, 0, 0);
    create_dialog_prop (US8, dialog, grid, 0, 1);
    create_dialog_prop (S16, dialog, grid, 0, 2);
    create_dialog_prop (US16, dialog, grid, 0, 3);

    create_dialog_prop (S32, dialog, grid, 2, 0);
    create_dialog_prop (US32, dialog, grid, 2, 1);
    create_dialog_prop (S64, dialog, grid, 2, 2);
    create_dialog_prop (US64, dialog, grid, 2, 3);

    create_dialog_prop (FLOAT32, dialog, grid, 0, 4);
    create_dialog_prop (FLOAT64, dialog, grid, 2, 4);

    create_dialog_prop (HEX, dialog, grid, 4, 0);
    create_dialog_prop (OCT, dialog, grid, 4, 1);
    create_dialog_prop (BIN, dialog, grid, 4, 2);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(vbox), hbox);

    dialog->config_endian = gtk_check_button_new_with_label(_("Show little endian decoding"));
    gtk_check_button_set_active(GTK_CHECK_BUTTON(dialog->config_endian), TRUE);
    g_signal_connect(G_OBJECT(dialog->config_endian), "toggled",
                     G_CALLBACK(config_toggled_cb), dialog);
    gtk_box_append(GTK_BOX(hbox), dialog->config_endian);

    dialog->config_hex = gtk_check_button_new_with_label(_("Show unsigned and float as hexadecimal"));
    g_signal_connect(G_OBJECT(dialog->config_hex), "toggled",
                     G_CALLBACK(config_toggled_cb), dialog);
    gtk_box_append(GTK_BOX(hbox), dialog->config_hex);

    label = gtk_label_new (NULL);
	stream_length_str = g_strdup_printf ("<small>%s</small>", _("Stream Length:"));
	gtk_label_set_markup (GTK_LABEL(label), stream_length_str);
	g_free (stream_length_str);
	gtk_widget_set_halign (label, GTK_ALIGN_END);
    gtk_grid_attach (GTK_GRID (grid), label, 4, 3, 1, 1);

    adjuster = (GtkAdjustment *)gtk_adjustment_new(8.0, 1.0, 32.0, 1.0, 8.0, 0);
    spin = gtk_spin_button_new(adjuster, 1.0, 0);
    g_signal_connect(G_OBJECT(spin), "value-changed",
                     G_CALLBACK(config_spinchange_cb), dialog);
    gtk_grid_attach (GTK_GRID (grid), spin, 5, 3, 1, 1);

    return vbox;
}

static char *
dialog_prop_get_text(HexDialogEntryTypes type,
                                  HexDialog *dialog,
                                  HexDialogVal64 *val)
{
    char *buf = "";

    if (HexDialogEntries[type].conv_function)
        buf = HexDialogEntries[type].conv_function(val, &dialog->properties);
    else
		g_warning ("No conversion function found");

    return buf;
}

static void update_dialog_prop(HexDialogEntryTypes type,
                               HexDialog *dialog,
                               HexDialogVal64 *val)
{
    char *buf = dialog_prop_get_text (type, dialog, val);

	gtk_editable_set_text (GTK_EDITABLE(dialog->entry[type]), buf);
}

/* if val is NULL, uses the previous used val */
void hex_dialog_updateview(HexDialog *dialog, HexDialogVal64 *val)
{
    int i;
    if (val)
    {
        for (i = 0; i < 8; i++)
        {
            dialog->val.v[i] = val->v[i];
        }
    }
    for (i = 0; i < ENTRY_MAX; i++)
    {
        update_dialog_prop(i, dialog, &dialog->val);
    }

}

/* used for conversions, this can be global, since it need not be
 * reentrant */
#define CONV_BUFSIZE 64
static char convbuffer[CONV_BUFSIZE];

/* conversion functions */

/* the conversion functions are slow on purpose, we interpret
 * all bits manually in order to allow for the future possibility
 * of different byte sizes.
 * also makes it endian safe (i think)
 */

inline static unsigned int pow2(int p)
{
    unsigned int i = 0, r = 1;
    for (i = 0; i < p; i++)
        r*=2;
    return r;
}

inline static unsigned long long int llpow2(int p)
{
    unsigned int i = 0;
    unsigned long long int r = 1;
    for (i = 0; i < p; i++)
        r*=2;
    return r;
}

static char *HexConvert_S8(HexDialogVal64 *val, HexConversionProperties *prop)
{
    int i, local = 0;
    for (i = 0; i < 7; i++)
        local += ((val->v[0] >> i) & 0x1) * pow2(i);
    if ((val->v[0] >> 7) & 0x1)
        local  = -(pow2(7) - local);
    snprintf(convbuffer, sizeof(convbuffer), "%d", local);
    return convbuffer;
}

static char *HexConvert_US8(HexDialogVal64 *val, HexConversionProperties *prop)
{
    int i, local = 0;
    for (i = 0; i < 8; i++)
        local += ((val->v[0] >> i) & 0x1) * pow2(i);

    if (!prop->hexHint)
        snprintf(convbuffer, sizeof(convbuffer), "%u", local);
    else
        snprintf(convbuffer, sizeof(convbuffer), "0x%02X", local);
    return convbuffer;
}

static char *HexConvert_S16(HexDialogVal64 *val, HexConversionProperties *prop)
{
    guchar in[2];
    int i, local = 0;
    if (prop->endian == LITTLE)
    {
        in[0] = val->v[0];
        in[1] = val->v[1];
    }
    else
    {
        in[0] = val->v[1];
        in[1] = val->v[0];
    }
    for (i = 0; i < 8; i++)
        local += ((in[0] >> i) & 0x1) * pow2(i);
    for (i = 0; i < 7; i++)
        local += ((in[1] >> i) & 0x1) * pow2(i + 8);
    if ((in[1] >> 7) & 0x1)
        local  = -(pow2(15) - local);
    snprintf(convbuffer, sizeof(convbuffer), "%d", local);
    return convbuffer;
}

static char *HexConvert_US16(HexDialogVal64 *val, HexConversionProperties *prop)
{
    guchar in[2];
    int i, local = 0;
    if (prop->endian == LITTLE)
    {
        in[0] = val->v[0];
        in[1] = val->v[1];
    }
    else
    {
        in[0] = val->v[1];
        in[1] = val->v[0];
    }
    for (i = 0; i < 8; i++)
        local += ((in[0] >> i) & 0x1) * pow2(i);
    for (i = 0; i < 8; i++)
        local += ((in[1] >> i) & 0x1) * pow2(i + 8);

    if (!prop->hexHint)
        snprintf(convbuffer, sizeof(convbuffer), "%u", local);
    else
        snprintf(convbuffer, sizeof(convbuffer), "0x%04X", local);

    return convbuffer;
}

static char *HexConvert_S32(HexDialogVal64 *val, HexConversionProperties *prop)
{
    guchar in[4];
    int i, local = 0;
    if (prop->endian == LITTLE)
    {
        in[0] = val->v[0];
        in[1] = val->v[1];
        in[2] = val->v[2];
        in[3] = val->v[3];
    }
    else
    {
        in[0] = val->v[3];
        in[1] = val->v[2];
        in[2] = val->v[1];
        in[3] = val->v[0];
    }
    for (i = 0; i < 8; i++)
        local += ((in[0] >> i) & 0x1) * pow2(i);
    for (i = 0; i < 8; i++)
        local += ((in[1] >> i) & 0x1) * pow2(i + 8);
    for (i = 0; i < 8; i++)
        local += ((in[2] >> i) & 0x1) * pow2(i + 16);
    for (i = 0; i < 7; i++)
        local += ((in[3] >> i) & 0x1) * pow2(i + 24);
    if ((in[3] >> 7) & 0x1)
        local  = -(pow2(31) - local);
    snprintf(convbuffer, sizeof(convbuffer), "%d", local);
    return convbuffer;
}

static char *HexConvert_US32(HexDialogVal64 *val, HexConversionProperties *prop)
{
    guchar in[4];
    unsigned int i, local = 0;
    if (prop->endian == LITTLE)
    {
        in[0] = val->v[0];
        in[1] = val->v[1];
        in[2] = val->v[2];
        in[3] = val->v[3];
    }
    else
    {
        in[0] = val->v[3];
        in[1] = val->v[2];
        in[2] = val->v[1];
        in[3] = val->v[0];
    }
    for (i = 0; i < 8; i++)
        local += ((in[0] >> i) & 0x1) * pow2(i);
    for (i = 0; i < 8; i++)
        local += ((in[1] >> i) & 0x1) * pow2(i + 8);
    for (i = 0; i < 8; i++)
        local += ((in[2] >> i) & 0x1) * pow2(i + 16);
    for (i = 0; i < 8; i++)
        local += ((in[3] >> i) & 0x1) * pow2(i + 24);

    if (!prop->hexHint)
        snprintf(convbuffer, sizeof(convbuffer), "%u", local);
    else
        snprintf(convbuffer, sizeof(convbuffer), "0x%08X", local);
    return convbuffer;
}

static char *HexConvert_S64(HexDialogVal64 *val, HexConversionProperties *prop)
{
    guchar in[8];
    long long i, local = 0;
    if (prop->endian == LITTLE)
    {
        in[0] = val->v[0];
        in[1] = val->v[1];
        in[2] = val->v[2];
        in[3] = val->v[3];
        in[4] = val->v[4];
        in[5] = val->v[5];
        in[6] = val->v[6];
        in[7] = val->v[7];
    }
    else
    {
        in[0] = val->v[7];
        in[1] = val->v[6];
        in[2] = val->v[5];
        in[3] = val->v[4];
        in[4] = val->v[3];
        in[5] = val->v[2];
        in[6] = val->v[1];
        in[7] = val->v[0];
    }
    for (i = 0; i < 8; i++)
        local += ((in[0] >> i) & 0x1) * llpow2(i);
    for (i = 0; i < 8; i++)
        local += ((in[1] >> i) & 0x1) * llpow2(i + 8);
    for (i = 0; i < 8; i++)
        local += ((in[2] >> i) & 0x1) * llpow2(i + 16);
    for (i = 0; i < 8; i++)
        local += ((in[3] >> i) & 0x1) * llpow2(i + 24);
    for (i = 0; i < 8; i++)
        local += ((in[4] >> i) & 0x1) * llpow2(i + 32);
    for (i = 0; i < 8; i++)
        local += ((in[5] >> i) & 0x1) * llpow2(i + 40);
    for (i = 0; i < 8; i++)
        local += ((in[6] >> i) & 0x1) * llpow2(i + 48);
    for (i = 0; i < 7; i++)
        local += ((in[7] >> i) & 0x1) * llpow2(i + 56);
    if ((in[7] >> 7) & 0x1)
        local  = -(llpow2(63) - local);
    snprintf(convbuffer, sizeof(convbuffer), "%lld", local);
    return convbuffer;
}

static char *HexConvert_US64(HexDialogVal64 *val, HexConversionProperties *prop)
{
    guchar in[8];
    long long unsigned i, local = 0;
    if (prop->endian == LITTLE)
    {
        in[0] = val->v[0];
        in[1] = val->v[1];
        in[2] = val->v[2];
        in[3] = val->v[3];
        in[4] = val->v[4];
        in[5] = val->v[5];
        in[6] = val->v[6];
        in[7] = val->v[7];
    }
    else
    {
        in[0] = val->v[7];
        in[1] = val->v[6];
        in[2] = val->v[5];
        in[3] = val->v[4];
        in[4] = val->v[3];
        in[5] = val->v[2];
        in[6] = val->v[1];
        in[7] = val->v[0];
    }
    for (i = 0; i < 8; i++)
        local += ((in[0] >> i) & 0x1) * llpow2(i);
    for (i = 0; i < 8; i++)
        local += ((in[1] >> i) & 0x1) * llpow2(i + 8);
    for (i = 0; i < 8; i++)
        local += ((in[2] >> i) & 0x1) * llpow2(i + 16);
    for (i = 0; i < 8; i++)
        local += ((in[3] >> i) & 0x1) * llpow2(i + 24);
    for (i = 0; i < 8; i++)
        local += ((in[4] >> i) & 0x1) * llpow2(i + 32);
    for (i = 0; i < 8; i++)
        local += ((in[5] >> i) & 0x1) * llpow2(i + 40);
    for (i = 0; i < 8; i++)
        local += ((in[6] >> i) & 0x1) * llpow2(i + 48);
    for (i = 0; i < 8; i++)
        local += ((in[7] >> i) & 0x1) * llpow2(i + 56);

    if (!prop->hexHint)
        snprintf(convbuffer, sizeof(convbuffer), "%llu", local);
    else
        snprintf(convbuffer, sizeof(convbuffer), "0x%016llX", local);
    return convbuffer;
}

/* for floats we just cast them, can't be bothered
 * interpretting them properly
 */
static char *HexConvert_32float(HexDialogVal64 *val, HexConversionProperties *prop)
{
    union
    {
        guchar c[4];
        float f;
    } in;

    float local = 0.0;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    if (prop->endian == LITTLE)
#else
    if (prop->endian == BIG)
#endif
    {
        in.c[0] = val->v[0];
        in.c[1] = val->v[1];
        in.c[2] = val->v[2];
        in.c[3] = val->v[3];
    }
    else
    {
        in.c[0] = val->v[3];
        in.c[1] = val->v[2];
        in.c[2] = val->v[1];
        in.c[3] = val->v[0];
    }

    local = in.f;

    if (!prop->hexHint)
        snprintf(convbuffer, sizeof(convbuffer), "%e", local);
    else
        snprintf(convbuffer, sizeof(convbuffer), "%A", local);
    return convbuffer;
}

static char *HexConvert_64float(HexDialogVal64 *val, HexConversionProperties *prop)
{
    union
    {
        guchar c[8];
        double f;
    } in;

    double local = 0.0;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    if (prop->endian == LITTLE)
#else
    if (prop->endian == BIG)
#endif
    {
        in.c[0] = val->v[0];
        in.c[1] = val->v[1];
        in.c[2] = val->v[2];
        in.c[3] = val->v[3];
        in.c[4] = val->v[4];
        in.c[5] = val->v[5];
        in.c[6] = val->v[6];
        in.c[7] = val->v[7];
    }
    else
    {
        in.c[0] = val->v[7];
        in.c[1] = val->v[6];
        in.c[2] = val->v[5];
        in.c[3] = val->v[4];
        in.c[4] = val->v[3];
        in.c[5] = val->v[2];
        in.c[6] = val->v[1];
        in.c[7] = val->v[0];
    }

    local = in.f;

    if (!prop->hexHint)
        snprintf(convbuffer, sizeof(convbuffer), "%e", local);
    else
        snprintf(convbuffer, sizeof(convbuffer), "%A", local);
    return convbuffer;
}

/* these three care not for endianness, they take the input as a stream */
static char *HexConvert_hex(HexDialogVal64 *val, HexConversionProperties *prop)
{
    int i;
    int local = 0;

    int v[4];

    v[0] = (prop->streamBitsHint >= 8) ? 8 : prop->streamBitsHint;
    v[1] = (prop->streamBitsHint >= 16) ? 8 : prop->streamBitsHint - 8;
    if (v[1] < 0) v[1] = 0;
    v[2] = (prop->streamBitsHint >= 24) ? 8 : prop->streamBitsHint - 16;
    if (v[2] < 0) v[2] = 0;
    v[3] = prop->streamBitsHint - 24;
    if (v[3] < 0) v[3] = 0;

    for (i = 0; i < v[0]; i++)
        local += ((val->v[0] >> i) & 0x1) * pow2(i);
    for (i = 0; i < v[1]; i++)
        local += ((val->v[1] >> i) & 0x1) * pow2(i + 8);
    for (i = 0; i < v[2]; i++)
        local += ((val->v[2] >> i) & 0x1) * pow2(i + 16);
    for (i = 0; i < v[3]; i++)
        local += ((val->v[3] >> i) & 0x1) * pow2(i + 24);

    if (v[3])
        snprintf(convbuffer, sizeof(convbuffer), "%02X %02X %02X %02X",
                 (local & 0x000000ff),  (local & 0x0000ff00) >> 8,
                 (local & 0x00ff0000) >> 16, (local & 0xff000000) >> 24);
    else if (v[2])
        snprintf(convbuffer, sizeof(convbuffer), "%02X %02X %02X",
                 (local & 0x000000ff),  (local & 0x0000ff00) >> 8,
                 (local & 0x00ff0000) >> 16);
    else if (v[1])
        snprintf(convbuffer, sizeof(convbuffer), "%02X %02X",
                 (local & 0x000000ff),  (local & 0x0000ff00) >> 8);
    else
        snprintf(convbuffer, sizeof(convbuffer), "%02X",
                 (local & 0x000000ff));

    return convbuffer;
}

static char *HexConvert_oct(HexDialogVal64 *val, HexConversionProperties *prop)
{
    int i;
    int local = 0;

    int v[4];

    v[0] = (prop->streamBitsHint >= 8) ? 8 : prop->streamBitsHint;
    v[1] = (prop->streamBitsHint >= 16) ? 8 : prop->streamBitsHint - 8;
    if (v[1] < 0) v[1] = 0;
    v[2] = (prop->streamBitsHint >= 24) ? 8 : prop->streamBitsHint - 16;
    if (v[2] < 0) v[2] = 0;
    v[3] = prop->streamBitsHint - 24;
    if (v[3] < 0) v[3] = 0;

    for (i = 0; i < v[0]; i++)
        local += ((val->v[0] >> i) & 0x1) * pow2(i);
    for (i = 0; i < v[1]; i++)
        local += ((val->v[1] >> i) & 0x1) * pow2(i + 8);
    for (i = 0; i < v[2]; i++)
        local += ((val->v[2] >> i) & 0x1) * pow2(i + 16);
    for (i = 0; i < v[3]; i++)
        local += ((val->v[3] >> i) & 0x1) * pow2(i + 24);

    if (v[3])
        snprintf(convbuffer, sizeof(convbuffer), "%03o %03o %03o %03o",
                 (local & 0x000000ff),  (local & 0x0000ff00) >> 8,
                 (local & 0x00ff0000) >> 16, (local & 0xff000000) >> 24);
    else if (v[2])
        snprintf(convbuffer, sizeof(convbuffer), "%03o %03o %03o",
                 (local & 0x000000ff),  (local & 0x0000ff00) >> 8,
                 (local & 0x00ff0000) >> 16);
    else if (v[1])
        snprintf(convbuffer, sizeof(convbuffer), "%03o %03o",
                 (local & 0x000000ff),  (local & 0x0000ff00) >> 8);
    else
        snprintf(convbuffer, sizeof(convbuffer), "%03o",
                 (local & 0x000000ff));

    return convbuffer;
}

static char *HexConvert_bin(HexDialogVal64 *val, HexConversionProperties *prop)
{
    int i;

    convbuffer[0] = '\0';

    g_return_val_if_fail(prop->streamBitsHint <= 32, convbuffer);

    for (i = 0; i < prop->streamBitsHint; i++)
    {
        int v = i < 8 ? 0 :
                i < 16 ? 1 :
                i < 24 ? 2 :
                3;
        int shift = 7 - ((v == 0) ? i : (i % (v * 8)));
        convbuffer[i] = (val->v[v] >> shift & 0x1) ? '1' : '0';
    }
    convbuffer[i] = '\0';
    return convbuffer;
}
