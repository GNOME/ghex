/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex.h - definition of a GtkHex widget, modified for use with GnomeMDI

   Copyright (C) 1997 - 2004 Free Software Foundation

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

#ifndef __GTKHEX_H__
#define __GTKHEX_H__

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <hex-document.h>

G_BEGIN_DECLS

/* how to group bytes? */
#define GROUP_BYTE 1
#define GROUP_WORD 2
#define GROUP_LONG 4

#define LOWER_NIBBLE TRUE
#define UPPER_NIBBLE FALSE

#define GTK_HEX(obj)          GTK_CHECK_CAST (obj, gtk_hex_get_type (), GtkHex)
#define GTK_HEX_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_hex_get_type (), GtkHexClass)
#define GTK_IS_HEX(obj)       GTK_CHECK_TYPE (obj, gtk_hex_get_type ())

typedef struct _GtkHex GtkHex;
typedef struct _GtkHexClass GtkHexClass;
typedef struct _GtkHexChangeData GtkHexChangeData;

typedef struct _GtkHex_Highlight GtkHex_Highlight;

/* start_line and end_line only have to be set (and valid) of
 * valid is set to TRUE. */
struct _GtkHex_Highlight
{
	gint start, end;
	gint start_line, end_line;
	GtkStyle *style; /* NULL for parent style */
	gint min_select;

	GtkHex_Highlight *prev, *next;
	gboolean valid;
};

/* used to automatically highlight all visible occurances
 * of the string.
 */
typedef struct _GtkHex_AutoHighlight GtkHex_AutoHighlight;

struct _GtkHex
{
	GtkFixed fixed;
	
	HexDocument *document;
	
	GtkWidget *xdisp, *adisp, *scrollbar;
	GtkWidget *offsets;

	PangoLayout *xlayout, *alayout, *olayout; /* Changes for Gnome 2.0 */

	GtkAdjustment *adj;

	PangoFontMetrics *disp_font_metrics;
	PangoFontDescription *font_desc;

	GdkGC *xdisp_gc, *adisp_gc, *offsets_gc;
	
	gint active_view;
	
	guint char_width, char_height;
	guint button;
	
	guint cursor_pos;
	GtkHex_Highlight selection;
	gint lower_nibble;
	
	guint group_type;
	
	gint lines, vis_lines, cpl, top_line;
	gint cursor_shown;
	
	gint xdisp_width, adisp_width;
	
	/* buffer for storing formatted data for rendering.
	   dynamically adjusts its size to the display size */
	guchar *disp_buffer;

	GtkHex_AutoHighlight *auto_highlight;
	
	gint scroll_dir;
	guint scroll_timeout;
	gboolean show_offsets;
	gboolean insert;
	gboolean selecting;
};

struct _GtkHexClass
{
	GtkFixedClass parent_class;

	GtkClipboard *clipboard, *primary;
	
	void (*cursor_moved)(GtkHex *);
	void (*data_changed)(GtkHex *, gpointer);
	void (*cut_clipboard)(GtkHex *);
	void (*copy_clipboard)(GtkHex *);
	void (*paste_clipboard)(GtkHex *);
};

guint gtk_hex_get_type(void);

GtkWidget *gtk_hex_new(HexDocument *);

void gtk_hex_set_cursor(GtkHex *, gint);
void gtk_hex_set_cursor_xy(GtkHex *, gint, gint);
void gtk_hex_set_nibble(GtkHex *, gint);

guint gtk_hex_get_cursor(GtkHex *);
guchar gtk_hex_get_byte(GtkHex *, guint);

void gtk_hex_set_group_type(GtkHex *, guint);

void gtk_hex_show_offsets(GtkHex *, gboolean);
void gtk_hex_set_font(GtkHex *, PangoFontMetrics *, PangoFontDescription *);

void gtk_hex_set_insert_mode(GtkHex *, gboolean);

PangoFontMetrics* gtk_hex_load_font (const char *font_name); 

void gtk_hex_copy_to_clipboard(GtkHex *gh);
void gtk_hex_cut_to_clipboard(GtkHex *gh);
void gtk_hex_paste_from_clipboard(GtkHex *gh);

void add_atk_namedesc(GtkWidget *widget, const gchar *name, const gchar *desc);
void add_atk_relation(GtkWidget *obj1, GtkWidget *obj2, AtkRelationType type);

void     gtk_hex_set_selection(GtkHex *gh, gint start, gint end);
gboolean gtk_hex_get_selection(GtkHex *gh, gint *start, gint *end);
void     gtk_hex_clear_selection(GtkHex *gh);
void     gtk_hex_delete_selection(GtkHex *gh);

GtkHex_AutoHighlight *gtk_hex_insert_autohighlight(GtkHex *gh,
												   const gchar *search,
												   gint len,
                                                   const gchar *colour);
void gtk_hex_delete_autohighlight(GtkHex *gh, GtkHex_AutoHighlight *ahl);

G_BEGIN_DECLS

#endif
