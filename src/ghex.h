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

#include "gtkhex.h"

#define NO_BUFFER_LABEL "No buffer"

typedef struct _FileEntry {
  gchar *file_name;
  gchar *path_end;
  FILE *file;

  guchar *contents;
  guint len;
  guint cursor_pos; /* this is only valid when hexedit is not shown!!! */
  
  GtkWidget *hexedit;
} FileEntry;

/* list of all edited files */
extern GSList *buffer_list;
extern FileEntry *active_fe;

/* should we save config when exiting ghex */
extern gint save_config_on_exit;

/* GTK ui stuff */
extern GtkWidget *app;
extern GtkWidget *tbar_box, *tbar;

extern GtkWidget *file_sel;
extern GtkWidget *find_dialog, *replace_dialog, *jump_dialog;

extern GtkCheckMenuItem *save_config_item;

extern GdkFont *def_font;
extern gchar def_font_name[];

void setup_ui();

void redraw_widget(GtkWidget *);

void menus_create(GtkMenuFactory *, GtkMenuEntry *, int);
void get_main_menu(GtkWidget **, GtkAcceleratorTable **);
guint get_desired_group_type();
void set_desired_group_type(gint type);

int add_view(FileEntry *);
void remove_view(FileEntry *);

void create_find_dialog(GtkWidget **);
void create_replace_dialog(GtkWidget **);
void create_jump_dialog(GtkWidget **);

void add_buffer_entry(FileEntry *);
void remake_buffer_menu();

void cursor_moved(GtkWidget *w);
void data_changed(GtkWidget *w, guint, guint);

void show_message(gchar *);
void report_error(gchar *);

/* IO functions */
FileEntry *open_file(gchar *);
void close_file(FileEntry *);
gint read_file(FileEntry *);
gint write_file(FileEntry *);
gint find_string_forward(FileEntry *, guint, guchar *, gint, guint *);
gint find_string_backward(FileEntry *, guint, guchar *, gint, guint *);

/* config stuff */
void save_configuration();
void load_configuration();

/* misc */
gint compare_data(guchar *, guchar *, gint);

#endif













