/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * hex-statusbar.h: declaration of HexStatusbar widget
 *
 * Copyright © 2022 Logan Rathbone <poprocks@gmail.com>
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

#ifndef HEX_STATUSBAR_H
#define HEX_STATUSBAR_H

#include <gtk/gtk.h>

#define HEX_TYPE_STATUSBAR (hex_statusbar_get_type ())
G_DECLARE_FINAL_TYPE (HexStatusbar, hex_statusbar, HEX, STATUSBAR, GtkWidget)

/* PUBLIC METHOD DECLARATIONS */

GtkWidget	*hex_statusbar_new (void);
void		hex_statusbar_set_status (HexStatusbar *self, const char *msg);
void		hex_statusbar_clear (HexStatusbar *self);

#endif /* HEX_STATUSBAR_H */
