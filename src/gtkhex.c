/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

/* gtkhex.c - definition of a GtkHex widget

   Copyright © 1997 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021 Logan Rathbone

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

   Original Author: Jaka Mocnik <jaka@gnu.org>
*/

#include <config.h>

#include <string.h>

#include "hex-document.h"
#include "gtkhex.h"

/* LAR - TEMPORARY FOR TESTING ONLY */

#ifdef ENABLE_DEBUG
#define TEST_DEBUG_FUNCTION_START g_debug ("%s: start", __func__);
#endif

#define NOT_IMPLEMENTED \
	g_critical("%s: NOT IMPLEMENTED", __func__);

/* LAR - new stuff that wasn't in old code */
#define CSS_NAME "hex"
//#define CSS_NAME "entry"

/* default minimum drawing area size (for ascii and hex widgets) in pixels. */
#define DEFAULT_DA_SIZE 100

/* LAR - defines copied from the old header. */

/* how to group bytes? */
#define GROUP_BYTE 1
#define GROUP_WORD 2
#define GROUP_LONG 4

#define LOWER_NIBBLE TRUE
#define UPPER_NIBBLE FALSE

/* ------ */

/* LAR - some more defines brought in from the old gtkhex-private.h */

#define VIEW_HEX 1
#define VIEW_ASCII 2

/* ----- */

#define DEFAULT_CPL 32
#define DEFAULT_LINES 10

#define SCROLL_TIMEOUT 100

#define is_displayable(c) (((c) >= 0x20) && ((c) < 0x7f))

enum {
	CURSOR_MOVED_SIGNAL,
	DATA_CHANGED_SIGNAL,
	CUT_CLIPBOARD_SIGNAL,
	COPY_CLIPBOARD_SIGNAL,
	PASTE_CLIPBOARD_SIGNAL,
	LAST_SIGNAL
};

enum {
  TARGET_STRING,
};

/* highlighting information.
 */
typedef struct _GtkHex_Highlight GtkHex_Highlight;

/* start_line and end_line only have to be set (and valid) of
 * valid is set to TRUE. */
struct _GtkHex_Highlight
{
	gint start, end;
	gint start_line, end_line;
	GdkRGBA *bg_color; /* NULL to use the style color */
	gint min_select;

	GtkHex_Highlight *prev, *next;
	gboolean valid;
};

/* used to automatically highlight all visible occurrences
 * of the string.
 */
struct _GtkHex_AutoHighlight
{
	gint search_view;
	gchar *search_string;
	gint search_len;

	gint view_min;
	gint view_max;

	GtkHex_Highlight *highlights;
	GtkHex_AutoHighlight *next, *prev;
};

/* TODO / NOTE - 'GtkHexClass' previously had these members:
 *		GtkClipboard *clipboard, *primary;
 * so when you see ->clipboard and ->primary, these are related to
 * clipboard stuff that needs to be rewritten. */

/* ------------------------------
 * Main GtkHex GObject definition
 * ------------------------------
 */

struct _GtkHex
{
	GtkWidget parent_instance;

	HexDocument *document;

	GtkWidget *box;				/* main box for layout */

	GtkWidget *xdisp, *adisp;	/* DrawingArea */
	GtkWidget *offsets;			/* DrawingArea */
	GtkWidget *scrollbar;

	PangoLayout *xlayout, *alayout, *olayout;

	GtkAdjustment *adj;

	int active_view;

	guint char_width, char_height;
	guint button;

	guint cursor_pos;
	GtkHex_Highlight selection;
	gint lower_nibble;

	guint group_type;

	int lines, vis_lines, cpl, top_line;
	int cursor_shown;

	int xdisp_width, adisp_width, extra_width;

	GtkHex_AutoHighlight *auto_highlight;

	int scroll_dir;
	guint scroll_timeout;
	gboolean show_offsets;
	int starting_offset;
	gboolean insert;
	gboolean selecting;

	/* Buffer for storing formatted data for rendering;
	   dynamically adjusts its size to the display size */
	guchar *disp_buffer;

	/* default characters per line and number of lines. */
	int default_cpl;
	int default_lines;
};

G_DEFINE_TYPE(GtkHex, gtk_hex, GTK_TYPE_WIDGET)

/* ----- */

static gint gtkhex_signals[LAST_SIGNAL] = { 0 };

static gchar *char_widths = NULL;

static void render_hex_highlights (GtkHex *gh, cairo_t *cr, gint cursor_line);
static void render_ascii_highlights (GtkHex *gh, cairo_t *cr, gint cursor_line);
static void render_hex_lines (GtkHex *gh, cairo_t *cr, gint, gint);
static void render_ascii_lines (GtkHex *gh, cairo_t *cr, gint, gint);

static void gtk_hex_validate_highlight(GtkHex *gh, GtkHex_Highlight *hl);
static void gtk_hex_invalidate_highlight(GtkHex *gh, GtkHex_Highlight *hl);
static void gtk_hex_invalidate_all_highlights(GtkHex *gh);

static void gtk_hex_update_all_auto_highlights(GtkHex *gh, gboolean delete,
		gboolean add);

static GtkHex_Highlight *gtk_hex_insert_highlight (GtkHex *gh,
		GtkHex_AutoHighlight *ahl,
		gint start, gint end);

static void gtk_hex_delete_highlight (GtkHex *gh, GtkHex_AutoHighlight *ahl,
		GtkHex_Highlight *hl);

static void gtk_hex_update_auto_highlight(GtkHex *gh, GtkHex_AutoHighlight *ahl,
		gboolean delete, gboolean add);

static void recalc_displays(GtkHex *gh);

/*
 * ?_to_pointer translates mouse coordinates in hex/ascii view
 * to cursor coordinates.
 */
static void
hex_to_pointer(GtkHex *gh, guint mx, guint my)
{
	guint cx, cy, x;
	
	cy = gh->top_line + my/gh->char_height;
	
	cx = 0; x = 0;
	while(cx < 2*gh->cpl) {
		x += gh->char_width;
		
		if(x > mx) {
			gtk_hex_set_cursor_xy(gh, cx/2, cy);
			gtk_hex_set_nibble(gh, ((cx%2 == 0)?UPPER_NIBBLE:LOWER_NIBBLE));
			
			cx = 2*gh->cpl;
		}
		
		cx++;
		
		if(cx%(2*gh->group_type) == 0)
			x += gh->char_width;
	}
}

static void
ascii_to_pointer(GtkHex *gh, gint mx, gint my)
{
	int cy;
	
	cy = gh->top_line + my/gh->char_height;
	
	gtk_hex_set_cursor_xy(gh, mx/gh->char_width, cy);
}


static int
get_char_height (GtkHex *gh)
{
	PangoContext *context;
	PangoFontMetrics *metrics;
	int height;

	context = gtk_widget_get_pango_context (GTK_WIDGET(gh));
	metrics = pango_context_get_metrics (context, NULL, NULL);

	height =
		PANGO_PIXELS(pango_font_metrics_get_height (metrics));
	
	return height;
}

static int
get_char_width (GtkHex *gh)
{
	PangoContext *context;
	PangoFontMetrics *metrics;
	int width;

	context = gtk_widget_get_pango_context (GTK_WIDGET(gh));
	metrics = pango_context_get_metrics (context, NULL, NULL);

	/* generally the digit width returned will be bigger, but let's take
	 * the max for now and run with it.
	 */
	width = MAX(pango_font_metrics_get_approximate_digit_width(metrics),
				pango_font_metrics_get_approximate_char_width(metrics));

	/* scale down from pango units to pixels */
	width = PANGO_PIXELS(width);
	
	return width;
}

void
format_xbyte(GtkHex *gh, gint pos, gchar buf[2]) {
	guint low, high;
	guchar c;

	c = gtk_hex_get_byte(gh, pos);
	low = c & 0x0F;
	high = (c & 0xF0) >> 4;
	
	buf[0] = ((high < 10)?(high + '0'):(high - 10 + 'A'));
	buf[1] = ((low < 10)?(low + '0'):(low - 10 + 'A'));
}

/*
 * format_[x|a]block() formats contents of the buffer
 * into displayable text in hex or ascii, respectively
 */
gint
format_xblock(GtkHex *gh, gchar *out, guint start, guint end)
{
	int i, j, low, high;
	guchar c;

	for(i = start + 1, j = 0; i <= end; i++) {
		c = gtk_hex_get_byte(gh, i - 1);
		low = c & 0x0F;
		high = (c & 0xF0) >> 4;
		
		out[j++] = ((high < 10)?(high + '0'):(high - 10 + 'A'));
		out[j++] = ((low < 10)?(low + '0'):(low - 10 + 'A'));
		
		if(i%gh->group_type == 0)
			out[j++] = ' ';
	}
	
	return j;
}

gint
format_ablock(GtkHex *gh, gchar *out, guint start, guint end)
{
	int i, j;
	guchar c;

	for (i = start, j = 0; i < end; i++, j++) {
		c = gtk_hex_get_byte(gh, i);
		if (is_displayable(c))
			out[j] = c;
		else
			out[j] = '.';
	}

	return end - start;
}

/*
 * get_[x|a]coords() translates offset from the beginning of
 * the block into x,y coordinates of the xdisp/adisp, respectively
 */
static gint
get_xcoords(GtkHex *gh, gint pos, gint *x, gint *y)
{
	gint cx, cy, spaces;
	
	if(gh->cpl == 0)
		return FALSE;
	
	cy = pos / gh->cpl;
	cy -= gh->top_line;
	if(cy < 0)
		return FALSE;
	
	cx = 2*(pos % gh->cpl);
	spaces = (pos % gh->cpl) / gh->group_type;
	
	cx *= gh->char_width;
	cy *= gh->char_height;
	spaces *= gh->char_width;
	*x = cx + spaces;
	*y = cy;
	
	return TRUE;
}

static gint
get_acoords (GtkHex *gh, gint pos, gint *x, gint *y)
{
	gint cx, cy;
	
	if (gh->cpl == 0)
		return FALSE;

	cy = pos / gh->cpl;
	cy -= gh->top_line;

	if (cy < 0)
		return FALSE;

	cy *= gh->char_height;
	cx = gh->char_width*(pos % gh->cpl);
	
	*x = cx;
	*y = cy;
	
	return TRUE;
}

static void
invalidate_xc (GtkHex *gh)
{
    GtkWidget *widget = gh->xdisp;
    gint cx, cy;

    if (get_xcoords (gh, gh->cursor_pos, &cx, &cy)) {
        if (gh->lower_nibble)
            cx += gh->char_width;

	/* LAR - TEST */
	gtk_widget_queue_draw (widget);
#if 0
        gtk_widget_queue_draw_area (widget,
                                    cx, cy,
                                    gh->char_width + 1,
                                    gh->char_height);
#endif
    }
}

static void
invalidate_ac (GtkHex *gh)
{
    GtkWidget *widget = gh->adisp;
    gint cx, cy;

    if (get_acoords (gh, gh->cursor_pos, &cx, &cy)) {
	/* LAR - TEST */
	gtk_widget_queue_draw (widget);
#if 0
        gtk_widget_queue_draw_area (widget,
                                    cx, cy,
                                    gh->char_width + 1,
                                    gh->char_height);
#endif
    }
}

// FIXME - THE NEXT 2 FUNCTIONS ARE DUPLICITOUS. MERGE INTO ONE.
/*
 * the cursor rendering stuff...
 */
static void
render_ac (GtkHex *gh,
           cairo_t *cr)
{
	GdkRGBA bg_color;
	GdkRGBA fg_color;
	GtkStateFlags state;
	GtkStyleContext *context;
	gint cx, cy;
	static guchar c[2] = "\0\0";
	
	g_return_if_fail (gtk_widget_get_realized (gh->adisp));

	context = gtk_widget_get_style_context (gh->adisp);
	state = gtk_widget_get_state_flags (gh->adisp);


	if(get_acoords(gh, gh->cursor_pos, &cx, &cy)) {
		c[0] = gtk_hex_get_byte(gh, gh->cursor_pos);
		if (! is_displayable (c[0]))
			c[0] = '.';
	} else {
		g_critical("%s: Something has gone wrong. Can't get coordinates!",
				__func__);
		return;
	}

	gtk_style_context_save (context);

	if(gh->active_view == VIEW_ASCII) {
		state |= GTK_STATE_FLAG_SELECTED;
		gtk_style_context_set_state (context, state);

		gtk_render_background (context, cr,
				cx,					// double x,
				cy,					// double y,
				gh->char_width,		// double width,
				gh->char_height - 1);	// double height

	} else {

		gtk_style_context_get_color (context, &fg_color);
		cairo_save (cr);
		cairo_set_source_rgba (cr,
				fg_color.red, fg_color.green, fg_color.blue,
				fg_color.alpha);
		cairo_set_line_width (cr, 1.0);
		cairo_rectangle (cr, cx + 0.5, cy + 0.5, gh->char_width,
				gh->char_height - 1);
		cairo_stroke (cr);
		cairo_restore (cr);
	}
	pango_layout_set_text (gh->alayout, c, 1);
	gtk_render_layout (context, cr,
			/* x: */ cx,
			/* y: */ cy,
			gh->alayout);

	gtk_style_context_restore (context);
}

static void
render_xc (GtkHex *gh,
           cairo_t *cr)
{
	GdkRGBA bg_color;
	GdkRGBA fg_color;
	GtkStateFlags state;
	GtkStyleContext *context;
	gint cx, cy, i;
	static guchar c[2];

	g_return_if_fail (gtk_widget_get_realized (gh->xdisp));

	TEST_DEBUG_FUNCTION_START

	context = gtk_widget_get_style_context (gh->xdisp);
	state = gtk_widget_get_state_flags (gh->xdisp);

	if (get_xcoords (gh, gh->cursor_pos, &cx, &cy)) {
		format_xbyte(gh, gh->cursor_pos, c);
		if (gh->lower_nibble) {
			cx += gh->char_width;
			i = 1;
		} else {
			c[1] = 0;
			i = 0;
		}
	} else {
		g_critical("%s: Something has gone wrong. Can't get coordinates!",
				__func__);
		return;
	}

	gtk_style_context_save (context);

	if(gh->active_view == VIEW_HEX) {

		state |= GTK_STATE_FLAG_SELECTED;
		gtk_style_context_set_state (context, state);

		gtk_render_background (context, cr,
				cx,					// double x,
				cy,					// double y,
				gh->char_width,		// double width,
				gh->char_height - 1);	// double height
	} else {

		gtk_style_context_get_color (context, &fg_color);
		cairo_save (cr);
		cairo_set_source_rgba (cr,
				fg_color.red, fg_color.green, fg_color.blue,
				fg_color.alpha);
		cairo_set_line_width (cr, 1.0);
		cairo_rectangle (cr, cx + 0.5, cy + 0.5, gh->char_width,
				gh->char_height - 1);
		cairo_stroke (cr);
		cairo_restore (cr);
	}
	pango_layout_set_text (gh->xlayout, &c[i], 1);
	gtk_render_layout (context, cr,
			/* x: */ cx,
			/* y: */ cy,
			gh->xlayout);
	gtk_style_context_restore (context);
}

static void
show_cursor(GtkHex *gh) {
	if(!gh->cursor_shown) {
		if (gtk_widget_get_realized (gh->xdisp) || gtk_widget_get_realized (gh->adisp)) {
			invalidate_xc (gh);
			invalidate_ac (gh);
		}
		gh->cursor_shown = TRUE;
	}
}

static void hide_cursor(GtkHex *gh) {
	if(gh->cursor_shown) {
		if (gtk_widget_get_realized (gh->xdisp) || gtk_widget_get_realized (gh->adisp)) {
			invalidate_xc (gh);
			invalidate_ac (gh);
		}
		gh->cursor_shown = FALSE;
	}
}

static void
render_hex_highlights (GtkHex *gh,
                       cairo_t *cr,
                       gint cursor_line)
{
	GtkHex_Highlight *curHighlight = &gh->selection;
	gint xcpl = gh->cpl*2 + gh->cpl/gh->group_type;
	   /* would be nice if we could cache that */

	GtkHex_AutoHighlight *nextList = gh->auto_highlight;
	GtkStateFlags state;
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (gh->xdisp);
	gtk_style_context_save (context);

	state = gtk_widget_get_state_flags (gh->xdisp);
	state |= GTK_STATE_FLAG_SELECTED;
	gtk_style_context_set_state (context, state);

	cairo_save (cr);

	while (curHighlight)
	{
		if (ABS(curHighlight->start - curHighlight->end) >=
				curHighlight->min_select)
		{
			int start, end;
			int sl, el;
			int cursor_off = 0;
			int len;

			gtk_hex_validate_highlight(gh, curHighlight);

			start = MIN(curHighlight->start, curHighlight->end);
			end = MAX(curHighlight->start, curHighlight->end);
			sl = curHighlight->start_line;
			el = curHighlight->end_line;

			if (cursor_line == sl)
			{
				cursor_off = 2*(start%gh->cpl) + (start%gh->cpl)/gh->group_type;
				if (cursor_line == el)
					len = 2*(end%gh->cpl + 1) + (end%gh->cpl)/gh->group_type;
				else
					len = xcpl;

				len = len - cursor_off;

				if (len > 0) {
					gtk_render_background (context, cr,
							cursor_off * gh->char_width,
							cursor_line * gh->char_height,
							len * gh->char_width,
							gh->char_height);
				}
		
			}
			else if (cursor_line == el)
			{
				cursor_off = 2*(end%gh->cpl + 1) + (end%gh->cpl)/gh->group_type;
				if (cursor_off > 0)
					gtk_render_background (context, cr,
					                 0,
					                 cursor_line * gh->char_height,
					                 cursor_off * gh->char_width,
					                 gh->char_height);
			}
			else if (cursor_line > sl && cursor_line < el)
			{
				gtk_render_background (context, cr,
				                 0,
				                 cursor_line * gh->char_height,
				                 xcpl * gh->char_width,
				                 gh->char_height);
			}
		}
		curHighlight = curHighlight->next;
		while (curHighlight == NULL && nextList)
		{
			curHighlight = nextList->highlights;
			nextList = nextList->next;
		}
	}
	cairo_restore (cr);
	gtk_style_context_restore (context);
}

static void
render_ascii_highlights (GtkHex *gh,
                         cairo_t *cr,
                         gint cursor_line)
{
	GtkHex_Highlight *curHighlight = &gh->selection;
	GtkHex_AutoHighlight *nextList = gh->auto_highlight;
	GtkStateFlags state;
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (gh->adisp);
	gtk_style_context_save (context);

	state = gtk_widget_get_state_flags (gh->adisp);
	state |= GTK_STATE_FLAG_SELECTED;

	gtk_style_context_set_state (context, state);

	cairo_save (cr);

	while (curHighlight)
	{
		if (ABS(curHighlight->start - curHighlight->end) >=
				curHighlight->min_select)
		{
			gint start, end;
			gint sl, el;
			gint cursor_off = 0;
			gint len;

			gtk_hex_validate_highlight(gh, curHighlight);

			start = MIN(curHighlight->start, curHighlight->end);
			end = MAX(curHighlight->start, curHighlight->end);
			sl = curHighlight->start_line;
			el = curHighlight->end_line;

			if (cursor_line == sl)
			{
				cursor_off = start % gh->cpl;
				if (cursor_line == el)
					len = end - start + 1;
				else
					len = gh->cpl - cursor_off;
				if (len > 0)
					gtk_render_background (context, cr,
					                 cursor_off * gh->char_width,
					                 cursor_line * gh->char_height,
					                 len * gh->char_width,
					                 gh->char_height);
			}
			else if (cursor_line == el)
			{
				cursor_off = end % gh->cpl + 1;
				if (cursor_off > 0)
					gtk_render_background (context, cr,
					                 0,
					                 cursor_line * gh->char_height,
					                 cursor_off * gh->char_width,
					                 gh->char_height);
			}
			else if (cursor_line > sl && cursor_line < el)
			{
				gtk_render_background (context, cr,
				                 0,
				                 cursor_line * gh->char_height,
				                 gh->cpl * gh->char_width,
				                 gh->char_height);
			}
		}
		curHighlight = curHighlight->next;
		while (curHighlight == NULL && nextList)
		{
			curHighlight = nextList->highlights;
			nextList = nextList->next;
		}
	}
	cairo_restore (cr);
	gtk_style_context_restore (context);
}

/*
 * when calling invalidate_*_lines() the imin and imax arguments are the
 * numbers of the first and last line TO BE INVALIDATED in the range
 * [0 .. gh->vis_lines-1] AND NOT [0 .. gh->lines]!
 */
static void
invalidate_lines (GtkHex *gh,
                  GtkWidget *widget,
                  gint imin,
                  gint imax)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation (widget, &allocation);

    /* LAR - TEST */
    gtk_widget_queue_draw (widget);
#if 0
    gtk_widget_queue_draw_area (widget,
                                0,
                                imin * gh->char_height,
                                allocation.width,
                                (imax - imin + 1) * gh->char_height);
#endif
}

static void
invalidate_hex_lines (GtkHex *gh,
                      gint imin,
                      gint imax)
{
    invalidate_lines (gh, gh->xdisp, imin, imax);
}

static void
invalidate_ascii_lines (GtkHex *gh,
                        gint imin,
                        gint imax)
{
    invalidate_lines (gh, gh->adisp, imin, imax);
}

static void
invalidate_offsets (GtkHex *gh,
                    gint imin,
                    gint imax)
{
    invalidate_lines (gh, gh->offsets, imin, imax);
}

/*
 * when calling render_*_lines() the min_lines and max_lines arguments are the
 * numbers of the first and last line TO BE DISPLAYED in the range
 * [0 .. gh->vis_lines-1] AND NOT [0 .. gh->lines]!
 */
static void
render_hex_lines (GtkHex *gh,
                  cairo_t *cr,
                  int min_lines,
                  int max_lines)
{
	GtkWidget *widget = gh->xdisp;
	GtkAllocation allocation;
	GtkStateFlags state;
	GtkStyleContext *context;
	int cursor_line;
	int xcpl = gh->cpl * 2 + gh->cpl / gh->group_type;
	int frm_len;

	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET(gh)));
	g_return_if_fail (gh->cpl > 0);

	TEST_DEBUG_FUNCTION_START

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);
	cursor_line = gh->cursor_pos / gh->cpl - gh->top_line;
	gtk_widget_get_allocation (widget, &allocation);

	g_debug("%s: WIDTH: %d HEIGHT: %d",
			__func__, allocation.width, allocation.height);

	/* render background. */
	gtk_render_background (context, cr,
			/* x: */		0,
			/* y: */		min_lines * gh->char_height,
			/* width: */	allocation.width,
			/* height: */	(max_lines - min_lines + 1) * gh->char_height);

	max_lines = MIN(max_lines, gh->vis_lines);
	max_lines = MIN(max_lines, gh->lines);

	/* FIXME - I have no idea what this does. It's too early in the morning
	 * and the coffee hasn't kicked in yet. Maybe break this down / comment
	 * it to make it clearer?
	 */
	frm_len = format_xblock (gh, gh->disp_buffer,
			(gh->top_line+min_lines)*gh->cpl,
			MIN((gh->top_line+max_lines+1)*gh->cpl,
				gh->document->file_size) );
	
	for (int i = min_lines; i <= max_lines; i++)
	{
		int tmp = frm_len - ((i - min_lines) * xcpl);

		if(tmp <= 0)
			break;

		render_hex_highlights (gh, cr, i);

		/* Set pango layout to the line of hex to render. */
		// FIXME - make this understandable.
		pango_layout_set_text (gh->xlayout,
				gh->disp_buffer + (i - min_lines) * xcpl,
				MIN(xcpl, tmp));

		gtk_render_layout (context, cr,
				/* x: */ 0,
				/* y: */ i * gh->char_height,
				gh->xlayout);
	}
	
	// TEST
	g_debug("%s: cursor_line: %d - min_lines: %d - max_lines: %d - cursor_shown: %d",
			__func__, cursor_line, min_lines, max_lines, gh->cursor_shown);
	if ( (cursor_line >= min_lines) && (cursor_line <= max_lines) &&
			(gh->cursor_shown) )
	{
		render_xc (gh, cr);
	}
}

static void
render_ascii_lines (GtkHex *gh,
                    cairo_t *cr,
                    int min_lines,
                    int max_lines)
{
	GtkWidget *widget = gh->adisp;
	GtkAllocation allocation;
	GtkStateFlags state;
	GtkStyleContext *context;
	int frm_len;
	guint cursor_line;

	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET(gh)));
	g_return_if_fail (gh->cpl);

	TEST_DEBUG_FUNCTION_START

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);
	cursor_line = gh->cursor_pos / gh->cpl - gh->top_line;
	gtk_widget_get_allocation(widget, &allocation);

	/* render background. */
	gtk_render_background (context, cr,
			/* x: */		0,
			/* y: */		min_lines * gh->char_height,
			/* width: */	allocation.width,
			/* height: */	(max_lines - min_lines + 1) * gh->char_height);
	
	max_lines = MIN(max_lines, gh->vis_lines);
	max_lines = MIN(max_lines, gh->lines);
	
	frm_len = format_ablock (gh, gh->disp_buffer,
			(gh->top_line+min_lines)*gh->cpl,
			MIN((gh->top_line+max_lines+1)*gh->cpl,
				gh->document->file_size));
	
	for (int i = min_lines; i <= max_lines; i++)
	{
		int tmp = frm_len - ((i - min_lines) * gh->cpl);
		if(tmp <= 0)
			break;

		render_ascii_highlights (gh, cr, i);

		pango_layout_set_text (gh->alayout,
				gh->disp_buffer + (i - min_lines) * gh->cpl,
				MIN(gh->cpl, tmp));

		gtk_render_layout (context, cr,
				/* x: */ 0,
				/* y: */ i * gh->char_height,
				gh->alayout);
	}

	if ((cursor_line >= min_lines) &&
			(cursor_line <= max_lines) &&
			(gh->cursor_shown))
	{
		render_ac (gh, cr);
	}
}

static void
render_offsets (GtkHex *gh,
                cairo_t *cr,
                int min_lines,
                int max_lines)
{
	GtkWidget *widget = gh->offsets;
	GdkRGBA fg_color;
	GtkAllocation allocation;
	GtkStateFlags state;
	GtkStyleContext *context;
	/* offset chars (8) + 1 (null terminator) */
	char offstr[9];

	TEST_DEBUG_FUNCTION_START 

	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (gh)));

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);

	gtk_widget_get_allocation(widget, &allocation);

	/* render background. */
	gtk_render_background (context, cr,
			/* x: */		0,
			/* y: */		min_lines * gh->char_height,
			/* width: */	allocation.width,
			/* height: */	(max_lines - min_lines + 1) * gh->char_height);
  
	/* update max_lines and min_lines - FIXME this is from original code -
	 * why?? - test and see. */
	max_lines = MIN(max_lines, gh->vis_lines);
	max_lines = MIN(max_lines, gh->lines - gh->top_line - 1);

	for (int i = min_lines; i <= max_lines; i++) {
		/* generate offset string and place in temporary buffer */
		sprintf(offstr, "%08X",
				(gh->top_line + i) * gh->cpl + gh->starting_offset);

		/* build pango layout for offset line; draw line with gtk. */
		pango_layout_set_text (gh->olayout, offstr, 8);
		gtk_render_layout (context, cr,
				/* x: */ 0,
				/* y: */ i * gh->char_height,
				gh->olayout);
	}
}

/* ascii / hex widget draw helper function */
static void
compute_scrolling (GtkHex *gh, cairo_t *cr)
{
	double dx, dy;

	TEST_DEBUG_FUNCTION_START

	dx = 0;
	dy = (gh->top_line - gtk_adjustment_get_value (gh->adj)) * gh->char_height;

	g_debug("%s: gh->top_line: %d - adj_val: %f - char_height: %d - dy: %f",
			__func__,
			gh->top_line, gtk_adjustment_get_value (gh->adj), gh->char_height, dy);

//	gh->top_line = gtk_adjustment_get_value(gh->adj);

	/* LAR - rewrite. */
#if 0
	gdk_window_scroll (gtk_widget_get_window (gh->xdisp), dx, dy);
	gdk_window_scroll (gtk_widget_get_window (gh->adisp), dx, dy);
	if (gh->offsets)
		gdk_window_scroll (gtk_widget_get_window (gh->offsets), dx, dy);
#endif

	cairo_translate(cr, dx, dy);
}

/* draw_func for the hex drawing area
 */
static void
hex_draw (GtkDrawingArea *drawing_area,
                           cairo_t *cr,
                           int width,
                           int height,
                           gpointer user_data)
{
	GtkHex *gh = GTK_HEX(user_data);
	int xcpl = 0;

	TEST_DEBUG_FUNCTION_START 
	g_return_if_fail(GTK_IS_HEX(gh));

	// TEST
//	compute_scrolling (gh, cr);

	/* Here's the idea here:  the hex drawing widget can expand at will,
	 * and we generate our cpl as a whole and to be passed to the ascii draw
	 * function based on that.
	 *
	 * Thus, we need to do some calculations in this function before drawing
	 * the hex lines and before proceeding to draw the ascii widget.
	 */

	g_debug("%s: width: %d - height: %d",
			__func__, width, height);

	/* Total number of characters that can be displayed per line on the hex
	 * widget (xcpl) is the simplest calculation:
	 */
	xcpl = width / gh->char_width;

	/* Next, extrapolate the number of ASCII characters per line; this is
	 * dependent upon the 'grouping' (ie, hex: 2 characters followed by a
	 * space, a 'word' (2 hex characters together followed by a space, ie,
	 * 4 + space... this is best illustrated with an example.
	 *
	 * given 16 xcpl, 'word' grouping:
	 *
	 * 16 [xcpl] / (2 [fixed value; # characters in a hex pair] * 4
	 * [gh->group_type] + 1 [space]) == 1
	 *
	 * Meaning: 1 full word only can fit on a hex line, in this hypothetical,
	 * then multiply the result by the group type again to get the number of
	 * _characters_ (ascii) that this represents. (4 * 1 = 4 in this case)
	 * Whew!
	 */
	gh->cpl = xcpl / (2 * gh->group_type + 1);
	gh->cpl *= gh->group_type;

	/* pixel width of hex drawing area */
	gh->xdisp_width = xcpl * gh->char_width;

	/* pixel width of ascii drawing area */
	gh->adisp_width = gh->cpl * gh->char_width;

	/* set visible lines */
	gh->vis_lines = height / gh->char_height;

	/* If gh->cpl is not greater than 0, something has gone wrong. */
	g_debug("%s: gh->cpl: %d", __func__, gh->cpl);
	g_return_if_fail (gh->cpl > 0);

	/* Now that we have gh->cpl defined, run this function to bump all
	 * required values:
	 */
	recalc_displays(gh);

	/* Finally, we can do what we wanted to do to begin with: draw our hex
	 * lines!
	 */
	render_hex_lines (gh, cr, 0, gh->vis_lines);
}


static void
ascii_draw (GtkDrawingArea *drawing_area,
                           cairo_t *cr,
                           int width,
                           int height,
                           gpointer user_data)
{
	GtkHex *gh = GTK_HEX(user_data);

	TEST_DEBUG_FUNCTION_START 
	g_return_if_fail(GTK_IS_HEX(gh));

	// TEST
//	compute_scrolling (gh, cr);

	gtk_drawing_area_set_content_width (drawing_area, gh->adisp_width);

	g_debug("%s: width: %d - height: %d",
			__func__, width, height);

	render_ascii_lines (gh, cr, 0, gh->vis_lines);

	/* LAR - TEST - I have no idea what the below is trying to
	 * accomplish. Keeping here for reference. */
#if 0
	GdkRectangle rect;
	gint imin, imax;

	gdk_cairo_get_clip_rectangle (cr, &rect);

	imin = (rect.y) / gh->char_height;
	imax = (rect.y + rect.height) / gh->char_height;
	if ((rect.y + rect.height) % gh->char_height)
		imax++;
	
	imax = MIN(imax, gh->vis_lines);

	render_ascii_lines (gh, cr, imin, imax);
#endif
}

static void
offsets_draw (GtkDrawingArea *drawing_area,
                           cairo_t *cr,
                           int width,
                           int height,
                           gpointer user_data)
{
	GtkHex *gh = GTK_HEX(user_data);

	g_return_if_fail(GTK_IS_HEX(gh));
	TEST_DEBUG_FUNCTION_START 

	// TEST
//	compute_scrolling (gh, cr);

	/* FIXME - MAGIC NUMBER - set the width to 8 + 1 = 9 characters
	 * (this is fixed based on offset length always being 8 chars.)  */
	gtk_drawing_area_set_content_width (drawing_area,
			9 * gh->char_width);

	g_debug("%s: width: %d - height: %d",
			__func__, width, height);

	render_offsets (gh, cr, 0, gh->vis_lines);

	/* LAR - TEST - I have no idea what the below is trying to
	 * accomplish. Keeping for reference. */

#if 0
	GdkRectangle rect;
	gint imin, imax;

	gdk_cairo_get_clip_rectangle (cr, &rect);

	imin = (rect.y) / gh->char_height;
	imax = (rect.y + rect.height) / gh->char_height;
	if ((rect.y + rect.height) % gh->char_height)
		imax++;

	imax = MIN(imax, gh->vis_lines);
	
	render_offsets (gh, cr, imin, imax);
#endif
}

// FIXME - CLEAN UP THIS MESS! Not touching it right now because I don't
// know whether we'll need to 'revive' some variables that aren't really
// used here, like 'old_cpl'
/*
 * this calculates how many bytes we can stuff into one line and how many
 * lines we can display according to the current size of the widget
 */
static void
recalc_displays(GtkHex *gh)
{
	GtkWidget *widget = GTK_WIDGET (gh);
	int old_cpl = gh->cpl;
	gboolean scroll_to_cursor;
	gdouble value;
	int total_cpl, xcpl;
	// TEST
	GtkStyleContext *context;
	GtkBorder padding;
	GtkBorder border;

	// TEST - no longer needed??
#if 0
	context = gtk_widget_get_style_context (widget);

	gtk_style_context_get_padding (context, &padding);
	gtk_style_context_get_border (context, &border);
#endif


	/*
	 * Only change the value of the adjustment to put the cursor on screen
	 * if the cursor is currently within the displayed portion.
	 */
	// FIXME - WTF??
	scroll_to_cursor = (gh->cpl == 0) ||
		((gh->cursor_pos / gh->cpl >= gtk_adjustment_get_value (gh->adj)) &&
		 (gh->cursor_pos / gh->cpl <= gtk_adjustment_get_value (gh->adj) +
			  gh->vis_lines - 1));
	
	// TEST - no longer needed?
#if 0
	gh->xdisp_width = 1;
	gh->adisp_width = 1;
#endif

#if 0
	// API CHANGE
	total_width -= 20 +		// LAR DUMB TEST
	               2 * padding.left + 2 * padding.right + req.width;

//	total_width -= 2*gtk_container_get_border_width(GTK_CONTAINER(gh)) +
//	               2 * padding.left + 2 * padding.right + req.width;

	if(gh->show_offsets)
		total_width -= padding.left + padding.right + 9 * gh->char_width;
#endif

	// TEST - MOVED TO HEX_DRAW
#if 0
	// TEST - DUMB HACK
	total_width = total_width - padding.left - padding.right - 
		border.left - border.right - 100;

	total_cpl = total_width / gh->char_width;

	/* Sanity check. */
	if (total_cpl == 0 || total_width < 0) {
		gh->cpl = gh->lines = gh->vis_lines = 0;
		g_critical("%s: Something has gone wrong; total cpl is 0 or "
				"width is too small.");
		return;
	}
	
	/* calculate how many bytes we can stuff in one line */
	gh->cpl = 0;
	do {
		if (gh->cpl % gh->group_type == 0 && total_cpl < gh->group_type * 3)
			break;
		
		gh->cpl++;        /* just added one more char */
		total_cpl -= 3;   /* 2 for xdisp, 1 for adisp */
		
		if (gh->cpl % gh->group_type == 0)	/* just ended a group */
			total_cpl--;

	} while (total_cpl > 0);

	/* If gh->cpl is not greater than 0, something has gone wrong. */
	g_debug("%s: gh->cpl: %d", __func__, gh->cpl);
	g_return_if_fail (gh->cpl > 0);
#endif

	if (gh->document->file_size == 0)
		gh->lines = 1;
	else {
		gh->lines = gh->document->file_size / gh->cpl;
		if (gh->document->file_size % gh->cpl)
			gh->lines++;
	}

	// TEST - TRYING MOVING TO HEX_DRAW
#if 0
	/* set visible lines */
	gh->vis_lines = height / gh->char_height;
#endif

	// MOVED TO HEX_DRAW
#if 0
	/* display width of ascii drawing area */
	gh->adisp_width = gh->cpl * gh->char_width;
#endif
	/* set number of hex characters per line */
	xcpl = gh->cpl * 2 + (gh->cpl - 1) / gh->group_type;

#if 0
	/* display width of hex drawing area */
	gh->xdisp_width = xcpl * gh->char_width;

	gh->extra_width = total_width - gh->xdisp_width - gh->adisp_width;
	g_debug("%s: TOTAL_WIDTH: %d - GH->ADISP_WIDTH: %d - GH->XDISP_WIDTH: %d - GH->EXTRA_WIDTH: %d",
			__func__,
			total_width, gh->adisp_width, gh->xdisp_width, gh->extra_width);
#endif

	if (gh->disp_buffer)
		g_free (gh->disp_buffer);
	
	gh->disp_buffer = g_malloc ((xcpl + 1) * (gh->vis_lines + 1));

	/* calculate new display position */
	value = MIN (gh->top_line * old_cpl / gh->cpl, gh->lines - gh->vis_lines);
	value = MAX (0, value);

	/* keep cursor on screen if it was on screen before */
	if (scroll_to_cursor &&
	    ((gh->cursor_pos / gh->cpl < value) ||
	     (gh->cursor_pos / gh->cpl > value + gh->vis_lines - 1))) {
		value = MIN (gh->cursor_pos / gh->cpl, gh->lines - gh->vis_lines);
		value = MAX (0, value);
	}

	/* adjust the scrollbar and display position to new values */
	gtk_adjustment_configure (gh->adj,
	                          value,             /* value */
	                          0,                 /* lower */
	                          gh->lines,         /* upper */
	                          1,                 /* step increment */
	                          gh->vis_lines - 1, /* page increment */
	                          gh->vis_lines      /* page size */);

	g_signal_emit_by_name(G_OBJECT(gh->adj), "changed");
	g_signal_emit_by_name(G_OBJECT(gh->adj), "value-changed");
}

/*
 * takes care of xdisp and adisp scrolling
 * connected to value-changed signal of scrollbar's GtkAdjustment
 */
static void display_scrolled(GtkAdjustment *adj, GtkHex *gh) {
	gint dx;
	gint dy;

	TEST_DEBUG_FUNCTION_START

	g_return_if_fail (gtk_widget_is_drawable (gh->xdisp) ||
			gtk_widget_is_drawable (gh->adisp));

	// TEST REMOVAL
//	dx = 0;
//	dy = (gh->top_line - gtk_adjustment_get_value (adj)) * gh->char_height;

	gh->top_line = gtk_adjustment_get_value(adj);

	/* LAR - rewrite. */
#if 0
	gdk_window_scroll (gtk_widget_get_window (gh->xdisp), dx, dy);
	gdk_window_scroll (gtk_widget_get_window (gh->adisp), dx, dy);
	if (gh->offsets)
		gdk_window_scroll (gtk_widget_get_window (gh->offsets), dx, dy);
#endif

	gtk_hex_update_all_auto_highlights(gh, TRUE, TRUE);
	gtk_hex_invalidate_all_highlights(gh);

	// TEST FOR SCROLLING
	gtk_widget_queue_draw (GTK_WIDGET(gh->adisp));
	gtk_widget_queue_draw (GTK_WIDGET(gh->xdisp));
	gtk_widget_queue_draw (GTK_WIDGET(gh->offsets));
}

/*
 * mouse signal handlers (button 1 and motion) for both displays
 */
static gboolean
scroll_timeout_handler(GtkHex *gh)
{
	if (gh->scroll_dir < 0) {
		gtk_hex_set_cursor (gh, MAX(0, (int)(gh->cursor_pos - gh->cpl)));
	}
	else if (gh->scroll_dir > 0) {
		gtk_hex_set_cursor(gh, MIN(gh->document->file_size - 1,
						gh->cursor_pos + gh->cpl));
	}
	return TRUE;
}

/* REWRITE */
static gboolean
scroll_cb (GtkEventControllerScroll *controller,
               double                    dx,
               double                    dy,
               gpointer                  user_data)
{
	GtkHex *gh = GTK_HEX (user_data);
//	GtkWidget *widget = GTK_WIDGET (gh->xdisp);
	guint button;
	double old_value, new_value;

	TEST_DEBUG_FUNCTION_START
	g_return_if_fail (GTK_IS_HEX(gh));
//	g_return_if_fail (GTK_IS_WIDGET(widget));

	g_debug("%s: dx: %f - dy: %f",
			__func__, dx, dy);

	old_value = gtk_adjustment_get_value(gh->adj);
	new_value = old_value + dy;

	gtk_adjustment_set_value(gh->adj, new_value);

		// OLD CODE - useless
#if 0
	gtk_widget_event(gh->scrollbar, (GdkEvent *)event);
#endif

	/* TFM: returns true if scroll event was handled; false otherwise.
	 */
	return TRUE;
}


static void
hex_pressed_cb (GtkGestureClick *gesture,
               int              n_press,
               double           x,
               double           y,
               gpointer         user_data)
{
	GtkHex *gh = GTK_HEX (user_data);
	GtkWidget *widget = GTK_WIDGET (gh->xdisp);
	guint button;

	TEST_DEBUG_FUNCTION_START 
	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	button = gtk_gesture_single_get_current_button
		(GTK_GESTURE_SINGLE(gesture));

	g_debug("%s: n_press: %d, x: %f - y: %f",
			__func__, n_press, x, y);

	/* Single-press */
	if (button == GDK_BUTTON_PRIMARY)
	{
		if (! gtk_widget_has_focus (widget)) {
			gtk_widget_grab_focus (GTK_WIDGET(gh));
		}
		
		gh->button = button;
		
		if (gh->active_view == VIEW_HEX) {
			hex_to_pointer(gh, x, y);

			if (! gh->selecting) {
				gh->selecting = TRUE;
				gtk_hex_set_selection(gh, gh->cursor_pos, gh->cursor_pos);
			}
		} else {
			hide_cursor(gh);
			gh->active_view = VIEW_HEX;
			show_cursor(gh);
			hex_pressed_cb (gesture, n_press, x, y, user_data);
		}
	}
	/* Middle-click press. */
	else if (button == GDK_BUTTON_MIDDLE)
	{
		g_debug("%s: MIDDLE CLICK - NOT IMPLEMENTED.");
#if 0
		GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));
		gchar *text;

		gh->active_view = VIEW_HEX;
		hex_to_pointer(gh, event->x, event->y);

		text = gtk_clipboard_wait_for_text(klass->primary);
		if (text) {
			hex_document_set_data(gh->document, gh->cursor_pos,
								  strlen(text), 0, text, TRUE);
			gtk_hex_set_cursor(gh, gh->cursor_pos + strlen(text));
			g_free(text);
		}
#endif
		gh->button = 0;
	}
	else
	{
		gh->button = 0;
	}
}

static void
hex_released_cb (GtkGestureClick *gesture,
               int              n_press,
               double           x,
               double           y,
               gpointer         user_data)
{
	GtkHex *gh = GTK_HEX (user_data);
	GtkWidget *widget = GTK_WIDGET (gh->xdisp);
	guint button;

	TEST_DEBUG_FUNCTION_START 
	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	button =
		gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE(gesture));

	/* Single-click */
	if (button == GDK_BUTTON_PRIMARY && n_press == 1)
	{
		// TEST - OLD CODE
		if (gh->scroll_timeout != -1) {
			g_source_remove(gh->scroll_timeout);
			gh->scroll_timeout = -1;
			gh->scroll_dir = 0;
		}
		gh->selecting = FALSE;
		gh->button = 0;
	}
}

static void
hex_drag_begin_cb (GtkGestureDrag *gesture,
               double          start_x,
               double          start_y,
               gpointer        user_data)
{
	GtkHex *gh = GTK_HEX (user_data);
	GtkWidget *widget = GTK_WIDGET (gh->xdisp);
	guint button;

	TEST_DEBUG_FUNCTION_START 
	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	g_debug("%s: offset_x: %f - offset_y: %f",
			__func__, start_x, start_y);

}

static void
hex_drag_end_cb (GtkGestureDrag *gesture,
               double          offset_x,
               double          offset_y,
               gpointer        user_data)
{
	GtkHex *gh = GTK_HEX (user_data);
	GtkWidget *widget = GTK_WIDGET (gh->xdisp);
	guint button;

	TEST_DEBUG_FUNCTION_START 
	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	g_debug("%s: offset_x: %f - offset_y: %f",
			__func__, offset_x, offset_y);
}

static void
hex_drag_update_cb (GtkGestureDrag *gesture,
               double          offset_x,
               double          offset_y,
               gpointer        user_data)
{
	GtkHex *gh = GTK_HEX (user_data);
	GtkWidget *widget = GTK_WIDGET (gh->xdisp);
	guint button;

	// TEST
	double start_x, start_y;
	double x, y;

	// TEST - FROM OLD CODE:
	GtkAllocation allocation;

	TEST_DEBUG_FUNCTION_START 
	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	g_debug("%s: offset_x: %f - offset_y: %f",
			__func__, offset_x, offset_y);

	gtk_widget_get_allocation(widget, &allocation);

	g_debug("%s: allocation: x: %d - y: %d - width: %d - height: %d",
			__func__,
			allocation.x, allocation.y, allocation.width, allocation.height);

	gtk_gesture_drag_get_start_point(gesture, &start_x, &start_y);

	x = start_x + offset_x;
	y = start_y + offset_y;

	g_debug("%s: x: %f - y: %f",
			__func__, x, y);

	if (y < 0) {
		gh->scroll_dir = -1;
	} else if (y >= allocation.height) {
		gh->scroll_dir = 1;
	} else {
		gh->scroll_dir = 0;
	}

	if (gh->scroll_dir != 0) {
		if (gh->scroll_timeout == -1) {
			gh->scroll_timeout =
				g_timeout_add(SCROLL_TIMEOUT,
							  G_SOURCE_FUNC(scroll_timeout_handler),
							  gh);
		}
		// FIXME - don't really like this - seems like it's setting up for
		// silent failure. Maybe put a debugging msg once understand better.
		return;
	}
	else {
		if (gh->scroll_timeout != -1) {
			g_source_remove(gh->scroll_timeout);
			gh->scroll_timeout = -1;
		}
	}
			
	if (gh->active_view == VIEW_HEX && gh->button == GDK_BUTTON_PRIMARY) {
		hex_to_pointer(gh, x, y);
	}
}

/* ASCII Widget - click and drag callbacks. */

static void
ascii_pressed_cb (GtkGestureClick *gesture,
               int              n_press,
               double           x,
               double           y,
               gpointer         user_data)
{
	GtkHex *gh = GTK_HEX (user_data);
	GtkWidget *widget = GTK_WIDGET (gh->adisp);
	guint button;

	TEST_DEBUG_FUNCTION_START 
	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	button = gtk_gesture_single_get_current_button
		(GTK_GESTURE_SINGLE(gesture));

	g_debug("%s: n_press: %d, x: %f - y: %f",
			__func__, n_press, x, y);

	/* Single-press */
	if (button == GDK_BUTTON_PRIMARY)
	{
		if (! gtk_widget_has_focus (widget)) {
			gtk_widget_grab_focus (GTK_WIDGET(gh));
		}
		
		gh->button = button;
		
		if (gh->active_view == VIEW_ASCII) {
			ascii_to_pointer(gh, x, y);

			if (! gh->selecting) {
				gh->selecting = TRUE;
				gtk_hex_set_selection(gh, gh->cursor_pos, gh->cursor_pos);
			}
		} else {
			hide_cursor(gh);
			gh->active_view = VIEW_ASCII;
			show_cursor(gh);
			ascii_pressed_cb (gesture, n_press, x, y, user_data);
		}
	}
	/* Middle-click press. */
	else if (button == GDK_BUTTON_MIDDLE)
	{
		g_debug("%s: MIDDLE CLICK - NOT IMPLEMENTED.");
#if 0
		GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));
		gchar *text;

		gh->active_view = VIEW_HEX;
		hex_to_pointer(gh, event->x, event->y);

		text = gtk_clipboard_wait_for_text(klass->primary);
		if (text) {
			hex_document_set_data(gh->document, gh->cursor_pos,
								  strlen(text), 0, text, TRUE);
			gtk_hex_set_cursor(gh, gh->cursor_pos + strlen(text));
			g_free(text);
		}
#endif
		gh->button = 0;
	}
	else
	{
		gh->button = 0;
	}
}

static void
ascii_released_cb (GtkGestureClick *gesture,
               int              n_press,
               double           x,
               double           y,
               gpointer         user_data)
{
	GtkHex *gh = GTK_HEX (user_data);
	GtkWidget *widget = GTK_WIDGET (gh->adisp);
	guint button;

	TEST_DEBUG_FUNCTION_START 
	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	button =
		gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE(gesture));

	/* Single-click */
	if (button == GDK_BUTTON_PRIMARY && n_press == 1)
	{
		// TEST - OLD CODE
		if (gh->scroll_timeout != -1) {
			g_source_remove(gh->scroll_timeout);
			gh->scroll_timeout = -1;
			gh->scroll_dir = 0;
		}
		gh->selecting = FALSE;
		gh->button = 0;
	}


}

static void
ascii_drag_update_cb (GtkGestureDrag *gesture,
               double          offset_x,
               double          offset_y,
               gpointer        user_data)
{
	GtkHex *gh = GTK_HEX (user_data);
	GtkWidget *widget = GTK_WIDGET (gh->adisp);
	guint button;

	// TEST
	double start_x, start_y;
	double x, y;

	// TEST - FROM OLD CODE:
	GtkAllocation allocation;

	TEST_DEBUG_FUNCTION_START 
	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	g_debug("%s: offset_x: %f - offset_y: %f",
			__func__, offset_x, offset_y);

	gtk_widget_get_allocation(widget, &allocation);

	g_debug("%s: allocation: x: %d - y: %d - width: %d - height: %d",
			__func__,
			allocation.x, allocation.y, allocation.width, allocation.height);

	gtk_gesture_drag_get_start_point(gesture, &start_x, &start_y);

	x = start_x + offset_x;
	y = start_y + offset_y;

	g_debug("%s: x: %f - y: %f",
			__func__, x, y);

	if (y < 0) {
		gh->scroll_dir = -1;
	} else if (y >= allocation.height) {
		gh->scroll_dir = 1;
	} else {
		gh->scroll_dir = 0;
	}

	if (gh->scroll_dir != 0) {
		if (gh->scroll_timeout == -1) {
			gh->scroll_timeout =
				g_timeout_add(SCROLL_TIMEOUT,
							  G_SOURCE_FUNC(scroll_timeout_handler),
							  gh);
		}
		// FIXME - don't really like this - seems like it's setting up for
		// silent failure. Maybe put a debugging msg once understand better.
		return;
	}
	else {
		if (gh->scroll_timeout != -1) {
			g_source_remove(gh->scroll_timeout);
			gh->scroll_timeout = -1;
		}
	}

	if (gh->active_view == VIEW_ASCII && gh->button == GDK_BUTTON_PRIMARY) {
		ascii_to_pointer(gh, x, y);
	}
}


static gboolean
key_press_cb (GtkEventControllerKey *controller,
               guint                  keyval,
               guint                  keycode,
               GdkModifierType        state,
               gpointer               user_data)
{
	GtkHex *gh = GTK_HEX(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	gboolean ret = TRUE;

	TEST_DEBUG_FUNCTION_START

	hide_cursor(gh);

	if (! state & GDK_SHIFT_MASK) {
		gh->selecting = FALSE;
	}
	else {
		gh->selecting = TRUE;
	}

	switch(keyval) {
		// TEST
	case GDK_KEY_F12:
		g_debug("F12 PRESSED - TESTING CLIPBOARD COPY");
		gtk_hex_copy_to_clipboard (gh);
		break;
	case GDK_KEY_BackSpace:
		if(gh->cursor_pos > 0) {
			hex_document_set_data(gh->document, gh->cursor_pos - 1,
								  0, 1, NULL, TRUE);
			if (gh->selecting)
				gh->selecting = FALSE;
			gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
		}
		break;
	case GDK_KEY_Tab:
	case GDK_KEY_KP_Tab:
		if (gh->active_view == VIEW_ASCII) {
			gh->active_view = VIEW_HEX;
		}
		else {
			gh->active_view = VIEW_ASCII;
		}
		break;
	case GDK_KEY_Delete:
		if(gh->cursor_pos < gh->document->file_size) {
			hex_document_set_data(gh->document, gh->cursor_pos,
								  0, 1, NULL, TRUE);
			gtk_hex_set_cursor(gh, gh->cursor_pos);
		}
		break;
	case GDK_KEY_Up:
		gtk_hex_set_cursor(gh, gh->cursor_pos - gh->cpl);
		break;
	case GDK_KEY_Down:
		gtk_hex_set_cursor(gh, gh->cursor_pos + gh->cpl);
		break;
	case GDK_KEY_Page_Up:
		gtk_hex_set_cursor(gh, MAX(0, (gint)gh->cursor_pos - gh->vis_lines*gh->cpl));
		break;
	case GDK_KEY_Page_Down:
		gtk_hex_set_cursor(gh, MIN((gint)gh->document->file_size, (gint)gh->cursor_pos + gh->vis_lines*gh->cpl));
		break;
	default:
		if (state & GDK_ALT_MASK) {
			show_cursor(gh);
			return FALSE;
		}
		if(gh->active_view == VIEW_HEX)
			switch(keyval) {
			case GDK_KEY_Left:
				if(!(state & GDK_SHIFT_MASK)) {
					gh->lower_nibble = !gh->lower_nibble;
					if(gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
				}
				else {
					gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
				}
				break;
			case GDK_KEY_Right:
				if(gh->cursor_pos >= gh->document->file_size)
					break;
				if(!(state & GDK_SHIFT_MASK)) {
					gh->lower_nibble = !gh->lower_nibble;
					if(!gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else {
					gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				break;
			default:
				if((keyval >= '0')&&(keyval <= '9')) {
					hex_document_set_nibble(gh->document, keyval - '0',
											gh->cursor_pos, gh->lower_nibble,
											gh->insert, TRUE);
					if (gh->selecting)
						gh->selecting = FALSE;
					gh->lower_nibble = !gh->lower_nibble;
					if(!gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else if((keyval >= 'A')&&(keyval <= 'F')) {
					hex_document_set_nibble(gh->document, keyval - 'A' + 10,
											gh->cursor_pos, gh->lower_nibble,
											gh->insert, TRUE);
					if (gh->selecting)
						gh->selecting = FALSE;
					gh->lower_nibble = !gh->lower_nibble;
					if(!gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else if((keyval >= 'a')&&(keyval <= 'f')) {
					hex_document_set_nibble(gh->document, keyval - 'a' + 10,
											gh->cursor_pos, gh->lower_nibble,
											gh->insert, TRUE);
					if (gh->selecting)
						gh->selecting = FALSE;
					gh->lower_nibble = !gh->lower_nibble;
					if(!gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else if((keyval >= GDK_KEY_KP_0)&&(keyval <= GDK_KEY_KP_9)) {
					hex_document_set_nibble(gh->document, keyval - GDK_KEY_KP_0,
											gh->cursor_pos, gh->lower_nibble,
											gh->insert, TRUE);
					if (gh->selecting)
						gh->selecting = FALSE;
					gh->lower_nibble = !gh->lower_nibble;
					if(!gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else
					ret = FALSE;

				break;      
			}
		else if(gh->active_view == VIEW_ASCII)
			switch(keyval) {
			case GDK_KEY_Left:
				gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
				break;
			case GDK_KEY_Right:
				gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				break;
			default:
				if(is_displayable(keyval)) {
					hex_document_set_byte(gh->document, keyval,
										  gh->cursor_pos, gh->insert, TRUE);
					if (gh->selecting)
						gh->selecting = FALSE;
					gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else if((keyval >= GDK_KEY_KP_0)&&(keyval <= GDK_KEY_KP_9)) {
					hex_document_set_byte(gh->document, keyval - GDK_KEY_KP_0 + '0',
											gh->cursor_pos, gh->insert, TRUE);
					if (gh->selecting)
						gh->selecting = FALSE;
					gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else
					ret = FALSE;

				break;
			}
		break;
	}

	show_cursor(gh);
	
	return ret;
}


static void
show_offsets_widget(GtkHex *gh)
{
	g_return_if_fail (GTK_IS_WIDGET (gh->offsets));

	TEST_DEBUG_FUNCTION_START 

	gtk_widget_show (gh->offsets);
}

static void
hide_offsets_widget(GtkHex *gh)
{
	g_return_if_fail (gtk_widget_get_realized (gh->offsets));

	TEST_DEBUG_FUNCTION_START 

	gtk_widget_hide (gh->offsets);
}

// FIXME - REORGANIZE!
/*
 * default data_changed signal handler
 */
static void gtk_hex_real_data_changed(GtkHex *gh, gpointer data) {
	HexChangeData *change_data = (HexChangeData *)data;
	gint start_line, end_line;
	guint lines;

	TEST_DEBUG_FUNCTION_START 

	if(gh->cpl == 0)
		return;

	if(change_data->start - change_data->end + 1 != change_data->rep_len) {
		lines = gh->document->file_size / gh->cpl;
		if(gh->document->file_size % gh->cpl)
			lines++;
		if(lines != gh->lines) {
			gh->lines = lines;
			gtk_adjustment_set_value(gh->adj, MIN(gtk_adjustment_get_value(gh->adj), gh->lines - gh->vis_lines));
			gtk_adjustment_set_value(gh->adj, MAX(0, gtk_adjustment_get_value(gh->adj)));
			if((gh->cursor_pos/gh->cpl < gtk_adjustment_get_value(gh->adj)) ||
			   (gh->cursor_pos/gh->cpl > gtk_adjustment_get_value(gh->adj) + gh->vis_lines - 1)) {
				gtk_adjustment_set_value(gh->adj, MIN(gh->cursor_pos/gh->cpl, gh->lines - gh->vis_lines));
				gtk_adjustment_set_value(gh->adj, MAX(0, gtk_adjustment_get_value(gh->adj)));
			}
			gtk_adjustment_set_lower(gh->adj, 0);
			gtk_adjustment_set_upper(gh->adj, gh->lines);
			gtk_adjustment_set_step_increment(gh->adj, 1);
			gtk_adjustment_set_page_increment(gh->adj, gh->vis_lines - 1);
			gtk_adjustment_set_page_size(gh->adj, gh->vis_lines);
			g_signal_emit_by_name(G_OBJECT(gh->adj), "changed");
			g_signal_emit_by_name(G_OBJECT(gh->adj), "value-changed");
		}
	}

	start_line = change_data->start/gh->cpl - gh->top_line;
	end_line = change_data->end/gh->cpl - gh->top_line;

	if(end_line < 0 ||
	   start_line > gh->vis_lines)
		return;

	start_line = MAX(start_line, 0);
	if(change_data->rep_len - change_data->end + change_data->start - 1 != 0)
		end_line = gh->vis_lines;
	else
		end_line = MIN(end_line, gh->vis_lines);

    invalidate_hex_lines (gh, start_line, end_line);
    invalidate_ascii_lines (gh, start_line, end_line);
    if (gh->show_offsets)
    {
        invalidate_offsets (gh, start_line, end_line);
    }
}

static void
bytes_changed(GtkHex *gh, int start, int end)
{
	int start_line;
	int end_line;

	g_return_if_fail(gh->cpl);	/* check for divide-by-zero issues */

	start_line = start/gh->cpl - gh->top_line;
	start_line = MAX(start_line, 0);

	end_line = end/gh->cpl - gh->top_line;

	g_return_if_fail(end_line >=0 && start_line <= gh->vis_lines);

    invalidate_hex_lines (gh, start_line, end_line);
    invalidate_ascii_lines (gh, start_line, end_line);

    if (gh->show_offsets)
    {
        invalidate_offsets (gh, start_line, end_line);
    }
}

// LAR - REWRITE - COPY/PASTE SHIT
#if 0
static void primary_get_cb(GtkClipboard *clipboard,
		GtkSelectionData *data, guint info,
		gpointer user_data)
{
	GtkHex *gh = GTK_HEX(user_data);

	if (gh->selection.start != gh->selection.end)
	{
		gint start_pos; 
		gint end_pos;
		guchar *text;

		start_pos = MIN(gh->selection.start, gh->selection.end);
		end_pos = MAX(gh->selection.start, gh->selection.end);
 
		text = hex_document_get_data(gh->document, start_pos,
				end_pos - start_pos);
		gtk_selection_data_set_text(data, text, end_pos - start_pos);
		g_free(text);
	}
}


static void primary_clear_cb(GtkClipboard *clipboard,
		gpointer user_data_or_owner) {
}
#endif

void
gtk_hex_set_selection(GtkHex *gh, gint start, gint end)
{
	gint length = gh->document->file_size;
	gint oe, os, ne, ns;
//	GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));

	// API CHANGE - SEE CLIPBOARD STUFF. DEFER.
#if 0
	static const GtkTargetEntry targets[] = {
		{ "STRING", 0, TARGET_STRING }
	};
	static const gint n_targets = sizeof(targets) / sizeof(targets[0]);
#endif

	if (end < 0)
		end = length;

	// CLIPBOARD - API CHANGES
//	if (gh->selection.start != gh->selection.end)
//		gtk_clipboard_clear(klass->primary);

	os = MIN(gh->selection.start, gh->selection.end);
	oe = MAX(gh->selection.start, gh->selection.end);

	gh->selection.start = CLAMP(start, 0, length);
	gh->selection.end = MIN(end, length);

	gtk_hex_invalidate_highlight(gh, &gh->selection);

	ns = MIN(gh->selection.start, gh->selection.end);
	ne = MAX(gh->selection.start, gh->selection.end);

	if(ns != os && ne != oe) {
		bytes_changed(gh, MIN(ns, os), MAX(ne, oe));
	}
	else if(ne != oe) {
		bytes_changed(gh, MIN(ne, oe), MAX(ne, oe));
	}
	else if(ns != os) {
		bytes_changed(gh, MIN(ns, os), MAX(ns, os));
	}

	// LAR - REWRITE - COPY/PASTE SHIT
#if 0
	if(gh->selection.start != gh->selection.end)
		gtk_clipboard_set_with_data(klass->primary, targets, n_targets,
				primary_get_cb, primary_clear_cb,
				gh);
#endif
}

gboolean gtk_hex_get_selection(GtkHex *gh, gint *start, gint *end)
{
	gint ss, se;

	if(gh->selection.start > gh->selection.end) {
		se = gh->selection.start;
		ss = gh->selection.end;
	}
	else {
		ss = gh->selection.start;
		se = gh->selection.end;
	}

	if(NULL != start)
		*start = ss;
	if(NULL != end)
		*end = se;

	return !(ss == se);
}

void gtk_hex_clear_selection(GtkHex *gh)
{
	gtk_hex_set_selection(gh, 0, 0);
}

void gtk_hex_delete_selection(GtkHex *gh)
{
	guint start;
	guint end;

	start = MIN(gh->selection.start, gh->selection.end);
	end = MAX(gh->selection.start, gh->selection.end);

	gtk_hex_set_selection(gh, 0, 0);

	if(start != end) {
		if(start < gh->cursor_pos)
			gtk_hex_set_cursor(gh, gh->cursor_pos - end + start);
		hex_document_delete_data(gh->document, MIN(start, end), end - start, TRUE);
	}
}

static void gtk_hex_validate_highlight(GtkHex *gh, GtkHex_Highlight *hl)
{
	if (!hl->valid)
	{
		hl->start_line = MIN(hl->start, hl->end) / gh->cpl - gh->top_line;
		hl->end_line = MAX(hl->start, hl->end) / gh->cpl - gh->top_line;
		hl->valid = TRUE;
	}
}

static void gtk_hex_invalidate_highlight(GtkHex *gh, GtkHex_Highlight *hl)
{
	hl->valid = FALSE;
}

static void gtk_hex_invalidate_all_highlights(GtkHex *gh)
{
	GtkHex_Highlight *cur = &gh->selection;
	GtkHex_AutoHighlight *nextList = gh->auto_highlight;

	while (cur)
	{
		gtk_hex_invalidate_highlight(gh, cur);
		cur = cur->next;
		while (cur == NULL && nextList)
		{
			cur = nextList->highlights;
			nextList = nextList->next;
		}
	}
}

static GtkHex_Highlight *gtk_hex_insert_highlight (GtkHex *gh,
		GtkHex_AutoHighlight *ahl,
		gint start, gint end)
{
	GdkRGBA rgba;
	gint length = gh->document->file_size;

	GtkHex_Highlight *new = g_malloc0(sizeof(GtkHex_Highlight));

	new->start = CLAMP(MIN(start, end), 0, length);
	new->end = MIN(MAX(start, end), length);

	new->valid = FALSE;

	new->min_select = 0;

	new->prev = NULL;
	new->next = ahl->highlights;
	if (new->next) new->next->prev = new;
	ahl->highlights = new;

	bytes_changed(gh, new->start, new->end);

	return new;
}

static void gtk_hex_delete_highlight (GtkHex *gh, GtkHex_AutoHighlight *ahl,
		GtkHex_Highlight *hl)
{
	int start, end;
	start = hl->start;
	end = hl->end;
	if (hl->prev) hl->prev->next = hl->next;
	if (hl->next) hl->next->prev = hl->prev;

	if (hl == ahl->highlights) ahl->highlights = hl->next;

	g_free(hl);
	bytes_changed(gh, start, end);
}

/* stolen from hex_document_compare_data - but uses
 * gtk_hex_* stuff rather than hex_document_* directly
 * and simply returns a gboolean.
 */
static gboolean gtk_hex_compare_data(GtkHex *gh, guchar *cmp, guint pos, gint len)
{
	int i;
	for (i = 0; i < len; i++)
	{
		guchar c = gtk_hex_get_byte(gh, pos + i);
		if (c != *(cmp + i))
			return FALSE;
	}
	return TRUE;
}

static gboolean gtk_hex_find_limited(GtkHex *gh, gchar *find, int findlen,
		guint lower, guint upper,
		guint *found)
{
	guint pos = lower;
	while (pos < upper)
	{
		if (gtk_hex_compare_data(gh, (guchar *)find, pos, findlen))
		{
			*found = pos;
			return TRUE;
		}
		pos++;
	}
	return FALSE;
}

/* removes any highlights that arn't visible
 * adds any new highlights that became visible
 */
static void gtk_hex_update_auto_highlight(GtkHex *gh, GtkHex_AutoHighlight *ahl,
		gboolean delete, gboolean add)
{
	gint del_min, del_max;
	gint add_min, add_max;
	guint foundpos = -1;
	gint prev_min = ahl->view_min;
	gint prev_max = ahl->view_max;
	GtkHex_Highlight *cur;

	ahl->view_min = gh->top_line * gh->cpl;
	ahl->view_max = (gh->top_line + gh->vis_lines) * gh->cpl;

	if (prev_min < ahl->view_min && prev_max < ahl->view_max)
	{
		del_min = prev_min - ahl->search_len;
		del_max = ahl->view_min - ahl->search_len;
		add_min = prev_max;
		add_max = ahl->view_max;
	}
	else if (prev_min > ahl->view_min && prev_max > ahl->view_max)
	{
		add_min = ahl->view_min - ahl->search_len;
		add_max = prev_min - ahl->search_len;
		del_min = ahl->view_max;
		del_max = prev_max;
	}
	else /* just refresh the lot */
	{
		del_min = 0;
		del_max = gh->cpl * gh->lines;
		add_min = ahl->view_min;
		add_max = ahl->view_max;
	}

	add_min = MAX(add_min, 0);
	del_min = MAX(del_min, 0);

	cur = ahl->highlights;
	while (delete && cur)
	{
		if (cur->start >= del_min && cur->start <= del_max)
		{
			GtkHex_Highlight *next = cur->next;
			gtk_hex_delete_highlight(gh, ahl, cur);
			cur = next;
		}
		else cur = cur->next;
	}
	while (add &&
		   gtk_hex_find_limited(gh, ahl->search_string, ahl->search_len,
								MAX(add_min, foundpos+1), add_max, &foundpos))
	{
		gtk_hex_insert_highlight(gh, ahl, foundpos, foundpos+(ahl->search_len)-1);
	}
}

static void gtk_hex_update_all_auto_highlights(GtkHex *gh, gboolean delete, gboolean add)
{
	GtkHex_AutoHighlight *cur = gh->auto_highlight;

	while (cur)
	{
		gtk_hex_update_auto_highlight(gh, cur, delete, add);
		cur = cur->next;
	}
}

void gtk_hex_copy_to_clipboard(GtkHex *gh)
{
	g_signal_emit_by_name(G_OBJECT(gh), "copy-clipboard");
}

void gtk_hex_cut_to_clipboard(GtkHex *gh)
{
	g_signal_emit_by_name(G_OBJECT(gh), "cut-clipboard");
}

void gtk_hex_paste_from_clipboard(GtkHex *gh)
{
	g_signal_emit_by_name(G_OBJECT(gh), "paste-clipboard");
}

static void
gtk_hex_real_copy_to_clipboard (GtkHex *gh)
{
	GtkWidget *widget = GTK_WIDGET(gh);
	GdkClipboard *clipboard;
	int start_pos, end_pos;

	TEST_DEBUG_FUNCTION_START

	start_pos = MIN(gh->selection.start, gh->selection.end);
	end_pos = MAX(gh->selection.start, gh->selection.end);

	clipboard = gtk_widget_get_clipboard (widget);

	if(start_pos != end_pos) {
		char *text;
		char *cp;
		
		text = hex_document_get_data(gh->document,
				start_pos,
				end_pos - start_pos);

		// TEST

		for (cp = text; *cp != '\0'; ++cp)
		{
			if (! is_displayable(*cp))
				*cp = '?';
		}

		printf("%s\n", text);

		gdk_clipboard_set_text (clipboard, text);

		g_free(text);
	}

#if 0
	gint start_pos; 
	gint end_pos;
	GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));

	start_pos = MIN(gh->selection.start, gh->selection.end);
	end_pos = MAX(gh->selection.start, gh->selection.end);
 
	if(start_pos != end_pos) {
		guchar *text = hex_document_get_data(gh->document, start_pos,
				end_pos - start_pos);
		gtk_clipboard_set_text(klass->clipboard, text, end_pos - start_pos);
		g_free(text);
	}
#endif
}

static void gtk_hex_real_cut_to_clipboard(GtkHex *gh,
		gpointer user_data)
{
	if(gh->selection.start != -1 && gh->selection.end != -1) {
		gtk_hex_real_copy_to_clipboard(gh);
		gtk_hex_delete_selection(gh);
	}
}

static void gtk_hex_real_paste_from_clipboard(GtkHex *gh,
		gpointer user_data)
{
	g_debug("%s: NOT IMPLEMENTED", __func__);

	// API CHANGES - clipboard stuff
#if 0
	GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));
	gchar *text;

	text = gtk_clipboard_wait_for_text(klass->clipboard);
	if(text) {
		hex_document_set_data(gh->document, gh->cursor_pos,
							  strlen(text), 0, text, TRUE);
		gtk_hex_set_cursor(gh, gh->cursor_pos + strlen(text));
		g_free(text);
	}
#endif
}

static void gtk_hex_finalize(GObject *gobject) {
	GtkHex *gh = GTK_HEX(gobject);
	
	if (gh->disp_buffer)
		g_free (gh->disp_buffer);

	if (gh->xlayout)
		g_object_unref (G_OBJECT (gh->xlayout));
	
	if (gh->alayout)
		g_object_unref (G_OBJECT (gh->alayout));
	
	if (gh->olayout)
		g_object_unref (G_OBJECT (gh->olayout));
	
	/* Boilerplate; keep here. Chain up to the parent class.
	 */
	G_OBJECT_CLASS(gtk_hex_parent_class)->finalize(gobject);
}



// REWRITE FOR GESTURES/EVENT CONTROLLERS
#if 0
static gboolean gtk_hex_key_press(GtkWidget *w, GdkEventKey *event) {
	GtkHex *gh = GTK_HEX(w);
	gint ret = TRUE;

	hide_cursor(gh);

	if(!(event->state & GDK_SHIFT_MASK)) {
		gh->selecting = FALSE;
	}
	else {
		gh->selecting = TRUE;
	}
	switch(event->keyval) {
	case GDK_KEY_BackSpace:
		if(gh->cursor_pos > 0) {
			hex_document_set_data(gh->document, gh->cursor_pos - 1,
								  0, 1, NULL, TRUE);
			if (gh->selecting)
				gh->selecting = FALSE;
			gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
		}
		break;
	case GDK_KEY_Tab:
	case GDK_KEY_KP_Tab:
		if (gh->active_view == VIEW_ASCII) {
			gh->active_view = VIEW_HEX;
		}
		else {
			gh->active_view = VIEW_ASCII;
		}
		break;
	case GDK_KEY_Delete:
		if(gh->cursor_pos < gh->document->file_size) {
			hex_document_set_data(gh->document, gh->cursor_pos,
								  0, 1, NULL, TRUE);
			gtk_hex_set_cursor(gh, gh->cursor_pos);
		}
		break;
	case GDK_KEY_Up:
		gtk_hex_set_cursor(gh, gh->cursor_pos - gh->cpl);
		break;
	case GDK_KEY_Down:
		gtk_hex_set_cursor(gh, gh->cursor_pos + gh->cpl);
		break;
	case GDK_KEY_Page_Up:
		gtk_hex_set_cursor(gh, MAX(0, (gint)gh->cursor_pos - gh->vis_lines*gh->cpl));
		break;
	case GDK_KEY_Page_Down:
		gtk_hex_set_cursor(gh, MIN((gint)gh->document->file_size, (gint)gh->cursor_pos + gh->vis_lines*gh->cpl));
		break;
	default:
		if (event->state & GDK_MOD1_MASK) {
			show_cursor(gh);
			return FALSE;
		}
		if(gh->active_view == VIEW_HEX)
			switch(event->keyval) {
			case GDK_KEY_Left:
				if(!(event->state & GDK_SHIFT_MASK)) {
					gh->lower_nibble = !gh->lower_nibble;
					if(gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
				}
				else {
					gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
				}
				break;
			case GDK_KEY_Right:
				if(gh->cursor_pos >= gh->document->file_size)
					break;
				if(!(event->state & GDK_SHIFT_MASK)) {
					gh->lower_nibble = !gh->lower_nibble;
					if(!gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else {
					gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				break;
			default:
				if(event->length != 1)
					ret = FALSE;
				else if((event->keyval >= '0')&&(event->keyval <= '9')) {
					hex_document_set_nibble(gh->document, event->keyval - '0',
											gh->cursor_pos, gh->lower_nibble,
											gh->insert, TRUE);
					if (gh->selecting)
						gh->selecting = FALSE;
					gh->lower_nibble = !gh->lower_nibble;
					if(!gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else if((event->keyval >= 'A')&&(event->keyval <= 'F')) {
					hex_document_set_nibble(gh->document, event->keyval - 'A' + 10,
											gh->cursor_pos, gh->lower_nibble,
											gh->insert, TRUE);
					if (gh->selecting)
						gh->selecting = FALSE;
					gh->lower_nibble = !gh->lower_nibble;
					if(!gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else if((event->keyval >= 'a')&&(event->keyval <= 'f')) {
					hex_document_set_nibble(gh->document, event->keyval - 'a' + 10,
											gh->cursor_pos, gh->lower_nibble,
											gh->insert, TRUE);
					if (gh->selecting)
						gh->selecting = FALSE;
					gh->lower_nibble = !gh->lower_nibble;
					if(!gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else if((event->keyval >= GDK_KEY_KP_0)&&(event->keyval <= GDK_KEY_KP_9)) {
					hex_document_set_nibble(gh->document, event->keyval - GDK_KEY_KP_0,
											gh->cursor_pos, gh->lower_nibble,
											gh->insert, TRUE);
					if (gh->selecting)
						gh->selecting = FALSE;
					gh->lower_nibble = !gh->lower_nibble;
					if(!gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else
					ret = FALSE;

				break;      
			}
		else if(gh->active_view == VIEW_ASCII)
			switch(event->keyval) {
			case GDK_KEY_Left:
				gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
				break;
			case GDK_KEY_Right:
				gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				break;
			default:
				if(event->length != 1)
					ret = FALSE;
				else if(is_displayable(event->keyval)) {
					hex_document_set_byte(gh->document, event->keyval,
										  gh->cursor_pos, gh->insert, TRUE);
					if (gh->selecting)
						gh->selecting = FALSE;
					gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else if((event->keyval >= GDK_KEY_KP_0)&&(event->keyval <= GDK_KEY_KP_9)) {
					hex_document_set_byte(gh->document, event->keyval - GDK_KEY_KP_0 + '0',
											gh->cursor_pos, gh->insert, TRUE);
					if (gh->selecting)
						gh->selecting = FALSE;
					gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else
					ret = FALSE;

				break;
			}
		break;
	}

	show_cursor(gh);
	
	return ret;
}
#endif

// SWITCH TO GESTURES/EVENT CONTROLLERS
#if 0
static gboolean gtk_hex_key_release(GtkWidget *w, GdkEventKey *event) {
	GtkHex *gh = GTK_HEX(w);

	if (event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R) {
		gh->selecting = FALSE;
	}

	return TRUE;
}

static gboolean gtk_hex_button_release(GtkWidget *w, GdkEventButton *event) {
	GtkHex *gh = GTK_HEX(w);

	if(event->state & GDK_SHIFT_MASK) {
		gh->selecting = FALSE;
	}

	return TRUE;
}
#endif

// COMMENTING OUT FOR NOW BECAUSE NOT USED - GETS CONFUSING 
#if 0
/* 
 * recalculate the width of both displays and reposition and resize all
 * the children widgets and adjust the scrollbar after resizing
 * connects to the size_allocate signal of the GtkHex widget
 */
static void
gtk_hex_size_allocate (GtkWidget *widget,
		int width,
		int height,
		int baseline)
{
	GtkHex *gh = GTK_HEX(widget);
	GtkAllocation my_alloc = { 0 };
	GtkStyleContext *context;
	GtkBorder padding;
	GtkBorder border;

	TEST_DEBUG_FUNCTION_START 

	g_debug("%s: width: %d - height: %d - baseline: %d",
			__func__, width, height, baseline);

	// LAR - TEST BASED ON OLD CODE
	hide_cursor(gh);

	/* recalculate displays - get sizing for gh->{x,a}disb, etc. */
	recalc_displays(gh, width, height);

	/* pull our widget's style context so we can grab the border & padding */
	context = gtk_widget_get_style_context (widget);

	gtk_style_context_get_padding (context, &padding);
	gtk_style_context_get_border (context, &border);

	/* fill up allocation with initial values. */

	my_alloc.x = border.left + padding.left;
	my_alloc.y = border.top + padding.top;
	my_alloc.height = MAX (height - border.top - border.bottom - padding.top -
							padding.bottom,
						1);

	/* First up:  the offsets widget. */

	if (gh->show_offsets)
	{
		my_alloc.width = 9 * gh->char_width;
		
		gtk_widget_size_allocate(gh->offsets, &my_alloc, baseline);

		my_alloc.x += my_alloc.width + padding.left + padding.right + 
			border.left + border.right;
	}

	/* Next up: the hex widget.
	 * Gear up the alloc's width to be the xdisp_width previously calc'd: */
	my_alloc.width = gh->xdisp_width;

	/* .. and place the hex widget accordingly. */
	gtk_widget_size_allocate(gh->xdisp, &my_alloc, baseline);

	/* Next up: the scrollbar. */
	my_alloc.x = width - border.right - padding.right;
	my_alloc.y = border.top;
	my_alloc.width = width;
	my_alloc.height = MAX(height - border.top - border.bottom, 1);
	
	gtk_widget_size_allocate(gh->scrollbar, &my_alloc, baseline);

	/* lastly: ascii widget. */
	my_alloc.x -= gh->adisp_width + padding.left;
	my_alloc.y = border.top + padding.top;
	my_alloc.width = gh->adisp_width;
	my_alloc.height = MAX (height - border.top - border.bottom - padding.top -
							padding.bottom,
						1);

	gtk_widget_size_allocate(gh->adisp, &my_alloc, baseline);

	show_cursor(gh);
}
#endif

static void
gtk_hex_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	GtkHex *gh = GTK_HEX(widget);
	graphene_rect_t rect;
	float width, height;
	cairo_t *cr;
	GtkWidget *child;

	TEST_DEBUG_FUNCTION_START 

	/* Update character width & height */
	gh->char_width = get_char_width(gh);
	gh->char_height = get_char_height(gh);

	/* get width and height, implicitly converted to floats so we can pass
	 * to graphene_rect_init below. */
	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);

	/* set visible lines - do this here and now as we can use the height
	 * of the widget as a whole.  */
	gh->vis_lines = height / gh->char_height;

	/* get a graphene rect so we can pass it to the next function and draw
	 * with cairo, which is all we want to do anyway. */
	graphene_rect_init (&rect,
		/* float x: */	0.0f,
		/* float y: */	0.0f,
			width, height);

	cr = gtk_snapshot_append_cairo (snapshot, &rect);

	g_debug("%s: width: %f - height: %f",
			__func__, width, height);


	// TEST for trying render_xc
	show_cursor(gh);



	/* queue draw functions for drawing areas - order matters here as
	 * we're pegging certain elements of the ascii widget being drawn
	 * to after the hex widget is drawn.
	 */
	gtk_widget_snapshot_child (widget, gh->xdisp, snapshot);
	gtk_widget_snapshot_child (widget, gh->adisp, snapshot);

	/* queue draw functions for other children */
	for (child = gtk_widget_get_first_child (widget);
							/* don't draw these as we already did above. */
			child != NULL && child != gh->xdisp && child != gh->adisp;
			child = gtk_widget_get_next_sibling (child))
	{
		gtk_widget_snapshot_child (widget, child, snapshot);
	}
}

static void gtk_hex_document_changed(HexDocument* doc, gpointer change_data,
        gboolean push_undo, gpointer data)
{
    gtk_hex_real_data_changed (GTK_HEX(data), change_data);
}


static void gtk_hex_size_request(GtkWidget *w, GtkRequisition *req) {
	GtkBorder padding;
	GtkHex *gh = GTK_HEX(w);
	GtkRequisition sb_req;
	GtkStateFlags state;
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (w);
	state = gtk_widget_get_state_flags (w);
	// API CHANGE
//	gtk_style_context_get_padding (context, state, &padding);
	gtk_style_context_get_padding (context, &padding);

	gtk_widget_get_preferred_size (gh->scrollbar, &sb_req, NULL);
	// API CHANGE
//	req->width = 2 * padding.left + 2 * padding.right + 2 * gtk_container_get_border_width (GTK_CONTAINER (w)) +
	req->width = 2 * padding.left + 2 * padding.right + 20 +	/* DUMB TEST */
		sb_req.width + gh->char_width * (gh->default_cpl + (gh->default_cpl - 1) /
										 gh->group_type);
	if(gh->show_offsets)
		req->width += padding.left + padding.right + 9 * gh->char_width;
	req->height = gh->default_lines * gh->char_height + padding.top + padding.bottom +
		// API CHANGE 
//		2*gtk_container_get_border_width(GTK_CONTAINER(w));
		20;		// LAR - DUMB TEST
}

static void
gtk_hex_get_preferred_width (GtkWidget *widget,
                             gint      *minimal_width,
                             gint      *natural_width)
{
    GtkRequisition requisition;

    gtk_hex_size_request (widget, &requisition);

    *minimal_width = *natural_width = requisition.width;
}

static void
gtk_hex_get_preferred_height (GtkWidget *widget,
                              gint      *minimal_height,
                              gint      *natural_height)
{
    GtkRequisition requisition;

    gtk_hex_size_request (widget, &requisition);

    *minimal_height = *natural_height = requisition.height;
}

static void
gtk_hex_class_init(GtkHexClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	/* Layout manager: box-style layout. */
	gtk_widget_class_set_layout_manager_type (widget_class,
			GTK_TYPE_BOX_LAYOUT);

	/* CSS name */

	gtk_widget_class_set_css_name (widget_class, CSS_NAME);

	/* SIGNALS */

	gtkhex_signals[CURSOR_MOVED_SIGNAL] =
		g_signal_new_class_handler ("cursor-moved",
				G_OBJECT_CLASS_TYPE(object_class),
			/* GSignalFlags signal_flags : */
				G_SIGNAL_RUN_FIRST,
			/* GCallback class_handler: */
				NULL,		/* no callback; plain signal. */
			/* no accumulator or accu_data */
				NULL, NULL,
			/* GSignalCMarshaller c_marshaller: */
				NULL,		/* use generic marshaller */
			/* GType return_type: */
				G_TYPE_NONE,
			/* guint n_params: */
				0);

	gtkhex_signals[DATA_CHANGED_SIGNAL] = 
		g_signal_new_class_handler ("data-changed",
				G_OBJECT_CLASS_TYPE(object_class),
				G_SIGNAL_RUN_FIRST,
				G_CALLBACK(gtk_hex_real_data_changed),
				NULL, NULL,
				NULL,
				G_TYPE_NONE,
				0);

	gtkhex_signals[CUT_CLIPBOARD_SIGNAL] =
		g_signal_new_class_handler ("cut-clipboard",
				G_OBJECT_CLASS_TYPE(object_class),
				G_SIGNAL_RUN_FIRST,
				G_CALLBACK(gtk_hex_real_cut_to_clipboard),
				NULL, NULL,
				NULL,
				G_TYPE_NONE,
				0);

	gtkhex_signals[COPY_CLIPBOARD_SIGNAL] = 
		g_signal_new_class_handler ("copy-clipboard",
				G_OBJECT_CLASS_TYPE(object_class),
				G_SIGNAL_RUN_FIRST,
				G_CALLBACK(gtk_hex_real_copy_to_clipboard),
				NULL, NULL,
				NULL,
				G_TYPE_NONE,
				0);

	gtkhex_signals[PASTE_CLIPBOARD_SIGNAL] = 
		g_signal_new_class_handler ("paste-clipboard",
				G_OBJECT_CLASS_TYPE(object_class),
				G_SIGNAL_RUN_FIRST,
				G_CALLBACK(gtk_hex_real_paste_from_clipboard),
				NULL, NULL,
				NULL,
				G_TYPE_NONE,
				0);
	
	// API CHANGES
//	klass->primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
//	klass->clipboard = gtk_clipboard_get(GDK_NONE);

	// LAR - NOTE - GTK4, THIS IS ONLY CALLED IF THE WIDGET DOES *NOT* HAVE
	// A LAYOUT MANAGER
	//
	// POSSIBLE TODO - subclass a GtkLayoutManager and set the `allocate'
	// vfunc.
//	GTK_WIDGET_CLASS(klass)->size_allocate = gtk_hex_size_allocate;

	
	// GONESVILLE WITH GTK4
//	GTK_WIDGET_CLASS(klass)->get_preferred_width = gtk_hex_get_preferred_width;
//	GTK_WIDGET_CLASS(klass)->get_preferred_height = gtk_hex_get_preferred_height;
	// LAR - no can do, gtk4 - just seems to draw a border??
//	LAR - TEST FOR GTK4
	widget_class->snapshot = gtk_hex_snapshot;

//	GTK4 API CHANGES - SWITCH TO GESTURES / EVENT CONTROLLERS
//	GTK_WIDGET_CLASS(klass)->key_press_event = gtk_hex_key_press;
//	GTK_WIDGET_CLASS(klass)->key_release_event = gtk_hex_key_release;
//	GTK_WIDGET_CLASS(klass)->button_release_event = gtk_hex_button_release;

	object_class->finalize = gtk_hex_finalize;
}

static void
gtk_hex_init(GtkHex *gh)
{
	GtkWidget *widget = GTK_WIDGET(gh);

	GtkCssProvider *provider;
	GtkStyleContext *context;

	GtkGesture *gesture;
	GtkEventController *controller;

	gh->disp_buffer = NULL;
	gh->default_cpl = DEFAULT_CPL;
	gh->default_lines = DEFAULT_LINES;

	gh->scroll_timeout = -1;

	gh->document = NULL;
	gh->starting_offset = 0;

	gh->xdisp_width = gh->adisp_width = DEFAULT_DA_SIZE;
	gh->extra_width = 0;
	gh->active_view = VIEW_HEX;
	gh->group_type = GROUP_BYTE;
	gh->lines = gh->vis_lines = gh->top_line = gh->cpl = 0;
	gh->cursor_pos = 0;
	gh->lower_nibble = FALSE;
	gh->cursor_shown = FALSE;
	gh->button = 0;
	gh->insert = FALSE; /* default to overwrite mode */
	gh->selecting = FALSE;

	gh->selection.start = gh->selection.end = 0;
	gh->selection.bg_color = NULL;
	gh->selection.min_select = 1;
	gh->selection.next = gh->selection.prev = NULL;
	gh->selection.valid = FALSE;

	gh->auto_highlight = NULL;

	// GTK4 - TEST - DON'T KNOW IF NECESSARY FOR KEYBOARD STUFF - FIXME
	gtk_widget_set_can_focus (widget, TRUE);
	gtk_widget_set_focusable (widget, TRUE);

	gtk_widget_set_vexpand (widget, TRUE);
	gtk_widget_set_hexpand (widget, TRUE);

	// API CHANGE - FIXME REWRITE.
//	gtk_widget_set_events(GTK_WIDGET(gh), GDK_KEY_PRESS_MASK);

	/* Init CSS */

	/* Set context var to the widget's context at large. */
	context = gtk_widget_get_style_context (widget);

	/* set up a provider so we can feed CSS through C code. */
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
									CSS_NAME " {\n"
									 "   font-family: Monospace;\n"
									 "   font-size: 12pt;\n"
	                                 "   border-style: solid;\n"
	                                 "   border-width: 1px;\n"
	                                 "   padding: 1px;\n"
									 "   border-color: black;\n"
#if 0
									 "}\n"
									 "#asciidisplay {\n"
									 "       border-style: solid;\n"
									 "       border-width: 2px;\n"
									 "       border-color: blue;\n"
#endif
	                                 "}\n", -1);

	/* add the provider to our widget's style context. */
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	/* Initialize Adjustment */
	gh->adj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

	/* Setup offsets widget. */

	gh->offsets = gtk_drawing_area_new();
	gtk_widget_set_parent (gh->offsets, widget);
	gtk_widget_set_halign (gh->offsets, GTK_ALIGN_START);
	gtk_widget_set_hexpand (gh->offsets, FALSE);

	/* Create the pango layout for the widget */
	gh->olayout = gtk_widget_create_pango_layout (gh->offsets, "");

//	gtk_widget_set_events (gh->offsets, GDK_EXPOSURE_MASK);

	gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (gh->offsets),
			offsets_draw,	// GtkDrawingAreaDrawFunc draw_func,
			gh,		// gpointer user_data,
			NULL);		// GDestroyNotify destroy);

	// FIXME - not sure what this buys us. Monitor. - header class?
	context = gtk_widget_get_style_context (GTK_WIDGET (gh->offsets));
	gtk_style_context_add_class (context, "header");


	/* hide it by default. */
	gtk_widget_hide (gh->offsets);




	/* Setup our Hex drawing area. */

	gh->xdisp = gtk_drawing_area_new();
	gtk_widget_set_parent (gh->xdisp, GTK_WIDGET (gh));
	gtk_widget_set_hexpand (gh->xdisp, TRUE);

	/* Create the pango layout for the widget */
	gh->xlayout = gtk_widget_create_pango_layout (gh->xdisp, "");

	// TEST / MONITOR - not sure if needed for keyboard events. - NOT
	// needed for mouse events.
//	gtk_widget_set_can_focus (gh->xdisp, TRUE);

	gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (gh->xdisp),
			hex_draw,	// GtkDrawingAreaDrawFunc draw_func,
			gh,			// gpointer user_data,
			NULL);		// GDestroyNotify destroy);



	// REWRITE - LAR
#if 0

	gtk_widget_set_events (gh->xdisp, GDK_EXPOSURE_MASK |
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			GDK_BUTTON_MOTION_MASK | GDK_SCROLL_MASK);

	g_signal_connect(G_OBJECT(gh->xdisp), "button_press_event",
					 G_CALLBACK(hex_button_cb), gh);
	g_signal_connect(G_OBJECT(gh->xdisp), "button_release_event",
					 G_CALLBACK(hex_button_cb), gh);
	g_signal_connect(G_OBJECT(gh->xdisp), "motion_notify_event",
					 G_CALLBACK(hex_motion_cb), gh);
	g_signal_connect(G_OBJECT(gh->xdisp), "scroll_event",
					 G_CALLBACK(hex_scroll_cb), gh);
#endif

	/* Set context var to hex widget's context... */
	context = gtk_widget_get_style_context (GTK_WIDGET (gh->xdisp));
	/* ... and add view class so we get certain theme colours for free. */
	gtk_style_context_add_class (context, "view");
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	/* Setup our ASCII widget. */

	gh->adisp = gtk_drawing_area_new();
	gtk_widget_set_parent (gh->adisp, GTK_WIDGET (gh));
	gtk_widget_set_halign (gh->adisp, GTK_ALIGN_START);
	gtk_widget_set_hexpand (gh->adisp, FALSE);
	gtk_widget_set_name (gh->adisp, "asciidisplay");
	
	/* Create the pango layout for the widget */
	gh->alayout = gtk_widget_create_pango_layout (gh->adisp, "");

	// LAR - TEST  - dont' know if needed - NOT needed for mouse clicks.
//	gtk_widget_set_can_focus (gh->adisp, TRUE);

	gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (gh->adisp),
			ascii_draw,	/* GtkDrawingAreaDrawFunc draw_func, */
			gh,			/* gpointer user_data, */
			NULL);		/* GDestroyNotify destroy); */

	/* Rinse and repeat as above for ascii widget / context / view. */
	context = gtk_widget_get_style_context (GTK_WIDGET (gh->adisp));
	gtk_style_context_add_class (context, "view");
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	// LAR - REWRITE
#if 0
	gtk_widget_set_events (gh->adisp, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK |
			GDK_SCROLL_MASK);

	g_signal_connect(G_OBJECT(gh->adisp), "button_press_event",
					 G_CALLBACK(ascii_button_cb), gh);
	g_signal_connect(G_OBJECT(gh->adisp), "button_release_event",
					 G_CALLBACK(ascii_button_cb), gh);
	g_signal_connect(G_OBJECT(gh->adisp), "motion_notify_event",
					 G_CALLBACK(ascii_motion_cb), gh);
	g_signal_connect(G_OBJECT(gh->adisp), "scroll_event",
					 G_CALLBACK(ascii_scroll_cb), gh);
#endif

	/* Set a minimum size for hex/ascii drawing areas. */

	gtk_widget_set_size_request (gh->adisp,
			DEFAULT_DA_SIZE, DEFAULT_DA_SIZE);
	gtk_widget_set_size_request (gh->xdisp,
			DEFAULT_DA_SIZE, DEFAULT_DA_SIZE);


	/* Connect gestures to ascii/hex drawing areas.
	 */

	/* Hex widget: */

	/* click gestures */
	gesture = gtk_gesture_click_new ();

	g_signal_connect (gesture, "pressed",
			G_CALLBACK(hex_pressed_cb),
			gh);
	g_signal_connect (gesture, "released",
			G_CALLBACK(hex_released_cb),
			gh);
	gtk_widget_add_controller (gh->xdisp,
			GTK_EVENT_CONTROLLER(gesture));

	/* drag gestures */
	gesture = gtk_gesture_drag_new ();

	g_signal_connect (gesture, "drag-begin",
			G_CALLBACK(hex_drag_begin_cb),
			gh);
	g_signal_connect (gesture, "drag-end",
			G_CALLBACK(hex_drag_end_cb),
			gh);
	g_signal_connect (gesture, "drag-update",
			G_CALLBACK (hex_drag_update_cb),
			gh);
	gtk_widget_add_controller (gh->xdisp,
			GTK_EVENT_CONTROLLER(gesture));

	/* ASCII widget: */

	/* click gestures */
	gesture = gtk_gesture_click_new ();

	g_signal_connect (gesture, "pressed",
			G_CALLBACK(ascii_pressed_cb),
			gh);
	g_signal_connect (gesture, "released",
			G_CALLBACK(ascii_released_cb),
			gh);
	gtk_widget_add_controller (gh->adisp,
			GTK_EVENT_CONTROLLER(gesture));

	/* drag gestures */
	gesture = gtk_gesture_drag_new ();

// TODO IF NEEDED
#if 0
	g_signal_connect (gesture, "drag-begin",
			G_CALLBACK(ascii_drag_begin_cb),
			gh);
	g_signal_connect (gesture, "drag-end",
			G_CALLBACK(ascii_drag_end_cb),
			gh);
#endif

	g_signal_connect (gesture, "drag-update",
			G_CALLBACK(ascii_drag_update_cb),
			gh);
	gtk_widget_add_controller (gh->adisp,
			GTK_EVENT_CONTROLLER(gesture));

	/* Event controller - scrolling */

	controller =
		gtk_event_controller_scroll_new
			(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
			 GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);

	g_signal_connect (controller, "scroll",
			G_CALLBACK(scroll_cb),
			gh);
	gtk_widget_add_controller (widget,
			GTK_EVENT_CONTROLLER(controller));

	/* Event controller - keyboard */
	
	controller = gtk_event_controller_key_new ();

	g_signal_connect(controller, "key-pressed",
			G_CALLBACK(key_press_cb),
			gh);
	gtk_widget_add_controller (widget,
			GTK_EVENT_CONTROLLER(controller));

	/* Setup Scrollbar */

	gh->scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, gh->adj);
	/* keep scrollbar to the right */
	gtk_widget_set_halign (gh->scrollbar, GTK_ALIGN_END);

	gtk_widget_set_parent (gh->scrollbar, GTK_WIDGET (gh));

	g_signal_connect(G_OBJECT(gh->adj), "value-changed",
					 G_CALLBACK(display_scrolled), gh);
}

GtkWidget *gtk_hex_new(HexDocument *owner) {
	GtkHex *gh;

	gh = GTK_HEX (g_object_new (GTK_TYPE_HEX, NULL));
	g_return_val_if_fail (gh != NULL, NULL);

	gh->document = owner;
    g_signal_connect (G_OBJECT (gh->document), "document-changed",
            G_CALLBACK (gtk_hex_document_changed), gh);
	
	return GTK_WIDGET(gh);
}


/*-------- public API starts here --------*/


/*
 * moves cursor to UPPER_NIBBLE or LOWER_NIBBLE of the current byte
 */
void gtk_hex_set_nibble(GtkHex *gh, gint lower_nibble) {
	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	if(gh->selecting) {
		bytes_changed(gh, gh->cursor_pos, gh->cursor_pos);
		gh->lower_nibble = lower_nibble;
	}
	else if(gh->selection.end != gh->selection.start) {
		guint start = MIN(gh->selection.start, gh->selection.end);
		guint end = MAX(gh->selection.start, gh->selection.end);
		gh->selection.end = gh->selection.start = 0;
		bytes_changed(gh, start, end);
		gh->lower_nibble = lower_nibble;
	}
	else {
		hide_cursor(gh);
		gh->lower_nibble = lower_nibble;
		show_cursor(gh);
	}
}

/*
 * moves cursor to byte index
 */
void
gtk_hex_set_cursor(GtkHex *gh, gint index) {
	guint y;
	guint old_pos = gh->cursor_pos;

	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	if((index >= 0) && (index <= gh->document->file_size)) {
		if(!gh->insert && index == gh->document->file_size)
			index--;

		index = MAX(index, 0);

		hide_cursor(gh);
		
		gh->cursor_pos = index;

		if(gh->cpl == 0)
			return;
		
		y = index / gh->cpl;
		if(y >= gh->top_line + gh->vis_lines) {
			gtk_adjustment_set_value(gh->adj, MIN(y - gh->vis_lines + 1, gh->lines - gh->vis_lines));
			gtk_adjustment_set_value(gh->adj, MAX(gtk_adjustment_get_value(gh->adj), 0));
			g_signal_emit_by_name(G_OBJECT(gh->adj), "value-changed");
		}
		else if (y < gh->top_line) {
			gtk_adjustment_set_value(gh->adj, y);
			g_signal_emit_by_name(G_OBJECT(gh->adj), "value-changed");
		}      

		if(index == gh->document->file_size)
			gh->lower_nibble = FALSE;
		
		if(gh->selecting) {
			gtk_hex_set_selection(gh, gh->selection.start, gh->cursor_pos);
			bytes_changed(gh, MIN(gh->cursor_pos, old_pos), MAX(gh->cursor_pos, old_pos));
		}
		else {
			guint start = MIN(gh->selection.start, gh->selection.end);
			guint end = MAX(gh->selection.start, gh->selection.end);
			bytes_changed(gh, start, end);
			gh->selection.end = gh->selection.start = gh->cursor_pos;
		}

		g_signal_emit_by_name(G_OBJECT(gh), "cursor-moved");

		bytes_changed(gh, old_pos, old_pos);
		show_cursor(gh);
	}
}

/*
 * moves cursor to column x in line y (in the whole buffer, not just the currently visible part)
 */
void gtk_hex_set_cursor_xy(GtkHex *gh, gint x, gint y) {
	gint cp;
	guint old_pos = gh->cursor_pos;

	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	cp = y*gh->cpl + x;

	if((y >= 0) && (y < gh->lines) && (x >= 0) &&
	   (x < gh->cpl) && (cp <= gh->document->file_size)) {
		if(!gh->insert && cp == gh->document->file_size)
			cp--;

		cp = MAX(cp, 0);

		hide_cursor(gh);
		
		gh->cursor_pos = cp;
		
		if(y >= gh->top_line + gh->vis_lines) {
			gtk_adjustment_set_value(gh->adj, MIN(y - gh->vis_lines + 1, gh->lines - gh->vis_lines));
			gtk_adjustment_set_value(gh->adj, MAX(0, gtk_adjustment_get_value(gh->adj)));
			g_signal_emit_by_name(G_OBJECT(gh->adj), "value-changed");
		}
		else if (y < gh->top_line) {
			gtk_adjustment_set_value(gh->adj, y);
			g_signal_emit_by_name(G_OBJECT(gh->adj), "value-changed");
		}      
		
		g_signal_emit_by_name(G_OBJECT(gh), "cursor-moved");
		
		if(gh->selecting) {
			gtk_hex_set_selection(gh, gh->selection.start, gh->cursor_pos);
			bytes_changed(gh, MIN(gh->cursor_pos, old_pos), MAX(gh->cursor_pos, old_pos));
		}
		else if(gh->selection.end != gh->selection.start) {
			guint start = MIN(gh->selection.start, gh->selection.end);
			guint end = MAX(gh->selection.start, gh->selection.end);
			gh->selection.end = gh->selection.start = 0;
			bytes_changed(gh, start, end);
		}
		bytes_changed(gh, old_pos, old_pos);
		show_cursor(gh);
	}
}

/*
 * returns cursor position
 */
guint gtk_hex_get_cursor(GtkHex *gh) {
	g_return_val_if_fail(gh != NULL, -1);
	g_return_val_if_fail(GTK_IS_HEX(gh), -1);

	return gh->cursor_pos;
}

/*
 * returns value of the byte at position offset
 */
guchar gtk_hex_get_byte(GtkHex *gh, guint offset) {
	g_return_val_if_fail(gh != NULL, 0);
	g_return_val_if_fail(GTK_IS_HEX(gh), 0);

	if((offset >= 0) && (offset < gh->document->file_size))
		return hex_document_get_byte(gh->document, offset);

	return 0;
}

/*
 * sets data group type (see GROUP_* defines at top of file)
 */
void gtk_hex_set_group_type(GtkHex *gh, guint gt) {
	GtkAllocation allocation;

	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	hide_cursor(gh);
	gh->group_type = gt;
	gtk_widget_get_allocation(GTK_WIDGET(gh), &allocation);
	// TEST
//	recalc_displays(gh, allocation.width, allocation.height);
	recalc_displays(gh);
	gtk_widget_queue_resize(GTK_WIDGET(gh));
	show_cursor(gh);
}

/*
 *  do we show the offsets of lines?
 */
void gtk_hex_show_offsets(GtkHex *gh, gboolean show)
{
	TEST_DEBUG_FUNCTION_START

	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	if(gh->show_offsets == show)
		return;

	gh->show_offsets = show;
	if(show)
		show_offsets_widget(gh);
	else
		hide_offsets_widget(gh);
}

void gtk_hex_set_starting_offset(GtkHex *gh, gint starting_offset)
{
	g_return_if_fail (gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));
	gh->starting_offset = starting_offset;
}

void gtk_hex_set_insert_mode(GtkHex *gh, gboolean insert)
{
	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	gh->insert = insert;

	if(!gh->insert && gh->cursor_pos > 0) {
		if(gh->cursor_pos >= gh->document->file_size)
			gh->cursor_pos = gh->document->file_size - 1;
	}
}

GtkHex_AutoHighlight *
gtk_hex_insert_autohighlight(GtkHex *gh,
		const gchar *search,
		gint len)
{
	GtkHex_AutoHighlight *new = g_malloc0(sizeof(GtkHex_AutoHighlight));

	new->search_string = g_memdup(search, len);
	new->search_len = len;

	new->highlights = NULL;

	new->next = gh->auto_highlight;
	new->prev = NULL;
	if (new->next) new->next->prev = new;
	gh->auto_highlight = new;

	new->view_min = 0;
	new->view_max = 0;

	gtk_hex_update_auto_highlight(gh, new, FALSE, TRUE);

	return new;
}

void gtk_hex_delete_autohighlight(GtkHex *gh, GtkHex_AutoHighlight *ahl)
{
	g_free(ahl->search_string);

	while (ahl->highlights)
	{
		gtk_hex_delete_highlight(gh, ahl, ahl->highlights);
	}

	if (ahl->next) ahl->next->prev = ahl->prev;
	if (ahl->prev) ahl->prev->next = ahl->next;

	if (gh->auto_highlight == ahl) gh->auto_highlight = ahl->next;

	g_free(ahl);
}

void gtk_hex_set_geometry(GtkHex *gh, gint cpl, gint vis_lines)
{
    gh->default_cpl = cpl;
    gh->default_lines = vis_lines;
}
