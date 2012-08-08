/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * ghex-window.c: a ghex window
 *
 * Copyright (C) 2002 - 2004 the Free Software Foundation
 *
 * GHex is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GHex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GHex; see the file COPYING.
 * If not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Author: Jaka Mocnik  <jaka@gnu.org>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <gio/gio.h>
#include <glib/gi18n.h>

#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h> /* for F_OK */

#include "ghex-window.h"
#include "findreplace.h"
#include "ui.h"
#include "converter.h"
#include "chartable.h"
#include "configuration.h"
#include "hex-dialog.h"

#define GHEX_WINDOW_DEFAULT_WIDTH 320
#define GHEX_WINDOW_DEFAULT_HEIGHT 256

G_DEFINE_TYPE (GHexWindow, ghex_window, GTK_TYPE_WINDOW)

static GList *window_list = NULL;
static GHexWindow *active_window = NULL;

/* what can be dragged in us... */
enum {
    TARGET_URI_LIST,
};

static void
ghex_window_drag_data_received(GtkWidget *widget,
                               GdkDragContext *context,
                               gint x, gint y,
                               GtkSelectionData *selection_data,
                               guint info, guint time)
{
    GHexWindow *win = GHEX_WINDOW(widget);
    GtkWidget *newwin;
    gchar **uri;
    gchar **uris_to_open;

    if (info != TARGET_URI_LIST)
        return;

    if (win->gh == NULL)
        newwin = GTK_WIDGET (win);
    else
        newwin = NULL;

    uri = uris_to_open = g_uri_list_extract_uris (gtk_selection_data_get_data (selection_data));
    while (*uri) {
        GError *err = NULL;
        gchar *filename = g_filename_from_uri (*uri, NULL, &err);

        uri++;
        if (filename == NULL) {
            GtkWidget *dlg;
            dlg = gtk_message_dialog_new (GTK_WINDOW (win),
                                          GTK_DIALOG_MODAL,
                                          GTK_MESSAGE_ERROR,
                                          GTK_BUTTONS_OK,
                                          _("Can not open URI:\n%s"),
                                          err->message);
            g_error_free (err);
            gtk_dialog_run (GTK_DIALOG (dlg));
            gtk_widget_destroy (dlg);
            continue;
        }

        if (newwin == NULL)
            newwin = ghex_window_new (GTK_APPLICATION (g_application_get_default ()));
        if (ghex_window_load (GHEX_WINDOW (newwin), filename)) {
            if (newwin != GTK_WIDGET (win))
                gtk_widget_show (newwin);
            newwin = NULL;
        }
        else {
            GtkWidget *dlg;
            dlg = gtk_message_dialog_new (GTK_WINDOW (win),
                                          GTK_DIALOG_MODAL,
                                          GTK_MESSAGE_ERROR,
                                          GTK_BUTTONS_OK,
                                          _("Can not open file:\n%s"),
                                          filename);
            gtk_widget_show (dlg);
            gtk_dialog_run (GTK_DIALOG (dlg));
            gtk_widget_destroy (dlg);
        }
        g_free (filename);
    }
    g_strfreev (uris_to_open);
}

gboolean
ghex_window_close(GHexWindow *win)
{
	HexDocument *doc;
	const GList *window_list;

	if(win->gh == NULL) {
        gtk_widget_destroy(GTK_WIDGET(win));
		return FALSE;;
	}

	doc = win->gh->document;
	
	if(doc->views->next == NULL) {
		if(!ghex_window_ok_to_close(win))
			return FALSE;
	}	

    /* We dont have to unref the document if the view is the only one */
    if(doc->views->next == NULL) {
        window_list = ghex_window_get_list();
        while(window_list) {
            ghex_window_remove_doc_from_list(GHEX_WINDOW(window_list->data),
                                             win->gh->document);
            window_list = window_list->next;
        }
    }

	/* If we have created the converter window disable the 
	 * "Get cursor value" button
	 */
	if (converter_get)
		gtk_widget_set_sensitive(converter_get, FALSE);

    if (win->advanced_find_dialog)
        delete_advanced_find_dialog(win->advanced_find_dialog);

    gtk_widget_destroy(GTK_WIDGET(win));

    if (doc->views == NULL) /* If we have destroyed the last view */
      g_object_unref (G_OBJECT (doc));

    return TRUE;
}

static gboolean 
ghex_window_focus_in_event(GtkWidget *win, GdkEventFocus *event)
{
    active_window = GHEX_WINDOW(win);

    update_dialog_titles();

    if (GTK_WIDGET_CLASS (ghex_window_parent_class)->focus_in_event)
        return GTK_WIDGET_CLASS (ghex_window_parent_class)->focus_in_event (win, event);
    else
        return TRUE;
}

void
ghex_window_set_action_visible (GHexWindow *win,
                                const char *name,
                                gboolean    visible)
{
    GtkAction *action;

    action = gtk_action_group_get_action (win->action_group, name);
    gtk_action_set_visible (action, visible);
}

void
ghex_window_set_action_sensitive (GHexWindow *win,
                                  const char *name,
                                  gboolean    sensitive)
{
    GtkAction *action;

    action = gtk_action_group_get_action (win->action_group, name);
    gtk_action_set_sensitive (action, sensitive);
}

static void
ghex_window_set_toggle_action_active (GHexWindow *win,
                                      const char *name,
                                      gboolean    active)
{
    GtkAction *action;

    action = gtk_action_group_get_action (win->action_group, name);
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);
}

static gboolean
ghex_window_get_toggle_action_active (GHexWindow *win,
                                      const char *name)
{
    GtkAction *action;

    action = gtk_action_group_get_action (win->action_group, name);
    return gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
}

void
ghex_window_set_sensitivity (GHexWindow *win)
{
    gboolean allmenus = (win->gh != NULL);

    win->undo_sens = (allmenus && (win->gh->document->undo_top != NULL));
    win->redo_sens = (allmenus && (win->gh->document->undo_stack != NULL && win->gh->document->undo_top != win->gh->document->undo_stack));

    ghex_window_set_action_visible (win, "View", allmenus);

    /* File menu */
    ghex_window_set_action_sensitive (win, "FileClose", allmenus);
    ghex_window_set_action_sensitive (win, "FileSave", allmenus && win->gh->document->changed);
    ghex_window_set_action_sensitive (win, "FileSaveAs", allmenus);
    ghex_window_set_action_sensitive (win, "FileExportToHTML", allmenus);
    ghex_window_set_action_sensitive (win, "FileRevert", allmenus && win->gh->document->changed);
    ghex_window_set_action_sensitive (win, "FilePrint", allmenus);
    ghex_window_set_action_sensitive (win, "FilePrintPreview", allmenus);

    /* Edit menu */
    ghex_window_set_action_sensitive (win, "EditFind", allmenus);
    ghex_window_set_action_sensitive (win, "EditReplace", allmenus);
    ghex_window_set_action_sensitive (win, "EditAdvancedFind", allmenus);
    ghex_window_set_action_sensitive (win, "EditGotoByte", allmenus);
    ghex_window_set_action_sensitive (win, "EditInsertMode", allmenus);
    ghex_window_set_action_sensitive (win, "EditUndo", allmenus && win->undo_sens);
    ghex_window_set_action_sensitive (win, "EditRedo", allmenus && win->redo_sens);
    ghex_window_set_action_sensitive (win, "EditCut", allmenus);
    ghex_window_set_action_sensitive (win, "EditCopy", allmenus);
    ghex_window_set_action_sensitive (win, "EditPaste", allmenus);
}

static void
ghex_window_doc_changed(HexDocument *doc, HexChangeData *change_data,
                        gboolean push_undo, gpointer user_data)
{
    GHexWindow *win = GHEX_WINDOW(user_data);

    if(!win->gh->document->changed)
        return;

    if(!win->changed) {
        ghex_window_set_sensitivity(win);
        win->changed = TRUE;
    }
    else if(push_undo) {
        if(win->undo_sens != ( win->gh->document->undo_top == NULL)) {
            win->undo_sens = (win->gh->document->undo_top != NULL);
            ghex_window_set_action_sensitive (win, "EditUndo", win->undo_sens);
        }
        if(win->redo_sens != (win->gh->document->undo_stack != NULL && (win->gh->document->undo_stack != win->gh->document->undo_top))) {
            win->redo_sens = (win->gh->document->undo_stack != NULL &&
                              (win->gh->document->undo_top != win->gh->document->undo_stack));
            ghex_window_set_action_sensitive (win, "EditRedo", win->redo_sens);
        }
    }
}

static void
ghex_window_destroy (GtkWidget *object)
{
        GHexWindow *win;

        g_return_if_fail(object != NULL);
        g_return_if_fail(GHEX_IS_WINDOW(object));

        win = GHEX_WINDOW(object);

        if(win->gh) {
            hex_document_remove_view(win->gh->document, GTK_WIDGET(win->gh));
            g_signal_handlers_disconnect_matched(win->gh->document,
                                                 G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                                 0, 0, NULL,
                                                 ghex_window_doc_changed,
                                                 win);
            win->gh = NULL;
        }

        if (win->action_group) {
            g_object_unref (win->action_group);
            win->action_group = NULL;
        }
        if (win->doc_list_action_group) {
            g_object_unref (win->doc_list_action_group);
            win->doc_list_action_group = NULL;
        }
        if (win->ui_manager) {
            g_object_unref (win->ui_manager);
            win->ui_manager = NULL;
        }

        if (win->dialog)
        {
            g_object_unref (G_OBJECT(win->dialog));
            win->dialog = NULL;
        }

        window_list = g_list_remove(window_list, win);

        if (window_list == NULL)
            active_window = NULL;
        else if(active_window == win)
            active_window = GHEX_WINDOW(window_list->data);

        if (GTK_WIDGET_CLASS (ghex_window_parent_class)->destroy)
            GTK_WIDGET_CLASS (ghex_window_parent_class)->destroy (object);
}

static gboolean
ghex_window_delete_event(GtkWidget *widget, GdkEventAny *e)
{
    ghex_window_close(GHEX_WINDOW(widget));
    return TRUE;
}

/* Normal items */
static const GtkActionEntry action_entries [] = {
    { "File", NULL, N_("_File") },
    { "Edit", NULL, N_("_Edit") },
    { "View", NULL, N_("_View") },
    { "GroupDataAs", NULL, N_("_Group Data As") }, // View submenu
    { "Windows", NULL, N_("_Windows") },
    { "Help", NULL, N_("_Help") },

    /* File menu */
    { "FileOpen", GTK_STOCK_OPEN, N_("_Open..."), "<control>O",
      N_("Open a file"),
      G_CALLBACK (open_cb) },
    { "FileSave", GTK_STOCK_SAVE, N_("_Save"), "<control>S",
      N_("Save the current file"),
      G_CALLBACK (save_cb) },
    { "FileSaveAs", GTK_STOCK_SAVE_AS, N_("Save _As..."), "<shift><control>S",
      N_("Save the current file with a different name"),
      G_CALLBACK (save_as_cb) },
    { "FileExportToHTML", NULL, N_("Save As _HTML..."), NULL,
      N_("Export data to HTML source"),
      G_CALLBACK (export_html_cb) },
    { "FileRevert", GTK_STOCK_REVERT_TO_SAVED, N_("_Revert"), NULL,
      N_("Revert to a saved version of the file"),
      G_CALLBACK (revert_cb) },
    { "FilePrint", GTK_STOCK_PRINT, N_("_Print"), "<control>P",
      N_("Print the current file"),
      G_CALLBACK (print_cb) },
    { "FilePrintPreview", GTK_STOCK_PRINT_PREVIEW, N_("Print Previe_w..."), "<shift><control>P",
      N_("Preview printed data"),
      G_CALLBACK (print_preview_cb) },
    { "FileClose", GTK_STOCK_CLOSE, N_("_Close"), "<control>W",
      N_("Close the current file"),
      G_CALLBACK (close_cb) },
    { "FileExit", GTK_STOCK_QUIT, N_("E_xit"), "<control>Q",
      N_("Exit the program"),
      G_CALLBACK (quit_app_cb) },

    /* Edit menu */
    { "EditUndo", GTK_STOCK_UNDO, N_("_Undo"), "<control>Z",
      N_("Undo the last action"),
      G_CALLBACK (undo_cb) },
    { "EditRedo", GTK_STOCK_REDO, N_("_Redo"), "<shift><control>Z",
      N_("Redo the undone action"),
      G_CALLBACK (redo_cb) },
    { "EditCopy", GTK_STOCK_COPY, N_("_Copy"), "<control>C",
      N_("Copy selection to clipboard"),
      G_CALLBACK (copy_cb) },
    { "EditCut", GTK_STOCK_CUT, N_("Cu_t"), "<control>X",
      N_("Cut selection"),
      G_CALLBACK (cut_cb) },
    { "EditPaste", GTK_STOCK_PASTE, N_("Pa_ste"), "<control>V",
      N_("Paste data from clipboard"),
      G_CALLBACK (paste_cb) },
    { "EditFind", GTK_STOCK_FIND, N_("_Find"), "<control>F",
      N_("Search for a string"),
      G_CALLBACK (find_cb) },
    { "EditAdvancedFind", GTK_STOCK_FIND, N_("_Advanced Find"), NULL,
      N_("Advanced Find"),
      G_CALLBACK (advanced_find_cb) },
    { "EditReplace", GTK_STOCK_FIND_AND_REPLACE, N_("R_eplace"), "<control>H",
      N_("Replace a string"),
      G_CALLBACK (replace_cb) },
    { "EditGotoByte", NULL, N_("_Goto Byte..."), "<control>J",
      N_("Jump to a certain position"),
      G_CALLBACK (jump_cb) },
    { "EditPreferences", GTK_STOCK_PREFERENCES, N_("_Preferences"), NULL,
      N_("Configure the application"),
      G_CALLBACK (prefs_cb) },

    /* View menu */
    { "ViewAddView", NULL, N_("_Add View"), NULL,
      N_("Add a new view to the buffer"),
      G_CALLBACK (add_view_cb) },
    { "ViewRemoveView", NULL, N_("_Remove View"), NULL,
      N_("Remove the current view of the buffer"),
      G_CALLBACK (remove_view_cb) },

    /* Help menu */
    { "HelpContents", GTK_STOCK_HELP, N_("_Contents"), "F1",
      N_("Help on this application"),
      G_CALLBACK (help_cb) },
    { "HelpAbout", GTK_STOCK_ABOUT, N_("_About"), NULL,
      N_("About this application"),
      G_CALLBACK (about_cb) }
};

/* Toggle items */
static const GtkToggleActionEntry toggle_entries[] = {
    /* Edit menu */
    { "EditInsertMode", NULL, N_("_Insert Mode"), "Insert",
      N_("Insert/overwrite data"),
      G_CALLBACK (insert_mode_cb), FALSE },

    /* Windows menu */
    { "CharacterTable", NULL, N_("Character _Table"), NULL,
      N_("Show the character table"),
      G_CALLBACK (character_table_cb), FALSE },
    { "Converter", NULL, N_("_Base Converter"), NULL,
      N_("Open base conversion dialog"),
      G_CALLBACK (converter_cb), FALSE },
    { "TypeDialog", NULL, N_("Type Conversion _Dialog"), NULL,
      N_("Show the type conversion dialog in the edit window"),
      G_CALLBACK (type_dialog_cb), TRUE }
};

/* Radio items in View -> Group Data As */
static GtkRadioActionEntry group_data_entries[] = {
    { "Bytes", NULL, N_("_Bytes"), NULL,
      N_("Group data by 8 bits"), GROUP_BYTE },
    { "Words", NULL, N_("_Words"), NULL,
      N_("Group data by 16 bits"), GROUP_WORD },
    { "Longwords", NULL, N_("_Longwords"), NULL,
      N_("Group data by 32 bits"), GROUP_LONG }
};

static void
menu_item_selected_cb (GtkWidget  *item,
                       GHexWindow *window)
{
    GtkAction *action;
    gchar *tooltip;

    action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (item));
    g_object_get (G_OBJECT (action), "tooltip", &tooltip, NULL);

    if (tooltip != NULL)
        gtk_statusbar_push (GTK_STATUSBAR (window->statusbar),
                            window->statusbar_tooltip_id,
                            tooltip);

    g_free (tooltip);
}

static void
menu_item_deselected_cb (GtkWidget  *item,
                         GHexWindow *window)
{
    gtk_statusbar_pop (GTK_STATUSBAR (window->statusbar),
                       window->statusbar_tooltip_id);
}

static void
connect_proxy_cb (GtkUIManager *ui,
                  GtkAction    *action,
                  GtkWidget    *proxy,
                  GHexWindow   *window)
{
    if (!GTK_IS_MENU_ITEM (proxy))
        return;

    g_signal_connect (G_OBJECT (proxy), "select",
                      G_CALLBACK (menu_item_selected_cb), window);
    g_signal_connect (G_OBJECT (proxy), "deselect",
                      G_CALLBACK (menu_item_deselected_cb), window);
}

static void
disconnect_proxy_cb (GtkUIManager *manager,
                     GtkAction    *action,
                     GtkWidget    *proxy,
                     GHexWindow   *window)
{
    if (!GTK_IS_MENU_ITEM (proxy))
        return;

    g_signal_handlers_disconnect_by_func
        (proxy, G_CALLBACK (menu_item_selected_cb), window);
    g_signal_handlers_disconnect_by_func
        (proxy, G_CALLBACK (menu_item_deselected_cb), window);
}

void
ghex_window_set_contents (GHexWindow *win,
                          GtkWidget  *child)
{
    if (win->contents)
        gtk_widget_destroy (win->contents);

    win->contents = child;
    gtk_box_pack_start (GTK_BOX (win->vbox), win->contents, TRUE, TRUE, 0);
}

void
ghex_window_destroy_contents (GHexWindow *win)
{
    gtk_widget_destroy (win->contents);
    win->contents = NULL;
}

static GObject *
ghex_window_constructor (GType                  type,
                         guint                  n_construct_properties,
                         GObjectConstructParam *construct_params)
{
    GObject    *object;
    GHexWindow *window;
    GtkWidget  *menubar;
    GError     *error = NULL;

    object = G_OBJECT_CLASS (ghex_window_parent_class)->constructor (type,
                             n_construct_properties,
                             construct_params);
    window = GHEX_WINDOW (object);

    window->ui_merge_id = 0;
    window->ui_manager = gtk_ui_manager_new ();
    g_signal_connect (G_OBJECT (window->ui_manager), "connect-proxy",
                      G_CALLBACK (connect_proxy_cb), window);
    g_signal_connect (G_OBJECT (window->ui_manager), "disconnect-proxy",
                      G_CALLBACK (disconnect_proxy_cb), window);

    /* Action group for static menu items */
    window->action_group = gtk_action_group_new ("GHexActions");
    gtk_action_group_set_translation_domain (window->action_group,
                                             GETTEXT_PACKAGE);
    gtk_action_group_add_actions (window->action_group, action_entries,
                                  G_N_ELEMENTS (action_entries),
                                  window);
    gtk_action_group_add_toggle_actions (window->action_group, toggle_entries,
                                         G_N_ELEMENTS (toggle_entries),
                                         window);
    gtk_action_group_add_radio_actions (window->action_group, group_data_entries,
                                        G_N_ELEMENTS (group_data_entries),
                                        GROUP_BYTE,
                                        G_CALLBACK (group_data_cb),
                                        window);
    gtk_ui_manager_insert_action_group (window->ui_manager,
                                        window->action_group, 0);
    gtk_window_add_accel_group (GTK_WINDOW (window),
                                gtk_ui_manager_get_accel_group (window->ui_manager));

    /* Action group for open documents */
    window->doc_list_action_group = gtk_action_group_new ("DocListActions");
    gtk_ui_manager_insert_action_group (window->ui_manager,
                                        window->doc_list_action_group, 0);

    /* Load menu description from resources framework */
    if (!gtk_ui_manager_add_ui_from_resource (window->ui_manager, "/org/gnome/ghex/ghex-ui.xml", &error)) {
        g_warning ("Failed to load ui: %s", error->message);
        g_error_free (error);
    }

    window->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    /* Attach menu */
    menubar = gtk_ui_manager_get_widget (window->ui_manager, "/MainMenu");
    gtk_box_pack_start (GTK_BOX (window->vbox), menubar, FALSE, TRUE, 0);
    gtk_widget_show (menubar);

    window->contents = NULL;

    /* Create statusbar */
    window->statusbar = gtk_statusbar_new ();
    window->statusbar_tooltip_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (window->statusbar),
                                                                 "tooltip");
    gtk_box_pack_end (GTK_BOX (window->vbox), window->statusbar, FALSE, TRUE, 0);
    gtk_widget_show (window->statusbar);

    gtk_container_add (GTK_CONTAINER (window), window->vbox);
    gtk_widget_show (window->vbox);

    return object;
}

static void
ghex_window_class_init(GHexWindowClass *class)
{
	GObjectClass   *gobject_class;
	GtkWidgetClass *widget_class;

	gobject_class = (GObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	gobject_class->constructor = ghex_window_constructor;

	widget_class->delete_event = ghex_window_delete_event;
	widget_class->destroy = ghex_window_destroy;
	widget_class->drag_data_received = ghex_window_drag_data_received;
    widget_class->focus_in_event = ghex_window_focus_in_event;
}

static void
ghex_window_init (GHexWindow *window)
{
    window_list = g_list_prepend(window_list, window);
}

void
ghex_window_sync_converter_item(GHexWindow *win, gboolean state)
{
    const GList *wnode;

    wnode = ghex_window_get_list();
    while(wnode) {
        if(GHEX_WINDOW(wnode->data) != win)
            ghex_window_set_toggle_action_active (GHEX_WINDOW (wnode->data), "Converter", state);
        wnode = wnode->next;
    }
}

void
ghex_window_sync_char_table_item(GHexWindow *win, gboolean state)
{
    const GList *wnode;

    wnode = ghex_window_get_list();
    while(wnode) {
        if(GHEX_WINDOW(wnode->data) != win)
            ghex_window_set_toggle_action_active (GHEX_WINDOW (wnode->data), "CharacterTable", state);
        wnode = wnode->next;
    }
}

GtkWidget *
ghex_window_new (GtkApplication *application)
{
    GHexWindow *win;
    const GList *doc_list;

	static const GtkTargetEntry drag_types[] = {
		{ "text/uri-list", 0, TARGET_URI_LIST }
	};

	win = GHEX_WINDOW(g_object_new(GHEX_TYPE_WINDOW,
                                   "application", application,
                                   "title", _("GHex"),
                                   NULL));

	gtk_drag_dest_set (GTK_WIDGET (win),
                       GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                       drag_types,
                       sizeof (drag_types) / sizeof (drag_types[0]),
                       GDK_ACTION_COPY);

    ghex_window_set_toggle_action_active (win, "Converter",
                                          converter && gtk_widget_get_visible (converter->window));
    ghex_window_set_toggle_action_active (win, "CharacterTable",
                                          char_table && gtk_widget_get_visible (char_table));

    ghex_window_set_sensitivity(win);

    doc_list = hex_document_get_list();
    while(doc_list) {
        ghex_window_add_doc_to_list(win, HEX_DOCUMENT(doc_list->data));
        doc_list = doc_list->next;
    }

    gtk_window_set_default_size(GTK_WINDOW(win),
                                GHEX_WINDOW_DEFAULT_WIDTH,
                                GHEX_WINDOW_DEFAULT_HEIGHT);

	return GTK_WIDGET(win);
}

static GtkWidget *
create_document_view(HexDocument *doc)
{
    GtkWidget *gh = hex_document_add_view(doc);

	gtk_hex_set_group_type(GTK_HEX(gh), def_group_type);

	if (def_metrics && def_font_desc) {
		gtk_hex_set_font(GTK_HEX(gh), def_metrics, def_font_desc);
	}

    return gh;
}

GtkWidget *
ghex_window_new_from_doc (GtkApplication *application,
                          HexDocument    *doc)
{
    GtkWidget *win = ghex_window_new (application);
    GtkWidget *gh = create_document_view(doc);

    gtk_widget_show(gh);
    GHEX_WINDOW(win)->gh = GTK_HEX(gh);
    ghex_window_set_contents (GHEX_WINDOW (win), gh);
    g_signal_connect(G_OBJECT(doc), "document_changed",
                     G_CALLBACK(ghex_window_doc_changed), win);
    ghex_window_set_doc_name(GHEX_WINDOW(win),
                             GHEX_WINDOW(win)->gh->document->path_end);
    ghex_window_set_sensitivity(GHEX_WINDOW(win));

    return win;
}

GtkWidget *
ghex_window_new_from_file (GtkApplication *application,
                           const gchar    *filename)
{
    GtkWidget *win = ghex_window_new (application);

    if(!ghex_window_load(GHEX_WINDOW(win), filename)) {
        gtk_widget_destroy(win);
        return NULL;
    }

    return win;
}

static void
ghex_window_sync_group_type(GHexWindow *win)
{
    const gchar *group_path;

    if(win->gh == NULL)
        return;

    switch(win->gh->group_type) {
    case GROUP_BYTE:
        group_path = "Bytes";
        break;
    case GROUP_WORD:
        group_path = "Words";
        break;
    case GROUP_LONG:
        group_path = "Longwords";
        break;
    default:
        group_path = NULL;
        break;
    }

    if(group_path == NULL)
        return;

    ghex_window_set_toggle_action_active (win, group_path, TRUE);
}

void
ghex_window_update_status_message(GHexWindow *win)
{
#define FMT_LEN    128
#define STATUS_LEN 128

    gchar fmt[FMT_LEN], status[STATUS_LEN];
    gint current_pos;
    gint ss, se, len;

    if(NULL == win->gh) {
        ghex_window_show_status(win, " ");
        return;
    }

    current_pos = gtk_hex_get_cursor(win->gh);
    if(g_snprintf(fmt, FMT_LEN, _("Offset: %s"), offset_fmt) < FMT_LEN) {
        g_snprintf(status, STATUS_LEN, fmt, current_pos);
        if(gtk_hex_get_selection(win->gh, &ss, &se)) {
            if(g_snprintf(fmt, FMT_LEN, _("; %s bytes from %s to %s selected"),
                          offset_fmt, offset_fmt, offset_fmt) < FMT_LEN) {
                len = strlen(status);
                if(len < STATUS_LEN) {
                    // Variables 'ss' and 'se' denotes the offsets of the first and
                    // the last bytes that are part of the selection.
                    g_snprintf(status + len, STATUS_LEN - len, fmt, se - ss + 1, ss, se);
                }
            }
        }
        ghex_window_show_status(win, status);
    }
    else
        ghex_window_show_status(win, " ");
}

static void
cursor_moved_cb(GtkHex *gtkhex, gpointer user_data)
{
    int i;
    int current_pos;
    HexDialogVal64 val;
	GHexWindow *win = GHEX_WINDOW(user_data);

    current_pos = gtk_hex_get_cursor(gtkhex);
    ghex_window_update_status_message(win);
    for (i = 0; i < 8; i++)
    {
        /* returns 0 on buffer overflow, which is what we want */
        val.v[i] = gtk_hex_get_byte(gtkhex, current_pos+i);
    }
    hex_dialog_updateview(win->dialog, &val);
}

gboolean
ghex_window_load(GHexWindow *win, const gchar *filename)
{
    HexDocument *doc;
    GtkWidget *gh;
    GtkWidget *vbox;
    const GList *window_list;
    gboolean active;

    g_return_val_if_fail(win != NULL, FALSE);
    g_return_val_if_fail(GHEX_IS_WINDOW(win), FALSE);
    g_return_val_if_fail(filename != NULL, FALSE);

    doc = hex_document_new_from_file (filename);
    if(!doc)
        return FALSE;
    hex_document_set_max_undo(doc, max_undo_depth);
    gh = create_document_view(doc);
    gtk_hex_show_offsets(GTK_HEX(gh), show_offsets_column);
    g_signal_connect(G_OBJECT(doc), "document_changed",
                     G_CALLBACK(ghex_window_doc_changed), win);
    g_signal_connect(G_OBJECT(doc), "undo",
                     G_CALLBACK(set_doc_menu_sensitivity), win);;
    g_signal_connect(G_OBJECT(doc), "redo",
                     G_CALLBACK(set_doc_menu_sensitivity), win);;
    g_signal_connect(G_OBJECT(doc), "undo_stack_forget",
                     G_CALLBACK(set_doc_menu_sensitivity), win);;
    g_signal_connect(G_OBJECT(gh), "cursor_moved",
                     G_CALLBACK(cursor_moved_cb), win);
    gtk_widget_show(gh);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(win), 4);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(vbox), gh, TRUE, TRUE, 4);

    win->dialog = hex_dialog_new();
    win->dialog_widget = hex_dialog_getview(win->dialog);
    gtk_box_pack_start(GTK_BOX(vbox), win->dialog_widget, FALSE, FALSE, 4);
    active = ghex_window_get_toggle_action_active (win, "TypeDialog");
    if (active)
    {
      gtk_widget_show(win->dialog_widget);
    }
    else
    {
      gtk_widget_hide(win->dialog_widget);
    }

    if(win->gh) {
        window_list = ghex_window_get_list();
        while(window_list) {
            ghex_window_remove_doc_from_list(GHEX_WINDOW(window_list->data),
                                             win->gh->document);
            window_list = window_list->next;
        }
        hex_document_remove_view(win->gh->document, GTK_WIDGET(win->gh));
        g_signal_handlers_disconnect_matched(win->gh->document,
                                             G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL,
                                             ghex_window_doc_changed,
                                             win);
    }
    ghex_window_set_contents (win, vbox);
    win->gh = GTK_HEX(gh);
    win->changed = FALSE;

    window_list = ghex_window_get_list();
    while(window_list) {
        ghex_window_add_doc_to_list(GHEX_WINDOW(window_list->data), win->gh->document);
        window_list = window_list->next;
    }

    ghex_window_sync_group_type(win);
    ghex_window_set_doc_name(win, win->gh->document->path_end);
    ghex_window_set_sensitivity(win);

    g_signal_emit_by_name(G_OBJECT(gh), "cursor_moved");
   
    return TRUE;
}

static gchar* 
encode_xml_and_escape_underscores(gchar *text)
{
	GString *str;
	gint length;
	const gchar *p;
 	const gchar *end;

  	g_return_val_if_fail (text != NULL, NULL);

    length = strlen (text);

	str = g_string_new ("");

  	p = text;
  	end = text + length;

  	while (p != end) {
        const gchar *next;
        next = g_utf8_next_char (p);

		switch (*p) {
        case '_':
            g_string_append (str, "__");
            break;
        case '&':
            g_string_append (str, "&amp;");
            break;
        case '<':
            g_string_append (str, "&lt;");
            break;
        case '>':
            g_string_append (str, "&gt;");
            break;
        case '"':
            g_string_append (str, "&quot;");
            break;
        case '\'':
            g_string_append (str, "&apos;");
            break;
        default:
            g_string_append_len (str, p, next - p);
            break;
        }
        
        p = next;
    }
    
	return g_string_free (str, FALSE);
}

static gchar* 
encode_xml (const gchar* text)
{
	GString *str;
	gint length;
	const gchar *p;
 	const gchar *end;

  	g_return_val_if_fail (text != NULL, NULL);

    length = strlen (text);

	str = g_string_new ("");

  	p = text;
  	end = text + length;

  	while (p != end) {
        const gchar *next;
        next = g_utf8_next_char (p);

		switch (*p) {
        case '&':
            g_string_append (str, "&amp;");
            break;
        case '<':
            g_string_append (str, "&lt;");
            break;
        case '>':
            g_string_append (str, "&gt;");
            break;
        case '"':
            g_string_append (str, "&quot;");
            break;
        case '\'':
            g_string_append (str, "&apos;");
            break;
        default:
            g_string_append_len (str, p, next - p);
            break;
        }
        
        p = next;
    }
    
	return g_string_free (str, FALSE);
}

static void
ghex_window_doc_menu_update (GHexWindow *win)
{
    GList *items, *l;

    /* Remove existing entries from UI manager */
    if (win->ui_merge_id > 0) {
        gtk_ui_manager_remove_ui (win->ui_manager,
                                  win->ui_merge_id);
        gtk_ui_manager_ensure_update (win->ui_manager);
    }
    win->ui_merge_id = gtk_ui_manager_new_merge_id (win->ui_manager);

    /* Populate the UI with entries from the action group */
    items = gtk_action_group_list_actions (win->doc_list_action_group);
    for (l = items; l && l->data; l = g_list_next (l)) {
        GtkAction *action;
        const gchar *action_name;

        action = (GtkAction *) l->data;
        action_name = gtk_action_get_name (action);

        gtk_ui_manager_add_ui (win->ui_manager,
                               win->ui_merge_id,
                               "/MainMenu/Windows/OpenDocuments",
                               action_name,
                               action_name,
                               GTK_UI_MANAGER_MENUITEM,
                               FALSE);
    }
}

void
ghex_window_remove_doc_from_list(GHexWindow *win, HexDocument *doc)
{
    GtkAction *action;
    gchar *action_name;

    action_name = g_strdup_printf ("FilesFile_%p", doc);
    action = gtk_action_group_get_action (win->doc_list_action_group,
                                          action_name);
    g_free (action_name);

    gtk_action_group_remove_action (win->doc_list_action_group,
                                    action);
    ghex_window_doc_menu_update (win);
}

void
ghex_window_add_doc_to_list(GHexWindow *win, HexDocument *doc)
{
    GtkAction *action;
    gchar *action_name;
    gchar *escaped_name;
    gchar *tip;

    escaped_name = encode_xml (doc->path_end);
    tip = g_strdup_printf(_("Activate file %s"), escaped_name);
    g_free(escaped_name);
    escaped_name = encode_xml_and_escape_underscores(doc->path_end);
    action_name = g_strdup_printf ("FilesFile_%p", doc);

    action = gtk_action_new (action_name, escaped_name, tip, NULL);
    g_signal_connect (action, "activate",
                      G_CALLBACK (file_list_activated_cb),
                      (gpointer) doc);
    gtk_action_group_add_action (win->doc_list_action_group,
                                 action);
    g_object_unref (action);

    ghex_window_doc_menu_update (win);

    g_free (tip);
    g_free (escaped_name);
    g_free (action_name);
}

const GList *
ghex_window_get_list()
{
    return window_list;
}

GHexWindow *
ghex_window_get_active()
{
    return active_window;
}

void
ghex_window_set_doc_name(GHexWindow *win, const gchar *name)
{
    if(name != NULL) {
        gchar *title = g_strdup_printf(_("%s - GHex"), name);
        gtk_window_set_title(GTK_WINDOW(win), title);
        g_free(title);
    }
    else gtk_window_set_title(GTK_WINDOW(win), _("GHex"));
}

struct _MessageInfo {
  GHexWindow * win;
  guint timeoutid;
  guint handlerid;
};
typedef struct _MessageInfo MessageInfo;

/* Called if the win is destroyed before the timeout occurs. */
static void
remove_timeout_cb (GtkWidget *win, MessageInfo *mi )
{
	g_return_if_fail (mi != NULL);

	g_source_remove (mi->timeoutid);
	g_free (mi);
}

static gint
remove_message_timeout (MessageInfo * mi)
{
	/* Remove the status message */
	/* NOTE : Use space ' ' not an empty string '' */
	ghex_window_update_status_message (mi->win);
    g_signal_handlers_disconnect_by_func(G_OBJECT(mi->win),
                                         remove_timeout_cb, mi);
	g_free (mi);

	return FALSE; /* removes the timeout */
}

static const guint32 flash_length = 3000; /* 3 seconds, I hope */

/**
 * ghex_window_set_status
 * @win: Pointer to GHexWindow object.
 * @msg: Text of message to be shown on the status bar.
 *
 * Description:
 * Show the message in the status bar; if no status bar do nothing.
 * -- SnM
 */
void
ghex_window_show_status (GHexWindow *win, const gchar *msg)
{
	g_return_if_fail (win != NULL);
	g_return_if_fail (GHEX_IS_WINDOW (win));
	g_return_if_fail (msg != NULL);

	gtk_statusbar_pop (GTK_STATUSBAR (win->statusbar), 0);
	gtk_statusbar_push (GTK_STATUSBAR (win->statusbar), 0, msg);
}

/**
 * ghex_window_flash
 * @win: Pointer to GHexWindow object
 * @flash: Text of message to be flashed
 *
 * Description:
 * Flash the message in the statusbar for a few moments; if no
 * statusbar, do nothing. For trivial little status messages,
 * e.g. "Auto saving..."
 **/
void
ghex_window_flash (GHexWindow * win, const gchar * flash)
{
	MessageInfo * mi;
	g_return_if_fail (win != NULL);
	g_return_if_fail (GHEX_IS_WINDOW (win));
	g_return_if_fail (flash != NULL);

	mi = g_new (MessageInfo, 1);

	ghex_window_show_status (win, flash);

	mi->timeoutid =
		g_timeout_add (flash_length,
                       (GSourceFunc) remove_message_timeout,
                       mi);
	mi->handlerid =
		g_signal_connect (G_OBJECT(win),
                          "destroy",
                          G_CALLBACK (remove_timeout_cb),
                          mi );
	mi->win = win;
}

GHexWindow *
ghex_window_find_for_doc(HexDocument *doc)
{
    const GList *win_node;
    GHexWindow *win;

    win_node = ghex_window_get_list();
    while(win_node) {
        win = GHEX_WINDOW(win_node->data);
        if(win->gh && win->gh->document == doc)
            return win;
        win_node = win_node->next;
    }
    return NULL;
}

gboolean
ghex_window_save_as(GHexWindow *win)
{
	HexDocument *doc;
	GtkWidget *file_sel;
	gboolean ret_val = TRUE;
    GtkResponseType resp;

	if(win->gh == NULL)
		return ret_val;

	doc = win->gh->document;
	file_sel =
        gtk_file_chooser_dialog_new(_("Select a file to save buffer as"),
                                    GTK_WINDOW(win),
                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                    GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                    NULL);

	if(doc->file_name)
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_sel),
                                      doc->file_name);
	gtk_window_set_modal(GTK_WINDOW(file_sel), TRUE);
	gtk_window_set_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	gtk_widget_show (file_sel);

	resp = gtk_dialog_run (GTK_DIALOG (file_sel));
	if(resp == GTK_RESPONSE_OK) {
		FILE *file;
		gchar *flash;
        gchar *gtk_file_name, *path_end;
        const gchar *filename;

        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_sel));
        if(access(filename, F_OK) == 0) {
            GtkWidget *mbox;

            gtk_file_name = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

			mbox = gtk_message_dialog_new(GTK_WINDOW(win),
										  GTK_DIALOG_MODAL|
										  GTK_DIALOG_DESTROY_WITH_PARENT,
										  GTK_MESSAGE_QUESTION,
										  GTK_BUTTONS_YES_NO,
                                          _("File %s exists.\n"
                                            "Do you want to overwrite it?"),
                                          gtk_file_name);
            g_free(gtk_file_name);

            gtk_dialog_set_default_response(GTK_DIALOG(mbox), GTK_RESPONSE_NO);

            ret_val = (ask_user(GTK_MESSAGE_DIALOG(mbox)) == GTK_RESPONSE_YES);
            gtk_widget_destroy (mbox);
        }

        if(ret_val) {
            if((file = fopen(filename, "wb")) != NULL) {
                if(hex_document_write_to_file(doc, file)) {
                    if(doc->file_name)
                        g_free(doc->file_name);
                    if(doc->path_end)
                        g_free(doc->path_end);
                    doc->file_name = g_strdup(filename);
                    doc->changed = FALSE;
                    win->changed = FALSE;

                    path_end = g_path_get_basename (doc->file_name);
                    doc->path_end = g_filename_to_utf8(path_end, -1, NULL, NULL, NULL);
                    ghex_window_set_doc_name(win, doc->path_end);
                    gtk_file_name = g_filename_to_utf8(doc->file_name, -1, NULL, NULL, NULL);
                    flash = g_strdup_printf(_("Saved buffer to file %s"), gtk_file_name);
                    ghex_window_flash(win, flash);
                    g_free(path_end);
                    g_free(gtk_file_name);
                    g_free(flash);
                }
                else {
                    display_error_dialog (win, _("Error saving file!"));
                    ret_val = FALSE;
                }
                fclose(file);
            }
            else {
                display_error_dialog (win, _("Can't open file for writing!"));
                ret_val = TRUE;
            }
        }
	}
	gtk_widget_destroy(file_sel);
	return ret_val;
}

static gboolean
ghex_window_path_exists (const gchar* path)
{
	GFile *file;
	gboolean res;

	g_return_val_if_fail (path != NULL, FALSE);
	file = g_file_new_for_path (path);
	g_return_val_if_fail (file != NULL, FALSE);
	res = g_file_query_exists (file, NULL);

	g_object_unref (file);
	return res;
}

gboolean
ghex_window_ok_to_close(GHexWindow *win)
{
	GtkWidget *mbox;
	gint reply;
	gboolean file_exists;
	GtkWidget *save_btn;
    HexDocument *doc;

    if(win->gh == NULL)
        return TRUE;

    doc = win->gh->document;
    file_exists = ghex_window_path_exists (doc->file_name);
    if (!hex_document_has_changed(doc) && file_exists)
        return TRUE;

	mbox = gtk_message_dialog_new(GTK_WINDOW(win),
								  GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
								  GTK_MESSAGE_QUESTION,
								  GTK_BUTTONS_NONE,
                                  _("File %s has changed since last save.\n"
                                    "Do you want to save changes?"),
                                  doc->path_end);
			
	save_btn = create_button(mbox, GTK_STOCK_NO, _("Do_n't save"));
	gtk_widget_show (save_btn);
	gtk_dialog_add_action_widget(GTK_DIALOG(mbox), save_btn, GTK_RESPONSE_NO);
	gtk_dialog_add_button(GTK_DIALOG(mbox), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button(GTK_DIALOG(mbox), GTK_STOCK_SAVE, GTK_RESPONSE_YES);
	gtk_dialog_set_default_response(GTK_DIALOG(mbox), GTK_RESPONSE_YES);
	gtk_window_set_resizable(GTK_WINDOW(mbox), FALSE);
	
	reply = gtk_dialog_run(GTK_DIALOG(mbox));
	
	gtk_widget_destroy(mbox);
		
	if(reply == GTK_RESPONSE_YES) {
		if(!file_exists) {
			if(!ghex_window_save_as(win)) {
				return FALSE;
			}
        }
		else {
            if(!hex_document_is_writable(doc)) {
                display_error_dialog (win, _("You don't have the permissions to save the file!"));
                return FALSE;
            }
            else if(!hex_document_write(doc)) {
                display_error_dialog(win, _("An error occurred while saving file!"));
				return FALSE;
			}
		}
	}
	else if(reply == GTK_RESPONSE_CANCEL)
		return FALSE;

	return TRUE;
}
