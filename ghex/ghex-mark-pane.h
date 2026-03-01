// vim: ts=4 sw=4 breakindent breakindentopt=shift\:4

/* ghex-mark-pane.h
 *
 * Copyright © 2023-2026 Logan Rathbone
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ghex-pane.h"

G_BEGIN_DECLS

#define GHEX_TYPE_MARK_PANE (ghex_mark_pane_get_type ())
G_DECLARE_FINAL_TYPE (GHexMarkPane, ghex_mark_pane, GHEX, MARK_PANE, GHexPane)

/* PUBLIC METHOD DECLARATIONS */

GtkWidget * ghex_mark_pane_new (void);
void ghex_mark_pane_set_hex (GHexMarkPane *self, HexView *hex);
HexView * ghex_mark_pane_get_hex (GHexMarkPane *self);
void ghex_mark_pane_refresh (GHexMarkPane *self);
void ghex_mark_pane_activate_mark_num (GHexMarkPane *self, int mark_num);

G_END_DECLS
