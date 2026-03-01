/* ghex-pane.h
 *
 * Copyright © 2026 Logan Rathbone
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

#include "gtkhex.h"

G_BEGIN_DECLS

#define GHEX_TYPE_PANE ghex_pane_get_type()
G_DECLARE_DERIVABLE_TYPE (GHexPane, ghex_pane, GHEX, PANE, GtkWidget)

struct _GHexPaneClass
{
	GtkWidgetClass parent_class;

	void (*close) (GHexPane *self);
};

/* Method declarations */

GtkWidget *	ghex_pane_new (void);
void ghex_pane_set_hex (GHexPane *self, HexView *hex);
HexView * ghex_pane_get_hex (GHexPane *self);

G_END_DECLS
