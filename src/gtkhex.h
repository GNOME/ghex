/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex.h - definition of a GtkHex widget, modified for use with GnomeMDI

   Copyright (C) 1997, 1998 Free Software Foundation

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

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* how to group bytes? */
#define GROUP_BYTE 1
#define GROUP_WORD 2
#define GROUP_LONG 4

#define VIEW_HEX 1
#define VIEW_ASCII 2

#define LOWER_NIBBLE TRUE
#define UPPER_NIBBLE FALSE

#define GTK_HEX(obj)          GTK_CHECK_CAST (obj, gtk_hex_get_type (), GtkHex)
#define GTK_HEX_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_hex_get_type (), GtkHexClass)
#define GTK_IS_HEX(obj)       GTK_CHECK_TYPE (obj, gtk_hex_get_type ())

typedef struct _GtkHex GtkHex;
typedef struct _GtkHexClass GtkHexClass;
typedef struct _GtkHexChangeData GtkHexChangeData;

struct _GtkHex {
	GtkFixed fixed;
	
	HexDocument *document;
	
	GtkWidget *xdisp, *adisp, *scrollbar;
	GtkWidget *offsets;

	GtkAdjustment *adj;
	
	GdkFont *disp_font;
	GdkGC *xdisp_gc, *adisp_gc, *offsets_gc;
	
	gint active_view;
	
	guint char_width, char_height;
	guint button;
	
	guint cursor_pos;
	gint lower_nibble;
	
	guint group_type;
	
	gint lines, vis_lines, cpl, top_line;
	gint cursor_shown;
	
	gint xdisp_width, adisp_width;
	
	/* buffer for storing formatted data for rendering.
	   dynamically adjusts its size to the display size */
	guchar *disp_buffer;
	
	gint scroll_dir;
	guint scroll_timeout;
	gboolean show_offsets;
	gboolean insert;
};

struct _GtkHexClass {
	GtkFixedClass parent_class;
	
	void (*cursor_moved)(GtkHex *);
	void (*data_changed)(GtkHex *, gpointer);
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
void gtk_hex_set_font(GtkHex *, GdkFont *);

void gtk_hex_set_insert_mode(GtkHex *, gboolean);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
