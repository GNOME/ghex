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

#define NO_BUFFER_LABEL "No buffer"
#define DEFAULT_FONT    "-*-courier-medium-r-normal--12-*-*-*-*-*-*-*"

extern GnomeUIInfo help_menu[], file_menu[], view_menu[], main_menu[];

#define DATA_TYPE_HEX   0
#define DATA_TYPE_ASCII 1

typedef struct _PropertyUI {
  GnomePropertyBox *pbox;
  GtkRadioButton *mdi_type[3];
  GtkRadioButton *group_type[3];
  GtkButton *font_button;
} PropertyUI;

typedef struct _JumpDialog {
  GtkWidget *window;
  GtkWidget *int_entry;
  GtkWidget *ok, *cancel;
} JumpDialog;

typedef struct _ReplaceDialog {
  GtkWidget *window;
  GtkWidget *f_string, *r_string;
  GtkWidget *replace, *replace_all, *next, *close;
  GtkWidget *type_button[2];

  gint search_type;
} ReplaceDialog; 

typedef struct _FindDialog {
  GtkWidget *window;
  GtkWidget *f_string;
  GtkWidget *f_next, *f_prev, *f_close;
  GtkWidget *type_button[2];

  gint search_type;
} FindDialog;

typedef struct _Converter {
  GtkWidget *window;
  GtkWidget *entry[4];
  GtkWidget *close;

  gulong value;
} Converter;

extern GnomeMDI *mdi;
extern gint mdi_mode;

extern GtkWidget *file_sel;

extern FindDialog find_dialog;
extern ReplaceDialog replace_dialog;
extern JumpDialog jump_dialog;
extern Converter converter;

extern PropertyUI prefs_ui;

extern GtkCheckMenuItem *save_config_item;

extern GdkFont *def_font;
extern gchar *def_font_name;

extern gint def_group_type;
extern guint group_type[3];
extern gchar *group_type_label[3];

extern guint mdi_type[3];
extern gchar *mdi_type_label[3];

extern guint search_type;
extern gchar *search_type_label[2];

void create_find_dialog(FindDialog *);
void create_replace_dialog(ReplaceDialog *);
void create_jump_dialog(JumpDialog *);
void create_converter(Converter *);
void create_prefs_dialog(PropertyUI *);

void show_message(gchar *);
void report_error(gchar *);
gint ask_user(GnomeMessageBox *);

/* config stuff */
void save_configuration();
void load_configuration();

#endif













