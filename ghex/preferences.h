/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* preferences.h - Declarations pertaining to preferences

   Copyright (C) 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021 Logan Rathbone <poprocks@gmail.com>

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

#ifndef GHEX_PREFERENCES_H
#define GHEX_PREFERENCES_H

#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gi18n.h>
#include <string.h>

#include <gtkhex.h>	/* for HexWidgetGroupType enum */
#include <hex-statusbar.h>	/* for HexWidgetStatusBarOffsetFormat enum */

#include "configuration.h"
#include "common-ui.h"
#include "common-macros.h"

G_BEGIN_DECLS

GtkWidget *	create_preferences_dialog (GtkWindow *parent);

G_END_DECLS

#endif /* GHEX_PREFERENCES_H */
