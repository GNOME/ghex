/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * ghex-window.c: a ghex window
 *
 * Copyright (C) 2002 the Free Software Foundation
 *
 * Author: Jaka Mocnik  <jaka@gnu.org>
 */

#include <config.h>

#include <gnome.h>
#include <bonobo-activation/bonobo-activation.h>
#include <libgnomeui/gnome-window-icon.h>
#include <bonobo.h>
#include <bonobo/bonobo-ui-main.h>

#include <math.h>
#include <ctype.h>

#include "ghex-window.h"
#include "ghex.h"

static BonoboWindowClass *parent_class;

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

	if (info != TARGET_URI_LIST)
		return;

    win->uris_to_open = g_strsplit(selection_data->data, "\r\n", 0);
    if (context->suggested_action == GDK_ACTION_ASK) {
        GtkWidget *menu = gtk_menu_new ();
		
        bonobo_window_add_popup (BONOBO_WINDOW (win), 
                                 GTK_MENU (menu), 
                                 "/popups/DnD");
        gtk_menu_popup (GTK_MENU (menu),
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        0,
                        GDK_CURRENT_TIME);
    }
    else {
        GtkWidget *newwin;
        gchar **uri = win->uris_to_open;
        
        if(win->gh == NULL)
            newwin = GTK_WIDGET(win);
        else
            newwin = NULL;
        while(*uri) {
            if(!g_ascii_strncasecmp("file:", *uri, 5)) {
                if(newwin == NULL) {
                    newwin = ghex_window_new_from_file((*uri) + 5);
                    gtk_widget_show(newwin);
                }
                else
                    ghex_window_load(newwin, (*uri) + 5);
                newwin = NULL;
            }
            uri++;
        }
        g_strfreev(win->uris_to_open);
        win->uris_to_open = NULL;
    }
}

gboolean
ghex_window_close(GHexWindow *win)
{
	HexDocument *doc;
	const GList *window_list;

	if(win->gh == NULL) {
		gtk_widget_destroy(GTK_WIDGET(win));
		return;
	}

	doc = win->gh->document;
	
	if(doc->views->next == NULL && hex_document_has_changed(doc)) {
		if(!hex_document_ok_to_close(doc))
			return;
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

    gtk_widget_destroy(GTK_WIDGET(win));
}

static gboolean 
ghex_window_focus_in_event(GtkWidget *win, GdkEventFocus *event)
{
    active_window = GHEX_WINDOW(win);

    update_dialog_titles();

    if(GTK_WIDGET_CLASS(parent_class)->focus_in_event)
        return GTK_WIDGET_CLASS(parent_class)->focus_in_event(win, event);
    else
        return TRUE;
}

void
ghex_window_set_sensitivity (GHexWindow *win)
{
    BonoboUIComponent *uic = win->uic;
    gboolean allmenus = (win->gh != NULL);
    gboolean anymenus = (ghex_window_get_list() != NULL);

    win->undo_sens = (allmenus && (win->gh->document->undo_top != NULL));
    win->redo_sens = (allmenus && (win->gh->document->undo_stack != NULL && win->gh->document->undo_top != win->gh->document->undo_stack));

    bonobo_ui_component_freeze(uic, NULL);
	bonobo_ui_component_set_prop (uic, "/menu/View", "hidden", 
                                  allmenus?"0":"1", NULL);
	bonobo_ui_component_set_prop (uic, "/commands/FileSave", "sensitive",
                                  (allmenus && win->gh->document->changed)?"1":"0", NULL);
	bonobo_ui_component_set_prop (uic, "/commands/FileSaveAs", "sensitive",
                                  allmenus?"1":"0", NULL);
	bonobo_ui_component_set_prop (uic, "/commands/ExportToHTML", "sensitive",
                                  allmenus?"1":"0", NULL);
	bonobo_ui_component_set_prop (uic, "/commands/FileRevert", "sensitive",
                                  (allmenus && win->gh->document->changed)?"1":"0", NULL);
	bonobo_ui_component_set_prop (uic, "/commands/FilePrint", "sensitive",
                                  allmenus ?"1":"0", NULL);
	bonobo_ui_component_set_prop (uic, "/commands/FilePrintPreview", "sensitive",
                                  allmenus?"1":"0", NULL);
	bonobo_ui_component_set_prop (uic, "/commands/Find", "sensitive",
                                  anymenus?"1":"0", NULL);
	bonobo_ui_component_set_prop (uic, "/commands/Replace", "sensitive",
                                  anymenus?"1":"0", NULL);
	bonobo_ui_component_set_prop (uic, "/commands/GoToByte", "sensitive",
                                  anymenus?"1":"0", NULL);
	bonobo_ui_component_set_prop (uic, "/commands/InsertMode", "sensitive",
                                  allmenus?"1":"0", NULL);
	bonobo_ui_component_set_prop (uic, "/commands/EditUndo", "sensitive",
                                  (allmenus && win->undo_sens)?"1":"0", NULL);
	bonobo_ui_component_set_prop (uic, "/commands/EditRedo", "sensitive",
                                  (allmenus && win->redo_sens)?"1":"0", NULL);
    bonobo_ui_component_thaw(uic, NULL);
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
            bonobo_ui_component_set_prop (win->uic, "/commands/EditUndo", "sensitive",
                                          win->undo_sens?"1":"0", NULL);
        }
        if(win->redo_sens != (win->gh->document->undo_stack != NULL && (win->gh->document->undo_stack != win->gh->document->undo_top))) {
            win->redo_sens = (win->gh->document->undo_stack != NULL &&
                              (win->gh->document->undo_top != win->gh->document->undo_stack));
            bonobo_ui_component_set_prop (win->uic, "/commands/EditRedo", "sensitive",
                                          win->redo_sens?"1":"0", NULL);
        }
    }
}

static void
ghex_window_destroy(GtkObject *object)
{
        GHexWindow *win;

        g_return_if_fail(object != NULL);
        g_return_if_fail(GHEX_IS_WINDOW(object));

        win = GHEX_WINDOW(object);

        if(win->uic) {
            bonobo_object_unref(win->uic);
            win->uic = NULL;
        }
        if(win->gh) {
            hex_document_remove_view(win->gh->document, GTK_WIDGET(win->gh));
            g_signal_handlers_disconnect_matched(win->gh->document,
                                                 G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                                 0, 0, NULL,
                                                 ghex_window_doc_changed,
                                                 win);
            win->gh = NULL;
        }

        window_list = g_list_remove(window_list, win);

        if(window_list == NULL) {
            bonobo_main_quit();
            active_window = NULL;
        }
        else if(active_window == win)
            active_window = GHEX_WINDOW(window_list->data);

        if(GTK_OBJECT_CLASS(parent_class)->destroy)
                GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

static gboolean
ghex_window_delete_event(GtkWidget *widget, GdkEventAny *e)
{
    ghex_window_close(GHEX_WINDOW(widget));
    return TRUE;
}

static void
ghex_window_class_init(GHexWindowClass *class)
{
	GObjectClass   *gobject_class;
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	gobject_class = (GObjectClass *) class;
	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = g_type_class_peek_parent (class);

	object_class->destroy = ghex_window_destroy;

	widget_class->delete_event = ghex_window_delete_event;
	widget_class->drag_data_received = ghex_window_drag_data_received;
    widget_class->focus_in_event = ghex_window_focus_in_event;
}

static void
ghex_window_init (GHexWindow *window)
{
    window_list = g_list_prepend(window_list, window);
}

GType
ghex_window_get_type (void) 
{
	static GType ghex_window_type = 0;
	
	if(!ghex_window_type) {
		static const GTypeInfo ghex_window_info =
		{
			sizeof(GHexWindowClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) ghex_window_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof(GHexWindow),
			0,		/* n_preallocs */
			(GInstanceInitFunc) ghex_window_init,
		};
		
		ghex_window_type = g_type_register_static(BONOBO_TYPE_WINDOW, 
                                                  "GHexWindow", 
                                                  &ghex_window_info, 0);
	}

	return ghex_window_type;
}

void
ghex_window_sync_converter_item(GHexWindow *win, gboolean state)
{
    const GList *wnode;

    wnode = ghex_window_get_list();
    while(wnode) {
        if(GHEX_WINDOW(wnode->data) != win)
            bonobo_ui_component_set_prop (GHEX_WINDOW(wnode->data)->uic,
                                          "/commands/Converter", "state",
                                          state?"1":"0", NULL);
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
            bonobo_ui_component_set_prop (GHEX_WINDOW(wnode->data)->uic,
                                          "/commands/CharacterTable", "state",
                                          state?"1":"0", NULL);
        wnode = wnode->next;
    }
}

static void
ghex_window_listener (BonoboUIComponent           *uic,
                      const char                  *path,
                      Bonobo_UIComponent_EventType type,
                      const char                  *state,
                      gpointer                     user_data)
{
	GHexWindow *win;
	GtkWidget *view;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (GHEX_IS_WINDOW (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	win = GHEX_WINDOW(user_data);

	if (!strcmp (path, "InsertMode")) {
        if (win->gh != NULL)
            gtk_hex_set_insert_mode(win->gh, *state == '1');
		return;
	}
    else if(!strcmp (path, "Converter")) {
        if(!converter)
            converter = create_converter();

        if(state && atoi(state)) {
            if(!GTK_WIDGET_VISIBLE(converter->window)) {
                gtk_window_position(GTK_WINDOW(converter->window), GTK_WIN_POS_MOUSE);
                gtk_widget_show(converter->window);
            }
            raise_and_focus_widget(converter->window);

            if (!ghex_window_get_active() && converter_get)
                gtk_widget_set_sensitive(converter_get, FALSE);
            else
                gtk_widget_set_sensitive(converter_get, TRUE);
        }
        else {
            if(GTK_WIDGET_VISIBLE(converter->window))
                gtk_widget_hide(converter->window);
        }
        ghex_window_sync_converter_item(win, atoi(state));
        return;
    }
    else if(!strcmp (path, "CharacterTable")) {
        if(!char_table)
            char_table = create_char_table();

        if(state && atoi(state)) {
            if(!GTK_WIDGET_VISIBLE(char_table)) {
                gtk_window_position(GTK_WINDOW(char_table), GTK_WIN_POS_MOUSE);
                gtk_widget_show(char_table);
            }
            raise_and_focus_widget(char_table);
        }
        else {
            if(GTK_WIDGET_VISIBLE(char_table))
                gtk_widget_hide(GTK_WIDGET(char_table));
        }
        ghex_window_sync_char_table_item(win, atoi(state));
        return;
    }
	if (!state || !atoi (state) || (win->gh == NULL))
		return;

	if (!strcmp (path, "Bytes"))
		gtk_hex_set_group_type(win->gh, GROUP_BYTE);
	else if (!strcmp (path, "Words"))
		gtk_hex_set_group_type(win->gh, GROUP_WORD);
	else if (!strcmp (path, "Longwords"))
		gtk_hex_set_group_type(win->gh, GROUP_LONG);
	else {
		g_warning("Unknown event: '%s'.", path);
	}
}

GtkWidget *
ghex_window_new(void)
{
    GHexWindow *win;
	BonoboUIContainer *ui_container;
	BonoboUIComponent *uic;
    CORBA_Environment ev;
    const GList *doc_list;

	static const GtkTargetEntry drag_types[] = {
		{ "text/uri-list", 0, TARGET_URI_LIST }
	};

	win = GHEX_WINDOW(g_object_new(GHEX_TYPE_WINDOW,
                                   "win_name", "ghex",
                                   "title", _("GHex"),
                                   NULL));

	gtk_drag_dest_set (GTK_WIDGET (win),
                       GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                       drag_types,
                       sizeof (drag_types) / sizeof (drag_types[0]),
                       GDK_ACTION_COPY | GDK_ACTION_ASK);

	/* add menu and toolbar */
	ui_container = bonobo_window_get_ui_container(BONOBO_WINDOW(win));
	uic = bonobo_ui_component_new("ghex");
    win->uic = uic;
	bonobo_ui_component_set_container(uic, BONOBO_OBJREF(ui_container),
                                      NULL);
	bonobo_ui_util_set_ui(uic, NULL, "ghex-ui.xml", "GHex", NULL);
	bonobo_ui_component_add_verb_list_with_data(uic, ghex_verbs, win);
    
	bonobo_ui_component_add_listener (uic, "Bytes",
                                      ghex_window_listener, win);
	bonobo_ui_component_add_listener (uic, "Words",
                                      ghex_window_listener, win);
	bonobo_ui_component_add_listener (uic, "Longwords",
                                      ghex_window_listener, win);
	bonobo_ui_component_add_listener (uic, "InsertMode",
                                      ghex_window_listener, win);
	bonobo_ui_component_add_listener (uic, "Converter",
                                      ghex_window_listener, win);
	bonobo_ui_component_add_listener (uic, "CharacterTable",
                                      ghex_window_listener, win);
    bonobo_ui_component_set_prop (uic, "/commands/Converter", "state",
                                  (converter && GTK_WIDGET_VISIBLE(converter->window))?"1":"0",
                                  NULL);
    bonobo_ui_component_set_prop (uic, "/commands/CharacterTable", "state",
                                  (char_table && GTK_WIDGET_VISIBLE(char_table))?"1":"0",
                                  NULL);
	bonobo_ui_component_set_prop (uic, "/status", "hidden", "0", NULL);

    ghex_window_set_sensitivity(win);

    doc_list = hex_document_get_list();
    while(doc_list) {
        ghex_window_add_doc_to_list(win, HEX_DOCUMENT(doc_list->data));
        doc_list = doc_list->next;
    }

	return GTK_WIDGET(win);
}

GtkWidget *
ghex_window_new_from_doc(HexDocument *doc)
{
    GtkWidget *win = ghex_window_new();
    GtkWidget *gh = hex_document_add_view(doc);

    gtk_widget_show(gh);
    GHEX_WINDOW(win)->gh = GTK_HEX(gh);
    bonobo_window_set_contents(BONOBO_WINDOW(win), gh);
    g_signal_connect(G_OBJECT(doc), "document_changed",
                     G_CALLBACK(ghex_window_doc_changed), win);
    ghex_window_set_doc_name(GHEX_WINDOW(win),
                             GHEX_WINDOW(win)->gh->document->path_end);
    ghex_window_set_sensitivity(GHEX_WINDOW(win));

    return win;
}

GtkWidget *
ghex_window_new_from_file(const gchar *filename)
{
    GtkWidget *win = ghex_window_new();

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
        group_path = "/commands/Bytes";
        break;
    case GROUP_WORD:
        group_path = "/commands/Words";
        break;
    case GROUP_LONG:
        group_path = "/commands/Longwords";
        break;
    default:
        group_path = NULL;
        break;
    }

    if(group_path == NULL)
        return;

    bonobo_ui_component_set_prop(win->uic, group_path, "state", "1", NULL);
}

static void
cursor_moved_cb(GtkHex *gtkhex, gpointer user_data)
{
	static gchar *cursor_pos, *format;
	GHexWindow *win = GHEX_WINDOW(user_data);

	if((format = g_strdup_printf(_("Offset: %s"), offset_fmt)) != NULL) {
		if((cursor_pos = g_strdup_printf(format, gtk_hex_get_cursor(gtkhex))) != NULL) {
			ghex_window_show_status(win, cursor_pos);
            g_free(cursor_pos);
		}
		g_free(format);
	}
}

gboolean
ghex_window_load(GHexWindow *win, const gchar *filename)
{
    gchar *full_path;
    HexDocument *doc;
    GtkWidget *gh;
    const GList *window_list;

    g_return_val_if_fail(win != NULL, FALSE);
    g_return_val_if_fail(GHEX_IS_WINDOW(win), FALSE);
    g_return_val_if_fail(filename != NULL, FALSE);

    if(*filename != '/') {
        gchar *cwd;
        
        cwd = g_get_current_dir();
        full_path = g_strconcat(cwd, "/", filename, NULL);
        g_free(cwd);
    }
    else
        full_path = g_strdup(filename);
    
    doc = hex_document_new(full_path);
    g_free(full_path);
    if(!doc)
        return FALSE;
    gh = hex_document_add_view(doc);
    gtk_hex_show_offsets(GTK_HEX(gh), show_offsets_column);
    g_signal_connect(G_OBJECT(doc), "document_changed",
                     G_CALLBACK(ghex_window_doc_changed), win);
    g_signal_connect(G_OBJECT(gh), "cursor_moved",
                     G_CALLBACK(cursor_moved_cb), win);
    gtk_widget_show(gh);
    
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
    bonobo_window_set_contents(BONOBO_WINDOW(win), gh);
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
   
    return TRUE;
}

static gchar* 
escape_underscores (const gchar* text)
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
        default:
            g_string_append_len (str, p, next - p);
            break;
        }
        
        p = next;
    }
    
	return g_string_free (str, FALSE);
}

void
ghex_window_remove_doc_from_list(GHexWindow *win, HexDocument *doc)
{
    gchar *verb_name = g_strdup_printf("FilesFile_%p", doc);
    gchar *menu_path = g_strdup_printf("/menu/Windows/OpenDocuments/%s", verb_name);
    gchar *cmd_path = g_strdup_printf("/commands/%s", verb_name);

    bonobo_ui_component_remove_verb(win->uic, verb_name);
    bonobo_ui_component_rm(win->uic, cmd_path, NULL);
    bonobo_ui_component_rm(win->uic, menu_path, NULL);
 
    g_free(cmd_path);
    g_free(menu_path);
    g_free(verb_name);
}

void
ghex_window_add_doc_to_list(GHexWindow *win, HexDocument *doc)
{
    gchar *menu = NULL, *verb_name;
    gchar *cmd = NULL;
    gchar *escaped_name;
    gchar *tip;

    escaped_name = escape_underscores(doc->path_end);
    tip = g_strdup_printf(_("Activate file %s"), doc->path_end);
    verb_name = g_strdup_printf("FilesFile_%p", doc);
    menu = g_strdup_printf("<menuitem name=\"%s\" verb=\"%s\" label=\"%s\"/>",
                           verb_name, verb_name, escaped_name);
    cmd = g_strdup_printf("<cmd name=\"%s\" _label=\"%s\" _tip=\"%s\"/>",
                          verb_name, escaped_name, tip);
    g_free(tip);
    g_free(escaped_name);

    bonobo_ui_component_set_translate(win->uic, "/menu/Windows/OpenDocuments", menu, NULL);
    bonobo_ui_component_set_translate(win->uic, "/commands/", cmd, NULL);
    bonobo_ui_component_add_verb(win->uic, verb_name, file_list_activated_cb, doc);

    g_free(menu);
    g_free(cmd);
    g_free(verb_name);
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

BonoboUIComponent *
ghex_window_get_ui_component(GHexWindow *win)
{
    return win->uic;
}

void
ghex_window_set_doc_name(GHexWindow *win, const gchar *name)
{
    gchar *title = g_strdup_printf(_("GHex - %s"), name);
    gtk_window_set_title(GTK_WINDOW(win), title);
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

	gtk_timeout_remove (mi->timeoutid);
	g_free (mi);
}

static gint
remove_message_timeout (MessageInfo * mi)
{
	GDK_THREADS_ENTER ();

	/* Remove the status message */
	/* NOTE : Use space ' ' not an empty string '' */
	ghex_window_show_status (mi->win, " ");
    g_signal_handlers_disconnect_by_func(G_OBJECT(mi->win),
                                         remove_timeout_cb, mi);
	g_free (mi);

	GDK_THREADS_LEAVE ();

	return FALSE; /* removes the timeout */
}

static const guint32 flash_length = 3000; /* 3 seconds, I hope */

/**
 * ghex_window_set_status
 * @win: Pointer a Bonobo window object.
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

	if (strcmp (msg, " "))
		bonobo_ui_component_set_status (win->uic, " ", NULL);
	bonobo_ui_component_set_status (win->uic, msg, NULL);
}

/**
 * ghex_window_flash
 * @app: Pointer a Bonobo window object
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
		gtk_timeout_add (flash_length,
                         (GtkFunction) remove_message_timeout,
                         mi);
	mi->handlerid =
		gtk_signal_connect (GTK_OBJECT(win),
                            "destroy",
                            GTK_SIGNAL_FUNC (remove_timeout_cb),
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
