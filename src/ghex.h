/*
 * ghex.h - defines GHex ;)
 * written by jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */

#ifndef GHEX_H
#define GHEX_H
#include <config.h>
#include <gnome.h>
#include <gtk/gtk.h>
#include <glib.h>

#include <stdio.h>

#include "hex-document.h"
#include "gtkhex.h"
#include "gnome-mdi.h"

#define NO_BUFFER_LABEL "No buffer"
#define DEFAULT_FONT    "-*-courier-medium-r-normal--12-*-*-*-*-*-*-*"

typedef struct _PropertyUI {
  GnomePropertyBox *pbox;
  GtkRadioButton *mdi_type[3];
  GtkRadioButton *group_type[3];
  GtkButton *font_button;
} PropertyUI;

extern GnomeMDI *mdi;
extern gint mdi_mode;

extern GtkWidget *file_sel;
extern GtkWidget *find_dialog, *replace_dialog, *jump_dialog;
extern PropertyUI *prefs_ui;

extern GtkCheckMenuItem *save_config_item;

extern GdkFont *def_font;
extern gchar *def_font_name;

extern gint def_group_type;
extern guint group_type[3];
extern gchar *group_type_label[3];

extern guint mdi_type[3];
extern gchar *mdi_type_label[3];

void setup_ui();

void redraw_widget(GtkWidget *);

GList *create_mdi_menus(GnomeMDI *);

void create_find_dialog(GtkWidget **);
void create_replace_dialog(GtkWidget **);
void create_jump_dialog(GtkWidget **);
void create_prefs_dialog(PropertyUI **);

void show_message(gchar *);
void report_error(gchar *);
gint ask_user(GnomeMessageBox *);

/* config stuff */
void save_configuration();
void load_configuration();

/* misc */
gint compare_data(guchar *, guchar *, gint);

#endif













