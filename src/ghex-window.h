/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * ghex-window.c: everything describing a single ghex window
 *
 * Copyright (C) 2002 - 2004 the Free Software Foundation
 *
 * Author: Jaka Mocnik  <jaka@gnu.org>
 */

#ifndef __GHEX_WINDOW_H__
#define __GHEX_WINDOW_H__

#include <config.h>

#include <gnome.h>
#include <bonobo-activation/bonobo-activation.h>
#include <libgnomeui/gnome-window-icon.h>
#include <gconf/gconf-client.h>
#include <bonobo.h>
#include <bonobo/bonobo-ui-main.h>

#include <math.h>
#include <ctype.h>

#include "gtkhex.h"

G_BEGIN_DECLS 

#define GHEX_TYPE_WINDOW            (ghex_window_get_type ())
#define GHEX_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GHEX_TYPE_WINDOW, GHexWindow))
#define GHEX_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GHEX_TYPE_WINDOW, GHexWindowClass))
#define GHEX_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GHEX_TYPE_WINDOW))
#define GHEX_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GHEX_TYPE_WINDOW))
#define GHEX_WINDOW_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GHEX_TYPE_WINDOW, GHexWindowClass))

typedef struct _GHexWindow      GHexWindow;
typedef struct _GHexWindowClass GHexWindowClass;

struct _GHexWindow 
{
    BonoboWindow win;

    GtkHex *gh;
    BonoboUIComponent *uic;
    gboolean changed, undo_sens, redo_sens;

    HexDialog *dialog;
    GtkWidget *dialog_widget;

    AdvancedFindDialog *advanced_find_dialog;

    gchar **uris_to_open;
};

struct _GHexWindowClass
{
    BonoboWindowClass klass;
};

GType             ghex_window_get_type           (void);
GtkWidget         *ghex_window_new               (void);
GtkWidget         *ghex_window_new_from_doc      (HexDocument *doc);
GtkWidget         *ghex_window_new_from_file     (const gchar *filename);
gboolean          ghex_window_load(GHexWindow *win, const gchar *filename);
gboolean          ghex_window_close              (GHexWindow *win);
BonoboUIComponent *ghex_window_get_ui_component  (GHexWindow *win);
const GList       *ghex_window_get_list          (void);
GHexWindow        *ghex_window_get_active        (void);
void              ghex_window_set_doc_name       (GHexWindow *win,
                                                  const gchar *name);
void              ghex_window_set_sensitivity    (GHexWindow *win);
void              ghex_window_show_status        (GHexWindow *win,
                                                  const gchar *msg);
void              ghex_window_flash              (GHexWindow *win,
                                                  const gchar * flash);
void              ghex_window_remove_doc_from_list(GHexWindow *win,
                                                   HexDocument *doc);
void              ghex_window_add_doc_to_list     (GHexWindow *win,
                                                   HexDocument *doc);
GHexWindow        *ghex_window_find_for_doc       (HexDocument *doc);

void ghex_window_sync_char_table_item(GHexWindow *win, gboolean state);
void ghex_window_sync_converter_item(GHexWindow *win, gboolean state);

gboolean ghex_window_ok_to_close(GHexWindow *win);
gboolean ghex_window_save_as(GHexWindow *win);


G_END_DECLS

#endif /* __GHEX_WINDOW_H__ */
