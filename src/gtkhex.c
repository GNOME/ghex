/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex.c - a GtkHex widget, modified for use in GHex

   Copyright (C) 1998 - 2003 Free Software Foundation

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

#include <gnome.h>
#include <gdk/gdkkeysyms.h>

#include "hex-document.h"
#include "gtkhex.h"

#include "ghex-marshal.h"

#define DISPLAY_BORDER 4

#define DEFAULT_CPL 32
#define DEFAULT_LINES 16

#define SCROLL_TIMEOUT 100

/* jaKa: this causes trouble to pango */
#if 0 
#  define is_displayable(c) ((char_widths[(guchar)c])?1:0)
#else 
#  define is_displayable(c) (((c) >= 0x20) && ((c) <= 0x7f))
#endif /* 0 */

typedef void (*DataChangedSignal)(GtkObject *, gpointer, gpointer);

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

static gint gtkhex_signals[LAST_SIGNAL] = { 0 };

static GtkFixedClass *parent_class = NULL;
static gchar *char_widths = NULL;

static void render_hex_lines(GtkHex *, gint, gint);
static void render_ascii_lines(GtkHex *, gint, gint);
static void gtk_hex_set_selection(GtkHex *gh, gint start, gint end);

static guint32 get_event_time() {
	GdkEvent *event;
	guint32 tm = GDK_CURRENT_TIME;
  
	event = gtk_get_current_event();
  
	if(event) {
		switch (event->type) {
		case GDK_MOTION_NOTIFY:
			tm = event->motion.time; break;
		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
			tm = event->button.time; break;
		case GDK_KEY_PRESS:
		case GDK_KEY_RELEASE:
			tm = event->key.time; break;
		case GDK_ENTER_NOTIFY:
		case GDK_LEAVE_NOTIFY:
			tm = event->crossing.time; break;
		case GDK_PROPERTY_NOTIFY:
			tm = event->property.time; break;
		case GDK_SELECTION_CLEAR:
		case GDK_SELECTION_REQUEST:
		case GDK_SELECTION_NOTIFY:
			tm = event->selection.time; break;
		case GDK_PROXIMITY_IN:
		case GDK_PROXIMITY_OUT:
			tm = event->proximity.time; break;
		default:			/* use current time */
			break;
		}
		
		gdk_event_free(event);
	}
  
	return tm;
}

/*
 * simply forces widget w to redraw itself
 */
static void redraw_widget(GtkWidget *w) {
	if(!GTK_WIDGET_REALIZED(w))
		return;

	gdk_window_invalidate_rect (w->window, NULL, FALSE);
}

static gint widget_get_xt(GtkWidget *w) {
	return w->style->xthickness; /* Changed in Gtk-2.0 -- SnM */
}

static gint widget_get_yt(GtkWidget *w) {
	return w->style->ythickness; /* Changed in Gtk-2.0 -- SnM */
}
/*
 * ?_to_pointer translates mouse coordinates in hex/ascii view
 * to cursor coordinates.
 */
static void hex_to_pointer(GtkHex *gh, guint mx, guint my) {
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

static void ascii_to_pointer(GtkHex *gh, gint mx, gint my) {
	int cy;
	
	cy = gh->top_line + my/gh->char_height;
	
	gtk_hex_set_cursor_xy(gh, mx/gh->char_width, cy);
}

static guint get_max_char_width(GtkHex *gh, PangoFontMetrics *font_metrics) {
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

void format_xbyte(GtkHex *gh, gint pos, gchar buf[2]) {
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
gint format_xblock(GtkHex *gh, gchar *out, guint start, guint end) {
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

gint format_ablock(GtkHex *gh, gchar *out, guint start, guint end) {
	int i, j;
	guchar c;

	for(i = start, j = 0; i < end; i++, j++) {
		c = gtk_hex_get_byte(gh, i);
		if(is_displayable(c))
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
static gint get_xcoords(GtkHex *gh, gint pos, gint *x, gint *y) {
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

static gint get_acoords(GtkHex *gh, gint pos, gint *x, gint *y) {
	gint cx, cy;
	
	if(gh->cpl == 0)
		return FALSE;

	cy = pos / gh->cpl;
	cy -= gh->top_line;
	if(cy < 0)
		return FALSE;
	cy *= gh->char_height;
	
	cx = gh->char_width*(pos % gh->cpl);
	
	*x = cx;
	*y = cy;
	
	return TRUE;
}


/*
 * renders a byte at offset pos in both displays
 */ 
static void render_byte(GtkHex *gh, gint pos) {
	gint cx, cy;
	gchar buf[2];

	if(gh->xdisp_gc == NULL || gh->adisp_gc == NULL ||
	   !GTK_WIDGET_REALIZED(gh->xdisp) || !GTK_WIDGET_REALIZED(gh->adisp))
		return;

	if(!get_xcoords(gh, pos, &cx, &cy))
		return;

	format_xbyte(gh, pos, buf);

	gdk_gc_set_foreground(gh->xdisp_gc, &GTK_WIDGET(gh)->style->base[GTK_STATE_NORMAL]);
	gdk_draw_rectangle(gh->xdisp->window, gh->xdisp_gc, TRUE,
					   cx, cy, 2*gh->char_width, gh->char_height);

	if(pos < gh->document->file_size) {
		gdk_gc_set_foreground(gh->xdisp_gc, &GTK_WIDGET(gh)->style->text[GTK_STATE_NORMAL]);
		/* Changes for Gnome 2.0 */
		pango_layout_set_text (gh->xlayout, buf, 2);
		gdk_draw_layout (gh->xdisp->window, gh->xdisp_gc, cx, cy, gh->xlayout);

	}
	
	if(!get_acoords(gh, pos, &cx, &cy))
		return;

	gdk_gc_set_foreground(gh->adisp_gc, &GTK_WIDGET(gh)->style->base[GTK_STATE_NORMAL]);
	gdk_draw_rectangle(gh->adisp->window, gh->adisp_gc, TRUE,
					   cx, cy, gh->char_width, gh->char_height);
	if(pos < gh->document->file_size) {
		gdk_gc_set_foreground(gh->adisp_gc, &GTK_WIDGET(gh)->style->text[GTK_STATE_NORMAL]);
		buf[0] = gtk_hex_get_byte(gh, pos);
		if(!is_displayable((guchar)buf[0]))
			buf[0] = '.';
		/* Changes for Gnome 2.0 */
		pango_layout_set_text (gh->alayout, buf, 1);
		gdk_draw_layout (gh->adisp->window, gh->adisp_gc, cx, cy, gh->alayout);

	}
}

/*
 * the cursor rendering stuff...
 */
static void render_ac(GtkHex *gh) {
	gint cx, cy;
	static guchar c[2] = "\0\0";
	GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));
	
	if(!GTK_WIDGET_REALIZED(gh->adisp))
		return;
	
	if(get_acoords(gh, gh->cursor_pos, &cx, &cy)) {
		c[0] = gtk_hex_get_byte(gh, gh->cursor_pos);
		if(!is_displayable(c[0]))
			c[0] = '.';

		if(gh->active_view == VIEW_ASCII) {
			gdk_gc_set_foreground(gh->adisp_gc, &GTK_WIDGET(gh)->style->base[GTK_STATE_ACTIVE]);
		}
		else {
			gdk_gc_set_foreground(gh->adisp_gc, &GTK_WIDGET(gh)->style->fg[GTK_STATE_INSENSITIVE]);
		}

		gdk_draw_rectangle(gh->adisp->window, gh->adisp_gc,
						   TRUE, cx, cy, gh->char_width, gh->char_height - 1);
		gdk_gc_set_foreground(gh->adisp_gc, &(GTK_WIDGET(gh)->style->black));
		/* Changes for Gnome 2.0 */
		pango_layout_set_text (gh->alayout, c, 1);
		gdk_draw_layout (gh->adisp->window, gh->adisp_gc, cx, cy, gh->alayout);
	}
}

static void render_xc(GtkHex *gh) {
	gint cx, cy, i;
	static guchar c[2];
	GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));

	if(!GTK_WIDGET_REALIZED(gh->xdisp))
		return;

	if(get_xcoords(gh, gh->cursor_pos, &cx, &cy)) {
		format_xbyte(gh, gh->cursor_pos, c);
		if(gh->lower_nibble) {
			cx += gh->char_width;
			i = 1;
		}
		else {
			c[1] = 0;
			i = 0;
		}

		if(gh->active_view == VIEW_HEX) {
			gdk_gc_set_foreground(gh->xdisp_gc, &GTK_WIDGET(gh)->style->base[GTK_STATE_ACTIVE]);
		}
		else {
			gdk_gc_set_foreground(gh->xdisp_gc, &GTK_WIDGET(gh)->style->fg[GTK_STATE_INSENSITIVE]);
		}

		gdk_draw_rectangle(GTK_WIDGET(gh->xdisp)->window, gh->xdisp_gc,
						   TRUE, cx, cy, gh->char_width, gh->char_height - 1);
		gdk_gc_set_foreground(gh->xdisp_gc, &(GTK_WIDGET(gh)->style->black));
		pango_layout_set_text (gh->xlayout, &c[i], 1);
		gdk_draw_layout (gh->xdisp->window, gh->xdisp_gc, cx, cy, gh->xlayout);
	}
}

static void show_cursor(GtkHex *gh) {
	if(!gh->cursor_shown) {
		if(gh->xdisp_gc != NULL || gh->adisp_gc != NULL ||
		   GTK_WIDGET_REALIZED(gh->xdisp) || GTK_WIDGET_REALIZED(gh->adisp)) {
			render_xc(gh);
			render_ac(gh);
		}
		gh->cursor_shown = TRUE;
	}
}

static void hide_cursor(GtkHex *gh) {
	if(gh->cursor_shown) {
		if(gh->xdisp_gc != NULL || gh->adisp_gc != NULL ||
		   GTK_WIDGET_REALIZED(gh->xdisp) || GTK_WIDGET_REALIZED(gh->adisp))
			render_byte(gh, gh->cursor_pos);
		gh->cursor_shown = FALSE;
	}
}

/*
 * when calling render_*_lines() the imin and imax arguments are the
 * numbers of the first and last line TO BE DISPLAYED in the range
 * [0 .. gh->vis_lines-1] AND NOT [0 .. gh->lines]!
 */
static void render_hex_lines(GtkHex *gh, gint imin, gint imax) {
	GtkWidget *w = gh->xdisp;
	gint i, cursor_line, sl, el, start, end;
	gint xcpl = gh->cpl*2 + gh->cpl/gh->group_type;
	gint frm_len, tmp, cursor_off = 0, len;

	if( (!GTK_WIDGET_REALIZED(gh)) || (gh->cpl == 0) )
		return;

	cursor_line = gh->cursor_pos / gh->cpl - gh->top_line;

	tmp = gh->sel_start;
	end = gh->sel_end;
	if(tmp < end)
		start = tmp;
	else {
		start = end;
		end = tmp;
	}
	end--;
	sl = start/gh->cpl - gh->top_line;
	el = end/gh->cpl - gh->top_line;

	gdk_gc_set_foreground(gh->xdisp_gc, &GTK_WIDGET(gh)->style->base[GTK_STATE_NORMAL]);
	gdk_draw_rectangle(w->window, gh->xdisp_gc, TRUE,
					   0, imin*gh->char_height, w->allocation.width,
					   (imax - imin + 1)*gh->char_height);
  
	imax = MIN(imax, gh->vis_lines);
	imax = MIN(imax, gh->lines);

	gdk_gc_set_foreground(gh->xdisp_gc, &GTK_WIDGET(gh)->style->text[GTK_STATE_NORMAL]);

	frm_len = format_xblock(gh, gh->disp_buffer, (gh->top_line+imin)*gh->cpl,
							MIN((gh->top_line+imax+1)*gh->cpl, gh->document->file_size) );
	
	for(i = imin; i <= imax; i++) {
		tmp = (gint)frm_len - (gint)((i - imin)*xcpl);
		if(tmp <= 0)
			return;

		if(gh->sel_start != gh->sel_end) {
			if(i == sl) {
				cursor_off = 2*(start%gh->cpl) + (start%gh->cpl)/gh->group_type;
				if(i == el)
					len = 2*(end%gh->cpl + 1) + (end%gh->cpl)/gh->group_type;
				else
					len = xcpl;
				len = len - cursor_off;
				if(len > 0)
					gtk_draw_box(GTK_WIDGET(gh)->style, gh->xdisp->window,
								 GTK_STATE_ACTIVE, GTK_SHADOW_NONE,
								 cursor_off*gh->char_width, i*gh->char_height,
								 len*gh->char_width, gh->char_height);
			}
			else if(i == el) {
				cursor_off = 2*(end%gh->cpl + 1) + (end%gh->cpl)/gh->group_type;
				if(cursor_off > 0)
					gtk_draw_box(GTK_WIDGET(gh)->style, gh->xdisp->window,
								 GTK_STATE_ACTIVE, GTK_SHADOW_NONE,
								 0, i*gh->char_height,
								 cursor_off*gh->char_width, gh->char_height);
			}
			else if(i > sl && i < el) {
				gtk_draw_box(GTK_WIDGET(gh)->style, gh->xdisp->window,
							 GTK_STATE_ACTIVE, GTK_SHADOW_NONE,
							 0, i*gh->char_height,
							 xcpl*gh->char_width, gh->char_height);
			}
		} 
		/* Changes for Gnome 2.0 */
		pango_layout_set_text (gh->xlayout, gh->disp_buffer + (i - imin)*xcpl, MIN(xcpl, tmp));
		gdk_draw_layout (w->window, gh->xdisp_gc, 0, i*gh->char_height, gh->xlayout);
	}
	
	if((cursor_line >= imin) && (cursor_line <= imax) && (gh->cursor_shown))
		render_xc(gh);
}

static void render_ascii_lines(GtkHex *gh, gint imin, gint imax) {
	GtkWidget *w = gh->adisp;
	gint i, tmp, frm_len, sl, el, start, end;
	guint cursor_line, cursor_off, len;
	
	if( (!GTK_WIDGET_REALIZED(gh)) || (gh->cpl == 0) )
		return;
	
	cursor_line = gh->cursor_pos / gh->cpl - gh->top_line;
	
	tmp = gh->sel_start;
	end = gh->sel_end;
	if(tmp < end)
		start = tmp;
	else {
		start = end;
		end = tmp;
	}
	end--;
	sl = start/gh->cpl - gh->top_line;
	el = end/gh->cpl - gh->top_line;

	gdk_gc_set_foreground(gh->adisp_gc, &GTK_WIDGET(gh)->style->base[GTK_STATE_NORMAL]);
	gdk_draw_rectangle(w->window, gh->adisp_gc, TRUE,
					   0, imin*gh->char_height, w->allocation.width,
					   (imax - imin + 1)*gh->char_height);
	
	imax = MIN(imax, gh->vis_lines);
	imax = MIN(imax, gh->lines);

	gdk_gc_set_foreground(gh->adisp_gc, &GTK_WIDGET(gh)->style->text[GTK_STATE_NORMAL]);
	
	frm_len = format_ablock(gh, gh->disp_buffer, (gh->top_line+imin)*gh->cpl,
							MIN((gh->top_line+imax+1)*gh->cpl, gh->document->file_size) );
	
	for(i = imin; i <= imax; i++) {
		tmp = (gint)frm_len - (gint)((i - imin)*gh->cpl);
		if(tmp <= 0)
			return;

		if(gh->sel_start != gh->sel_end) {
			if(i == sl) {
				cursor_off = start%gh->cpl;
				if(i == el)
					len = end - start + 1;
				else
					len = gh->cpl - cursor_off;
				if(len > 0)
					gtk_draw_box(GTK_WIDGET(gh)->style, gh->adisp->window,
								 GTK_STATE_ACTIVE, GTK_SHADOW_NONE,
								 cursor_off*gh->char_width, i*gh->char_height,
								 len*gh->char_width, gh->char_height);
			}
			else if(i == el) {
				cursor_off = end%gh->cpl + 1;
				if(cursor_off > 0)
					gtk_draw_box(GTK_WIDGET(gh)->style, gh->adisp->window,
								 GTK_STATE_ACTIVE, GTK_SHADOW_NONE,
								 0, i*gh->char_height,
								 cursor_off*gh->char_width, gh->char_height);
			}
			else if(i > sl && i < el) {
				gtk_draw_box(GTK_WIDGET(gh)->style, gh->adisp->window,
							 GTK_STATE_ACTIVE, GTK_SHADOW_NONE,
							 0, i*gh->char_height,
							 gh->cpl*gh->char_width, gh->char_height);
			}
		}  
		/* Changes for Gnome 2.0 */
		pango_layout_set_text (gh->alayout, gh->disp_buffer + (i - imin)*gh->cpl, MIN(gh->cpl, tmp));
		gdk_draw_layout (w->window, gh->adisp_gc, 0, i*gh->char_height, gh->alayout);

	}
	
	if((cursor_line >= imin) && (cursor_line <= imax) && (gh->cursor_shown))
		render_ac(gh);
}

static void render_offsets(GtkHex *gh, gint imin, gint imax) {
	GtkWidget *w = gh->offsets;
	gint i;
	gchar offstr[9];

	if(!GTK_WIDGET_REALIZED(gh))
		return;

	if(gh->offsets_gc == NULL) {
		gh->offsets_gc = gdk_gc_new(gh->offsets->window);
		gdk_gc_set_exposures(gh->offsets_gc, TRUE);
	}
	
	gdk_gc_set_foreground(gh->offsets_gc, &GTK_WIDGET(gh)->style->base[GTK_STATE_INSENSITIVE]);
	gdk_draw_rectangle(w->window, gh->offsets_gc, TRUE,
					   0, imin*gh->char_height, w->allocation.width,
					   (imax - imin + 1)*gh->char_height);
  
	imax = MIN(imax, gh->vis_lines);
	imax = MIN(imax, gh->lines - gh->top_line - 1);

	gdk_gc_set_foreground(gh->offsets_gc, &GTK_WIDGET(gh)->style->text[GTK_STATE_NORMAL]);
	
	for(i = imin; i <= imax; i++) {
		sprintf(offstr, "%08X", (gh->top_line + i)*gh->cpl);
		/* Changes for Gnome 2.0 */
		pango_layout_set_text (gh->olayout, offstr, 8);
		gdk_draw_layout (w->window, gh->offsets_gc, 0, i*gh->char_height, gh->olayout);

	}
}

/*
 * expose signal handlers for both displays
 */
static void hex_expose(GtkWidget *w, GdkEventExpose *event, GtkHex *gh) {
	gint imin, imax;
	
	imin = (event->area.y) / gh->char_height;
	imax = (event->area.y + event->area.height) / gh->char_height;
	if((event->area.y + event->area.height) % gh->char_height)
		imax++;

	imax = MIN(imax, gh->vis_lines);
	
	render_hex_lines(gh, imin, imax);
}

static void ascii_expose(GtkWidget *w, GdkEventExpose *event, GtkHex *gh) {
	gint imin, imax;

	imin = (event->area.y) / gh->char_height;
	imax = (event->area.y + event->area.height) / gh->char_height;
	if((event->area.y + event->area.height) % gh->char_height)
		imax++;
	
	imax = MIN(imax, gh->vis_lines);

	render_ascii_lines(gh, imin, imax);
}

static void offsets_expose(GtkWidget *w, GdkEventExpose *event, GtkHex *gh) {
	gint imin, imax;
	
	imin = (event->area.y) / gh->char_height;
	imax = (event->area.y + event->area.height) / gh->char_height;
	if((event->area.y + event->area.height) % gh->char_height)
		imax++;

	imax = MIN(imax, gh->vis_lines);
	
	render_offsets(gh, imin, imax);
}

/*
 * expose signal handler for the GtkHex itself: draws shadows around both displays
 */
static void draw_shadow(GtkWidget *widget, GdkRectangle *area) {
	GtkHex *gh = GTK_HEX(widget);
	gint border = GTK_CONTAINER(widget)->border_width;
	gint x;
	
	gdk_window_clear_area (widget->window, area->x, area->y,
						   area->width, area->height);

	x = border;
	if(gh->show_offsets) {
		gtk_draw_shadow(widget->style, widget->window,
						GTK_STATE_NORMAL, GTK_SHADOW_IN,
						border, border, 8*gh->char_width + 2*widget_get_xt(widget),
						widget->allocation.height - 2*border);
		x += 8*gh->char_width + 2*widget_get_xt(widget);
	}

	gtk_draw_shadow(widget->style, widget->window,
					GTK_STATE_NORMAL, GTK_SHADOW_IN,
					x, border, gh->xdisp_width + 2*widget_get_xt(widget),
					widget->allocation.height - 2*border);
	
	gtk_draw_shadow(widget->style, widget->window,
					GTK_STATE_NORMAL, GTK_SHADOW_IN,
					widget->allocation.width - border - gh->adisp_width - gh->scrollbar->requisition.width - 2*widget_get_xt(widget), border,
					gh->adisp_width + 2*widget_get_xt(widget),
					widget->allocation.height - 2*border);
}

/*
 * this calculates how many bytes we can stuff into one line and how many
 * lines we can display according to the current size of the widget
 */
static void recalc_displays(GtkHex *gh, guint width, guint height) {
	gint total_width = width;
	gint total_cpl, xcpl;
	gint old_cpl = gh->cpl;
	GtkRequisition req;

	gtk_widget_size_request(gh->scrollbar, &req);
	
	gh->xdisp_width = 1;
	gh->adisp_width = 1;

	total_width -= 2*GTK_CONTAINER(gh)->border_width +
		4*widget_get_xt(GTK_WIDGET(gh)) + req.width;

	if(gh->show_offsets)
		total_width -= 2*widget_get_xt(GTK_WIDGET(gh)) + 8*gh->char_width;

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

	gh->lines = gh->document->file_size / gh->cpl;
	if(gh->document->file_size % gh->cpl)
		gh->lines++;
	
	gh->vis_lines = ( (gint) (height - 2*GTK_CONTAINER(gh)->border_width - 2*widget_get_yt(GTK_WIDGET(gh))) ) / ( (gint) gh->char_height );

	gh->adisp_width = gh->cpl*gh->char_width + 1;
	xcpl = gh->cpl*2 + gh->cpl/gh->group_type;
	if(gh->cpl % gh->group_type == 0)
		xcpl--;
	gh->xdisp_width = xcpl*gh->char_width + 1;

	if(gh->disp_buffer)
		g_free(gh->disp_buffer);
	
	gh->disp_buffer = g_malloc( (xcpl + 1) * (gh->vis_lines + 1) );

	/* adjust the scrollbar and display position to
	   new sizes */
	gh->adj->value = MIN(gh->top_line*old_cpl / gh->cpl, gh->lines - gh->vis_lines);
	gh->adj->value = MAX(0, gh->adj->value);
	if((gh->cursor_pos/gh->cpl < gh->adj->value) ||
	   (gh->cursor_pos/gh->cpl > gh->adj->value + gh->vis_lines - 1)) {
		gh->adj->value = MIN(gh->cursor_pos/gh->cpl, gh->lines - gh->vis_lines);
		gh->adj->value = MAX(0, gh->adj->value);
	}
	gh->adj->lower = 0;
	gh->adj->upper = gh->lines;
	gh->adj->step_increment = 1;
	gh->adj->page_increment = gh->vis_lines - 1;
	gh->adj->page_size = gh->vis_lines;
	
	gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "changed");
	gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "value_changed");
}

/*
 * takes care of xdisp and adisp scrolling
 * connected to value_changed signal of scrollbar's GtkAdjustment
 * I cant really remember anymore, but I think it was mostly copied
 * from testgtk.c ;)
 */
static void display_scrolled(GtkAdjustment *adj, GtkHex *gh) {
	gint source_min = ((gint)adj->value - gh->top_line) * gh->char_height;
	gint source_max = source_min + gh->xdisp->allocation.height;
	gint dest_min = 0;
	gint dest_max = gh->xdisp->allocation.height;
	
	GdkRectangle rect;
	GdkEvent *event;
	
	if(gh->xdisp_gc == NULL ||
	   gh->adisp_gc == NULL ||
	   (!GTK_WIDGET_DRAWABLE(gh->xdisp)) ||
	   (!GTK_WIDGET_DRAWABLE(gh->adisp)))
		return;

	gh->top_line = (gint)adj->value;
	
	rect.x = 0;
	rect.width = -1;

	if (source_min < 0) {
		rect.y = 0;
		rect.height = -source_min;
		if (rect.height > gh->xdisp->allocation.height)
			rect.height = gh->xdisp->allocation.height;
		source_min = 0;
		dest_min = rect.height;
	}
	else {
		rect.y = 2*gh->xdisp->allocation.height - source_max;
		if (rect.y < 0)
			rect.y = 0;
		rect.height = gh->xdisp->allocation.height - rect.y;
		
		source_max = gh->xdisp->allocation.height;
		dest_max = rect.y;
	}
	
	if (source_min != source_max) {
		gdk_draw_pixmap (gh->xdisp->window,
						 gh->xdisp_gc,
						 gh->xdisp->window,
						 0, source_min,
						 0, dest_min,
						 gh->xdisp->allocation.width,
						 source_max - source_min);
		gdk_draw_pixmap (gh->adisp->window,
						 gh->adisp_gc,
						 gh->adisp->window,
						 0, source_min,
						 0, dest_min,
						 gh->adisp->allocation.width,
						 source_max - source_min);
		if(gh->offsets) {
			if(gh->offsets_gc == NULL) {
				gh->offsets_gc = gdk_gc_new(gh->offsets->window);
				gdk_gc_set_exposures(gh->offsets_gc, TRUE);
			}
			gdk_draw_pixmap (gh->offsets->window,
							 gh->offsets_gc,
							 gh->offsets->window,
							 0, source_min,
							 0, dest_min,
							 gh->offsets->allocation.width,
							 source_max - source_min);
		}
	}

	rect.width = gh->xdisp->allocation.width;
	gdk_window_invalidate_rect (gh->xdisp->window, &rect, FALSE);
	rect.width = gh->adisp->allocation.width;
	gdk_window_invalidate_rect (gh->adisp->window, &rect, FALSE);
	if(gh->offsets) {
		rect.width = gh->offsets->allocation.width;
		gdk_window_invalidate_rect (gh->offsets->window, &rect, FALSE);
	}
}

/*
 * mouse signal handlers (button 1 and motion) for both displays
 */
static gboolean scroll_timeout_handler(GtkHex *gh) {
	if(gh->scroll_dir < 0)
		gtk_hex_set_cursor(gh, MAX(0, (int)(gh->cursor_pos - gh->cpl)));
	else if(gh->scroll_dir > 0)
		gtk_hex_set_cursor(gh, MIN(gh->document->file_size - 1,
								   gh->cursor_pos + gh->cpl));
	return TRUE;
}

static void hex_button_cb(GtkWidget *w, GdkEventButton *event, GtkHex *gh) {
	if( (event->type == GDK_BUTTON_RELEASE) &&
		(event->button == 1) ) {
		if(gh->scroll_timeout != -1) {
			gtk_timeout_remove(gh->scroll_timeout);
			gh->scroll_timeout = -1;
			gh->scroll_dir = 0;
		}
		gh->selecting = FALSE;
		gtk_grab_remove(w);
		gh->button = 0;
	}
	else if((event->type == GDK_BUTTON_PRESS) && (event->button == 1)) {
		if (!GTK_WIDGET_HAS_FOCUS (gh))
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
		}
	}
	else if((event->type == GDK_BUTTON_PRESS) && (event->button == 2)) {
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

static void hex_motion_cb(GtkWidget *w, GdkEventMotion *event, GtkHex *gh) {
	gint x, y;

	gdk_window_get_pointer(w->window, &x, &y, NULL);

	if(y < 0)
		gh->scroll_dir = -1;
	else if(y >= w->allocation.height)
		gh->scroll_dir = 1;
	else
		gh->scroll_dir = 0;

	if(gh->scroll_dir != 0) {
		if(gh->scroll_timeout == -1)
			gh->scroll_timeout = gtk_timeout_add(SCROLL_TIMEOUT, (GtkFunction)scroll_timeout_handler, gh);
		return;
	}
	else {
		if(gh->scroll_timeout != -1) {
			gtk_timeout_remove(gh->scroll_timeout);
			gh->scroll_timeout = -1;
		}
	}
			
	if(event->window != w->window)
		return;

	if((gh->active_view == VIEW_HEX) && (gh->button == 1)) {
		hex_to_pointer(gh, x, y);
	}
}

static void ascii_button_cb(GtkWidget *w, GdkEventButton *event, GtkHex *gh) {
	if( (event->type == GDK_BUTTON_RELEASE) &&
		(event->button == 1) ) {
		if(gh->scroll_timeout != -1) {
			gtk_timeout_remove(gh->scroll_timeout);
			gh->scroll_timeout = -1;
			gh->scroll_dir = 0;
		}
		gh->selecting = FALSE;
		gtk_grab_remove(w);
		gh->button = 0;
	}
	else if( (event->type == GDK_BUTTON_PRESS) && (event->button == 1) ) {
		if (!GTK_WIDGET_HAS_FOCUS (gh))
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
		}
	}
	else if((event->type == GDK_BUTTON_PRESS) && (event->button == 2)) {
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

static void ascii_motion_cb(GtkWidget *w, GdkEventMotion *event, GtkHex *gh) {
	gint x, y;

	gdk_window_get_pointer(w->window, &x, &y, NULL);

	if(y < 0)
		gh->scroll_dir = -1;
	else if(y >= w->allocation.height)
		gh->scroll_dir = 1;
	else
		gh->scroll_dir = 0;

	if(gh->scroll_dir != 0) {
		if(gh->scroll_timeout == -1)
			gh->scroll_timeout = gtk_timeout_add(SCROLL_TIMEOUT, (GtkFunction)scroll_timeout_handler, gh);
		return;
	}
	else {
		if(gh->scroll_timeout != -1) {
			gtk_timeout_remove(gh->scroll_timeout);
			gh->scroll_timeout = -1;
		}
	}

	if(event->window != w->window)
		return;

	if((gh->active_view == VIEW_ASCII) && (gh->button == 1)) {
		ascii_to_pointer(gh, x, y);
	}
}

static void show_offsets_widget(GtkHex *gh) {
	gh->offsets = gtk_drawing_area_new();

	/* Modify the font for the widget */
	gtk_widget_modify_font (gh->offsets, gh->font_desc);
 
	/* Create the pango layout for the widget */
	gh->olayout = gtk_widget_create_pango_layout (gh->offsets, "");

	gtk_widget_set_events (gh->offsets, GDK_EXPOSURE_MASK);
	gtk_signal_connect(GTK_OBJECT(gh->offsets), "expose_event",
					   GTK_SIGNAL_FUNC(offsets_expose), gh);
	gtk_fixed_put(GTK_FIXED(gh), gh->offsets, 0, 0);
	gtk_widget_show(gh->offsets);
}

static void hide_offsets_widget(GtkHex *gh) {
	if(gh->offsets) {
		gtk_container_remove(GTK_CONTAINER(gh), gh->offsets);
		gh->offsets = NULL;
		gh->offsets_gc = NULL;
	}
}

static void hex_realize(GtkWidget *widget, GtkHex *gh) {
	gh->xdisp_gc = gdk_gc_new(gh->xdisp->window);
	gdk_gc_set_exposures(gh->xdisp_gc, TRUE);
}
	
static void ascii_realize(GtkWidget *widget, GtkHex *gh) {
	gh->adisp_gc = gdk_gc_new(gh->adisp->window);
	gdk_gc_set_exposures(gh->adisp_gc, TRUE);
}

static void gtk_hex_realize(GtkWidget *widget) {
	if(GTK_WIDGET_CLASS(parent_class)->realize)
		(* GTK_WIDGET_CLASS(parent_class)->realize)(widget);  	

	gdk_window_set_back_pixmap(widget->window, NULL, TRUE);
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
			gh->adj->value = MIN(gh->adj->value, gh->lines - gh->vis_lines);
			gh->adj->value = MAX(0, gh->adj->value);
			if((gh->cursor_pos/gh->cpl < gh->adj->value) ||
			   (gh->cursor_pos/gh->cpl > gh->adj->value + gh->vis_lines - 1)) {
				gh->adj->value = MIN(gh->cursor_pos/gh->cpl, gh->lines - gh->vis_lines);
				gh->adj->value = MAX(0, gh->adj->value);
			}
			gh->adj->lower = 0;
			gh->adj->upper = gh->lines;
			gh->adj->step_increment = 1;
			gh->adj->page_increment = gh->vis_lines - 1;
			gh->adj->page_size = gh->vis_lines;
			gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "changed");
			gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "value_changed");
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

	render_hex_lines(gh, start_line, end_line);
	render_ascii_lines(gh, start_line, end_line);
}

static void bytes_changed(GtkHex *gh, gint start, gint end)
{
	gint start_line = start/gh->cpl - gh->top_line;
	gint end_line = end/gh->cpl - gh->top_line;

	if(end_line < 0 ||
	   start_line > gh->vis_lines)
		return;

	start_line = MAX(start_line, 0);

	render_hex_lines(gh, start_line, end_line);
	render_ascii_lines(gh, start_line, end_line);
}

static void primary_get_cb(GtkClipboard *clipboard,
						   GtkSelectionData *data, guint info,
						   gpointer user_data) {
	GtkHex *gh = GTK_HEX(user_data);
	if(gh->sel_start != gh->sel_end) {
		gint start_pos; 
		gint end_pos;
		guchar *text;
		GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));

		start_pos = MIN(gh->sel_start, gh->sel_end);
		end_pos = MAX(gh->sel_start, gh->sel_end);
 
		text = hex_document_get_data(gh->document, start_pos,
											 end_pos - start_pos);
		gtk_selection_data_set_text(data, text, end_pos - start_pos);
		g_free(text);
	}
}

static void primary_clear_cb(GtkClipboard *clipboard,
							 gpointer user_data_or_owner) {
}

static void gtk_hex_set_selection(GtkHex *gh, gint start, gint end)
{
	gint length = gh->document->file_size;
	gint oe, os, ne, ns;
	GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));

	static const GtkTargetEntry targets[] = {
		{ "STRING", 0, TARGET_STRING }
	};
	static const gint n_targets = sizeof(targets) / sizeof(targets[0]);

	if (end < 0)
		end = length;

	if(gh->sel_start != gh->sel_end)
		gtk_clipboard_clear(klass->primary);

	os = MIN(gh->sel_start, gh->sel_end);
	oe = MAX(gh->sel_start, gh->sel_end);
	gh->sel_start = CLAMP(start, 0, length);
	gh->sel_end = MIN(end, length);
	ns = MIN(gh->sel_start, gh->sel_end);
	ne = MAX(gh->sel_start, gh->sel_end);
	if(ns != os && ne != oe) {
		bytes_changed(gh, MIN(ns, os), MAX(ne, oe));
	}
	else if(ne != oe) {
		bytes_changed(gh, MIN(ne, oe), MAX(ne, oe));
	}
	else if(ns != os) {
		bytes_changed(gh, MIN(ns, os), MAX(ns, os));		
	}

	if(gh->sel_start != gh->sel_end)
		gtk_clipboard_set_with_data(klass->primary, targets, n_targets,
									primary_get_cb, primary_clear_cb,
									gh);
}

void gtk_hex_select_region (GtkHex *gh,
							gint         start,
							gint         end) {
	
	gtk_hex_set_selection(gh, start, end);
}

void gtk_hex_delete_selection(GtkHex *gh)
{
	guint start;
	guint end;

	start = MIN(gh->sel_start, gh->sel_end);
	end = MAX(gh->sel_end, gh->sel_start);

	gtk_hex_select_region(gh, 0, 0);

	if(start != end) {
		if(start < gh->cursor_pos)
			gtk_hex_set_cursor(gh, gh->cursor_pos - end + start);
		hex_document_delete_data(gh->document, MIN(start, end), end - start, TRUE);
	}
}

void gtk_hex_copy_clipboard(GtkHex *gh)
{
	gtk_signal_emit_by_name(GTK_OBJECT(gh), "copy_clipboard");
}

void gtk_hex_cut_clipboard(GtkHex *gh)
{
	gtk_signal_emit_by_name(GTK_OBJECT(gh), "cut_clipboard");
}

void gtk_hex_paste_clipboard(GtkHex *gh)
{
	gtk_signal_emit_by_name(GTK_OBJECT(gh), "paste_clipboard");
}

static void gtk_hex_real_copy_clipboard(GtkHex *gh)
{
	guint32 time;
	gint start_pos; 
	gint end_pos;
	GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));

	start_pos = MIN(gh->sel_start, gh->sel_end);
	end_pos = MAX(gh->sel_start, gh->sel_end);
 
	if(start_pos != end_pos) {
		guchar *text = hex_document_get_data(gh->document, start_pos,
											 end_pos - start_pos);
		gtk_clipboard_set_text(klass->clipboard, text, end_pos - start_pos);
		g_free(text);
	}
}

static void gtk_hex_real_cut_clipboard(GtkHex *gh)
{
	if(gh->sel_start != -1 && gh->sel_end != -1) {
		gtk_hex_real_copy_clipboard(gh);
		gtk_hex_delete_selection(gh);
	}
}

static void gtk_hex_real_paste_clipboard(GtkHex *gh)
{
	GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));
	gchar *text;

	text = gtk_clipboard_wait_for_text(klass->clipboard);
	if(text) {
		hex_document_set_data(gh->document, gh->cursor_pos,
							  strlen(text), 0, text, TRUE);
		gtk_hex_set_cursor(gh, gh->cursor_pos + strlen(text));
		g_free(text);
	}
}

static void gtk_hex_finalize(GObject *o) {
	GtkHex *gh = GTK_HEX(o);
	
	if(gh->disp_buffer)
		g_free(gh->disp_buffer);

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

static gint gtk_hex_key_press(GtkWidget *w, GdkEventKey *event) {
	GtkHex *gh = GTK_HEX(w);
	guint old_cp = gh->cursor_pos;
	gint ret = TRUE;
	gint cx, cy;

	hide_cursor(gh);

	if(!(event->state & GDK_SHIFT_MASK)) {
		gh->selecting = FALSE;
	}
	else {
		gh->selecting = TRUE;
	}
	switch(event->keyval) {
	case GDK_BackSpace:
		if(gh->cursor_pos > 0) {
			hex_document_set_data(gh->document, gh->cursor_pos - 1,
								  0, 1, NULL, TRUE);
			gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
		}
		break;
	case GDK_Tab:
	case GDK_KP_Tab:
			if (event->state & GDK_CONTROL_MASK) {
				if (gh->active_view == VIEW_ASCII) {
					get_acoords(gh, gh->cursor_pos, &cx, &cy);
					gtk_draw_box(GTK_WIDGET(gh)->style, gh->adisp->window, GTK_STATE_NORMAL, GTK_SHADOW_IN, cx, cy, gh->char_width, gh->char_height-1);
					gh->active_view = VIEW_HEX;
				}
				else {
					get_xcoords(gh, gh->cursor_pos, &cx, &cy);
					gtk_draw_box(GTK_WIDGET(gh)->style, gh->xdisp->window, GTK_STATE_NORMAL, GTK_SHADOW_IN, cx, cy, gh->char_width, gh->char_height-1);
					gh->active_view = VIEW_ASCII;
				}
			}
			break;
	case GDK_Delete:
		if(gh->cursor_pos < gh->document->file_size) {
			hex_document_set_data(gh->document, gh->cursor_pos,
								  0, 1, NULL, TRUE);
		}
		break;
	case GDK_Up:
	case GDK_KP_8:
		gtk_hex_set_cursor(gh, gh->cursor_pos - gh->cpl);
		break;
	case GDK_Down:
	case GDK_KP_2:
		gtk_hex_set_cursor(gh, gh->cursor_pos + gh->cpl);
		break;
	case GDK_Page_Up:
		gtk_hex_set_cursor(gh, MAX(0, (gint)gh->cursor_pos - gh->vis_lines*gh->cpl));
		break;
	case GDK_Page_Down:
		gtk_hex_set_cursor(gh, MIN((gint)gh->document->file_size, (gint)gh->cursor_pos + gh->vis_lines*gh->cpl));
		break;
	default:
		if (event->state & GDK_MOD1_MASK) {
			show_cursor(gh);
			return FALSE;
		}
		if(gh->active_view == VIEW_HEX)
			switch(event->keyval) {
			case GDK_Left:
			case GDK_KP_4:
				if(!(event->state & GDK_SHIFT_MASK)) {
					gh->lower_nibble = !gh->lower_nibble;
					if(gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
				}
				else {
					gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
				}
				break;
			case GDK_Right:
			case GDK_KP_6:
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
					gh->lower_nibble = !gh->lower_nibble;
					if(!gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else if((event->keyval >= 'A')&&(event->keyval <= 'F')) {
					hex_document_set_nibble(gh->document, event->keyval - 'A' + 10,
											gh->cursor_pos, gh->lower_nibble,
											gh->insert, TRUE);
					gh->lower_nibble = !gh->lower_nibble;
					if(!gh->lower_nibble)
						gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				}
				else if((event->keyval >= 'a')&&(event->keyval <= 'f')) {
					hex_document_set_nibble(gh->document, event->keyval - 'a' + 10,
											gh->cursor_pos, gh->lower_nibble,
											gh->insert, TRUE);
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
			case GDK_Left:
			case GDK_KP_4:
				gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
				break;
			case GDK_Right:
			case GDK_KP_6:
				gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
				break;
			default:
				if(event->length != 1)
					ret = FALSE;
				else if(is_displayable(event->keyval)) {
					hex_document_set_byte(gh->document, event->keyval,
										  gh->cursor_pos, gh->insert, TRUE);
					old_cp = gh->cursor_pos;
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

/* 
 * recalculate the width of both displays and reposition and resize all
 * the children widgets and adjust the scrollbar after resizing
 * connects to the size_allocate signal of the GtkHex widget
 */
static void gtk_hex_size_allocate(GtkWidget *w, GtkAllocation *alloc) {
	GtkHex *gh;
	GtkAllocation my_alloc;
	gint border_width, xt, yt;

	gh = GTK_HEX(w);
	hide_cursor(gh);
	
	recalc_displays(gh, alloc->width, alloc->height);

	w->allocation = *alloc;
	if(GTK_WIDGET_REALIZED(w))
		gdk_window_move_resize (w->window,
								alloc->x, 
								alloc->y,
								alloc->width, 
								alloc->height);

	border_width = GTK_CONTAINER(w)->border_width;
	xt = widget_get_xt(w);
	yt = widget_get_yt(w);

   	my_alloc.x = border_width + xt;
	my_alloc.y = border_width + yt;
	my_alloc.height = MAX(alloc->height - 2*border_width - 2*yt, 1);
	if(gh->show_offsets) {
		my_alloc.width = 8*gh->char_width;
		gtk_widget_size_allocate(gh->offsets, &my_alloc);
		my_alloc.x += 2*xt + my_alloc.width;
	}
	my_alloc.width = gh->xdisp_width;
	gtk_widget_size_allocate(gh->xdisp, &my_alloc);
	my_alloc.x = alloc->width - border_width - gh->scrollbar->requisition.width;
	my_alloc.y = border_width;
	my_alloc.width = gh->scrollbar->requisition.width;
	my_alloc.height = MAX(alloc->height - 2*border_width, 1);
	gtk_widget_size_allocate(gh->scrollbar, &my_alloc);
	my_alloc.x -= gh->adisp_width + xt;
	my_alloc.y = border_width + yt;
	my_alloc.width = gh->adisp_width;
	my_alloc.height = MAX(alloc->height - 2*border_width - 2*yt, 1);
	gtk_widget_size_allocate(gh->adisp, &my_alloc);
	
	show_cursor(gh);
}

static gint gtk_hex_expose(GtkWidget *w, GdkEventExpose *event) {
	draw_shadow(w, &event->area);
	
	if(GTK_WIDGET_CLASS(parent_class)->expose_event)
		(* GTK_WIDGET_CLASS(parent_class)->expose_event)(w, event);  
	
	return TRUE;
}

static void gtk_hex_size_request(GtkWidget *w, GtkRequisition *req) {
	GtkHex *gh = GTK_HEX(w);
	GtkRequisition sb_req;

	gtk_widget_size_request(gh->scrollbar, &sb_req);
	req->width = 4*widget_get_xt(w) + 2*GTK_CONTAINER(w)->border_width +
		sb_req.width + gh->char_width * (DEFAULT_CPL + (DEFAULT_CPL - 1) /
										 gh->group_type);
	if(gh->show_offsets)
		req->width += 2*widget_get_xt(w) + 8*gh->char_width;
	req->height = DEFAULT_LINES * gh->char_height + 2*widget_get_yt(w) +
		2*GTK_CONTAINER(w)->border_width;
}

static void gtk_hex_class_init(GtkHexClass *klass, gpointer data) {
	GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	
	gtkhex_signals[CURSOR_MOVED_SIGNAL] = 
		gtk_signal_new ("cursor_moved",
			GTK_RUN_FIRST,
			GTK_CLASS_TYPE(object_class),
			GTK_SIGNAL_OFFSET (GtkHexClass, cursor_moved),
			gtk_signal_default_marshaller, GTK_TYPE_NONE, 0);

	gtkhex_signals[DATA_CHANGED_SIGNAL] = 
		gtk_signal_new ("data_changed",
			GTK_RUN_FIRST,
			GTK_CLASS_TYPE(object_class),
			GTK_SIGNAL_OFFSET (GtkHexClass, data_changed),
			ghex_marshal_VOID__POINTER, GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	gtkhex_signals[CUT_CLIPBOARD_SIGNAL] = 
		gtk_signal_new ("cut_clipboard",
			GTK_RUN_FIRST,
			GTK_CLASS_TYPE(object_class),
			GTK_SIGNAL_OFFSET (GtkHexClass, cut_clipboard),
			gtk_signal_default_marshaller, GTK_TYPE_NONE, 0);

	gtkhex_signals[COPY_CLIPBOARD_SIGNAL] = 
		gtk_signal_new ("copy_clipboard",
			GTK_RUN_FIRST,
			GTK_CLASS_TYPE(object_class),
			GTK_SIGNAL_OFFSET (GtkHexClass, copy_clipboard),
			gtk_signal_default_marshaller, GTK_TYPE_NONE, 0);

	gtkhex_signals[PASTE_CLIPBOARD_SIGNAL] = 
		gtk_signal_new ("paste_clipboard",
			GTK_RUN_FIRST,
			GTK_CLASS_TYPE(object_class),
			GTK_SIGNAL_OFFSET (GtkHexClass, paste_clipboard),
			gtk_signal_default_marshaller, GTK_TYPE_NONE, 0);

	klass->cursor_moved = NULL;
	klass->data_changed = gtk_hex_real_data_changed;
	klass->cut_clipboard = gtk_hex_real_cut_clipboard;
	klass->copy_clipboard = gtk_hex_real_copy_clipboard;
	klass->paste_clipboard = gtk_hex_real_paste_clipboard;

	klass->primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	klass->clipboard = gtk_clipboard_get(GDK_NONE);

	GTK_WIDGET_CLASS(klass)->size_allocate = gtk_hex_size_allocate;
	GTK_WIDGET_CLASS(klass)->size_request = gtk_hex_size_request;
	GTK_WIDGET_CLASS(klass)->expose_event = gtk_hex_expose;
	GTK_WIDGET_CLASS(klass)->key_press_event = gtk_hex_key_press;
	GTK_WIDGET_CLASS(klass)->realize = gtk_hex_realize;

	/* Changed in Gnome 2.0 -- SnM */
	G_OBJECT_CLASS(klass)->finalize = gtk_hex_finalize;

	parent_class = gtk_type_class (gtk_fixed_get_type ());
}

static void gtk_hex_init(GtkHex *gh, gpointer klass) {
	gh->scroll_timeout = -1;

	gh->disp_buffer = NULL;
	gh->document = NULL;

	gh->xdisp_width = gh->adisp_width = 200;
	gh->adisp_gc = gh->xdisp_gc = NULL;
	gh->active_view = VIEW_HEX;
	gh->group_type = GROUP_BYTE;
	gh->lines = gh->vis_lines = gh->top_line = gh->cpl = 0;
	gh->cursor_pos = 0;
	gh->lower_nibble = FALSE;
	gh->cursor_shown = FALSE;
	gh->button = 0;
	gh->insert = FALSE; /* default to overwrite mode */
	gh->selecting = FALSE;

	/* get ourselves a decent monospaced font for rendering text */
	gh->disp_font_metrics = gtk_hex_load_font (DEFAULT_FONT);
	gh->font_desc = pango_font_description_from_string (DEFAULT_FONT);

	gh->char_width = get_max_char_width(gh, gh->disp_font_metrics);
	gh->char_height = PANGO_PIXELS (pango_font_metrics_get_ascent (gh->disp_font_metrics)) + PANGO_PIXELS (pango_font_metrics_get_descent (gh->disp_font_metrics)) + 2;

	
	GTK_WIDGET_SET_FLAGS(gh, GTK_CAN_FOCUS);
	gtk_widget_set_events(GTK_WIDGET(gh), GDK_KEY_PRESS_MASK);
	gtk_container_border_width(GTK_CONTAINER(gh), DISPLAY_BORDER);
	
	gh->adj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
	gh->xdisp = gtk_drawing_area_new();

	/* Modify the font for the widget */
	gtk_widget_modify_font (gh->xdisp, gh->font_desc);

	/* Create the pango layout for the widget */
	gh->xlayout = gtk_widget_create_pango_layout (gh->xdisp, "");

	gtk_widget_set_events (gh->xdisp, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
						   GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK);
	gtk_signal_connect(GTK_OBJECT(gh->xdisp), "expose_event",
					   GTK_SIGNAL_FUNC(hex_expose), gh);
	gtk_signal_connect(GTK_OBJECT(gh->xdisp), "button_press_event",
					   GTK_SIGNAL_FUNC(hex_button_cb), gh);
	gtk_signal_connect(GTK_OBJECT(gh->xdisp), "button_release_event",
					   GTK_SIGNAL_FUNC(hex_button_cb), gh);
	gtk_signal_connect(GTK_OBJECT(gh->xdisp), "motion_notify_event",
					   GTK_SIGNAL_FUNC(hex_motion_cb), gh);
	gtk_signal_connect(GTK_OBJECT(gh->xdisp), "realize",
					   GTK_SIGNAL_FUNC(hex_realize), gh);
	gtk_fixed_put(GTK_FIXED(gh), gh->xdisp, 0, 0);
	gtk_widget_show(gh->xdisp);
	
	gh->adisp = gtk_drawing_area_new();

	/* Modify the font for the widget */
	gtk_widget_modify_font (gh->adisp, gh->font_desc);

	/* Create the pango layout for the widget */
	gh->alayout = gtk_widget_create_pango_layout (gh->adisp, "");

	gtk_widget_set_events (gh->adisp, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
						   GDK_BUTTON_RELEASE_MASK |GDK_BUTTON_MOTION_MASK);
	gtk_signal_connect(GTK_OBJECT(gh->adisp), "expose_event",
					   GTK_SIGNAL_FUNC(ascii_expose), gh);
	gtk_signal_connect(GTK_OBJECT(gh->adisp), "button_press_event",
					   GTK_SIGNAL_FUNC(ascii_button_cb), gh);
	gtk_signal_connect(GTK_OBJECT(gh->adisp), "button_release_event",
					   GTK_SIGNAL_FUNC(ascii_button_cb), gh);
	gtk_signal_connect(GTK_OBJECT(gh->adisp), "motion_notify_event",
					   GTK_SIGNAL_FUNC(ascii_motion_cb), gh);
	gtk_signal_connect(GTK_OBJECT(gh->adisp), "realize",
					   GTK_SIGNAL_FUNC(ascii_realize), gh);
	gtk_fixed_put(GTK_FIXED(gh), gh->adisp, 0, 0);
	gtk_widget_show(gh->adisp);
	
	gtk_signal_connect(GTK_OBJECT(gh->adj), "value_changed",
					   GTK_SIGNAL_FUNC(display_scrolled), gh);

	gh->scrollbar = gtk_vscrollbar_new(gh->adj);
	gtk_fixed_put(GTK_FIXED(gh), gh->scrollbar, 0, 0);
	gtk_widget_show(gh->scrollbar);
}

guint gtk_hex_get_type() {
	static guint gh_type = 0;
	
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

	gh = GTK_HEX (g_object_new (gtk_hex_get_type(), NULL));
	g_return_val_if_fail (gh != NULL, NULL);

	gh->document = owner;
	
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
	else if(gh->sel_end != gh->sel_start) {
		guint start = MIN(gh->sel_start, gh->sel_end);
		guint end = MAX(gh->sel_start, gh->sel_end);
		gh->sel_end = gh->sel_start = 0;
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

		hide_cursor(gh);
		
		gh->cursor_pos = index;

		if(gh->cpl == 0)
			return;
		
		y = index / gh->cpl;
		if(y >= gh->top_line + gh->vis_lines) {
			gh->adj->value = MIN(y - gh->vis_lines + 1, gh->lines - gh->vis_lines);
			gh->adj->value = MAX(gh->adj->value, 0);
			gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "value_changed");
		}
		else if (y < gh->top_line) {
			gh->adj->value = y;
			gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "value_changed");
		}      

		if(index == gh->document->file_size)
			gh->lower_nibble = FALSE;
		
		gtk_signal_emit_by_name(GTK_OBJECT(gh), "cursor_moved");

		if(gh->selecting) {
			gtk_hex_set_selection(gh, gh->sel_start, gh->cursor_pos);
			bytes_changed(gh, MIN(gh->cursor_pos, old_pos), MAX(gh->cursor_pos, old_pos));
		}
		else if(gh->sel_end != gh->sel_start) {
			guint start = MIN(gh->sel_start, gh->sel_end);
			guint end = MAX(gh->sel_start, gh->sel_end);
			gh->sel_end = gh->sel_start = 0;
			bytes_changed(gh, start, end);
		}
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

		hide_cursor(gh);
		
		gh->cursor_pos = cp;
		
		if(y >= gh->top_line + gh->vis_lines) {
			gh->adj->value = MIN(y - gh->vis_lines + 1, gh->lines - gh->vis_lines);
			gh->adj->value = MAX(0, gh->adj->value);
			gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "value_changed");
		}
		else if (y < gh->top_line) {
			gh->adj->value = y;
			gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "value_changed");
		}      
		
		gtk_signal_emit_by_name(GTK_OBJECT(gh), "cursor_moved");
		
		if(gh->selecting) {
			gtk_hex_set_selection(gh, gh->sel_start, gh->cursor_pos);
			bytes_changed(gh, MIN(gh->cursor_pos, old_pos), MAX(gh->cursor_pos, old_pos));
		}
		else if(gh->sel_end != gh->sel_start) {
			guint start = MIN(gh->sel_start, gh->sel_end);
			guint end = MAX(gh->sel_start, gh->sel_end);
			gh->sel_end = gh->sel_start = 0;
			bytes_changed(gh, start, end);
		}

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
	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	hide_cursor(gh);
	gh->group_type = gt;
	recalc_displays(gh, GTK_WIDGET(gh)->allocation.width, GTK_WIDGET(gh)->allocation.height);
	gtk_widget_queue_resize(GTK_WIDGET(gh));
	show_cursor(gh);
}

/*
 * sets font for displaying data
 */
void gtk_hex_set_font(GtkHex *gh, PangoFontMetrics *font_metrics, PangoFontDescription *font_desc) {
	PangoFontDescription *tmp_desc;
	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	if (gh->disp_font_metrics)
		pango_font_metrics_unref (gh->disp_font_metrics);

	if (gh->font_desc)
		pango_font_description_free (gh->font_desc);
	
	gh->disp_font_metrics = pango_font_metrics_ref (font_metrics);
	gh->font_desc = pango_font_description_copy (font_desc);

	if (gh->xdisp)
		gtk_widget_modify_font (gh->xdisp, gh->font_desc);

	if (gh->adisp)
		gtk_widget_modify_font (gh->adisp, gh->font_desc);

	if (gh->offsets)
		gtk_widget_modify_font (gh->offsets, gh->font_desc);


	gh->char_width = get_max_char_width(gh, gh->disp_font_metrics);
	gh->char_height = PANGO_PIXELS (pango_font_metrics_get_ascent (gh->disp_font_metrics)) + PANGO_PIXELS (pango_font_metrics_get_descent (gh->disp_font_metrics)) + 2;
	recalc_displays(gh, GTK_WIDGET(gh)->allocation.width, GTK_WIDGET(gh)->allocation.height);
	
	redraw_widget(GTK_WIDGET(gh));
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

void gtk_hex_set_insert_mode(GtkHex *gh, gboolean insert)
{
	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	gh->insert = insert;

	if(gh->cursor_pos >= gh->document->file_size)
		gh->cursor_pos = gh->document->file_size - 1;
}

PangoFontMetrics* gtk_hex_load_font (const char *font_name)
{
	PangoContext *context;
	PangoFont *new_font;
	PangoFontDescription *new_desc;
	PangoFontMetrics *new_metrics;

	new_desc = pango_font_description_from_string (font_name);

	context = gdk_pango_context_get();

	/* FIXME - Should get the locale language here */
	pango_context_set_language (context, gtk_get_default_language());

	new_font = pango_context_load_font (context, new_desc);

	new_metrics = pango_font_get_metrics (new_font, pango_context_get_language (context));

	pango_font_description_free (new_desc);
	g_object_unref (G_OBJECT (context));
	g_object_unref (G_OBJECT (new_font));

	return new_metrics;
}

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


