/*
 * gtkhex.h - definition of a GtkHex widget, modified for use with GnomeMDI
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
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
#define IS_GTK_HEX(obj)       GTK_CHECK_TYPE (obj, gtk_hex_get_type ())
  
typedef struct _GtkHex GtkHex;
typedef struct _GtkHexClass GtkHexClass;
typedef struct _GtkHexChangeData GtkHexChangeData;

struct _GtkHex {
  GtkFixed fixed;

  HexDocument *document;

  GtkWidget *xdisp, *adisp, *scrollbar;

  GtkAdjustment *adj;
  
  GdkFont *disp_font;
  GdkGC *xdisp_gc, *adisp_gc;

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
};

struct _GtkHexClass {
  GtkFixedClass parent_class;

  void (*cursor_moved)(GtkHex *);
  void (*data_changed)(GtkHex *, gpointer);
};

guint gtk_hex_get_type(void);

GtkWidget *gtk_hex_new(HexDocument *);

void gtk_hex_data_changed(GtkHex *, gint, gint);

void gtk_hex_set_cursor(GtkHex *, gint);
void gtk_hex_set_cursor_xy(GtkHex *, gint, gint);
void gtk_hex_set_nibble(GtkHex *, gint);

gint gtk_hex_get_cursor(GtkHex *);
guchar gtk_hex_get_byte(GtkHex *, guint);

void gtk_hex_set_group_type(GtkHex *, guint);

void gtk_hex_set_font(GtkHex *, GdkFont *);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif



