/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* print.c - print a HexDocument

   Copyright (C) 1998 - 2001 Free Software Foundation

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
   Printing module by: Chema Celorio <chema@celorio.com>
*/

#include "ghex.h"
#include "gtkhex.h"
#include <libgnomeprint/gnome-print-master-preview.h>

#define is_printable(c) (((((guchar)c)>=0x20) && (((guchar)c)<=0x7F))?1:0)

const GnomePaper *def_paper;
gchar *data_font_name, *header_font_name;
gdouble data_font_size, header_font_size;
gint shaded_box_size;

static void print_header(GHexPrintJobInfo *pji, unsigned int page);
static void end_page(GnomePrintContext *pc);
static void print_row(GHexPrintJobInfo *pji, unsigned int offset, unsigned int bytes, int row);
static void format_hex(HexDocument *doc, guint gt, gchar *out, guint start, guint end);
static void format_ascii(HexDocument *doc, gchar *out, guint start, guint end);
static void print_shaded_boxes( GHexPrintJobInfo *pji, guint page, guint max_row);
static void print_shaded_box( GHexPrintJobInfo *pji, guint row, guint rows);
static gboolean print_verify_fonts (void);

static void print_header(GHexPrintJobInfo *pji, unsigned int page)
{
	guchar* text1 = g_strdup(pji->doc->file_name);
	guchar* text2 = g_strdup_printf(_("Page: %i/%i"),page,pji->pages);
	gfloat x, y, len;
	
	gnome_print_setfont(pji->pc, pji->h_font);
	/* Print the file name */
	y = pji->page_height - pji->margin_top -
		2.1*gnome_font_get_ascender(pji->h_font) -
		1.1*gnome_font_get_descender(pji->h_font);
	len = gnome_font_get_width_string (pji->h_font, text1);
	x = pji->page_width/2 - len/2;
	gnome_print_moveto(pji->pc, x, y);
	gnome_print_show(pji->pc, text1);
	gnome_print_stroke(pji->pc);

	/* Print the page/pages  */
	y = pji->page_height - pji->margin_top -
		gnome_font_get_ascender(pji->h_font);
	len = gnome_font_get_width_string (pji->h_font, text2);
	x = pji->page_width - len - 36;
	gnome_print_moveto(pji->pc, x, y);
	gnome_print_show(pji->pc, text2);
	gnome_print_stroke(pji->pc); 
}

#define TEMP_LEN 256

static void print_row(GHexPrintJobInfo *pji, unsigned int offset, unsigned int bytes, int row)
{
	gfloat x, y;
	gchar *temp = g_malloc(TEMP_LEN + 1);

	y = pji->page_height -
		pji->margin_top -
		pji->header_height -
		(pji->font_char_height*(row + 1));
	/* Print Offset */ 
	x = pji->margin_left;
	gnome_print_moveto(pji->pc, x , y);
	g_snprintf(temp, TEMP_LEN, "%08X", offset);
	gnome_print_show(pji->pc, temp);
	gnome_print_stroke(pji->pc);
	/* Print Hex */
	x = pji->margin_left +
		pji->font_char_width*pji->offset_chars +
		pji->pad_size ;
	gnome_print_moveto(pji->pc, x, y);
	format_hex(pji->doc, pji->gt, temp, offset, offset + bytes);
	gnome_print_show(pji->pc, temp);
	gnome_print_stroke(pji->pc);
	/* Print Ascii */
	x = 2*pji->pad_size + pji->margin_left + pji->font_char_width*
		(pji->offset_chars + pji->bytes_per_row*(2 + (1/((float)pji->gt))));
	gnome_print_moveto(pji->pc, x, y);
	format_ascii(pji->doc, temp, offset, offset + bytes);
	gnome_print_show(pji->pc, temp);
	gnome_print_stroke(pji->pc);

	g_free(temp);
}

static void end_page(GnomePrintContext *pc)
{
	gnome_print_showpage(pc);
}

static void format_hex(HexDocument *doc, guint gt, gchar *out, guint start, guint end)
{
	gint i, j, low, high;
	guchar c;

	for(i = start + 1, j = 0; i <= end; i++) {
		c = hex_document_get_byte(doc, i - 1);
		low = c & 0x0F;
		high = (c & 0xF0) >> 4;

		out[j++] = ((high < 10)?(high + '0'):(high - 10 + 'A'));
		out[j++] = ((low < 10)?(low + '0'):(low - 10 + 'A'));

		if(i % gt == 0)
          out[j++] = ' ';
	}
	out[j++] = 0;
}

static void format_ascii(HexDocument *doc, gchar *out, guint start, guint end)
{
	gint i, j;
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

static void print_shaded_boxes(GHexPrintJobInfo *pji, guint page, guint max_row)
{
	gint i;
	gint box_size = shaded_box_size;

	if(box_size == 0)
		return;

	for(i = box_size + 1;
		i <= pji->rows_per_page && i <= max_row;
		i += box_size*2)
		print_shaded_box(pji, i, ((i + box_size - 1) > max_row ? max_row - i + 1 : box_size));
}

static void print_shaded_box(GHexPrintJobInfo *pji, guint row, guint rows)
{
	gfloat box_top = 0 ;
	gfloat box_bottom = 0;
	gfloat box_right = 0;
	gfloat box_left = 0;
	gfloat box_adjustment_top = 0;
	gfloat box_adjustment_bottom = 0;

	box_adjustment_top = pji->font_char_height*0.25;
	box_adjustment_bottom = pji->font_char_height*0.125;
	box_top = pji->page_height - pji->margin_top - pji->header_height -
		      row*pji->font_char_height - box_adjustment_top;
	box_bottom = box_top - rows*pji->font_char_height + box_adjustment_bottom;
	box_left   = pji->margin_left;
	box_right  = pji->page_width - pji->margin_right;
	
	gnome_print_setrgbcolor(pji->pc, 0.95, 0.95, 0.95);
	gnome_print_moveto(pji->pc, box_right, box_top);
	gnome_print_lineto(pji->pc, box_left,  box_top);
	gnome_print_lineto(pji->pc, box_left,  box_bottom);
	gnome_print_lineto(pji->pc, box_right, box_bottom);
	gnome_print_lineto(pji->pc, box_right, box_top);
	gnome_print_closepath(pji->pc);
	gnome_print_fill(pji->pc);
	gnome_print_setrgbcolor(pji->pc, 0.0, 0.0, 0.0);
}

static gboolean print_verify_fonts()
{
	GnomeFont *test_font;
	guchar *test_font_name;

	test_font_name = g_strdup(data_font_name); 
	test_font = gnome_font_new(test_font_name, 10);
	if(test_font==NULL)
	{
		gchar *errstr = g_strdup_printf(_("GHex could not find the font \"%s\".\n"
										  "GHex is unable to print without this font installed."),
										test_font_name);
		gnome_app_error(mdi->active_window, errstr);
		g_free(errstr);
		return FALSE;
	}
	gnome_font_unref(test_font);
	g_free(test_font_name);
	
	test_font_name = g_strdup(header_font_name);
	test_font = gnome_font_new(test_font_name, 10);
	if(test_font==NULL)
	{
		gchar *errstr = g_strdup_printf(_("GHex could not find the font \"%s\".\n"
										  "GHex is unable to print without this font installed."),
										test_font_name);
		gnome_app_error(mdi->active_window, errstr);
		g_free(errstr);
		return FALSE;
	}
	gnome_font_unref(test_font);
	g_free(test_font_name);	
 
	return TRUE;
}

/**
 * ghex_print_job_info_new:
 * @doc: Pointer to the HexDocument to be printed.
 * @group_type: How to group bytes. GROUP_BYTE, GROUP_WORD, or GROUP_LONG.
 *
 * Return value: A pointer to a newly-created GHexPrintJobInfo object.  NULL if
unable to create.
 *
 * Creates a new GHexPrintJobInfo object.
 **/
GHexPrintJobInfo *
ghex_print_job_info_new(HexDocument *doc, guint group_type)
{
	GHexPrintJobInfo *pji;
	GnomeFont *d_font;
	GnomeFont *h_font;
	guint32 glyph;
	ArtPoint point;

	if (!doc || !print_verify_fonts())
		return NULL;

	/* Create the header and data fonts */
	d_font = gnome_font_new(data_font_name, data_font_size);
	if (!d_font)
		return NULL;

	h_font = gnome_font_new(header_font_name, header_font_size);
	if (!h_font) {
		gnome_font_unref(d_font);
		return NULL;
	}

	pji = g_new0(GHexPrintJobInfo, 1);
	pji->h_font = h_font;
	pji->d_font = d_font;
	pji->gt = group_type;

	pji->master = gnome_print_master_new();

	gnome_print_master_set_paper(pji->master, def_paper);

	pji->doc = doc;

	pji->page_width = gnome_paper_pswidth(def_paper);
	pji->page_height = gnome_paper_psheight(def_paper);

	/* Convert inches to ps points */
	pji->margin_top = .75 * 72; /* Printer margins, not page margins */
	pji->margin_bottom = .75 * 72;
	pji->margin_left = .75 * 72;
	pji->margin_right = .75 * 72;
	pji->header_height = 2.2*(gnome_font_get_ascender(GNOME_FONT(h_font)) +
		gnome_font_get_descender(GNOME_FONT(h_font)));

	/* Get font character width */
	glyph = ' ';
	gnome_font_get_glyph_stdadvance(GNOME_FONT(d_font), glyph, &point);
	pji->font_char_width = point.x;

	pji->font_char_height = gnome_font_get_ascender(GNOME_FONT(d_font)) +
		gnome_font_get_descender(GNOME_FONT(d_font));

	/* Add 10% spacing between lines */
	pji->font_char_height *= 1.1;
	pji->pad_size = .5 * 72;
	pji->offset_chars = 8;

	pji->printable_width = pji->page_width -
		pji->margin_left -
		pji->margin_right;
	pji->printable_height = pji->page_height -
		pji->margin_top -
		pji->margin_bottom;

	pji->bytes_per_row = (pji->printable_width - pji->pad_size*2 -
						(pji->offset_chars *
						 pji->font_char_width))/
				((3 + (1/((float)pji->gt))) *
				 pji->font_char_width);
	pji->bytes_per_row -= pji->bytes_per_row % pji->gt;
	pji->rows_per_page = (pji->printable_height - pji->header_height) /
		pji->font_char_height - 1;
	pji->pages = (((doc->file_size/pji->bytes_per_row) + 1)/
				pji->rows_per_page) + 1;
	pji->page_first = 1;
	pji->page_last = pji->pages;
	pji->preview = FALSE;
	pji->printer = NULL;
	pji->range = GNOME_PRINT_RANGE_ALL;

	return pji;
}

/**
 * ghex_print_job_info_destroy:
 * @pji: Pointer to the GHexPrintJobInfo to be destroyed.
 *
 * Destroys the GHexPrintJobInfo object pointed to by pji.
 **/
void
ghex_print_job_info_destroy(GHexPrintJobInfo *pji)
{
	gnome_print_master_close(pji->master);
	gnome_font_unref(pji->h_font);
	gnome_font_unref(pji->d_font);
	g_free(pji);
}

/**
 * ghex_print_job_execute:
 * @pji: Pointer to the GHexPrintJobInfo object.
 *
 * Performs the printing job described by the GHexPrintJobInfo object.
 **/
void
ghex_print_job_execute(GHexPrintJobInfo *pji)
{
	gint i;
	gint j;

	g_return_if_fail(pji != NULL);

	pji->pc = gnome_print_master_get_context(pji->master);

	g_return_if_fail(pji->pc != NULL);

	for(i = pji->page_first; i <= pji->page_last; i++) {
		int max_row;
		print_header(pji, i);
		gnome_print_setfont(pji->pc, pji->d_font);
		max_row = (pji->bytes_per_row*pji->rows_per_page*(i) >
				pji->doc->file_size ?
				(int)((pji->doc->file_size-1)-
				      (pji->bytes_per_row *
				       pji->rows_per_page*(i-1))) /
				pji->bytes_per_row + 1:
				pji->rows_per_page);
		print_shaded_boxes(pji, i, max_row);
		for(j = 1; j <= pji->rows_per_page; j++) {
			int file_offset = pji->bytes_per_row*(j - 1) +
				pji->bytes_per_row*pji->rows_per_page*(i - 1);
			int length;
			length = (file_offset + pji->bytes_per_row >
					pji->doc->file_size ?
					pji->doc->file_size - file_offset :
					pji->bytes_per_row);
			if(file_offset >= pji->doc->file_size)
				break;
			print_row(pji, file_offset, length, j);
		}
		end_page(pji->pc);
	}
	gnome_print_context_close(pji->pc);
}
