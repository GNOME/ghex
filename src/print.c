/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* print.c - print a HexDocument

   Copyright (C) 1998, 1999, 2000 Free Software Foundation

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
   Printing module by: Chema Celorio <chema@celorio.com>
*/

#include "ghex.h"
#include "gtkhex.h"

#include <libgnomeprint/gnome-print.h>

#define is_printable(c) (((((guchar)c)>=0x20) && (((guchar)c)<=0x7F))?1:0)

typedef struct {
	GnomePrintContext *pc;
	HexDocument *doc;

	int   pages;
	float page_width, page_height;
	float margin_top, margin_bottom, margin_left, margin_right;
	float printable_width, printable_height;

	float header_height;
	
	float font_char_width;
	float font_char_height;

	int   bytes_per_row, rows_per_page;
	float pad_size;
	int   offset_chars ; /* How many chars are used in the offset window */
	int   gt;            /* group_type */
} GhexPrintJobInfo;

static void start_job(GnomePrintContext *pc);
static void print_header(GhexPrintJobInfo *pji, unsigned int page);
static void end_page(GnomePrintContext *pc);
static void print_row(GhexPrintJobInfo *pji, unsigned int offset, unsigned int bytes, int row);
static void end_job(GnomePrintContext *pc);

static void format_hex(HexDocument *doc, guint group_type, gchar *out, guint start, guint end);
static void format_ascii(HexDocument *doc, gchar *out, guint start, guint end);

void print_document(HexDocument *doc, guint group_type, GnomePrinter *printer)
{
	int i, j;
	float width;
	GhexPrintJobInfo *pji;

	pji = g_new0(GhexPrintJobInfo, 1);
	
	pji->pc = gnome_print_context_new_with_paper_size (printer, "US-Letter");
	pji->doc = doc;

	g_return_if_fail(pji->pc != NULL);

    /* For now we will print in US-Letter 8.5x11 */
    /* Is important to define this as variables so that when
    we implement diferent page sizes the rest of the code will work */
    pji->page_width  = 8.5;
    pji->page_height = 11;
    pji->margin_top = .75; /* Printer margins, not page margins */
    pji->margin_bottom = .75; 
    pji->margin_left = .75;
    pji->margin_right = .75;
    pji->header_height = 1;
    pji->font_char_width = .0808;
    pji->font_char_height = .14;
    pji->pad_size = .5;   
    pji->offset_chars = 8;

	pji->printable_width  = pji->page_width - pji->margin_left - pji->margin_right;
	pji->printable_height = pji->page_height - pji->margin_top - pji->margin_bottom;
	width = pji->printable_width - pji->pad_size*2 - pji->offset_chars*pji->font_char_width;
	pji->bytes_per_row = 0;
	while(TRUE) {
		width -= 3*pji->font_char_width;
		if(width >= 0)
			pji->bytes_per_row++;
		else
			break;
		if(pji->bytes_per_row % group_type == 0)
			width -= pji->font_char_width;
	}
	pji->rows_per_page = (pji->printable_height - pji->header_height)/pji->font_char_height - 1 ;
	pji->pages = (((doc->file_size/pji->bytes_per_row) + 1)/pji->rows_per_page) + 1;
	pji->gt = group_type;

	start_job(pji->pc);
	for(i = 1; i <= pji->pages; i++)
	{
		print_header(pji, i);
		for(j=1; j <= pji->rows_per_page; j++)
		{
			int file_offset = pji->bytes_per_row*(j - 1) + pji->bytes_per_row*pji->rows_per_page*(i - 1);
			int length;
			if(file_offset > doc->file_size)
			    break;
			length = (file_offset + pji->bytes_per_row > doc->file_size ? doc->file_size - file_offset : pji->bytes_per_row),
			print_row(pji, file_offset, length, j);
		}
		end_page(pji->pc);
	}
    end_job(pji->pc);

	gnome_print_context_close (pji->pc);
}

static void start_job(GnomePrintContext *pc)
{
}

static void print_header(GhexPrintJobInfo *pji, unsigned int page)
{
    guchar* text1 = g_strdup(pji->doc->file_name);
    guchar* text2 = g_strdup_printf(_("Page: %i/%i"),page,pji->pages);
	GnomeFont *font;
	float x,y,len;
	
	font = gnome_font_new("Helvetica", 12);
	gnome_print_setfont(pji->pc, font);

	/* Print the file name */
	y = pji->page_height - pji->margin_top - pji->header_height/2;
    len = gnome_font_get_width_string (font, text1);
	x = pji->page_width/2 - (len/72)/2;
	gnome_print_moveto(pji->pc, x*72, y*72);
	gnome_print_show(pji->pc, text1);
	gnome_print_stroke(pji->pc);

	/* Print the page/pages  */
	y = pji->page_height - pji->margin_top - pji->header_height/4;
    len = gnome_font_get_width_string (font, text2);
	x = pji->page_width - (len/72) - .5;
	gnome_print_moveto(pji->pc, x*72 , y*72);
	gnome_print_show(pji->pc, text2);
	gnome_print_stroke(pji->pc); 

	/* Set the font for the rest of the page */
	font = gnome_font_new ("Courier", 10);
	gnome_print_setfont (pji->pc, font);
}

static void print_row(GhexPrintJobInfo *pji, unsigned int offset, unsigned int bytes, int row)
{
	float x, y;
	gchar *temp = malloc(265);

	y = pji->page_height - pji->margin_top - pji->header_height - (pji->font_char_height*(row + 1));

	/* Print Offset */ 
	x = pji->margin_left;
	gnome_print_moveto(pji->pc, x*72 , y*72);
	sprintf(temp, "%08X", offset);
	gnome_print_show(pji->pc, temp);
	gnome_print_stroke(pji->pc);
	/* Print Hex */
	x = pji->margin_left + pji->font_char_width*pji->offset_chars + pji->pad_size ;
	gnome_print_moveto(pji->pc, x*72 , y*72);
	format_hex(pji->doc, pji->gt, temp, offset, offset + bytes);
	gnome_print_show(pji->pc, temp);
	gnome_print_stroke(pji->pc);
	/* Print Ascii */
	x = pji->margin_left + pji->font_char_width*(pji->offset_chars + pji->bytes_per_row*3) + pji->pad_size*2  ;
	gnome_print_moveto(pji->pc, x*72 , y*72);
	format_ascii(pji->doc, temp, offset, offset + bytes);
	gnome_print_show(pji->pc, temp);
	gnome_print_stroke(pji->pc);
}

static void end_page(GnomePrintContext *pc)
{
	gnome_print_showpage (pc);
}

static void end_job(GnomePrintContext *pc)
{
}

static void format_hex(HexDocument *doc, guint group_type, gchar *out, guint start, guint end)
{
	int i, j, low, high;
	guchar c;

	for(i = start + 1, j = 0; i <= end; i++) {
		c = hex_document_get_byte(doc, i - 1);
		low = c & 0x0F;
		high = (c & 0xF0) >> 4;

		out[j++] = ((high < 10)?(high + '0'):(high - 10 + 'A'));
		out[j++] = ((low < 10)?(low + '0'):(low - 10 + 'A'));

		if(i % group_type == 0)
          out[j++] = ' ';
	}
	out[j++] = 0;
}

static void format_ascii(HexDocument *doc, gchar *out, guint start, guint end)
{
	int i, j;
	guchar c;

	for(i = start, j = 0; i < end; i++, j++) {
		c = hex_document_get_byte(doc, i);
		if (is_printable(c))
			out[j] = c;
		else
			out[j] = '.';
	}
	out[j++] = 0;
}
