/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ghex.h - defines GHex ;)

   Copyright (C) 1998 - 2002 Free Software Foundation

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

#ifndef __GHEX_H__
#define __GHEX_H__

#include <config.h>
#include <gnome.h>

#include <libgnomeprint/gnome-print.h>

#include <libgnomeprint/gnome-print-master.h>

#include <stdio.h>

#include "hex-document.h"
#include "gtkhex.h"
#include "ghex-marshal.h"
#include "ghex-window.h"

G_BEGIN_DECLS

#define NO_BUFFER_LABEL "No buffer"

#define MAX_MAX_UNDO_DEPTH 100000

#define MESSAGE_LEN 512

#define DATA_TYPE_HEX   0
#define DATA_TYPE_ASCII 1

#define NUM_MDI_MODES 4

#define GHEX_URL "http://pluton.ijs.si/~jaka/gnome.html#GHEX"

typedef struct _PropertyUI {
	GtkWidget *pbox;
	GtkRadioButton *group_type[3];
	GtkWidget *font_button, *undo_spin, *box_size_spin;
	GtkWidget *offset_menu, *offset_choice[3];
	GtkWidget *format, *offsets_col;
	GtkWidget *paper_sel, *print_font_sel;
	GtkWidget *df_button, *hf_button;
	GtkWidget *df_label, *hf_label;
	GnomeFont *data_font, *header_font;
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
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *f_string;
	GtkWidget *f_next, *f_prev, *f_close;
	GtkWidget *type_button[2];
	
	gint search_type;
} FindDialog;

typedef struct _Converter {
	GtkWidget *window;
	GtkWidget *entry[5];
	GtkWidget *close;
	GtkWidget *get;

	gulong value;
} Converter;

typedef struct {
	GnomePrintMaster *master;
	GnomePrintContext *pc;
	GnomePrintConfig *config;

	GnomeFont *d_font, *h_font;
	HexDocument *doc;

	int   pages;
	gint range;
	gint page_first;
	gint page_last;
	gdouble page_width, page_height;
	gdouble margin_top, margin_bottom, margin_left, margin_right;
	gdouble printable_width, printable_height;

	gdouble header_height;
	
	gdouble font_char_width;
	gdouble font_char_height;

	int   bytes_per_row, rows_per_page;
	gdouble pad_size;
	int   offset_chars ; /* How many chars are used in the offset window */
	int   gt;            /* group_type */
	gboolean preview;
} GHexPrintJobInfo;

extern int restarted;
extern const struct poptOption options[];
extern GSList *cl_files;

extern GtkWidget      *file_sel;
extern FindDialog     *find_dialog;
extern ReplaceDialog  *replace_dialog;
extern JumpDialog     *jump_dialog;
extern Converter      *converter;
extern GtkWidget      *char_table;
extern PropertyUI     *prefs_ui;

/* our preferred settings; as only one copy of them is required,
   we'll make them global vars, although this is a bit ugly */
extern PangoFontMetrics *def_metrics;
extern PangoFontDescription *def_font_desc;

extern gchar      *def_font_name;
extern gchar      *data_font_name, *header_font_name;
extern gdouble    data_font_size, header_font_size;    
extern guint      max_undo_depth;
extern gchar      *offset_fmt;
extern gboolean   show_offsets_column;

extern gint       shaded_box_size;
extern gint       def_group_type;

extern guint group_type[3];
extern gchar *group_type_label[3];

extern guint search_type;
extern gchar *search_type_label[2];

/* creation of dialogs */
FindDialog    *create_find_dialog    (void);
ReplaceDialog *create_replace_dialog (void);
JumpDialog    *create_jump_dialog    (void);
Converter     *create_converter      (void);
GtkWidget     *create_char_table     (void);
PropertyUI    *create_prefs_dialog   (void);

/* various ui convenience functions */
void create_dialog_title   (GtkWidget *, gchar *);
gint ask_user              (GnomeMessageBox *);
GtkWidget *create_button   (GtkWidget *, const gchar *, gchar *);

/* printing */
void ghex_print_job_execute(GHexPrintJobInfo *pji);
void ghex_print_update_page_size_and_margins (HexDocument *doc, GHexPrintJobInfo *pji);

GHexPrintJobInfo *ghex_print_job_info_new(HexDocument *doc, guint group_type);
void ghex_print_job_info_destroy(GHexPrintJobInfo *pji);

/* config stuff */
void save_configuration    (void);
void load_configuration    (void);

/* hiding widgets on cancel or delete_event */
gint delete_event_cb(GtkWidget *, GdkEventAny *);
void cancel_cb(GtkWidget *, GtkWidget *);

/* session managment */
gint
save_session    (GnomeClient        *client,
				 gint                phase,
				 GnomeRestartStyle   save_style,
				 gint                shutdown,
				 GnomeInteractStyle  interact_style,
				 gint                fast,
				 gpointer            client_data);
void client_die (GnomeClient *client, gpointer client_data);

extern BonoboUIVerb ghex_verbs [];

/* Initializes the gconf client */
void ghex_prefs_init (void);

void find_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void replace_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void jump_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void set_byte_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void set_word_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void set_long_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void undo_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void redo_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void add_view_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void remove_view_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void insert_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void quit_app_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);
void file_list_activated_cb (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname);

void display_error_dialog (GHexWindow *win, const gchar *msg);
void update_dialog_titles (void);
void raise_and_focus_widget(GtkWidget *);

gboolean hex_document_ok_to_close(HexDocument *doc);


G_END_DECLS

#endif /* __GHEX_H__ */
