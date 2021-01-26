/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

/* gtkhex.c - definition of a GtkHex widget

   Copyright © 1997 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021 Logan Rathbone <poprocks@gmail.com>

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

#include "gtkhex.h"

#include <string.h>

/* Not optional. */
#include <config.h>

/* Don't move these from the source file as they are not part of the public
 * header.
 */
#include "gtkhex-layout-manager.h"

/* DEFINES */

#define CSS_NAME "hex"
/* default minimum drawing area size (for ascii and hex widgets) in pixels. */
#define DEFAULT_DA_SIZE 50
/* default characters per line (cpl) */
#define DEFAULT_CPL 32
#define DEFAULT_LINES 10
#define SCROLL_TIMEOUT 100

#define is_displayable(c) (((c) >= 0x20) && ((c) < 0x7f))
#define is_copyable(c) (is_displayable(c) || (c) == 0x0a || (c) == 0x0d)

/* ENUMS */

enum {
	UPPER_NIBBLE,
	LOWER_NIBBLE
};

enum {
	VIEW_HEX,
	VIEW_ASCII
};

enum {
	CURSOR_MOVED_SIGNAL,
	DATA_CHANGED_SIGNAL,
	CUT_CLIPBOARD_SIGNAL,
	COPY_CLIPBOARD_SIGNAL,
	PASTE_CLIPBOARD_SIGNAL,
	DRAW_COMPLETE_SIGNAL,
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

/* GtkHexPasteData - allow us to get around the issue of C-strings being
 * nul-teminated, so we can paste `0x00`'s cleanly.
 */
#define GTK_TYPE_HEX_PASTE_DATA (gtk_hex_paste_data_get_type ())
G_DECLARE_FINAL_TYPE (GtkHexPasteData, gtk_hex_paste_data, GTK, HEX_PASTE_DATA,
		GObject)

#define GTK_HEX_PASTE_DATA_MAGIC 256

/* GtkHexPasteData - Method Declarations */

static GtkHexPasteData * gtk_hex_paste_data_new (guchar *doc_data,
		guint elems);

/* GtkHexPasteData - GObject Definition */

struct _GtkHexPasteData
{
	GObject parent_instance;

	guchar *doc_data;
	int *paste_data;
	guint elems;
};

G_DEFINE_TYPE (GtkHexPasteData, gtk_hex_paste_data, G_TYPE_OBJECT)

/* </GtkHexPasteData Decls> */


/* ------------------------------
 * Main GtkHex GObject definition
 * ------------------------------
 */

struct _GtkHex
{
	GtkWidget parent_instance;

	HexDocument *document;

	GtkLayoutManager *layout_manager;

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
static char *doc_data_to_string (const guchar *data, guint len);

/* GtkHexPasteData - Helper Functions */

/* Helper function for the copy and paste stuff, since the data returned by
 * hex_document_get_data is NOT null-temrinated.
 *
 * String returned should be freed with g_free.
 */
static char *
doc_data_to_string (const guchar *data, guint len)
{
	char *str;

	str = g_malloc (len + 1);
	memcpy (str, data, len);
	str[len] = '\0';

	return str;
}

/* Creates a magic_int_array for GtkHexPasteData. Should be freed with g_free.
 */
static int *
doc_data_to_magic_int_array (const guchar *data, guint len)
{
	int *arr = 0;
	guint i;

	arr = g_malloc (len * (sizeof *arr));

	for (i = 0; i < len; ++i)
	{
		arr[i] = data[i];

		if (arr[i] == 0)
			arr[i] = GTK_HEX_PASTE_DATA_MAGIC;
	}
	return arr;
}

static guchar *
magic_int_array_to_data (int *arr, guint len)
{
	guchar *data;
	guint i;

	data = g_malloc (len);

	for (i = 0;
			i < len;
			++i, ++arr, ++data)
	{
		g_assert (arr);

		if (*arr == GTK_HEX_PASTE_DATA_MAGIC) {
			*data = 0;
		} else if (*arr < GTK_HEX_PASTE_DATA_MAGIC) {
			*data = *arr;
		} else {
			g_error ("%s: Programmer error. Nothing in a magic_int_array "
					"shall be greater than %d",
					__func__,
					GTK_HEX_PASTE_DATA_MAGIC);
		}
	}
	return data;
}

/* FIXME/TODO - this transforms certain problematic characters for copy/paste
 * to a '?'. Maybe find a home for this guy at some point.
 */
#if 0
{
	char *cp;
		for (cp = text; *cp != '\0'; ++cp)
		{
			if (! is_copyable(*cp))
				*cp = '?';
		}
}
#endif
		
/* GtkHexPasteData - Constructors and Destructors */

static void
gtk_hex_paste_data_init (GtkHexPasteData *self)
{
}

static void
gtk_hex_paste_data_class_init (GtkHexPasteDataClass *klass)
{
}


/* GtkHexPasteData - Method Definitions (all private) */

static GtkHexPasteData *
gtk_hex_paste_data_new (guchar *doc_data, guint elems)
{
	GtkHexPasteData *self;

	g_return_val_if_fail (doc_data, NULL);
	g_return_val_if_fail (elems, NULL);

	self = g_object_new (GTK_TYPE_HEX_PASTE_DATA, NULL);

	self->doc_data = doc_data;
	self->elems = elems;

	self->paste_data = doc_data_to_magic_int_array (self->doc_data,
			self->elems);

	g_assert (self->paste_data);

	return self;
}

/* String returned should be freed with g_free. */
static char *
gtk_hex_paste_data_get_string (GtkHexPasteData *self)
{
	char *string;

	g_return_val_if_fail (self->doc_data, NULL);
	g_return_val_if_fail (self->elems, NULL);

	string = doc_data_to_string (self->doc_data, self->elems);

	return string;
}

/* GtkHex - Method Definitions */

static void
popup_context_menu(GtkWidget *widget, double x, double y)
{
	GtkWidget *popover;
	GtkBuilder *builder;
	GMenuModel *menu;
	GdkRectangle rect = { 0 };

	rect.x = x;
	rect.y = y;

	builder = gtk_builder_new_from_resource ("/org/gnome/ghex/context-menu.ui");
	menu = G_MENU_MODEL(gtk_builder_get_object (builder, "context-menu"));
	popover = gtk_popover_menu_new_from_model (menu);

	/* required by TFM. */
	gtk_widget_set_parent (popover, widget);

	gtk_popover_set_pointing_to (GTK_POPOVER(popover), &rect);
	gtk_popover_popup (GTK_POPOVER(popover));

	g_object_unref (menu);
	g_object_unref (builder);
}

/* ACTIONS */

static void
copy_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GtkHex *gh = GTK_HEX(widget);

	g_return_if_fail (GTK_IS_HEX(gh));
	(void)action_name, (void)parameter;

	gtk_hex_copy_to_clipboard (gh);
}

static void
cut_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GtkHex *gh = GTK_HEX(widget);

	g_return_if_fail (GTK_IS_HEX(gh));
	(void)action_name, (void)parameter;

	gtk_hex_cut_to_clipboard (gh);
}

static void
paste_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GtkHex *gh = GTK_HEX(widget);

	g_return_if_fail (GTK_IS_HEX(gh));
	(void)action_name, (void)parameter;

	gtk_hex_paste_from_clipboard (gh);
}

static void
redo_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GtkHex *gh = GTK_HEX(widget);
	HexDocument *doc;
	HexChangeData *cd;

	(void)action_name, (void)parameter;

	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (HEX_IS_DOCUMENT(gh->document));

	/* shorthand. */
	doc = gh->document;

	if (doc->undo_stack && doc->undo_top != doc->undo_stack) {
		hex_document_redo(doc);

		cd = doc->undo_top->data;

		gtk_hex_set_cursor(gh, cd->start);
		gtk_hex_set_nibble(gh, cd->lower_nibble);
	}
}

static void
doc_undo_redo_cb (HexDocument *doc, gpointer user_data)
{
	GtkHex *gh = GTK_HEX(user_data);
	g_return_if_fail (GTK_IS_HEX (gh));
	
	gtk_widget_action_set_enabled (GTK_WIDGET(gh),
			"gtkhex.undo", hex_document_can_undo (doc));
	gtk_widget_action_set_enabled (GTK_WIDGET(gh),
			"gtkhex.redo", hex_document_can_redo (doc));
}

static void
undo_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GtkHex *gh = GTK_HEX(widget);
	HexDocument *doc;
	HexChangeData *cd;

	(void)action_name, (void)parameter;

	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (HEX_IS_DOCUMENT(gh->document));

	/* shorthand */
	doc = gh->document;

	if (doc->undo_top) {
		cd = doc->undo_top->data;

		hex_document_undo(doc);

		gtk_hex_set_cursor(gh, cd->start);
		gtk_hex_set_nibble(gh, cd->lower_nibble);
	}
}

/*
 * ?_to_pointer translates mouse coordinates in hex/ascii view
 * to cursor coordinates.
 */
static void
hex_to_pointer (GtkHex *gh, guint mx, guint my)
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
ascii_to_pointer (GtkHex *gh, gint mx, gint my)
{
	int cy;
	
	cy = gh->top_line + my/gh->char_height;
	
	gtk_hex_set_cursor_xy (gh, mx/gh->char_width, cy);
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
	
	/* update layout manager */
	if (GTK_IS_HEX_LAYOUT(gh->layout_manager)) {
		gtk_hex_layout_set_char_width (gh->layout_manager, width);
	}
	return width;
}

static void
format_xbyte (GtkHex *gh, gint pos, gchar buf[2]) {
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

static gint
format_ablock (GtkHex *gh, gchar *out, guint start, guint end)
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

    if (get_xcoords (gh, gh->cursor_pos, &cx, &cy))
	{
        if (gh->lower_nibble)
            cx += gh->char_width;

		gtk_widget_queue_draw (widget);
    }
}

static void
invalidate_ac (GtkHex *gh)
{
    GtkWidget *widget = gh->adisp;
    gint cx, cy;

    if (get_acoords (gh, gh->cursor_pos, &cx, &cy))
	{
		gtk_widget_queue_draw (widget);
    }
}

/* FIXME - THE NEXT 2 FUNCTIONS ARE DUPLICITOUS. MERGE INTO ONE. */
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

	if (get_acoords(gh, gh->cursor_pos, &cx, &cy)) {
		c[0] = gtk_hex_get_byte(gh, gh->cursor_pos);
		if (! is_displayable (c[0]))
			c[0] = '.';
	} else {
		g_critical("%s: Something has gone wrong. Can't get coordinates!",
				__func__);
		return;
	}

	gtk_style_context_save (context);

	if (gh->active_view == VIEW_ASCII)
	{
		state |= GTK_STATE_FLAG_SELECTED;
		gtk_style_context_set_state (context, state);

		gtk_render_background (context, cr,
				cx,					/* double x, */
				cy,					/* double y, */
				gh->char_width,		/* double width, */
				gh->char_height - 1);	/* double height */
	}
	else
	{
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

	if (gh->active_view == VIEW_HEX)
	{
		state |= GTK_STATE_FLAG_SELECTED;
		gtk_style_context_set_state (context, state);

		gtk_render_background (context, cr,
				cx,					/* double x, */
				cy,					/* double y, */
				gh->char_width,		/* double width, */
				gh->char_height - 1);	/* double height */
	}
	else
	{
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

/* FIXME - next 2 functions are duplicitous. Merge into one.
 */
static void
show_cursor (GtkHex *gh)
{
	if (!gh->cursor_shown)
	{
		if (gtk_widget_get_realized (gh->xdisp) ||
				gtk_widget_get_realized (gh->adisp))
		{
			invalidate_xc (gh);
			invalidate_ac (gh);
		}
		gh->cursor_shown = TRUE;
	}
}

static void
hide_cursor (GtkHex *gh)
{
	if (gh->cursor_shown)
	{
		if (gtk_widget_get_realized (gh->xdisp) ||
				gtk_widget_get_realized (gh->adisp))
		{
			invalidate_xc (gh);
			invalidate_ac (gh);
		}
		gh->cursor_shown = FALSE;
	}
}

/* FIXME - Next 2 functions are duplicitous. Merge. */

static void
render_hex_highlights (GtkHex *gh,
                       cairo_t *cr,
                       gint cursor_line)
{
	GtkHex_Highlight *curHighlight = &gh->selection;
	gint xcpl = gh->cpl*2 + gh->cpl/gh->group_type;

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

/* FIXME - Previously, this function was more sophisticated, and only
 * redrew part of the drawing area requested. Need to make an executive
 * decision as to whether that will be feasible to do for ghex4, or just
 * eliminate those and queue a redraw for the drawing area in question.
 */
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
#if 0
    gtk_widget_get_allocation (widget, &allocation);
#endif

	(void)gh, (void)imin, (void)imax; /* unused for now. See comment above. */

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

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);
	cursor_line = gh->cursor_pos / gh->cpl - gh->top_line;
	gtk_widget_get_allocation (widget, &allocation);

	/* render background. */
	gtk_render_background (context, cr,
			/* x: */		0,
			/* y: */		min_lines * gh->char_height,
			/* width: */	allocation.width,
			/* height: */	(max_lines - min_lines + 1) * gh->char_height);

	max_lines = MIN(max_lines, gh->vis_lines);
	max_lines = MIN(max_lines, gh->lines);

	/* FIXME -  Maybe break this down/comment it to make it clearer?
	 */
	frm_len = format_xblock (gh, gh->disp_buffer,
			(gh->top_line + min_lines) * gh->cpl,
			MIN( (gh->top_line + max_lines + 1) * gh->cpl,
				gh->document->file_size ));
	
	for (int i = min_lines; i <= max_lines; i++)
	{
		int tmp = frm_len - ((i - min_lines) * xcpl);

		if (tmp <= 0)
			break;

		render_hex_highlights (gh, cr, i);

		/* Set pango layout to the line of hex to render. */

		pango_layout_set_text (gh->xlayout,
				gh->disp_buffer + (i - min_lines) * xcpl,
				MIN(xcpl, tmp));

		gtk_render_layout (context, cr,
				/* x: */ 0,
				/* y: */ i * gh->char_height,
				gh->xlayout);
	}
	
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
			(gh->top_line + min_lines) * gh->cpl,
			MIN( (gh->top_line + max_lines + 1) * gh->cpl,
				gh->document->file_size ));
	
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
  
	/* update max_lines and min_lines */
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

	g_return_if_fail (GTK_IS_HEX(gh));

	/* Total number of characters that can be displayed per line on the hex
	 * widget (xcpl) is the simplest calculation:
	 */
	xcpl = width / gh->char_width;

	/* FIXME - This doesn't quite jibe with our new layout manager. Our
	 * calculations here are fine, but the layout manager has no knowledge
	 * of it, so sometimes characters get cut off if using larger group
	 * types.
	 */
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

	/* If gh->cpl is not greater than 0, something has gone wrong. */
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
	g_return_if_fail(GTK_IS_HEX(gh));

	render_ascii_lines (gh, cr, 0, gh->vis_lines);
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

	render_offsets (gh, cr, 0, gh->vis_lines);
}

/*
 * this calculates how many bytes we can stuff into one line and how many
 * lines we can display according to the current size of the widget
 */
static void
recalc_displays(GtkHex *gh)
{
	GtkWidget *widget = GTK_WIDGET (gh);
	int xcpl;

	/*
	 * Only change the value of the adjustment to put the cursor on screen
	 * if the cursor is currently within the displayed portion.
	 */
	if (gh->document->file_size == 0 || gh->cpl == 0)
		gh->lines = 1;
	else {
		gh->lines = gh->document->file_size / gh->cpl;
		if (gh->document->file_size % gh->cpl)
			gh->lines++;
	}

	/* FIXME - different than 'xcpl' in hex_draw. confusing.
	 */
	/* set number of hex characters per line */
	xcpl = gh->cpl * 2 + (gh->cpl - 1) / gh->group_type;

	if (gh->disp_buffer)
		g_free (gh->disp_buffer);
	
	gh->disp_buffer = g_malloc ((xcpl + 1) * (gh->vis_lines + 1));
}

static void
recalc_scrolling (GtkHex *gh)
{
	gboolean scroll_to_cursor;
	gdouble value;

	scroll_to_cursor = (gh->cpl == 0) ||
		((gh->cursor_pos / gh->cpl >= gtk_adjustment_get_value (gh->adj)) &&
		 (gh->cursor_pos / gh->cpl <= gtk_adjustment_get_value (gh->adj) +
			  gh->vis_lines - 1));

	/* calculate new display position */
	if (gh->cpl == 0)		/* avoid divide by zero (FIXME - feels hackish) */
		value = 0;
	else
		value = MIN (gh->top_line, gh->lines - gh->vis_lines);

	/* clamp value */
	value = MAX (0, value);

	/* keep cursor on screen if it was on screen before */
	if (gh->cpl == 0) {		/* avoid divide by zero (FIXME - feels hackish) */
		value = 0;
	}
	else if (scroll_to_cursor &&
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
}

/*
 * takes care of xdisp and adisp scrolling
 * connected to value-changed signal of scrollbar's GtkAdjustment
 */
static void
display_scrolled (GtkAdjustment *adj, GtkHex *gh)
{
	gint dx;
	gint dy;

	g_return_if_fail (gtk_widget_is_drawable (gh->xdisp) &&
			gtk_widget_is_drawable (gh->adisp));

	gh->top_line = gtk_adjustment_get_value (adj);

	gtk_hex_update_all_auto_highlights(gh, TRUE, TRUE);
	gtk_hex_invalidate_all_highlights(gh);

	/* FIXME - this works, but feels hackish. The problem is, _snapshot_child
	 * does nothing if it 'detects' that a widget does not need to be redrawn
	 * which is what it seems to think re: our drawing areas on a scroll event.
	 */
	gtk_widget_queue_draw (GTK_WIDGET(gh->adisp));
	gtk_widget_queue_draw (GTK_WIDGET(gh->xdisp));
	gtk_widget_queue_draw (GTK_WIDGET(gh->offsets));
	gtk_widget_queue_draw (GTK_WIDGET(gh));
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

static gboolean
scroll_cb (GtkEventControllerScroll *controller,
               double                    dx,
               double                    dy,
               gpointer                  user_data)
{
	GtkHex *gh = GTK_HEX (user_data);
	guint button;
	double old_value, new_value;

	g_return_val_if_fail (GTK_IS_HEX(gh), FALSE);

	old_value = gtk_adjustment_get_value(gh->adj);
	new_value = old_value + dy;

	gtk_adjustment_set_value(gh->adj, new_value);

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

	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	button = gtk_gesture_single_get_current_button
		(GTK_GESTURE_SINGLE(gesture));

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
	/* Right-click */
	else if (button == GDK_BUTTON_SECONDARY)
	{
		popup_context_menu(widget, x, y);
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

	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	button =
		gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE(gesture));

	/* Single-click */
	if (button == GDK_BUTTON_PRIMARY && n_press == 1)
	{
		if (gh->scroll_timeout != -1) {
			g_source_remove(gh->scroll_timeout);
			gh->scroll_timeout = -1;
			gh->scroll_dir = 0;
		}
		gh->selecting = FALSE;
		gh->button = 0;
	}
}

/* FIXME/TODO - UNUSED FOR NOW - HERE'S BOILERPLATE IF NEEDED LATER */
#if 0
static void
hex_drag_begin_cb (GtkGestureDrag *gesture,
               double          start_x,
               double          start_y,
               gpointer        user_data)
{
	GtkHex *gh = GTK_HEX (user_data);
	GtkWidget *widget = GTK_WIDGET (gh->xdisp);
	guint button;

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

	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	g_debug("%s: offset_x: %f - offset_y: %f",
			__func__, offset_x, offset_y);
}
#endif

static void
hex_drag_update_cb (GtkGestureDrag *gesture,
               double          offset_x,
               double          offset_y,
               gpointer        user_data)
{
	GtkHex *gh = GTK_HEX (user_data);
	GtkWidget *widget = GTK_WIDGET (gh->xdisp);
	guint button;
	double start_x, start_y;
	double x, y;
	GtkAllocation allocation;

	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	gtk_widget_get_allocation (widget, &allocation);
	gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);

	x = start_x + offset_x;
	y = start_y + offset_y;

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

	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	button = gtk_gesture_single_get_current_button
		(GTK_GESTURE_SINGLE(gesture));

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
	/* Right-click */
	else if (button == GDK_BUTTON_SECONDARY)
	{
		popup_context_menu(widget, x, y);
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

	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	button =
		gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE(gesture));

	/* Single-click */
	if (button == GDK_BUTTON_PRIMARY && n_press == 1)
	{
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
	double start_x, start_y;
	double x, y;
	GtkAllocation allocation;

	g_return_if_fail (GTK_IS_HEX(gh));
	g_return_if_fail (GTK_IS_WIDGET(widget));

	gtk_widget_get_allocation(widget, &allocation);
	gtk_gesture_drag_get_start_point(gesture, &start_x, &start_y);

	x = start_x + offset_x;
	y = start_y + offset_y;

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

	hide_cursor(gh);

	/* don't trample over Ctrl */
	if (state & GDK_CONTROL_MASK) {
		return FALSE;
	}

	/* Figure out if we're holding shift or not. */
	if (! (state & GDK_SHIFT_MASK)) {
		gh->selecting = FALSE;
	}
	else {
		gh->selecting = TRUE;
	}

	/* FIXME - This could use a cleanup. Mostly flown in from old code.
	 */
	switch(keyval) {
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

static gboolean
key_release_cb (GtkEventControllerKey *controller,
               guint                  keyval,
               guint                  keycode,
               GdkModifierType        state,
               gpointer               user_data)
{
	GtkHex *gh = GTK_HEX(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	gboolean ret = TRUE;

	/* avoid shift key getting 'stuck'
	 */
	if (state & GDK_SHIFT_MASK) {
		gh->selecting = FALSE;
	}
	return ret;
}

static void
show_offsets_widget(GtkHex *gh)
{
	g_return_if_fail (GTK_IS_WIDGET (gh->offsets));

	gtk_widget_show (gh->offsets);
}

static void
hide_offsets_widget(GtkHex *gh)
{
	g_return_if_fail (gtk_widget_get_realized (gh->offsets));

	gtk_widget_hide (gh->offsets);
}

/* FIXME - Reorganize/clean up. Mostly flown in from old code.
 */
/*
 * default data_changed signal handler
 */
static void gtk_hex_real_data_changed (GtkHex *gh, gpointer data)
{
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
bytes_changed (GtkHex *gh, int start, int end)
{
	int start_line;
	int end_line;

	/* check for divide-by-zero issues */
	g_return_if_fail (gh->cpl);

	start_line = start / gh->cpl - gh->top_line;
	start_line = MAX (start_line, 0);

	end_line = end / gh->cpl - gh->top_line;

	/* Nothing needs to be done in some instances */
	if (end_line < 0 || start_line > gh->vis_lines)
		return;

    invalidate_hex_lines (gh, start_line, end_line);
    invalidate_ascii_lines (gh, start_line, end_line);

    if (gh->show_offsets)
    {
        invalidate_offsets (gh, start_line, end_line);
    }
}

static void
gtk_hex_validate_highlight(GtkHex *gh, GtkHex_Highlight *hl)
{
	if (!hl->valid)
	{
		hl->start_line = MIN(hl->start, hl->end) / gh->cpl - gh->top_line;
		hl->end_line = MAX(hl->start, hl->end) / gh->cpl - gh->top_line;
		hl->valid = TRUE;
	}
}

static void
gtk_hex_invalidate_highlight (GtkHex *gh, GtkHex_Highlight *hl)
{
	hl->valid = FALSE;
}

static void
gtk_hex_invalidate_all_highlights (GtkHex *gh)
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

static GtkHex_Highlight *
gtk_hex_insert_highlight (GtkHex *gh,
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

static void
gtk_hex_delete_highlight (GtkHex *gh, GtkHex_AutoHighlight *ahl,
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
static gboolean
gtk_hex_compare_data (GtkHex *gh, guchar *cmp, guint pos, gint len)
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

static gboolean
gtk_hex_find_limited (GtkHex *gh, gchar *find, int findlen,
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
static void
gtk_hex_update_auto_highlight(GtkHex *gh, GtkHex_AutoHighlight *ahl,
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
		gtk_hex_insert_highlight(gh, ahl,
				foundpos, foundpos+(ahl->search_len)-1);
	}
}

static void
gtk_hex_update_all_auto_highlights (GtkHex *gh,
		gboolean delete, gboolean add)
{
	GtkHex_AutoHighlight *cur = gh->auto_highlight;

	while (cur)
	{
		gtk_hex_update_auto_highlight(gh, cur, delete, add);
		cur = cur->next;
	}
}

static void
gtk_hex_real_copy_to_clipboard (GtkHex *gh)
{
	GtkWidget *widget = GTK_WIDGET(gh);
	GdkClipboard *clipboard;
	GtkHexPasteData *paste;
	GdkContentProvider *provider_union;
	GdkContentProvider *provider_array[2];
	guint start_pos, end_pos, len;
	guchar *doc_data;
	char *string;

	clipboard = gtk_widget_get_clipboard (widget);

	start_pos = MIN(gh->selection.start, gh->selection.end);
	end_pos = MAX(gh->selection.start, gh->selection.end);

	/* +1 because we're counting the number of characters to grab here.
	 * You have to actually include the first character in the range.
	 */
	len = end_pos - start_pos + 1;
	g_return_if_fail (len);

	/* Grab the raw data from the HexDocument. */
	doc_data = hex_document_get_data (gh->document, start_pos, len);

	/* Setup a union of HexPasteData and a plain C string */
	paste = gtk_hex_paste_data_new (doc_data, len);
	g_return_if_fail (GTK_IS_HEX_PASTE_DATA(paste));
	string = gtk_hex_paste_data_get_string (paste);

	provider_array[0] =
		gdk_content_provider_new_typed (GTK_TYPE_HEX_PASTE_DATA, paste);
	provider_array[1] =
		gdk_content_provider_new_typed (G_TYPE_STRING, string);

	provider_union = gdk_content_provider_new_union (provider_array, 2);

	/* Finally, set our content to our newly created union. */
	gdk_clipboard_set_content (clipboard, provider_union);
}

static void
gtk_hex_real_cut_to_clipboard(GtkHex *gh,
		gpointer user_data)
{
	(void)user_data;

	if (gh->selection.start != -1 && gh->selection.end != -1) {
		gtk_hex_real_copy_to_clipboard(gh);
		gtk_hex_delete_selection(gh);
	}
}

static void
plaintext_paste_received_cb (GObject *source_object,
		GAsyncResult *result,
		gpointer user_data)
{
	GtkHex *gh = GTK_HEX(user_data);
	GdkClipboard *clipboard;
	char *text;
	GError *error = NULL;

	g_debug ("%s: We DON'T have our special HexPasteData. Falling back "
			"to plaintext paste.",
			__func__);

	clipboard = GDK_CLIPBOARD (source_object);

	/* Get the resulting text of the read operation */
	text = gdk_clipboard_read_text_finish (clipboard, result, &error);

	if (text) {
		hex_document_set_data (gh->document,
				gh->cursor_pos,
				strlen(text),
				0,	/* rep_len (0 to insert w/o replacing; what we want) */
				(guchar *)text,
				TRUE);

		gtk_hex_set_cursor(gh, gh->cursor_pos + strlen(text));
		
		g_free(text);
	}
	else {
		g_critical ("Error pasting text: %s", 
				error->message);
		g_error_free (error);
	}
}

static void
gtk_hex_real_paste_from_clipboard (GtkHex *gh,
		gpointer user_data)
{
	GtkWidget *widget = GTK_WIDGET(gh);
	GdkClipboard *clipboard;
	GdkContentProvider *content;
	GValue value = G_VALUE_INIT;
	GtkHexPasteData *paste;
	gboolean have_hex_paste_data = FALSE;

	(void)user_data;	/* unused */

	clipboard = gtk_widget_get_clipboard (widget);
	content = gdk_clipboard_get_content (clipboard);
	g_value_init (&value, GTK_TYPE_HEX_PASTE_DATA);

	/* If the clipboard contains our special HexPasteData, we'll use it.
	 * If not, just fall back to plaintext.
	 *
	 * Note the double test here; it seems the test is semi-superfluous for
	 * *this* purpose because _get_content will itself return NULL if the
	 * clipboard data we're getting is not owned by the process; that will
	 * pretty much *always* be the case when we're falling back to plaintext,
	 * ie, when pasting from external apps. Oh well.
	 */
	have_hex_paste_data =
		GDK_IS_CONTENT_PROVIDER (content) &&
		gdk_content_provider_get_value (content,
				&value,
				NULL);	/* GError - NULL to ignore */

	if (have_hex_paste_data)
	{
		g_debug("%s: We HAVE our special HexPasteData.",
				__func__);

		paste = GTK_HEX_PASTE_DATA(g_value_get_object (&value));

		hex_document_set_data (gh->document,
				gh->cursor_pos,
				paste->elems,
				0,	/* rep_len (0 to insert w/o replacing; what we want) */
				paste->doc_data,
				TRUE);

		gtk_hex_set_cursor(gh, gh->cursor_pos + paste->elems);
	}
	else {
		gdk_clipboard_read_text_async (clipboard,
				NULL,	/* GCancellable *cancellable */
				plaintext_paste_received_cb,
				gh);
	}
}

static void
gtk_hex_real_draw_complete (GtkHex *gh,
		gpointer user_data)
{
	(void)user_data;

	recalc_scrolling (gh);
}

static void
gtk_hex_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	GtkHex *gh = GTK_HEX(widget);
	graphene_rect_t rect;
	float width, height;
	cairo_t *cr;
	GtkWidget *child;

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

	/* queue child draw functions
	 */
	for (child = gtk_widget_get_first_child (widget);
			child != NULL;
			child = gtk_widget_get_next_sibling (child))
	{
		gtk_widget_snapshot_child (widget, child, snapshot);
	}

	g_signal_emit_by_name(G_OBJECT(gh), "draw-complete");
}

static void
gtk_hex_document_changed (HexDocument* doc, gpointer change_data,
        gboolean push_undo, gpointer data)
{
	GtkHex *gh = GTK_HEX(data);
	g_return_if_fail (GTK_IS_HEX (gh));

    gtk_hex_real_data_changed (gh, change_data);

	gtk_widget_action_set_enabled (GTK_WIDGET(gh),
			"gtkhex.undo", hex_document_can_undo (doc));
	gtk_widget_action_set_enabled (GTK_WIDGET(gh),
			"gtkhex.redo", hex_document_can_redo (doc));
}

static void
gtk_hex_dispose (GObject *object)
{
	GtkHex *gh = GTK_HEX(object);
	GtkWidget *widget = GTK_WIDGET(gh);
	GtkWidget *child;

	/* Unparent children
	 */
	g_clear_pointer (&gh->xdisp, gtk_widget_unparent);
	g_clear_pointer (&gh->adisp, gtk_widget_unparent);
	g_clear_pointer (&gh->offsets, gtk_widget_unparent);
	g_clear_pointer (&gh->scrollbar, gtk_widget_unparent);

	/* Clear layout manager
	 */
	g_clear_object (&gh->layout_manager);

	/* Clear pango layouts
	 */
	g_clear_object (&gh->xlayout);
	g_clear_object (&gh->alayout);
	g_clear_object (&gh->olayout);
	
	/* Chain up */
	G_OBJECT_CLASS(gtk_hex_parent_class)->dispose(object);
}

static void
gtk_hex_finalize (GObject *gobject)
{
	GtkHex *gh = GTK_HEX(gobject);
	
	if (gh->disp_buffer)
		g_free (gh->disp_buffer);

	/* Boilerplate; keep here. Chain up to the parent class.
	 */
	G_OBJECT_CLASS(gtk_hex_parent_class)->finalize(gobject);
}


static void
gtk_hex_class_init (GtkHexClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	/* vfuncs */

	object_class->dispose = gtk_hex_dispose;
	object_class->finalize = gtk_hex_finalize;
	widget_class->snapshot = gtk_hex_snapshot;

	/* Layout manager: box-style layout. */

	gtk_widget_class_set_layout_manager_type (widget_class,
			GTK_TYPE_HEX_LAYOUT);

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
	
	gtkhex_signals[DRAW_COMPLETE_SIGNAL] = 
		g_signal_new_class_handler ("draw-complete",
				G_OBJECT_CLASS_TYPE(object_class),
				G_SIGNAL_RUN_FIRST,
				G_CALLBACK(gtk_hex_real_draw_complete),
				NULL, NULL,
				NULL,
				G_TYPE_NONE,
				0);

	/* ACTIONS */

	gtk_widget_class_install_action (widget_class, "gtkhex.copy",
			NULL,   /* GVariant string param_type */
			copy_action);

	gtk_widget_class_install_action (widget_class, "gtkhex.cut",
			NULL,
			cut_action);

	gtk_widget_class_install_action (widget_class, "gtkhex.paste",
			NULL,
			paste_action);

	gtk_widget_class_install_action (widget_class, "gtkhex.undo",
			NULL,
			undo_action);

	gtk_widget_class_install_action (widget_class, "gtkhex.redo",
			NULL,
			redo_action);

	/* SHORTCUTS FOR ACTIONS (not to be confused with keybindings, which are
	 * set up in gtk_hex_init) */

	/* Ctrl+c - copy */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_c,
			GDK_CONTROL_MASK,
			"gtkhex.copy",
			NULL);	/* no args. */

	/* Ctrl+x - cut */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_x,
			GDK_CONTROL_MASK,
			"gtkhex.cut",
			NULL);

	/* Ctrl+v - paste */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_v,
			GDK_CONTROL_MASK,
			"gtkhex.paste",
			NULL);

	/* Ctrl+z - undo */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_z,
			GDK_CONTROL_MASK,
			"gtkhex.undo",
			NULL);

	/* Ctrl+y - redo */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_y,
			GDK_CONTROL_MASK,
			"gtkhex.redo",
			NULL);
}

static void
gtk_hex_init(GtkHex *gh)
{
	GtkWidget *widget = GTK_WIDGET(gh);

	GtkHexLayoutChild *child_info;

	GtkCssProvider *provider;
	GtkStyleContext *context;

	GtkGesture *gesture;
	GtkEventController *controller;

	gh->layout_manager = gtk_widget_get_layout_manager (widget);

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

	gtk_widget_set_can_focus (widget, TRUE);
	gtk_widget_set_focusable (widget, TRUE);

	/* Init CSS */

	/* Set context var to the widget's context at large. */
	context = gtk_widget_get_style_context (widget);

	/* set up a provider so we can feed CSS through C code. */
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
									CSS_NAME " {\n"
									 "   font-family: Monospace;\n"
									 "   font-size: 12pt;\n"
	                                 "   padding-left: 12px;\n"
	                                 "   padding-right: 12px;\n"
	                                 "}\n", -1);

	/* add the provider to our widget's style context. */
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);

	/* Setup offsets widget. */

	gh->offsets = gtk_drawing_area_new();
	gtk_widget_set_parent (gh->offsets, widget);
	child_info = GTK_HEX_LAYOUT_CHILD (gtk_layout_manager_get_layout_child
			(gh->layout_manager, gh->offsets));
	gtk_hex_layout_child_set_column (child_info, OFFSETS_COLUMN);

	/* Create the pango layout for the widget */
	gh->olayout = gtk_widget_create_pango_layout (gh->offsets, "");

	gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (gh->offsets),
			offsets_draw,
			gh,
			NULL);		/* GDestroyNotify destroy); */

	context = gtk_widget_get_style_context (GTK_WIDGET (gh->offsets));
	gtk_style_context_add_class (context, "header");

	/* hide it by default. */
	gtk_widget_hide (gh->offsets);


	/* Setup our Hex drawing area. */

	gh->xdisp = gtk_drawing_area_new();
	gtk_widget_set_parent (gh->xdisp, widget);
	child_info = GTK_HEX_LAYOUT_CHILD (gtk_layout_manager_get_layout_child
			(gh->layout_manager, gh->xdisp));
	gtk_hex_layout_child_set_column (child_info, HEX_COLUMN);

	/* Create the pango layout for the widget */
	gh->xlayout = gtk_widget_create_pango_layout (gh->xdisp, "");

	gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (gh->xdisp),
			hex_draw,
			gh,
			NULL);		/* GDestroyNotify destroy); */

	/* Set context var to hex widget's context */
	context = gtk_widget_get_style_context (GTK_WIDGET (gh->xdisp));
	/* ... and add view class so we get certain theme colours for free. */
	gtk_style_context_add_class (context, "view");
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	/* Setup our ASCII widget. */

	gh->adisp = gtk_drawing_area_new();
	gtk_widget_set_parent (gh->adisp, widget);
	child_info = GTK_HEX_LAYOUT_CHILD (gtk_layout_manager_get_layout_child
			(gh->layout_manager, gh->adisp));
	gtk_hex_layout_child_set_column (child_info, ASCII_COLUMN);

	/* Create the pango layout for the widget */
	gh->alayout = gtk_widget_create_pango_layout (gh->adisp, "");

	gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (gh->adisp),
			ascii_draw,
			gh,
			NULL);		/* GDestroyNotify destroy); */

	/* Rinse and repeat as above for ascii widget / context / view. */
	context = gtk_widget_get_style_context (GTK_WIDGET (gh->adisp));
	gtk_style_context_add_class (context, "view");
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	/* Set a minimum size for hex/ascii drawing areas. */

	gtk_widget_set_size_request (gh->adisp,
			DEFAULT_DA_SIZE, DEFAULT_DA_SIZE);
	gtk_widget_set_size_request (gh->xdisp,
			DEFAULT_DA_SIZE, DEFAULT_DA_SIZE);

	/* Initialize Adjustment */
	gh->adj = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

	/* Setup scrollbar. */
	gh->scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL,
			gh->adj);

	gtk_widget_set_parent (gh->scrollbar, widget);
	child_info = GTK_HEX_LAYOUT_CHILD (gtk_layout_manager_get_layout_child
			(gh->layout_manager, gh->scrollbar));
	gtk_hex_layout_child_set_column (child_info, SCROLLBAR_COLUMN);

	/* Connect gestures to ascii/hex drawing areas.
	 */

	/* Hex widget: */

	/* click gestures */
	gesture = gtk_gesture_click_new ();

	/* listen for any button */
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE(gesture), 0);

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

	/* FIXME/TODO - Some boilerplate if decide to use these signals. If
	 * still unused by 4.0 beta, just remove. */
#if 0
	g_signal_connect (gesture, "drag-begin",
			G_CALLBACK(hex_drag_begin_cb),
			gh);
	g_signal_connect (gesture, "drag-end",
			G_CALLBACK(hex_drag_end_cb),
			gh);
#endif
	g_signal_connect (gesture, "drag-update",
			G_CALLBACK (hex_drag_update_cb),
			gh);
	gtk_widget_add_controller (gh->xdisp,
			GTK_EVENT_CONTROLLER(gesture));

	/* ASCII widget: */

	/* click gestures */
	gesture = gtk_gesture_click_new ();

	/* listen for any button */
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE(gesture), 0);

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

	/* FIXME/TODO - Some boilerplate if decide to use these signals. If
	 * still unused by 4.0 beta, just remove. */
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

	/* Event controller - keyboard - for the widget *as a whole* */
	
	controller = gtk_event_controller_key_new ();

	g_signal_connect(controller, "key-pressed",
			G_CALLBACK(key_press_cb),
			gh);
	g_signal_connect(controller, "key-released",
			G_CALLBACK(key_release_cb),
			gh);

	gtk_widget_add_controller (widget,
			GTK_EVENT_CONTROLLER(controller));

	/* Connect signal for adjustment */

	g_signal_connect(G_OBJECT(gh->adj), "value-changed",
					 G_CALLBACK(display_scrolled), gh);

	/* ACTIONS - Undo / Redo should start out disabled. */

	gtk_widget_action_set_enabled (GTK_WIDGET(gh),
			"gtkhex.undo", FALSE);
	gtk_widget_action_set_enabled (GTK_WIDGET(gh),
			"gtkhex.redo", FALSE);
}

GtkWidget *
gtk_hex_new (HexDocument *owner)
{
	GtkHex *gh;

	gh = GTK_HEX (g_object_new (GTK_TYPE_HEX, NULL));
	g_return_val_if_fail (gh != NULL, NULL);

	gh->document = owner;

	/* Setup document signals
	 * (can't do in _init because we don't have access to that object yet).
	 */
    g_signal_connect (G_OBJECT (gh->document), "document-changed",
            G_CALLBACK (gtk_hex_document_changed), gh);

    g_signal_connect (G_OBJECT (gh->document), "undo",
            G_CALLBACK (doc_undo_redo_cb), gh);
	
    g_signal_connect (G_OBJECT (gh->document), "redo",
            G_CALLBACK (doc_undo_redo_cb), gh);
	
	return GTK_WIDGET(gh);
}


/*-------- public API starts here --------*/

void
gtk_hex_copy_to_clipboard (GtkHex *gh)
{
	g_signal_emit_by_name(G_OBJECT(gh), "copy-clipboard");
}

void
gtk_hex_cut_to_clipboard (GtkHex *gh)
{
	g_signal_emit_by_name(G_OBJECT(gh), "cut-clipboard");
}

void
gtk_hex_paste_from_clipboard (GtkHex *gh)
{
	g_signal_emit_by_name(G_OBJECT(gh), "paste-clipboard");
}

void
gtk_hex_set_selection (GtkHex *gh, gint start, gint end)
{
	gint length = gh->document->file_size;
	gint oe, os, ne, ns;

	if (end < 0)
		end = length;

	os = MIN(gh->selection.start, gh->selection.end);
	oe = MAX(gh->selection.start, gh->selection.end);

	gh->selection.start = CLAMP(start, 0, length);
	gh->selection.end = MIN(end, length);

	gtk_hex_invalidate_highlight(gh, &gh->selection);

	ns = MIN(gh->selection.start, gh->selection.end);
	ne = MAX(gh->selection.start, gh->selection.end);

	if (ns != os && ne != oe) {
		bytes_changed(gh, MIN(ns, os), MAX(ne, oe));
	}
	else if (ne != oe) {
		bytes_changed(gh, MIN(ne, oe), MAX(ne, oe));
	}
	else if (ns != os) {
		bytes_changed(gh, MIN(ns, os), MAX(ns, os));
	}
}

gboolean
gtk_hex_get_selection (GtkHex *gh, gint *start, gint *end)
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

	if (start)
		*start = ss;
	if (end)
		*end = se;

	return !(ss == se);
}

void
gtk_hex_clear_selection(GtkHex *gh)
{
	gtk_hex_set_selection(gh, 0, 0);
}

void
gtk_hex_delete_selection(GtkHex *gh)
{
	guint start;
	guint end;

	start = MIN(gh->selection.start, gh->selection.end);
	end = MAX(gh->selection.start, gh->selection.end);

	gtk_hex_clear_selection (gh);

	if (start != end)
	{
		if (start < gh->cursor_pos)
			gtk_hex_set_cursor (gh, gh->cursor_pos - end + start);

		hex_document_delete_data (gh->document,
				MIN(start, end), end - start, TRUE);
	}
}

/*
 * moves cursor to UPPER_NIBBLE or LOWER_NIBBLE of the current byte
 */
void
gtk_hex_set_nibble (GtkHex *gh, gint lower_nibble)
{
	g_return_if_fail (GTK_IS_HEX(gh));

	if (gh->selecting) {
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

	if ((index >= 0) && (index <= gh->document->file_size))
	{
		if(!gh->insert && index == gh->document->file_size)
			index--;

		index = MAX(index, 0);

		hide_cursor(gh);
		
		gh->cursor_pos = index;

		if(gh->cpl == 0)
			return;
		
		y = index / gh->cpl;
		if(y >= gh->top_line + gh->vis_lines) {
			gtk_adjustment_set_value(gh->adj,
					MIN(y - gh->vis_lines + 1, gh->lines - gh->vis_lines));
			gtk_adjustment_set_value(gh->adj,
					MAX(gtk_adjustment_get_value(gh->adj), 0));
		}
		else if (y < gh->top_line) {
			gtk_adjustment_set_value(gh->adj, y);
		}      

		if(index == gh->document->file_size)
			gh->lower_nibble = FALSE;
		
		if(gh->selecting) {
			gtk_hex_set_selection(gh, gh->selection.start, gh->cursor_pos);
			bytes_changed (gh,
					MIN(gh->cursor_pos, old_pos), MAX(gh->cursor_pos,
						old_pos));
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
 * moves cursor to column x in line y (in the whole buffer, not just the
 * currently visible part)
 */
void
gtk_hex_set_cursor_xy (GtkHex *gh, gint x, gint y)
{
	gint cp;
	guint old_pos = gh->cursor_pos;

	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	cp = y*gh->cpl + x;

	if ((y >= 0) && (y < gh->lines) && (x >= 0) &&
	   (x < gh->cpl) && (cp <= gh->document->file_size))
	{
		if (!gh->insert && cp == gh->document->file_size)
			cp--;

		cp = MAX(cp, 0);

		hide_cursor(gh);
		
		gh->cursor_pos = cp;
		
		if (y >= gh->top_line + gh->vis_lines) {
			gtk_adjustment_set_value (gh->adj,
					MIN(y - gh->vis_lines + 1, gh->lines - gh->vis_lines));
			gtk_adjustment_set_value (gh->adj,
					MAX(0, gtk_adjustment_get_value(gh->adj)));
		}
		else if (y < gh->top_line) {
			gtk_adjustment_set_value(gh->adj, y);
		}      
	
		g_signal_emit_by_name(G_OBJECT(gh), "cursor-moved");
		
		if (gh->selecting) {
			gtk_hex_set_selection(gh, gh->selection.start, gh->cursor_pos);
			bytes_changed(gh,
					MIN(gh->cursor_pos, old_pos), MAX(gh->cursor_pos,
						old_pos));
		}
		else if (gh->selection.end != gh->selection.start) {
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
guint
gtk_hex_get_cursor(GtkHex *gh)
{
	g_return_val_if_fail(gh != NULL, -1);
	g_return_val_if_fail(GTK_IS_HEX(gh), -1);

	return gh->cursor_pos;
}

/*
 * returns value of the byte at position offset
 */
guchar
gtk_hex_get_byte (GtkHex *gh, guint offset)
{
	g_return_val_if_fail(gh != NULL, 0);
	g_return_val_if_fail(GTK_IS_HEX(gh), 0);

	if((offset >= 0) && (offset < gh->document->file_size))
		return hex_document_get_byte(gh->document, offset);

	return 0;
}

/*
 * sets data group type (see GROUP_* defines at top of file)
 */
void
gtk_hex_set_group_type (GtkHex *gh, guint gt)
{
	/* FIXME - See comment above about redraws. */
#if 0
	GtkAllocation allocation;
#endif

	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	hide_cursor(gh);
	gh->group_type = gt;
#if 0
	gtk_widget_get_allocation(GTK_WIDGET(gh), &allocation);
	recalc_displays(gh, allocation.width, allocation.height);
#endif
	recalc_displays(gh);
	gtk_widget_queue_resize(GTK_WIDGET(gh));
	show_cursor(gh);
}

/*
 *  do we show the offsets of lines?
 */
void
gtk_hex_show_offsets(GtkHex *gh, gboolean show)
{
	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	if (gh->show_offsets == show)
		return;

	gh->show_offsets = show;
	if (show)
		show_offsets_widget(gh);
	else
		hide_offsets_widget(gh);
}

void
gtk_hex_set_starting_offset (GtkHex *gh, gint starting_offset)
{
	g_return_if_fail (gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));
	gh->starting_offset = starting_offset;
}

void
gtk_hex_set_insert_mode (GtkHex *gh, gboolean insert)
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

GtkAdjustment *
gtk_hex_get_adjustment(GtkHex *gh)
{
	g_return_val_if_fail (GTK_IS_ADJUSTMENT(gh->adj), NULL);

	return gh->adj;
}

HexDocument *
gtk_hex_get_document (GtkHex *gh)
{
	g_return_val_if_fail (HEX_IS_DOCUMENT(gh->document), NULL);

	return gh->document;
}

gboolean
gtk_hex_get_insert_mode (GtkHex *gh)
{
	g_assert (GTK_IS_HEX (gh));

	return gh->insert;
}

guint
gtk_hex_get_group_type (GtkHex *gh)
{
	g_assert (GTK_IS_HEX (gh));

	return gh->group_type;
}
