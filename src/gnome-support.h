/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gnome-support.h - forward decls of GNOMEificating functions

   Copyright (C) 1997, 1998 Free Software Foundation

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

   Author: Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
*/

#ifndef GNOME_SUPPORT_H
#define GNOME_SUPPORT_H

#include <gnome.h>
#include <getopt.h>

extern int restarted;
extern const struct poptOption options[];

int save_state      (GnomeClient        *client,
                     gint                phase,
                     GnomeRestartStyle   save_style,
                     gint                shutdown,
                     GnomeInteractStyle  interact_style,
                     gint                fast,
                     gpointer            client_data);

gint client_die     (GnomeClient *client, gpointer client_data);

#endif
