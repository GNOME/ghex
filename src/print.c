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

#define is_printable(c) (((((guchar)c)>=0x20) && (((guchar)c)<=0x7F))?1:0)

GnomePaper *def_paper;
PreviewWindow *preview_window = NULL;

static void start_job(GnomePrintContext *pc);
static void print_header(GHexPrintJobInfo *pji, unsigned int page);
static void end_page(GnomePrintContext *pc);
static void print_row(GHexPrintJobInfo *pji, unsigned int offset, unsigned int bytes, int row);
static void end_job(GnomePrintContext *pc);

static void format_hex(HexDocument *doc, guint gt, gchar *out, guint start, guint end);
static void format_ascii(HexDocument *doc, gchar *out, guint start, guint end);

static void close_preview(PreviewWindow *pw);
static gboolean preview_delete_event_cb(GtkWidget *w, GdkEventAny *e, PreviewWindow *pw);
static void preview_buttons_set_sensitivity(PreviewWindow *pw);
static void preview_next_cb(GtkWidget *w, PreviewWindow *pw);
static void preview_prev_cb(GtkWidget *w, PreviewWindow *pw);
static void preview_first_cb(GtkWidget *w, PreviewWindow *pw);
static void preview_last_cb(GtkWidget *w, PreviewWindow *pw);
static void preview_zoom_value_changed_cb(GtkAdjustment *adj, PreviewWindow *pw);
static void preview_close_cb(GtkWidget *w, PreviewWindow *pw);


static void close_preview(PreviewWindow *pw)
{
	if(pw->job) {
		gnome_print_context_close(pw->job->pc);
		g_free(pw->job);
		pw->job = NULL;
	}
}

static gboolean preview_delete_event_cb(GtkWidget *w, GdkEventAny *e, PreviewWindow *pw)
{
	close_preview(pw);

	gtk_widget_hide(pw->window);

	return TRUE;
}

static void preview_buttons_set_sensitivity(PreviewWindow *pw)
{
	gint page, no_pages;

	no_pages = gnome_print_preview_job_num_pages(pw->job->pj);
	page = gnome_print_preview_job_current_page(pw->job->pj);

	gtk_widget_set_sensitive(pw->prev, page > 0);
	gtk_widget_set_sensitive(pw->next, page < no_pages - 1);
}

static void preview_next_cb(GtkWidget *w, PreviewWindow *pw)
{
	gint page, no_pages;

	no_pages = gnome_print_preview_job_num_pages(pw->job->pj);
	page = gnome_print_preview_job_current_page(pw->job->pj);

	page++;
	if(page < no_pages)
		gnome_print_preview_job_page_show(pw->job->pj, page);
	if(page >= no_pages - 1)
		gtk_widget_set_sensitive(pw->next, FALSE);
	if(page > 0)
		gtk_widget_set_sensitive(pw->prev, TRUE);
}

static void preview_prev_cb(GtkWidget *w, PreviewWindow *pw)
{
	gint page, no_pages;

	no_pages = gnome_print_preview_job_num_pages(pw->job->pj);
	page = gnome_print_preview_job_current_page(pw->job->pj);

	page--;
	if(page >= 0)
		gnome_print_preview_job_page_show(pw->job->pj, page);
	if(page <= 0)
		gtk_widget_set_sensitive(pw->prev, FALSE);
	if(page < no_pages - 1)
		gtk_widget_set_sensitive(pw->next, TRUE);
}

static void preview_first_cb(GtkWidget *w, PreviewWindow *pw)
{
	gint page;

	page = gnome_print_preview_job_current_page(pw->job->pj);

	if(page != 0)
		gnome_print_preview_job_page_show(pw->job->pj, 0);
	preview_buttons_set_sensitivity(pw);
}

static void preview_last_cb(GtkWidget *w, PreviewWindow *pw)
{
	gint page, no_pages;

	no_pages = gnome_print_preview_job_num_pages(pw->job->pj);
	page = gnome_print_preview_job_current_page(pw->job->pj);

	if(page != no_pages - 1)
		gnome_print_preview_job_page_show(pw->job->pj, no_pages - 1);
	preview_buttons_set_sensitivity(pw);
}

static void preview_zoom_value_changed_cb(GtkAdjustment *adj, PreviewWindow *pw)
{
	gnome_canvas_set_pixels_per_unit(GNOME_CANVAS(pw->canvas), adj->value/100.0);
}

static void preview_close_cb(GtkWidget *w, PreviewWindow *pw)
{
	close_preview(pw);

	gtk_widget_hide(pw->window);
}

PreviewWindow *create_preview_window()
{
	PreviewWindow *pw;
	GtkWidget *sw, *label;
	GtkWidget *hbox, *vbox;

	gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
	gtk_widget_set_default_visual(gdk_rgb_get_visual());
		
	pw = (PreviewWindow *)g_new0(PreviewWindow, 1);
	pw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect(GTK_OBJECT(pw->window), "delete_event",
					   GTK_SIGNAL_FUNC(preview_delete_event_cb),
					   pw);
	vbox = gtk_vbox_new(FALSE, 2);
	gtk_widget_show(vbox);
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_widget_show(hbox);

	/* navigation buttons */
	/* TODO: this needs fixing, UP and DOWN are not suitable names! */
	pw->first = gnome_stock_button(GNOME_STOCK_BUTTON_UP);
	gtk_box_pack_start(GTK_BOX(hbox), pw->first, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(pw->first), "clicked",
					   GTK_SIGNAL_FUNC(preview_first_cb), pw);
	gtk_widget_show(pw->first);
	pw->prev = gnome_stock_button(GNOME_STOCK_BUTTON_PREV);
	gtk_box_pack_start(GTK_BOX(hbox), pw->prev, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(pw->prev), "clicked",
					   GTK_SIGNAL_FUNC(preview_prev_cb), pw);
	gtk_widget_show(pw->prev);
	pw->next = gnome_stock_button(GNOME_STOCK_BUTTON_NEXT);
	gtk_box_pack_start(GTK_BOX(hbox), pw->next, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(pw->next), "clicked",
					   GTK_SIGNAL_FUNC(preview_next_cb), pw);
	gtk_widget_show(pw->next);
	pw->last = gnome_stock_button(GNOME_STOCK_BUTTON_DOWN);
	gtk_box_pack_start(GTK_BOX(hbox), pw->last, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(pw->last), "clicked",
					   GTK_SIGNAL_FUNC(preview_last_cb), pw);
	gtk_widget_show(pw->last);

	/* zoom */
	/* FIXME: prevent setting zoom for every incrementation as this
	   is _grossly_ slow */
	label = gtk_label_new(_("Zoom:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	gtk_widget_show(label);
	pw->zoom_adj = GTK_ADJUSTMENT(gtk_adjustment_new(100.0, 1.0, 1000.0,
													 10.0, 100.0, 100.0));
	pw->zoom = gtk_spin_button_new(pw->zoom_adj, 0.0, 1);
	gtk_box_pack_start(GTK_BOX(hbox), pw->zoom, TRUE, TRUE, 0);
	gtk_widget_show(pw->zoom);

	/* close button */
	pw->close = gnome_stock_button(GNOME_STOCK_BUTTON_CLOSE);
	gtk_box_pack_start(GTK_BOX(hbox), pw->close, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(pw->close), "clicked",
					   GTK_SIGNAL_FUNC(preview_close_cb), pw);
	gtk_widget_show(pw->close);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* scrolled window with canvas */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show(sw);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	pw->canvas = gnome_canvas_new_aa();
	gnome_canvas_set_pixels_per_unit(GNOME_CANVAS(pw->canvas), 1);
	gtk_widget_show(pw->canvas);
	gtk_container_add(GTK_CONTAINER(sw), pw->canvas);

	/* connect this after creating the canvas! */
	gtk_signal_connect(GTK_OBJECT(pw->zoom_adj), "value_changed",
					   GTK_SIGNAL_FUNC(preview_zoom_value_changed_cb), pw);

	gtk_container_add(GTK_CONTAINER(pw->window), vbox);

	return pw;
}

void print_document(HexDocument *doc, guint gt, GnomePrinter *printer)
{
	int i, j;
	GHexPrintJobInfo *pji;
	const gchar *paper_name;

	pji = g_new0(GHexPrintJobInfo, 1);

	pji->gt = gt;
	paper_name = gnome_paper_name(def_paper);
	if(printer) {
		pji->pc = gnome_print_context_new_with_paper_size(printer, paper_name);
		pji->pj = NULL;
	}
	else { /* printer == NULL => preview */
		if(!preview_window)
			preview_window = create_preview_window();
		else {
			GnomeCanvasGroup *root;
			GList *children;
			GtkObject *item;

			root = gnome_canvas_root(GNOME_CANVAS(preview_window->canvas));
			if(root) {
				children = root->item_list;
				
				while(children) {
					item = GTK_OBJECT(children->data);
					children = children->next;
					gtk_object_destroy(GTK_OBJECT(item));
				}
			}
		}

		create_dialog_title(preview_window->window, _("GHex (%s): Print Preview"));

		pji->pc = gnome_print_preview_new(GNOME_CANVAS(preview_window->canvas),
										  paper_name);
		if(preview_window->job) {
			gnome_print_context_close(preview_window->job->pc);
			g_free(preview_window->job);
		}
		preview_window->job = pji;
		gtk_widget_show(preview_window->window);
	}
	pji->doc = doc;

	g_return_if_fail(pji->pc != NULL);

    pji->page_width  = gnome_paper_pswidth(def_paper);
    pji->page_height = gnome_paper_psheight(def_paper);

	/* I've taken the liberty to convert inches to ps points */
    pji->margin_top = .75 * 72;       /* Printer margins, not page margins */
    pji->margin_bottom = .75 * 72; 
    pji->margin_left = .75 * 72;
    pji->margin_right = .75 * 72;
    pji->header_height = 1 * 72;
    pji->font_char_width = 0.0808 * 72;
    pji->font_char_height = .14 * 72;
    pji->pad_size = .5 * 72;   
    pji->offset_chars = 8;

	pji->printable_width  = pji->page_width - pji->margin_left - pji->margin_right;
	pji->printable_height = pji->page_height - pji->margin_top - pji->margin_bottom;
	pji->bytes_per_row = (pji->printable_width - pji->pad_size*2 - (pji->offset_chars * pji->font_char_width))/
		                 ((3 + (1/((float)pji->gt)))*pji->font_char_width);
	pji->bytes_per_row -= pji->bytes_per_row % pji->gt;
	pji->rows_per_page = (pji->printable_height - pji->header_height)/pji->font_char_height - 1 ;
	pji->pages = (((doc->file_size/pji->bytes_per_row) + 1)/pji->rows_per_page) + 1;

	start_job(pji->pc);
	for(i = 1; i <= pji->pages; i++)
	{
		print_header(pji, i);
		for(j = 1; j <= pji->rows_per_page; j++)
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

	if(printer) {
		gnome_print_context_close(pji->pc);
		g_free(pji);
	}
	else {
		pji->pj = gnome_print_preview_get_job(GNOME_PRINT_PREVIEW(pji->pc));
		gnome_print_preview_job_page_show(pji->pj, 0);
		preview_buttons_set_sensitivity(preview_window);
	}
}

static void start_job(GnomePrintContext *pc)
{
}

static void print_header(GHexPrintJobInfo *pji, unsigned int page)
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
	x = pji->page_width/2 - len/2;
	gnome_print_moveto(pji->pc, x, y);
	gnome_print_show(pji->pc, text1);
	gnome_print_stroke(pji->pc);

	/* Print the page/pages  */
	y = pji->page_height - pji->margin_top - pji->header_height/4;
    len = gnome_font_get_width_string (font, text2);
	x = pji->page_width - len - 36;
	gnome_print_moveto(pji->pc, x, y);
	gnome_print_show(pji->pc, text2);
	gnome_print_stroke(pji->pc); 

	/* Set the font for the rest of the page */
	font = gnome_font_new ("Courier", 10);
	gnome_print_setfont (pji->pc, font);
}

static void print_row(GHexPrintJobInfo *pji, unsigned int offset, unsigned int bytes, int row)
{
	float x, y;
	gchar *temp = g_malloc(265);

	y = pji->page_height - pji->margin_top - pji->header_height - (pji->font_char_height*(row + 1));

	/* Print Offset */ 
	x = pji->margin_left;
	gnome_print_moveto(pji->pc, x , y);
	sprintf(temp, "%08X", offset);
	gnome_print_show(pji->pc, temp);
	gnome_print_stroke(pji->pc);
	/* Print Hex */
	x = pji->margin_left + pji->font_char_width*pji->offset_chars + pji->pad_size ;
	gnome_print_moveto(pji->pc, x, y);
	format_hex(pji->doc, pji->gt, temp, offset, offset + bytes);
	gnome_print_show(pji->pc, temp);
	gnome_print_stroke(pji->pc);
	/* Print Ascii */
	x = pji->margin_left + pji->font_char_width*( pji->offset_chars + pji->bytes_per_row* (2+(1/((float)pji->gt)))) + pji->pad_size*2  ;
	gnome_print_moveto(pji->pc, x, y);
	format_ascii(pji->doc, temp, offset, offset + bytes);
	gnome_print_show(pji->pc, temp);
	gnome_print_stroke(pji->pc);

	g_free(temp);
}

static void end_page(GnomePrintContext *pc)
{
	gnome_print_showpage (pc);
}

static void end_job(GnomePrintContext *pc)
{
}

static void format_hex(HexDocument *doc, guint gt, gchar *out, guint start, guint end)
{
	int i, j, low, high;
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
