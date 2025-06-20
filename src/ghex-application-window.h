/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ghex-application-window.h - GHex main application window declarations

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

   Original GHex Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifndef GHEX_APPLICATION_WINDOW_H
#define GHEX_APPLICATION_WINDOW_H

#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gi18n.h>

#include "gtkhex.h"
#include "configuration.h"
#include "hex-info-bar.h"
#include "hex-statusbar.h"
#include "hex-dialog.h"
#include "findreplace.h"
#include "mark-dialog.h"
#include "chartable.h"
#include "converter.h"
#include "preferences.h"
#include "common-ui.h"
#include "hex-buffer-malloc.h"

G_BEGIN_DECLS

#define GHEX_TYPE_APPLICATION_WINDOW (ghex_application_window_get_type ())
G_DECLARE_FINAL_TYPE (GHexApplicationWindow, ghex_application_window,
				GHEX, APPLICATION_WINDOW,
				AdwApplicationWindow)

GtkWidget *	ghex_application_window_new (AdwApplication *app);
void		ghex_application_window_add_hex (GHexApplicationWindow *self,
				HexWidget *gh);
void		ghex_application_window_set_hex (GHexApplicationWindow *self,
				HexWidget *gh);
void		ghex_application_window_activate_tab (GHexApplicationWindow *self,
				HexWidget *gh);
void		ghex_application_window_open_file (GHexApplicationWindow *self,
				GFile *file);
HexWidget *	ghex_application_window_get_hex (GHexApplicationWindow *self);

G_END_DECLS

#endif
