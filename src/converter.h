/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex.h - definition of a GtkHex widget, modified for use with GnomeMDI

   Copyright (C) 2004 Free Software Foundation

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

#ifndef __GHEX_CONVERTER_H__
#define __GHEX_CONVERTER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _Converter {
	GtkWidget *window;
	GtkWidget *entry[5];
	GtkWidget *close;
	GtkWidget *get;

	gulong value;
} Converter;

/* Defined in converter.c: used by close_cb and converter_cb */
extern GtkWidget *converter_get;
extern Converter *converter;

Converter *create_converter(void);

G_END_DECLS

#endif /* !__GHEX_CONVERTER_H__ */
