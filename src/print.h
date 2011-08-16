/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* print.h - printing related stuff for ghex

   Copyright (C) 1998 - 2004 Free Software Foundation

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

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifndef __GHEX_PRINT_H__
#define __GHEX_PRINT_H__

#include <gtk/gtk.h>

#include "hex-document.h"

G_BEGIN_DECLS

typedef struct _GHexPrintJobInfo  GHexPrintJobInfo;

struct _GHexPrintJobInfo {
	GtkPrintOperation *master;
	GtkPrintContext *pc;
	GtkPrintSettings *config;

	PangoFontDescription *d_font, *h_font;
	HexDocument *doc;

	int   pages;
	gint range;
	gint page_first;
	gint page_last;

	gdouble header_height;
	
	gint font_char_width;
	gint font_char_height;

	int   bytes_per_row, rows_per_page;
	gdouble pad_size;
	int   offset_chars ; /* How many chars are used in the offset window */
	int   gt;            /* group_type */
	gboolean preview;
};

/* printing */
void begin_print (GtkPrintOperation *operation,
                  GtkPrintContext   *context,
                  gpointer           data);
void print_page (GtkPrintOperation *operation,
                 GtkPrintContext   *context,
                 gint               page_nr,
                 gpointer           data);
GHexPrintJobInfo *ghex_print_job_info_new(HexDocument *doc, guint group_type);
void ghex_print_job_info_destroy(GHexPrintJobInfo *pji);

G_END_DECLS

#endif /* !__GHEX_PRINT_H__ */
