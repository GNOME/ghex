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
   If not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifndef __GHEX_PRINT_H__
#define __GHEX_PRINT_H__

#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-job.h>

#include "hex-document.h"

G_BEGIN_DECLS

typedef struct _GHexPrintJobInfo  GHexPrintJobInfo;

struct _GHexPrintJobInfo {
	GnomePrintJob *master;
	GnomePrintContext *pc;
	GnomePrintConfig *config;

	GnomeFont *d_font, *h_font;
	HexDocument *doc;

	int   pages;
	gint range;
	gint page_first;
	gint page_last;
	gdouble page_width, page_height;
	gdouble margin_top, margin_bottom, margin_left, margin_right;
	gdouble printable_width, printable_height;

	gdouble header_height;
	
	gdouble font_char_width;
	gdouble font_char_height;

	int   bytes_per_row, rows_per_page;
	gdouble pad_size;
	int   offset_chars ; /* How many chars are used in the offset window */
	int   gt;            /* group_type */
	gboolean preview;
};

/* printing */
void ghex_print_job_execute(GHexPrintJobInfo *pji,
			    void (*progress_func)(gint, gint, gpointer),
			    gpointer data);
void ghex_print_update_page_size_and_margins (HexDocument *doc,
					      GHexPrintJobInfo *pji);
GHexPrintJobInfo *ghex_print_job_info_new(HexDocument *doc, guint group_type);
void ghex_print_job_info_destroy(GHexPrintJobInfo *pji);

G_END_DECLS

#endif /* !__GHEX_PRINT_H__ */
