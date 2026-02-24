/* ghex-application-window.h
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <adwaita.h>

#include "gtkhex.h"
#include "ghex-view-container.h"

G_BEGIN_DECLS

#define GHEX_TYPE_APPLICATION_WINDOW (ghex_application_window_get_type())

G_DECLARE_FINAL_TYPE (GHexApplicationWindow, ghex_application_window, GHEX, APPLICATION_WINDOW, AdwApplicationWindow)

GtkWidget *	ghex_application_window_new (AdwApplication *app);
void ghex_application_window_new_file (GHexApplicationWindow *self);
void ghex_application_window_open_file (GHexApplicationWindow *self, GFile *file);
GHexViewContainer * ghex_application_window_get_active_view (GHexApplicationWindow *self);
void ghex_application_window_set_active_view (GHexApplicationWindow *self, GHexViewContainer *container);

G_END_DECLS
