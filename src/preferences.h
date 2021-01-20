/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* preferences.h - Declarations pertaining to preferences

   Copyright (C) 2004 Free Software Foundation
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

#ifndef GHEX_PREFERENCES_H
#define GHEX_PREFERENCES_H

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <gtkhex.h>	/* for GROUP_* enums */

G_BEGIN_DECLS

GtkWidget *	create_preferences_dialog(void);
//void		set_current_prefs(PropertyUI *pui);

G_END_DECLS

#endif /* GHEX_PREFERENCES_H */
