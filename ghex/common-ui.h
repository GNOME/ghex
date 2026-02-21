/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* common-ui.h - Common UI utility functions

   Copyright © 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021 Logan Rathbone

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

#ifndef COMMON_UI_H
#define COMMON_UI_H

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gtkhex.h"
#include "configuration.h"
#include "print.h"

G_BEGIN_DECLS

/* various ui convenience functions */

void common_help_cb (GtkWindow *parent);
void common_about_cb (GtkWindow *parent);
void common_print (GtkWindow *parent, HexWidget *gh, gboolean preview);
void common_set_gtkhex_font_from_settings (HexWidget *gh);
void display_dialog (GtkWindow *parent, const char *msg);
char *common_get_ui_basename (HexDocument *doc);

G_END_DECLS

#endif /* COMMON_UI_H */
