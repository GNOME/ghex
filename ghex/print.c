/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* print.c - print a HexDocument

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
   Printing module by: Chema Celorio <chema@celorio.com>
*/

#include "print.h"

#include <config.h>

#define is_printable(c) (((((guchar)c)>=0x20) && (((guchar)c)<0x7F))?1:0)

static void print_header(GHexPrintJobInfo *pji, unsigned int page);
static void print_row(GHexPrintJobInfo *pji, unsigned int offset,
					  unsigned int bytes, int row);
static void format_hex(HexDocument *doc, guint gt, char *out,
					   guint start, guint end);
static void format_ascii(HexDocument *doc, char *out,
						 guint start, guint end);
static void print_shaded_boxes( GHexPrintJobInfo *pji, guint page,
								guint max_row);
static void print_shaded_box( GHexPrintJobInfo *pji, guint row, guint rows);

static void print_header(GHexPrintJobInfo *pji, unsigned int page)
{
	PangoLayout *layout;

	cairo_t *cr = gtk_print_context_get_cairo_context (pji->pc);
	char *text1 = g_file_get_path (hex_document_get_file (pji->doc));
	char *text2 = g_strdup_printf (_("Page: %i/%i"), page, pji->pages);
	char *pagetext = g_strdup_printf ("%d", page);
	double x, y;
	int width, height;

	layout = gtk_print_context_create_pango_layout (pji->pc);
	pango_layout_set_text (layout, pagetext, -1);
	pango_layout_set_font_description (layout, pji->h_font);
	pango_layout_set_indent (layout, 0);
	cairo_move_to (cr, 0, 0);
	pango_cairo_show_layout (cr, layout);
	g_object_unref (layout);

	/* Print the file name */
	layout = gtk_print_context_create_pango_layout (pji->pc);
	pango_layout_set_text (layout, text1, -1);
	pango_layout_set_font_description (layout, pji->h_font);
	pango_layout_set_indent (layout, 0);
	pango_layout_get_pixel_size (layout, &width, &height);
	x = (gtk_print_context_get_width (pji->pc) - width) / 2;
	y = height;
	cairo_move_to (cr, x, y);
	pango_cairo_show_layout (cr, layout);
	g_object_unref (layout);

	/* Print the page/pages  */
	layout = gtk_print_context_create_pango_layout (pji->pc);
	pango_layout_set_text (layout, text2, -1);
	pango_layout_set_font_description (layout, pji->h_font);
	pango_layout_set_indent (layout, 0);
	pango_layout_get_pixel_size (layout, &width, &height);
	x = gtk_print_context_get_width (pji->pc) - width - 36;
	cairo_move_to (cr, x, 0);
	pango_cairo_show_layout (cr, layout);
	g_object_unref (layout);

	g_free(text1);
	g_free(text2);
	g_free(pagetext);
}

static void print_row(GHexPrintJobInfo *pji, unsigned int offset,
					  unsigned int bytes, int row)
{
	PangoLayout *layout;
	double x, y;
	const int TEMP_LEN = 256;
	char *temp = g_malloc(TEMP_LEN + 1);
	cairo_t *cr = gtk_print_context_get_cairo_context (pji->pc);

	y = pji->header_height +
		(pji->font_char_height*(row + 1));

	/* Print Offset */ 
	cairo_move_to (cr, 0, y);
	layout = gtk_print_context_create_pango_layout (pji->pc);
	g_snprintf(temp, TEMP_LEN, "%08X", offset);
	pango_layout_set_text (layout, temp, -1);
	pango_layout_set_font_description (layout, pji->d_font);
	pango_layout_set_indent (layout, 0);
	pango_cairo_show_layout (cr, layout);
	g_object_unref (layout);

	/* Print Hex */
	x = pji->font_char_width*pji->offset_chars +
		pji->pad_size ;
	cairo_move_to (cr, x, y);
	format_hex(pji->doc, pji->gt, temp, offset, offset + bytes);
	layout = gtk_print_context_create_pango_layout (pji->pc);
	pango_layout_set_text (layout, temp, -1);
	pango_layout_set_font_description (layout, pji->d_font);
	pango_layout_set_indent (layout, 0);
	pango_cairo_show_layout (cr, layout);
	g_object_unref (layout);
	/* Print Ascii */
	x = 2*pji->pad_size + pji->font_char_width*
		(pji->offset_chars + 2*pji->bytes_per_row
		+ pji->bytes_per_row/pji->gt - 1);
	cairo_move_to (cr, x, y);
	format_ascii(pji->doc, temp, offset, offset + bytes);
	layout = gtk_print_context_create_pango_layout (pji->pc);
	pango_layout_set_text (layout, temp, -1);
	pango_layout_set_font_description (layout, pji->d_font);
	pango_layout_set_indent (layout, 0);
	pango_cairo_show_layout (cr, layout);
	g_object_unref (layout);

	g_free(temp);
}

static void format_hex (HexDocument *doc, guint gt, char *out,
		guint start, guint end)
{
	int i, j, low, high;
	guchar c;

	for (i = start + 1, j = 0; i <= end; i++) {
		c = hex_buffer_get_byte (hex_document_get_buffer (doc), i - 1);
		low = c & 0x0F;
		high = (c & 0xF0) >> 4;

		out[j++] = ((high < 10)?(high + '0'):(high - 10 + 'A'));
		out[j++] = ((low < 10)?(low + '0'):(low - 10 + 'A'));

		if (i % gt == 0)
          out[j++] = ' ';
	}
	out[j++] = 0;
}

static void format_ascii (HexDocument *doc,
		char *out, guint start, guint end)
{
	int i, j;
	guchar c;

	for (i = start, j = 0; i < end; i++, j++) {
		c = hex_buffer_get_byte (hex_document_get_buffer (doc), i);
		if (is_printable(c))
			out[j] = c;
		else
			out[j] = '.';
	}
	out[j++] = 0;
}

static void print_shaded_boxes(GHexPrintJobInfo *pji, guint page,
		guint max_row)
{
	guint i;
	guint box_size = shaded_box_size;

	if (box_size == 0)
		return;

	for (i = box_size + 1;
		i <= pji->rows_per_page && i <= max_row;
		i += box_size*2)
	{
		print_shaded_box (pji, i+1, ((i + box_size - 1) > max_row ?
								  max_row - i + 1 : box_size));
	}
}

static void print_shaded_box (GHexPrintJobInfo *pji, guint row, guint rows)
{
	double box_top;
	cairo_t *cr = gtk_print_context_get_cairo_context (pji->pc);

	box_top = pji->header_height + row * pji->font_char_height;

	cairo_save (cr);
	cairo_set_source_rgb (cr, 0.90, 0.90, 0.90);
	cairo_rectangle (cr,
	                 0, box_top,
	                 gtk_print_context_get_width(pji->pc),
	                 rows * pji->font_char_height);
	cairo_fill (cr);
	cairo_restore (cr);
}

/**
 * ghex_print_job_info_new:
 * @doc: Pointer to the HexDocument to be printed.
 * @group_type: How to group bytes, as HexWidgetGroupType.
 *
 * Return value: A pointer to a newly-created GHexPrintJobInfo object.
 * NULL if unable to create.
 *
 * Creates a new GHexPrintJobInfo object.
 **/
GHexPrintJobInfo *
ghex_print_job_info_new (HexDocument *doc, HexWidgetGroupType group_type)
{
	GHexPrintJobInfo *pji;
	PangoFontDescription *d_font;
	PangoFontDescription *h_font;

	if (!doc)
		return NULL;

	/* Create the header and data fonts */
	d_font = pango_font_description_from_string (data_font_name);
	if (!d_font)
		return NULL;

	h_font = pango_font_description_from_string (header_font_name);
	if (!h_font) {
		pango_font_description_free (d_font);
		return NULL;
	}

	pji = g_new0(GHexPrintJobInfo, 1);
	pji->h_font = h_font;
	pji->d_font = d_font;
	pji->gt = group_type;

	pji->master = NULL;

	pji->doc = doc;

	pji->pad_size = .5 * 72;
	pji->offset_chars = 8;

	pji->preview = FALSE;
	pji->config = NULL;

	return pji;
}

/**
 * ghex_print_job_info_destroy:
 * @pji: Pointer to the GHexPrintJobInfo to be destroyed.
 *
 * Destroys the GHexPrintJobInfo object pointed to by pji.
 **/
void
ghex_print_job_info_destroy (GHexPrintJobInfo *pji)
{
	pango_font_description_free (pji->h_font);
	pango_font_description_free (pji->d_font);

	if (pji->config != NULL)
		g_object_unref (pji->config);

	if (pji->master != NULL)
		g_object_unref (pji->master);

	g_free(pji);
}

void
begin_print (GtkPrintOperation *operation,
             GtkPrintContext   *context,
             gpointer           data)
{
    PangoLayout *layout;
    GHexPrintJobInfo *pji = (GHexPrintJobInfo *)data;
    pji->pc = context;
    int font_width, font_height;
    int printable_width, printable_height;
	gint64 payload = hex_buffer_get_payload_size (hex_document_get_buffer (pji->doc));

    layout = gtk_print_context_create_pango_layout (context);
    pango_layout_set_text (layout, " ", -1);
    pango_layout_set_font_description (layout, pji->h_font);
    pango_layout_set_indent (layout, 0);
    pango_layout_get_pixel_size (layout, NULL, &font_height);
    pji->header_height = 2 * font_height;
    g_object_unref (layout);

    layout = gtk_print_context_create_pango_layout (context);
    pango_layout_set_font_description (layout, pji->d_font);
    pango_layout_set_indent (layout, 0);
    pango_layout_set_text (layout, " ", -1);
    pango_layout_get_pixel_size (layout, &font_width, &font_height);
    pji->font_char_width = font_width;
    pji->font_char_height = font_height;
    g_object_unref (layout);

    printable_height = gtk_print_context_get_height (pji->pc);
    printable_width = gtk_print_context_get_width (pji->pc);

    pji->bytes_per_row = (printable_width - pji->pad_size * 2 -
                          (pji->offset_chars *
                           pji->font_char_width)) / pji->font_char_width;
    pji->bytes_per_row /= 3*pji->gt + 1;
    pji->bytes_per_row *= pji->gt;
    pji->rows_per_page = (printable_height - pji->header_height) /
                          pji->font_char_height - 2;
    pji->pages = (((payload/pji->bytes_per_row) + 1)/
                   pji->rows_per_page) + 1;
    gtk_print_operation_set_n_pages (pji->master, pji->pages);
}

void
print_page (GtkPrintOperation *operation,
            GtkPrintContext   *context,
            int               page_nr,
            gpointer           data)
{
	int j, max_row;
	gint64 payload;

	GHexPrintJobInfo *pji = (GHexPrintJobInfo *)data;
	g_return_if_fail(pji != NULL);

	pji->pc = context;
	g_return_if_fail(pji->pc != NULL);

	payload = hex_buffer_get_payload_size (hex_document_get_buffer (pji->doc));

	print_header (pji, page_nr+1);
	max_row = (pji->bytes_per_row*pji->rows_per_page*(page_nr+1) >
			payload ?
			(int)((payload-1)-
			      (pji->bytes_per_row *
			       pji->rows_per_page*(page_nr))) /
			       pji->bytes_per_row + 1:
			       pji->rows_per_page);
	print_shaded_boxes (pji, page_nr, max_row);
	for (j = 1; j <= pji->rows_per_page; j++) {
		int file_offset = pji->bytes_per_row*(j - 1) +
			pji->bytes_per_row*pji->rows_per_page*(page_nr);
		int length = (file_offset + pji->bytes_per_row >
			payload ?
			payload - file_offset :
			pji->bytes_per_row);
		if (file_offset >= payload)
			break;
		print_row (pji, file_offset, length, j);
	}
}
