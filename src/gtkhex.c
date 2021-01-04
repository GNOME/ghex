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

#define DEFAULT_FONT "Monospace 12"

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

	gchar *colour;

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
	GtkFixed fixed;

	HexDocument *document;

	GtkWidget *xdisp, *adisp, *scrollbar;
	GtkWidget *offsets;

	PangoLayout *xlayout, *alayout, *olayout;

	GtkAdjustment *adj;

	PangoFontMetrics *disp_font_metrics;
	PangoFontDescription *font_desc;

	gint active_view;

	guint char_width, char_height;
	guint button;

	guint cursor_pos;
	GtkHex_Highlight selection;
	gint lower_nibble;

	guint group_type;

	gint lines, vis_lines, cpl, top_line;
	gint cursor_shown;

	gint xdisp_width, adisp_width, extra_width;

	GtkHex_AutoHighlight *auto_highlight;

	gint scroll_dir;
	guint scroll_timeout;
	gboolean show_offsets;
	gint starting_offset;
	gboolean insert;
	gboolean selecting;

	/* Buffer for storing formatted data for rendering;
	   dynamically adjusts its size to the display size */
	guchar *disp_buffer;

	/* FIXME - I think this is `characters per line` == cpl
	 * These could probably just be defines, but changing this is not a top
	 * priority right now. */
	gint default_cpl;
	gint default_lines;
};

static gint gtkhex_signals[LAST_SIGNAL] = { 0 };

static GtkFixedClass *parent_class = NULL;
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

/* LAR: REWRITE */
static guint
get_max_char_width(GtkHex *gh, PangoFontMetrics *font_metrics)
{
	/* this is, I guess, a rather dirty trick, but
	   right now i can't think of anything better */
	guint i;
	guint maxwidth = 0;
	PangoRectangle logical_rect;
	PangoLayout *layout;
	gchar str[2]; 

	if (char_widths == NULL)
		char_widths = (gchar*)g_malloc(0x100);

	char_widths[0] = 0;

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (gh), "");
	pango_layout_set_font_description (layout, gh->font_desc);

	for(i = 1; i < 0x100; i++) {
		logical_rect.width = 0;
		/* Check if the char is displayable. Caused trouble to pango */
		if (is_displayable((guchar)i)) {
			sprintf (str, "%c", (gchar)i);
			pango_layout_set_text(layout, str, -1);
			pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
		}
		char_widths[i] = logical_rect.width;
	}

	for(i = '0'; i <= 'z'; i++)
		maxwidth = MAX(maxwidth, char_widths[i]);

	g_object_unref (G_OBJECT (layout));
	return maxwidth;
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
	
	if (! gtk_widget_get_realized (gh->adisp))
		return;

	context = gtk_widget_get_style_context (gh->adisp);
	state = gtk_widget_get_state_flags (gh->adisp);
	state |= GTK_STATE_FLAG_SELECTED;

	if(get_acoords(gh, gh->cursor_pos, &cx, &cy)) {
		c[0] = gtk_hex_get_byte(gh, gh->cursor_pos);
		if (! is_displayable (c[0]))
			c[0] = '.';

		/* LAR - maybe put a gtk_render_background in here somewhere. */
#if 0
		gtk_style_context_get_background_color (context, state, &bg_color);
		gdk_cairo_set_source_rgba (cr, &bg_color);
#endif

		if(gh->active_view == VIEW_ASCII) {
			cairo_rectangle (cr, cx, cy, gh->char_width, gh->char_height - 1);
			cairo_fill (cr);
			//gtk_style_context_get_color (context, state, &fg_color);
			// API CHANGE
			gtk_style_context_get_color (context, &fg_color);
		}
		else {
			cairo_set_line_width (cr, 1.0);
			cairo_rectangle (cr, cx + 0.5, cy + 0.5, gh->char_width, gh->char_height - 1);
			cairo_stroke (cr);
			// API CHANGE
			//gtk_style_context_get_color (context, state & ~GTK_STATE_FLAG_SELECTED, &fg_color);
			gtk_style_context_get_color (context, &fg_color);
		}
		gdk_cairo_set_source_rgba (cr, &fg_color);
		cairo_move_to (cr, cx, cy);
		pango_layout_set_text (gh->alayout, c, 1);
		pango_cairo_show_layout (cr, gh->alayout);
	}
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

	if(!gtk_widget_get_realized(gh->xdisp))
		return;

	context = gtk_widget_get_style_context (gh->xdisp);
	state = gtk_widget_get_state_flags (gh->xdisp);
	state |= GTK_STATE_FLAG_SELECTED;

	if (get_xcoords (gh, gh->cursor_pos, &cx, &cy)) {
		format_xbyte(gh, gh->cursor_pos, c);
		if (gh->lower_nibble) {
			cx += gh->char_width;
			i = 1;
		} else {
			c[1] = 0;
			i = 0;
		}

		/* LAR - maybe put a gtk_render_background in here somewhere */
#if 0
		gtk_style_context_get_background_color (context, state, &bg_color);
		gdk_cairo_set_source_rgba (cr, &bg_color);
#endif

		if(gh->active_view == VIEW_HEX) {
			cairo_rectangle (cr, cx, cy, gh->char_width, gh->char_height - 1);
			cairo_fill (cr);
			// API CHANGE
			//gtk_style_context_get_color (context, state, &fg_color);
			gtk_style_context_get_color (context, &fg_color);
		}
		else {
			cairo_set_line_width (cr, 1.0);
			cairo_rectangle (cr, cx + 0.5, cy + 0.5, gh->char_width, gh->char_height - 1);
			cairo_stroke (cr);
			// API CHANGE
			//gtk_style_context_get_color (context, state & ~GTK_STATE_FLAG_SELECTED, &fg_color);
			gtk_style_context_get_color (context, &fg_color);
		}
		gdk_cairo_set_source_rgba (cr, &fg_color);
		cairo_move_to (cr, cx, cy);
		pango_layout_set_text (gh->xlayout, &c[i], 1);
		pango_cairo_show_layout (cr, gh->xlayout);
	}
}

static void show_cursor(GtkHex *gh) {
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
	state = gtk_widget_get_state_flags (gh->xdisp);

	gtk_style_context_save (context);
	state |= GTK_STATE_FLAG_SELECTED;
	gtk_style_context_set_state (context, state);

	cairo_save (cr);

	while (curHighlight)
	{
		if (ABS(curHighlight->start - curHighlight->end) >= curHighlight->min_select)
		{
			GdkRGBA bg_color;
			gint start, end;
			gint sl, el;
			gint cursor_off = 0;
			gint len;

			gtk_hex_validate_highlight(gh, curHighlight);

			start = MIN(curHighlight->start, curHighlight->end);
			end = MAX(curHighlight->start, curHighlight->end);
			sl = curHighlight->start_line;
			el = curHighlight->end_line;

			if (curHighlight->bg_color) {
				gdk_cairo_set_source_rgba (cr, curHighlight->bg_color);
			} else {
				/* LAR: gtk_render_background? */
#if 0
				gtk_style_context_get_background_color (context, state, &bg_color);
				gdk_cairo_set_source_rgba (cr, &bg_color);
#endif
			}

			if (cursor_line == sl)
			{
				cursor_off = 2*(start%gh->cpl) + (start%gh->cpl)/gh->group_type;
				if (cursor_line == el)
					len = 2*(end%gh->cpl + 1) + (end%gh->cpl)/gh->group_type;
				else
					len = xcpl;
				len = len - cursor_off;
				if (len > 0)
					cairo_rectangle (cr,
					                 cursor_off * gh->char_width,
					                 cursor_line * gh->char_height,
					                 len * gh->char_width,
					                 gh->char_height);
			}
			else if (cursor_line == el)
			{
				cursor_off = 2*(end%gh->cpl + 1) + (end%gh->cpl)/gh->group_type;
				if (cursor_off > 0)
					cairo_rectangle (cr,
					                 0,
					                 cursor_line * gh->char_height,
					                 cursor_off * gh->char_width,
					                 gh->char_height);
			}
			else if (cursor_line > sl && cursor_line < el)
			{
				cairo_rectangle (cr,
				                 0,
				                 cursor_line * gh->char_height,
				                 xcpl * gh->char_width,
				                 gh->char_height);
			}

			cairo_fill (cr);
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
	state = gtk_widget_get_state_flags (gh->adisp);

	gtk_style_context_save (context);
	state |= GTK_STATE_FLAG_SELECTED;
	gtk_style_context_set_state (context, state);

	cairo_save (cr);

	while (curHighlight)
	{
		if (ABS(curHighlight->start - curHighlight->end) >= curHighlight->min_select)
		{
			GdkRGBA bg_color;
			gint start, end;
			gint sl, el;
			gint cursor_off = 0;
			gint len;

			gtk_hex_validate_highlight(gh, curHighlight);

			start = MIN(curHighlight->start, curHighlight->end);
			end = MAX(curHighlight->start, curHighlight->end);
			sl = curHighlight->start_line;
			el = curHighlight->end_line;

			if (curHighlight->bg_color) {
				gdk_cairo_set_source_rgba (cr, curHighlight->bg_color);
			} else {
				/* LAR - gtk_render_background? */
#if 0
				gtk_style_context_get_background_color (context, state, &bg_color);
				gdk_cairo_set_source_rgba (cr, &bg_color);
#endif
			}
			if (cursor_line == sl)
			{
				cursor_off = start % gh->cpl;
				if (cursor_line == el)
					len = end - start + 1;
				else
					len = gh->cpl - cursor_off;
				if (len > 0)
					cairo_rectangle (cr,
					                 cursor_off * gh->char_width,
					                 cursor_line * gh->char_height,
					                 len * gh->char_width,
					                 gh->char_height);
			}
			else if (cursor_line == el)
			{
				cursor_off = end % gh->cpl + 1;
				if (cursor_off > 0)
					cairo_rectangle (cr,
					                 0,
					                 cursor_line * gh->char_height,
					                 cursor_off * gh->char_width,
					                 gh->char_height);
			}
			else if (cursor_line > sl && cursor_line < el)
			{
				cairo_rectangle (cr,
				                 0,
				                 cursor_line * gh->char_height,
				                 gh->cpl * gh->char_width,
				                 gh->char_height);
			}

			cairo_fill (cr);
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
 * when calling render_*_lines() the imin and imax arguments are the
 * numbers of the first and last line TO BE DISPLAYED in the range
 * [0 .. gh->vis_lines-1] AND NOT [0 .. gh->lines]!
 */
static void
render_hex_lines (GtkHex *gh,
                  cairo_t *cr,
                  gint imin,
                  gint imax)
{
	GtkWidget *w = gh->xdisp;
	GdkRGBA bg_color;
	GdkRGBA fg_color;
	GtkAllocation allocation;
	GtkStateFlags state;
	GtkStyleContext *context;
	gint i, cursor_line;
	gint xcpl = gh->cpl*2 + gh->cpl/gh->group_type;
	gint frm_len, tmp;

	if( (!gtk_widget_get_realized(GTK_WIDGET (gh))) || (gh->cpl == 0) )
		return;

	context = gtk_widget_get_style_context (w);
	state = gtk_widget_get_state_flags (w);

	/* gtk_render_background? */
//	gtk_style_context_get_background_color (context, state, &bg_color);
//	// API CHANGE
	//gtk_style_context_get_color (context, state, &fg_color);
	gtk_style_context_get_color (context, &fg_color);

	cursor_line = gh->cursor_pos / gh->cpl - gh->top_line;

	gtk_widget_get_allocation(w, &allocation);
//	gdk_cairo_set_source_rgba (cr, &bg_color);
	cairo_rectangle (cr, 0, imin * gh->char_height, allocation.width, (imax - imin + 1) * gh->char_height);
	cairo_fill (cr);
  
	imax = MIN(imax, gh->vis_lines);
	imax = MIN(imax, gh->lines);

	gdk_cairo_set_source_rgba (cr, &fg_color);

	frm_len = format_xblock (gh, gh->disp_buffer,
			(gh->top_line+imin)*gh->cpl,
			MIN((gh->top_line+imax+1)*gh->cpl,
				gh->document->file_size) );
	
	for (i = imin; i <= imax; i++) {
		tmp = (gint)frm_len - (gint)((i - imin)*xcpl);
		if(tmp <= 0)
			break;

		render_hex_highlights (gh, cr, i);
		cairo_move_to (cr, 0, i * gh->char_height);
		pango_layout_set_text (gh->xlayout, gh->disp_buffer + (i - imin) * xcpl, MIN(xcpl, tmp));
		pango_cairo_show_layout (cr, gh->xlayout);
	}
	
	if((cursor_line >= imin) && (cursor_line <= imax) && (gh->cursor_shown))
		render_xc (gh, cr);
}

static void
render_ascii_lines (GtkHex *gh,
                    cairo_t *cr,
                    gint imin,
                    gint imax)
{
	GtkWidget *w = gh->adisp;
	GdkRGBA bg_color;
	GdkRGBA fg_color;
	GtkAllocation allocation;
	GtkStateFlags state;
	GtkStyleContext *context;
	gint i, tmp, frm_len;
	guint cursor_line;

	if( (!gtk_widget_get_realized(GTK_WIDGET(gh))) || (gh->cpl == 0) )
		return;

	context = gtk_widget_get_style_context (w);
	state = gtk_widget_get_state_flags (w);

	/* LAR: gtk_render_background?  */
//	gtk_style_context_get_background_color (context, state, &bg_color);
//	// API CHANGE
//	gtk_style_context_get_color (context, state, &fg_color);
	gtk_style_context_get_color (context, &fg_color);

	cursor_line = gh->cursor_pos / gh->cpl - gh->top_line;

	gtk_widget_get_allocation(w, &allocation);
//	gdk_cairo_set_source_rgba (cr, &bg_color);
	cairo_rectangle (cr, 0, imin * gh->char_height, allocation.width, (imax - imin + 1) * gh->char_height);
	cairo_fill (cr);
	
	imax = MIN(imax, gh->vis_lines);
	imax = MIN(imax, gh->lines);

	gdk_cairo_set_source_rgba (cr, &fg_color);
	
	frm_len = format_ablock (gh, gh->disp_buffer, (gh->top_line+imin)*gh->cpl,
							MIN((gh->top_line+imax+1)*gh->cpl, gh->document->file_size) );
	
	for (i = imin; i <= imax; i++) {
		tmp = (gint)frm_len - (gint)((i - imin)*gh->cpl);
		if(tmp <= 0)
			break;

		render_ascii_highlights (gh, cr, i);

		cairo_move_to (cr, 0, i * gh->char_height);
		pango_layout_set_text (gh->alayout, gh->disp_buffer + (i - imin)*gh->cpl, MIN(gh->cpl, tmp));
		pango_cairo_show_layout (cr, gh->alayout);
	}
	
	if ((cursor_line >= imin) && (cursor_line <= imax) && (gh->cursor_shown))
		render_ac (gh, cr);
}

static void
render_offsets (GtkHex *gh,
                cairo_t *cr,
                gint imin,
                gint imax)
{
	GtkWidget *w = gh->offsets;
	GdkRGBA bg_color;
	GdkRGBA fg_color;
	GtkAllocation allocation;
	GtkStateFlags state;
	GtkStyleContext *context;
	gint i;
	gchar offstr[9];

	if (! gtk_widget_get_realized (GTK_WIDGET (gh)))
		return;

	context = gtk_widget_get_style_context (w);
	state = gtk_widget_get_state_flags (w);

	/* LAR - gtk_render_background? */
//	gtk_style_context_get_background_color (context, state, &bg_color);
//	API CHANGE
//	gtk_style_context_get_color (context, state, &fg_color);
	gtk_style_context_get_color (context, &fg_color);

	gtk_widget_get_allocation(w, &allocation);
//	gdk_cairo_set_source_rgba (cr, &bg_color);
	cairo_rectangle (cr, 0, imin * gh->char_height, allocation.width, (imax - imin + 1) * gh->char_height);
	cairo_fill (cr);
  
	imax = MIN(imax, gh->vis_lines);
	imax = MIN(imax, gh->lines - gh->top_line - 1);

	gdk_cairo_set_source_rgba (cr, &fg_color);
	
	for(i = imin; i <= imax; i++) {
		sprintf(offstr, "%08X", (gh->top_line + i)*gh->cpl + gh->starting_offset);
		cairo_move_to (cr, 0, i * gh->char_height);
		pango_layout_set_text (gh->olayout, offstr, 8);
		pango_cairo_show_layout (cr, gh->olayout);
	}
}

/*
 * draw signal handlers for both displays
 */
static void
hex_draw (GtkDrawingArea *drawing_area,
                           cairo_t *cr,
                           int width,
                           int height,
                           gpointer user_data)
{
	GtkHex *gh = GTK_HEX(user_data);
	g_return_if_fail(GTK_IS_HEX(gh));

	/* LAR - TEST - I have no idea what this function is trying to
	 * accomplish. May need a rewrite. */

	render_hex_lines (gh, cr, 0, gh->vis_lines);
#if 0
	GdkRectangle rect;
	gint imin, imax;

	gdk_cairo_get_clip_rectangle (cr, &rect);

	imin = (rect.y) / gh->char_height;
	imax = (rect.y + rect.height) / gh->char_height;
	if ((rect.y + rect.height) % gh->char_height)
		imax++;

	imax = MIN(imax, gh->vis_lines);
	
	render_hex_lines (gh, cr, imin, imax);
#endif
}

static void
ascii_draw (GtkDrawingArea *drawing_area,
                           cairo_t *cr,
                           int width,
                           int height,
                           gpointer user_data)
{
	GtkHex *gh = GTK_HEX(user_data);
	g_return_if_fail(GTK_IS_HEX(gh));

	/* LAR - TEST - I have no idea what this function is trying to
	 * accomplish. May need a rewrite. */

	render_ascii_lines (gh, cr, 0, gh->vis_lines);

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
	/* LAR - TEST - I have no idea what this function is trying to
	 * accomplish. May need a rewrite. */

	GtkHex *gh = GTK_HEX(user_data);

	g_return_if_fail(GTK_IS_HEX(gh));

	render_offsets (gh, cr, 0, gh->vis_lines);

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

/*
 * draw signal handler for the GtkHex itself: draws shadows around both displays
 */
static void
draw_shadow (GtkWidget *widget,
             cairo_t *cr)
{
	GtkHex *gh = GTK_HEX(widget);
	GtkAllocation allocation;
	GtkBorder padding;
	GtkStateFlags state;
	GtkStyleContext *context;
	// LAR - TEST
	gint border = 10;
//	gint border = gtk_container_get_border_width(GTK_CONTAINER(widget));
	gint x;

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);
	// API CHANGE
	gtk_style_context_get_padding (context, &padding);
//	gtk_style_context_get_padding (context, state, &padding);

	x = border;
	gtk_widget_get_allocation(widget, &allocation);
	if (gh->show_offsets) {
		gtk_render_frame (context,
		                  cr,
		                  border,
		                  border,
		                  9 * gh->char_width + padding.left + padding.right,
		                  allocation.height - 2 * border);
		x += 9 * gh->char_width + padding.left + padding.right + gh->extra_width/2;
	}

	gtk_render_frame (context,
	                  cr,
	                  x,
	                  border,
	                  gh->xdisp_width + padding.left + padding.right,
	                  allocation.height - 2 * border);

	/* Draw a frame around the ascii display + scrollbar */
	gtk_render_frame (context,
	                  cr,
	                  allocation.width - border - gh->adisp_width - padding.left - padding.right,
	                  border,
	                  gh->adisp_width + padding.left + padding.right,
	                  allocation.height - 2 * border);
}

/*
 * this calculates how many bytes we can stuff into one line and how many
 * lines we can display according to the current size of the widget
 */
static void recalc_displays(GtkHex *gh, guint width, guint height) {
	gboolean scroll_to_cursor;
	gdouble value;
	gint total_width = width;
	gint total_cpl, xcpl;
	gint old_cpl = gh->cpl;
	GtkBorder padding;
	GtkStateFlags state;
	GtkStyleContext *context;
	GtkRequisition req;

	context = gtk_widget_get_style_context (GTK_WIDGET (gh));
	state = gtk_widget_get_state_flags (GTK_WIDGET (gh));
	// API CHANGE
	//gtk_style_context_get_padding (context, state, &padding);
	gtk_style_context_get_padding (context, &padding);

	/*
	 * Only change the value of the adjustment to put the cursor on screen
	 * if the cursor is currently within the displayed portion.
	 */
	scroll_to_cursor = (gh->cpl == 0) ||
	                   ((gh->cursor_pos / gh->cpl >= gtk_adjustment_get_value (gh->adj)) &&
	                    (gh->cursor_pos / gh->cpl <= gtk_adjustment_get_value (gh->adj) + gh->vis_lines - 1));

	gtk_widget_get_preferred_size (gh->scrollbar, &req, NULL);
	
	gh->xdisp_width = 1;
	gh->adisp_width = 1;

	// API CHANGE
	total_width -= 20 +		// LAR DUMB TEST
	               2 * padding.left + 2 * padding.right + req.width;

//	total_width -= 2*gtk_container_get_border_width(GTK_CONTAINER(gh)) +
//	               2 * padding.left + 2 * padding.right + req.width;

	if(gh->show_offsets)
		total_width -= padding.left + padding.right + 9 * gh->char_width;

	total_cpl = total_width / gh->char_width;

	if((total_cpl == 0) || (total_width < 0)) {
		gh->cpl = gh->lines = gh->vis_lines = 0;
		return;
	}
	
	/* calculate how many bytes we can stuff in one line */
	gh->cpl = 0;
	do {
		if(gh->cpl % gh->group_type == 0 && total_cpl < gh->group_type*3)
			break;
		
		gh->cpl++;        /* just added one more char */
		total_cpl -= 3;   /* 2 for xdisp, 1 for adisp */
		
		if(gh->cpl % gh->group_type == 0) /* just ended a group */
			total_cpl--;
	} while(total_cpl > 0);

	if(gh->cpl == 0)
		return;

	if(gh->document->file_size == 0)
		gh->lines = 1;
	else {
		gh->lines = gh->document->file_size / gh->cpl;
		if(gh->document->file_size % gh->cpl)
			gh->lines++;
	}

	// API CHANGE
	gh->vis_lines = ((gint) (height - 20 /* LAR DUMB TEST */ - padding.top - padding.bottom)) / ((gint) gh->char_height);


//	gh->vis_lines = ((gint) (height - 2 * gtk_container_get_border_width (GTK_CONTAINER (gh)) - padding.top - padding.bottom)) / ((gint) gh->char_height);

	gh->adisp_width = gh->cpl*gh->char_width;
	xcpl = gh->cpl*2 + (gh->cpl - 1)/gh->group_type;
	gh->xdisp_width = xcpl*gh->char_width;

	gh->extra_width = total_width - gh->xdisp_width - gh->adisp_width;

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
 * connected to value_changed signal of scrollbar's GtkAdjustment
 */
static void display_scrolled(GtkAdjustment *adj, GtkHex *gh) {
	gint dx;
	gint dy;

	if ((!gtk_widget_is_drawable(gh->xdisp)) ||
	    (!gtk_widget_is_drawable(gh->adisp)))
		return;

	dx = 0;
	dy = (gh->top_line - (gint)gtk_adjustment_get_value (adj)) * gh->char_height;

	gh->top_line = (gint)gtk_adjustment_get_value(adj);

	/* LAR - rewrite. */
#if 0
	gdk_window_scroll (gtk_widget_get_window (gh->xdisp), dx, dy);
	gdk_window_scroll (gtk_widget_get_window (gh->adisp), dx, dy);
	if (gh->offsets)
		gdk_window_scroll (gtk_widget_get_window (gh->offsets), dx, dy);
#endif

	gtk_hex_update_all_auto_highlights(gh, TRUE, TRUE);
	gtk_hex_invalidate_all_highlights(gh);
}

/*
 * mouse signal handlers (button 1 and motion) for both displays
 */
static gboolean
scroll_timeout_handler(GtkHex *gh)
{
	if(gh->scroll_dir < 0)
		gtk_hex_set_cursor(gh, MAX(0, (int)(gh->cursor_pos - gh->cpl)));
	else if(gh->scroll_dir > 0)
		gtk_hex_set_cursor(gh, MIN(gh->document->file_size - 1,
						gh->cursor_pos + gh->cpl));
	return TRUE;
}

/* REWRITE */
#if 0
static void
hex_scroll_cb(GtkWidget *w, GdkEventScroll *event, GtkHex *gh)
{
	gtk_widget_event(gh->scrollbar, (GdkEvent *)event);
}
#endif

/* REWRITE */
#if 0
static void
hex_button_cb(GtkWidget *w, GdkEventButton *event, GtkHex *gh) {
	if( (event->type == GDK_BUTTON_RELEASE) &&
		(event->button == GDK_BUTTON_PRIMARY) ) {
		if(gh->scroll_timeout != -1) {
			g_source_remove(gh->scroll_timeout);
			gh->scroll_timeout = -1;
			gh->scroll_dir = 0;
		}
		gh->selecting = FALSE;
		gtk_grab_remove(w);
		gh->button = 0;
	}
	else if((event->type == GDK_BUTTON_PRESS) && (event->button == GDK_BUTTON_PRIMARY)) {
		if (!gtk_widget_has_focus (GTK_WIDGET (gh)))
			gtk_widget_grab_focus (GTK_WIDGET(gh));
		
		gtk_grab_add(w);

		gh->button = event->button;
		
		if(gh->active_view == VIEW_HEX) {
			hex_to_pointer(gh, event->x, event->y);

			if(!gh->selecting) {
				gh->selecting = TRUE;
				gtk_hex_set_selection(gh, gh->cursor_pos, gh->cursor_pos);
			}
		}
		else {
			hide_cursor(gh);
			gh->active_view = VIEW_HEX;
			show_cursor(gh);
			hex_button_cb(w, event, gh);
		}
	}
	else if((event->type == GDK_BUTTON_PRESS) && (event->button == GDK_BUTTON_MIDDLE)) {
		GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));
		gchar *text;

		gh->active_view = VIEW_HEX;
		hex_to_pointer(gh, event->x, event->y);

		text = gtk_clipboard_wait_for_text(klass->primary);
		if(text) {
			hex_document_set_data(gh->document, gh->cursor_pos,
								  strlen(text), 0, text, TRUE);
			gtk_hex_set_cursor(gh, gh->cursor_pos + strlen(text));
			g_free(text);
		}
		gh->button = 0;
	}
	else
		gh->button = 0;
}
#endif

// REWRITE
#if 0
static void hex_motion_cb(GtkWidget *w, GdkEventMotion *event, GtkHex *gh) {
	GtkAllocation allocation;
	GdkDeviceManager *device_manager;
	GdkDevice *pointer;
	gint x, y;

	gtk_widget_get_allocation(w, &allocation);

	device_manager = gdk_display_get_device_manager (gtk_widget_get_display (w));
	pointer = gdk_device_manager_get_client_pointer (device_manager);
	gdk_window_get_device_position (gtk_widget_get_window (w), pointer, &x, &y, NULL);

	if(y < 0)
		gh->scroll_dir = -1;
	else if(y >= allocation.height)
		gh->scroll_dir = 1;
	else
		gh->scroll_dir = 0;

	if(gh->scroll_dir != 0) {
		if(gh->scroll_timeout == -1)
			gh->scroll_timeout =
				g_timeout_add(SCROLL_TIMEOUT,
							  (GSourceFunc)scroll_timeout_handler, gh);
		return;
	}
	else {
		if(gh->scroll_timeout != -1) {
			g_source_remove(gh->scroll_timeout);
			gh->scroll_timeout = -1;
		}
	}
			
	if(event->window != gtk_widget_get_window(w))
		return;

	if((gh->active_view == VIEW_HEX) && (gh->button == 1)) {
		hex_to_pointer(gh, x, y);
	}
}
#endif

#if 0
static void ascii_scroll_cb(GtkWidget *w, GdkEventScroll *event, GtkHex *gh) {
	gtk_widget_event(gh->scrollbar, (GdkEvent *)event);
}
#endif

#if 0
static void ascii_button_cb(GtkWidget *w, GdkEventButton *event, GtkHex *gh) {
	if( (event->type == GDK_BUTTON_RELEASE) &&
		(event->button == GDK_BUTTON_PRIMARY) ) {
		if(gh->scroll_timeout != -1) {
			g_source_remove(gh->scroll_timeout);
			gh->scroll_timeout = -1;
			gh->scroll_dir = 0;
		}
		gh->selecting = FALSE;
		gtk_grab_remove(w);
		gh->button = 0;
	}
	else if( (event->type == GDK_BUTTON_PRESS) && (event->button == GDK_BUTTON_PRIMARY) ) {
		if (!gtk_widget_has_focus (GTK_WIDGET (gh)))
			gtk_widget_grab_focus (GTK_WIDGET(gh));
		
		gtk_grab_add(w);
		gh->button = event->button;
		if(gh->active_view == VIEW_ASCII) {
			ascii_to_pointer(gh, event->x, event->y);
			if(!gh->selecting) {
				gh->selecting = TRUE;
				gtk_hex_set_selection(gh, gh->cursor_pos, gh->cursor_pos);
			}
		}
		else {
			hide_cursor(gh);
			gh->active_view = VIEW_ASCII;
			show_cursor(gh);
			ascii_button_cb(w, event, gh);
		}
	}
	else if((event->type == GDK_BUTTON_PRESS) && (event->button == GDK_BUTTON_MIDDLE)) {
		GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));
		gchar *text;

		gh->active_view = VIEW_ASCII;
		ascii_to_pointer(gh, event->x, event->y);

		text = gtk_clipboard_wait_for_text(klass->primary);
		if(text) {
			hex_document_set_data(gh->document, gh->cursor_pos,
								  strlen(text), 0, text, TRUE);
			gtk_hex_set_cursor(gh, gh->cursor_pos + strlen(text));
			g_free(text);
		}
		gh->button = 0;
	}
	else
		gh->button = 0;
}
#endif

#if 0
static void ascii_motion_cb(GtkWidget *w, GdkEventMotion *event, GtkHex *gh) {
	GtkAllocation allocation;
	GdkDeviceManager *device_manager;
	GdkDevice *pointer;
	gint x, y;

	gtk_widget_get_allocation(w, &allocation);

	device_manager = gdk_display_get_device_manager (gtk_widget_get_display (w));
	pointer = gdk_device_manager_get_client_pointer (device_manager);
	gdk_window_get_device_position (gtk_widget_get_window (w), pointer, &x, &y, NULL);

	if(y < 0)
		gh->scroll_dir = -1;
	else if(y >= allocation.height)
		gh->scroll_dir = 1;
	else
		gh->scroll_dir = 0;

	if(gh->scroll_dir != 0) {
		if(gh->scroll_timeout == -1)
			gh->scroll_timeout =
				g_timeout_add(SCROLL_TIMEOUT,
							  (GSourceFunc)scroll_timeout_handler, gh);
		return;
	}
	else {
		if(gh->scroll_timeout != -1) {
			g_source_remove(gh->scroll_timeout);
			gh->scroll_timeout = -1;
		}
	}

	if(event->window != gtk_widget_get_window(w))
		return;

	if((gh->active_view == VIEW_ASCII) && (gh->button == 1)) {
		ascii_to_pointer(gh, x, y);
	}
}
#endif

static void
show_offsets_widget(GtkHex *gh)
{
	GtkStyleContext *context;

	gh->offsets = gtk_drawing_area_new();

	/* Modify the font for the widget */
	// LAR - NOPE - CSS.
//	gtk_widget_modify_font (gh->offsets, gh->font_desc);
 
	/* Create the pango layout for the widget */
	gh->olayout = gtk_widget_create_pango_layout (gh->offsets, "");

//	gtk_widget_set_events (gh->offsets, GDK_EXPOSURE_MASK);

	gtk_drawing_area_set_draw_func (gh->offsets,
			offsets_draw,	// GtkDrawingAreaDrawFunc draw_func,
			gh,		// gpointer user_data,
			NULL);		// GDestroyNotify destroy);

	context = gtk_widget_get_style_context (GTK_WIDGET (gh->xdisp));
	gtk_style_context_add_class (context, "header");

	gtk_fixed_put(GTK_FIXED(gh), gh->offsets, 0, 0);
	gtk_widget_show(gh->offsets);
}

static void hide_offsets_widget(GtkHex *gh) {
	g_debug("%s: NOT IMPLEMENTED", __func__);
	// API CHANGES
#if 0
	if(gh->offsets) {
		gtk_container_remove(GTK_CONTAINER(gh), gh->offsets);
		gh->offsets = NULL;
	}
#endif
}

/*
 * default data_changed signal handler
 */
static void gtk_hex_real_data_changed(GtkHex *gh, gpointer data) {
	HexChangeData *change_data = (HexChangeData *)data;
	gint start_line, end_line;
	guint lines;

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

static void bytes_changed(GtkHex *gh, gint start, gint end)
{
	gint start_line = start/gh->cpl - gh->top_line;
	gint end_line = end/gh->cpl - gh->top_line;

	if(end_line < 0 ||
	   start_line > gh->vis_lines)
		return;

	start_line = MAX(start_line, 0);

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

void gtk_hex_set_selection(GtkHex *gh, gint start, gint end)
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

	if (gdk_rgba_parse (&rgba, ahl->colour))
		new->bg_color = gdk_rgba_copy (&rgba);
	else
		new->bg_color = NULL;


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

	if (hl->bg_color)
		gdk_rgba_free (hl->bg_color);

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

static void gtk_hex_real_copy_to_clipboard(GtkHex *gh)
{
	// LAR - REWRITE FOR GTK4
	(void)gh;
	g_debug("%s: NOT IMPLEMENTED", __func__);

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

static void gtk_hex_finalize(GObject *o) {
	GtkHex *gh = GTK_HEX(o);
	
	if (gh->disp_buffer)
		g_free (gh->disp_buffer);

	if (gh->disp_font_metrics)
		pango_font_metrics_unref (gh->disp_font_metrics);

	if (gh->font_desc)
		pango_font_description_free (gh->font_desc);

	if (gh->xlayout)
		g_object_unref (G_OBJECT (gh->xlayout));
	
	if (gh->alayout)
		g_object_unref (G_OBJECT (gh->alayout));
	
	if (gh->olayout)
		g_object_unref (G_OBJECT (gh->olayout));
	
	/* Changes for Gnome 2.0 -- SnM */	
	if(G_OBJECT_CLASS(parent_class)->finalize)
		(* G_OBJECT_CLASS(parent_class)->finalize)(G_OBJECT(o));  
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

/* 
 * recalculate the width of both displays and reposition and resize all
 * the children widgets and adjust the scrollbar after resizing
 * connects to the size_allocate signal of the GtkHex widget
 */
static void gtk_hex_size_allocate(GtkWidget *w, GtkAllocation *alloc) {
	GtkHex *gh;
	GtkAllocation my_alloc;
	GtkBorder padding;
	GtkStateFlags state;
	GtkStyleContext *context;
	gint border_width;

	gh = GTK_HEX(w);
	hide_cursor(gh);
	
	recalc_displays(gh, alloc->width, alloc->height);

	// API CHANGE
#if 0
	gtk_widget_set_allocation(w, alloc);
	if(gtk_widget_get_realized(w))
		gdk_window_move_resize (gtk_widget_get_window(w),
								alloc->x, 
								alloc->y,
								alloc->width, 
								alloc->height);
#endif 

	// LAR - API CHANGE
//	border_width = gtk_container_get_border_width(GTK_CONTAINER(w));
	border_width = 20;	// DUMB TEST

	context = gtk_widget_get_style_context (w);
	state = gtk_widget_get_state_flags (w);
	// API CHANGE
//	gtk_style_context_get_padding (context, state, &padding);
	gtk_style_context_get_padding (context, &padding);

	my_alloc.x = border_width + padding.left;
	my_alloc.y = border_width + padding.top;
	my_alloc.height = MAX (alloc->height - 2 * border_width - padding.top - padding.bottom, 1);
	if(gh->show_offsets) {
		my_alloc.width = 9*gh->char_width;
		// API CHANGE - ADDED THE -1 AS A TEST
		gtk_widget_size_allocate(gh->offsets, &my_alloc, -1);
		gtk_widget_queue_draw(gh->offsets);
		my_alloc.x += padding.left + padding.right + my_alloc.width + gh->extra_width/2;
	}

	my_alloc.width = gh->xdisp_width;
	// LAR - TEST
	gtk_widget_size_allocate(gh->xdisp, &my_alloc, -1);
	my_alloc.x = alloc->width - border_width;
	my_alloc.y = border_width;
	// LAR - TEST
	my_alloc.width = alloc->width;
	my_alloc.height = MAX(alloc->height - 2*border_width, 1);
	// LAR - TEST
	gtk_widget_size_allocate(gh->scrollbar, &my_alloc, -1);
	my_alloc.x -= gh->adisp_width + padding.left;
	my_alloc.y = border_width + padding.top;
	my_alloc.width = gh->adisp_width;
	my_alloc.height = MAX (alloc->height - 2 * border_width - padding.top - padding.bottom, 1);
	// LAR - TEST
	gtk_widget_size_allocate(gh->adisp, &my_alloc, -1);
	
	show_cursor(gh);
}

// LAR - no can do, gtk4
#if 0
static gboolean
gtk_hex_draw (GtkWidget *w,
              cairo_t *cr)
{
	if (GTK_WIDGET_CLASS (parent_class)->draw)
		(* GTK_WIDGET_CLASS (parent_class)->draw) (w, cr);

	draw_shadow (w, cr);

	return TRUE;
}
#endif

static void
offsets_draw (GtkDrawingArea *drawing_area,
                           cairo_t *cr,
                           int width,
                           int height,
                           gpointer user_data);

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

static void gtk_hex_class_init(GtkHexClass *klass, gpointer data) {
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	
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

	GTK_WIDGET_CLASS(klass)->size_allocate = gtk_hex_size_allocate;
	// GONESVILLE WITH GTK4
//	GTK_WIDGET_CLASS(klass)->get_preferred_width = gtk_hex_get_preferred_width;
//	GTK_WIDGET_CLASS(klass)->get_preferred_height = gtk_hex_get_preferred_height;
	// LAR - no can do, gtk4 - just seems to draw a border??
//	GTK_WIDGET_CLASS(klass)->draw = gtk_hex_draw;

//	GTK4 API CHANGES - SWITCH TO GESTURES / EVENT CONTROLLERS
//	GTK_WIDGET_CLASS(klass)->key_press_event = gtk_hex_key_press;
//	GTK_WIDGET_CLASS(klass)->key_release_event = gtk_hex_key_release;
//	GTK_WIDGET_CLASS(klass)->button_release_event = gtk_hex_button_release;

	object_class->finalize = gtk_hex_finalize;
}

static void gtk_hex_init(GtkHex *gh, gpointer klass) {
	GtkCssProvider *provider;
	GtkStyleContext *context;

	gh->disp_buffer = NULL;
	gh->default_cpl = DEFAULT_CPL;
	gh->default_lines = DEFAULT_LINES;

	gh->scroll_timeout = -1;

	gh->document = NULL;
	gh->starting_offset = 0;

	gh->xdisp_width = gh->adisp_width = 200;
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

	/* get ourselves a decent monospaced font for rendering text */
	gh->disp_font_metrics = gtk_hex_load_font (DEFAULT_FONT);
	gh->font_desc = pango_font_description_from_string (DEFAULT_FONT);

	gh->char_width = get_max_char_width(gh, gh->disp_font_metrics);
	gh->char_height = PANGO_PIXELS (pango_font_metrics_get_ascent (gh->disp_font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (gh->disp_font_metrics)) + 2;
	
	gtk_widget_set_can_focus(GTK_WIDGET(gh), TRUE);
	// API CHANGE
//	gtk_widget_set_events(GTK_WIDGET(gh), GDK_KEY_PRESS_MASK);
	
	context = gtk_widget_get_style_context (GTK_WIDGET (gh));

	// LAR - this looks wrong.
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
	                                 "GtkHex {\n"
	                                 "   border-style: solid;\n"
	                                 "   border-width: 1px;\n"
	                                 "   padding: 1px;\n"
	                                 "}\n", -1);
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);


	gh->adj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
	gh->xdisp = gtk_drawing_area_new();

	/* Modify the font for the widget */
	// API CHANGE - USE CSS
//	gtk_widget_modify_font (gh->xdisp, gh->font_desc);

	/* Create the pango layout for the widget */
	gh->xlayout = gtk_widget_create_pango_layout (gh->xdisp, "");


	// LAR - NO CAN DO - GTK4
	// here's a test instead!
	gtk_widget_set_can_focus (GTK_WIDGET(gh->xdisp), TRUE);
//	gtk_widget_set_events (gh->xdisp, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
//						   GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK | GDK_SCROLL_MASK);

	// TEST FOR GTK4 - draw
	gtk_drawing_area_set_draw_func (gh->xdisp,
			hex_draw,	// GtkDrawingAreaDrawFunc draw_func,
			gh,		// gpointer user_data,
			NULL);		// GDestroyNotify destroy);
	// REWRITE - LAR
#if 0
	g_signal_connect(G_OBJECT(gh->xdisp), "button_press_event",
					 G_CALLBACK(hex_button_cb), gh);
	g_signal_connect(G_OBJECT(gh->xdisp), "button_release_event",
					 G_CALLBACK(hex_button_cb), gh);
	g_signal_connect(G_OBJECT(gh->xdisp), "motion_notify_event",
					 G_CALLBACK(hex_motion_cb), gh);
	g_signal_connect(G_OBJECT(gh->xdisp), "scroll_event",
					 G_CALLBACK(hex_scroll_cb), gh);
#endif

	context = gtk_widget_get_style_context (GTK_WIDGET (gh->xdisp));
	gtk_style_context_add_class (context, "view");

	gtk_fixed_put(GTK_FIXED(gh), gh->xdisp, 0, 0);
	gtk_widget_show(gh->xdisp);
	
	gh->adisp = gtk_drawing_area_new();

	/* Modify the font for the widget */
	// LAR - no can do - gtk4
//	gtk_widget_modify_font (gh->adisp, gh->font_desc);

	/* Create the pango layout for the widget */
	gh->alayout = gtk_widget_create_pango_layout (gh->adisp, "");

	// LAR - TEST FOR GTK4
	gtk_widget_set_can_focus (GTK_WIDGET(gh->adisp), TRUE);
//	gtk_widget_set_events (gh->adisp, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
//						   GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK | GDK_SCROLL_MASK);

	// TEST FOR GTK4 - draw
	gtk_drawing_area_set_draw_func (gh->adisp,
			ascii_draw,	// GtkDrawingAreaDrawFunc draw_func,
			gh,		// gpointer user_data,
			NULL);		// GDestroyNotify destroy);

//	g_signal_connect(G_OBJECT(gh->adisp), "draw",
//					 G_CALLBACK(ascii_draw), gh);
	// LAR - REWRITE
#if 0
	g_signal_connect(G_OBJECT(gh->adisp), "button_press_event",
					 G_CALLBACK(ascii_button_cb), gh);
	g_signal_connect(G_OBJECT(gh->adisp), "button_release_event",
					 G_CALLBACK(ascii_button_cb), gh);
	g_signal_connect(G_OBJECT(gh->adisp), "motion_notify_event",
					 G_CALLBACK(ascii_motion_cb), gh);
	g_signal_connect(G_OBJECT(gh->adisp), "scroll_event",
					 G_CALLBACK(ascii_scroll_cb), gh);
#endif

	context = gtk_widget_get_style_context (GTK_WIDGET (gh->adisp));
	gtk_style_context_add_class (context, "view");

	gtk_fixed_put(GTK_FIXED(gh), gh->adisp, 0, 0);
	gtk_widget_show(gh->adisp);
	
	g_signal_connect(G_OBJECT(gh->adj), "value-changed",
					 G_CALLBACK(display_scrolled), gh);

	gh->scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, gh->adj);
	gtk_fixed_put(GTK_FIXED(gh), gh->scrollbar, 0, 0);
	gtk_widget_show(gh->scrollbar);
}

GType gtk_hex_get_type() {
	static GType gh_type = 0;
	
	if(!gh_type) {
		GTypeInfo gh_info = {
			sizeof (GtkHexClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gtk_hex_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GtkHex),
			0,
			(GInstanceInitFunc) gtk_hex_init	
		};
	
		gh_type = g_type_register_static (gtk_fixed_get_type(),
							"GtkHex",
							&gh_info,
							0);	
	}
	
	return gh_type;
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
void gtk_hex_set_cursor(GtkHex *gh, gint index) {
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
 * sets data group type (see GROUP_* defines in gtkhex.h)
 */
void gtk_hex_set_group_type(GtkHex *gh, guint gt) {
	GtkAllocation allocation;

	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	hide_cursor(gh);
	gh->group_type = gt;
	gtk_widget_get_allocation(GTK_WIDGET(gh), &allocation);
	recalc_displays(gh, allocation.width, allocation.height);
	gtk_widget_queue_resize(GTK_WIDGET(gh));
	show_cursor(gh);
}

/*
 * sets font for displaying data
 */
void
gtk_hex_set_font(GtkHex *gh,
		PangoFontMetrics *font_metrics,
		const PangoFontDescription *font_desc)
{
	GtkAllocation allocation;

	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	if (gh->disp_font_metrics)
		pango_font_metrics_unref (gh->disp_font_metrics);

	if (gh->font_desc)
		pango_font_description_free (gh->font_desc);
	
	gh->disp_font_metrics = pango_font_metrics_ref (font_metrics);
	gh->font_desc = pango_font_description_copy (font_desc);

	// API CHANGE - USE CSS
#if 0
	if (gh->xdisp)
		gtk_widget_modify_font (gh->xdisp, gh->font_desc);

	if (gh->adisp)
		gtk_widget_modify_font (gh->adisp, gh->font_desc);

	if (gh->offsets)
		gtk_widget_modify_font (gh->offsets, gh->font_desc);
#endif


	gh->char_width = get_max_char_width(gh, gh->disp_font_metrics);
	gh->char_height = PANGO_PIXELS (pango_font_metrics_get_ascent (gh->disp_font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (gh->disp_font_metrics)) + 2;
	gtk_widget_get_allocation(GTK_WIDGET(gh), &allocation);
	recalc_displays(gh, allocation.width, allocation.height);
	
	gtk_widget_queue_draw(GTK_WIDGET(gh));
}

/*
 *  do we show the offsets of lines?
 */
void gtk_hex_show_offsets(GtkHex *gh, gboolean show)
{
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

PangoFontMetrics* gtk_hex_load_font (const char *font_name)
{
	PangoContext *context;
	PangoFont *new_font;
	PangoFontDescription *new_desc;
	PangoFontMetrics *new_metrics;

	new_desc = pango_font_description_from_string (font_name);

	// LAR - TEST
	context = pango_context_new();

	/* FIXME - Should get the locale language here */
	pango_context_set_language (context, gtk_get_default_language());

	new_font = pango_context_load_font (context, new_desc);

	new_metrics = pango_font_get_metrics (new_font, pango_context_get_language (context));

	pango_font_description_free (new_desc);
	g_object_unref (G_OBJECT (context));
	g_object_unref (G_OBJECT (new_font));

	return new_metrics;
}

GtkHex_AutoHighlight *
gtk_hex_insert_autohighlight(GtkHex *gh,
		const gchar *search,
		gint len,
		const gchar *colour)
{
	GtkHex_AutoHighlight *new = g_malloc0(sizeof(GtkHex_AutoHighlight));

	new->search_string = g_memdup(search, len);
	new->search_len = len;

	new->colour = g_strdup(colour);

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
	g_free(ahl->colour);

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

// LAR - atk is gone
#if 0
void add_atk_namedesc (GtkWidget *widget, const gchar *name, const gchar *desc)
{
        AtkObject *atk_widget;

        g_return_if_fail (GTK_IS_WIDGET (widget));
        atk_widget = gtk_widget_get_accessible (widget);

        if (name)
                atk_object_set_name (atk_widget, name);
        if (desc)
                atk_object_set_description (atk_widget, desc);
}

void add_atk_relation (GtkWidget *obj1, GtkWidget *obj2, AtkRelationType type)
{

        AtkObject *atk_obj1, *atk_obj2;
        AtkRelationSet *relation_set;
        AtkRelation *relation;

        g_return_if_fail (GTK_IS_WIDGET (obj1));
        g_return_if_fail (GTK_IS_WIDGET (obj2));

        atk_obj1 = gtk_widget_get_accessible (obj1);
        atk_obj2 = gtk_widget_get_accessible (obj2);

        relation_set = atk_object_ref_relation_set (atk_obj1);
        relation = atk_relation_new (&atk_obj2, 1, type);
        atk_relation_set_add (relation_set, relation);
        g_object_unref (G_OBJECT (relation));
}
#endif
