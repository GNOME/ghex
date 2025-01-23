/* vim: colorcolumn=80 ts=4 sw=4
 */
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

#ifndef GHEX_PRINT_H
#define GHEX_PRINT_H

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gtkhex.h"
#include "hex-document.h"
#include "configuration.h"

G_BEGIN_DECLS

typedef struct _GHexPrintJobInfo  GHexPrintJobInfo;

struct _GHexPrintJobInfo {
	GtkPrintOperation *master;
	GtkPrintContext *pc;
	GtkPrintSettings *config;

	PangoFontDescription *d_font, *h_font;
	HexDocument *doc;

	int pages;
	int range;
	int page_first;
	int page_last;

	double header_height;
	
	int font_char_width;
	int font_char_height;

	int  bytes_per_row, rows_per_page;
	double pad_size;
	int offset_chars ; /* How many chars are used in the offset window */
	int gt;            /* group_type */
	gboolean preview;
};

/* FUNCTION DECLARATIONS */

void begin_print (GtkPrintOperation *operation,
                  GtkPrintContext   *context,
                  gpointer           data);
void print_page (GtkPrintOperation *operation,
                 GtkPrintContext   *context,
                 int               page_nr,
                 gpointer           data);
GHexPrintJobInfo *ghex_print_job_info_new (HexDocument *doc,
		HexWidgetGroupType group_type);
void ghex_print_job_info_destroy(GHexPrintJobInfo *pji);

G_END_DECLS

#endif /* GHEX_PRINT_H */
