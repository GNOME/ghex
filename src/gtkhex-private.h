/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex-private.h - private GtkHex API; used by accessibility code

   Copyright (C) 1997 - 2004 Free Software Foundation

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
   If not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifndef __GTKHEX_PRIVATE_H__
#define __GTKHEX_PRIVATE_H__

#include "gtkhex.h"

gint format_ablock(GtkHex *gh, gchar *out, guint start, guint end);
gint format_xblock(GtkHex *gh, gchar *out, guint start, guint end);
void format_xbyte(GtkHex *gh, gint pos, gchar buf[2]);

#endif /* __GTKHEX_PRIVATE_H__ */
