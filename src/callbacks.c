/*
 * callbacks.c - callbacks for GHex widgets
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */
#include <gnome.h>
#include <libgnome/gnome-help.h> 
#include <string.h>
#include "hex-document.h"
#include "ghex.h"
#include "callbacks.h"

void about_cb (GtkWidget *widget) {
  GtkWidget *about;

  gchar *authors[] = {
	  "Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>",
          NULL
  };

  about = gnome_about_new ( _("GHex, a binary file editor"), VERSION,
			    "(C) 1998 Jaka Mocnik", authors,
			    _("A small contribution to the GNOME project.\n"
			      "Send bug reports (patches preferred ;)) to:\n"
			      "<jaka.mocnik@kiss.uni-lj.si>"), NULL);

  gtk_widget_show (about);
}

void quit_app_cb (GtkWidget *widget) {
  if(gnome_mdi_remove_all(mdi, FALSE))
    gtk_object_destroy(GTK_OBJECT(mdi));
}

void show_help_cb (GtkWidget *widget) {
  static GnomeHelpMenuEntry entry = {
    "ghex",
    "index.html"
  };

  gnome_help_display(NULL, &entry);
}

void set_group_type_cb(GtkWidget *w, guint *type) {
  if( GTK_CHECK_MENU_ITEM(w)->active && mdi->active_view)
    gtk_hex_set_group_type(GTK_HEX(mdi->active_view), *type);
}

void properties_modified_cb(GtkWidget *w, GnomePropertyBox *pbox) {
  gnome_property_box_changed(pbox);
}

void save_cb(GtkWidget *w) {
  if(mdi->active_child)
    if(hex_document_write(HEX_DOCUMENT(mdi->active_child)))
      report_error(_("Error saving file!")); 
}

void revert_cb(GtkWidget *w) {
  if(mdi->active_child)
    hex_document_read(HEX_DOCUMENT(mdi->active_child));
}

void open_selected_file(GtkWidget *w) {
  HexDocument *new_doc;

  if((new_doc = hex_document_new((gtk_file_selection_get_filename (GTK_FILE_SELECTION (file_sel))))) != NULL) {
    gnome_mdi_add_child(mdi, GNOME_MDI_CHILD(new_doc));
    gnome_mdi_add_view(mdi, GNOME_MDI_CHILD(new_doc));
  }
  else
    report_error(_("Can not open file!"));

  gtk_widget_destroy(GTK_WIDGET(file_sel));
  file_sel = NULL;
}

void save_selected_file(GtkWidget *w) {
  HexDocument *doc;
  gchar *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_sel));
  int i;

  if(mdi->active_child == NULL)
    return;

  doc = HEX_DOCUMENT(mdi->active_child);

  if((doc->file = fopen(filename, "w")) != NULL) {
    if(fwrite(doc->buffer, doc->buffer_size, 1, doc->file) == 1) {
      if(doc->file_name)
	free(doc->file_name);
      doc->file_name = strdup(filename);

      for(i = strlen(doc->file_name);
	  (i >= 0) && (doc->file_name[i] != '/');
	  i--)
	;
      if(doc->file_name[i] == '/')
	doc->path_end = &doc->file_name[i+1];
      else
	doc->path_end = doc->file_name;

      gnome_mdi_child_set_name(GNOME_MDI_CHILD(doc), doc->path_end);
    }
    else
      report_error(_("Error saving file!"));
    fclose(doc->file);
  }
  else
    report_error(_("Can't open file for writing!"));

  gtk_widget_destroy(GTK_WIDGET(file_sel));
  file_sel = NULL;
}

void cancel_cb(GtkWidget *w, GtkWidget **me) {
  gtk_widget_destroy(*me);
  *me = NULL;
}

gint delete_event_cb(GtkWidget *w, gpointer who_cares, GtkWidget **me) {
  gtk_widget_destroy(*me);
  *me = NULL;

  return TRUE;  /* stop default delete_event handlers */
}

void prop_destroy_cb(GtkWidget *w, PropertyUI **data) {
  g_free(*data);
  *data = NULL;
}

void open_cb(GtkWidget *w) {
  if(file_sel == NULL)
    file_sel = gtk_file_selection_new(_("Select a file to open"));

  gtk_window_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);

  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->ok_button),
		      "clicked", GTK_SIGNAL_FUNC(open_selected_file),
		      NULL);
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->cancel_button),
		      "clicked", GTK_SIGNAL_FUNC(cancel_cb),
		      &file_sel);
  gtk_widget_show (file_sel);
}

void save_as_cb(GtkWidget *w) {
  if(mdi->active_child == NULL)
    return;

  if(file_sel == NULL)
    file_sel = gtk_file_selection_new(_("Select a file to save buffer as"));

  gtk_window_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);

  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->ok_button),
		      "clicked", GTK_SIGNAL_FUNC(save_selected_file),
		      NULL);
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_sel)->cancel_button),
		      "clicked", GTK_SIGNAL_FUNC(cancel_cb),
		      &file_sel);
  gtk_widget_show (file_sel);
}

void close_cb(GtkWidget *w) {
  if(mdi->active_child == NULL)
    return;

  gnome_mdi_remove_child(mdi, mdi->active_child, FALSE);
}

void find_cb(GtkWidget *w) {
  if(find_dialog == NULL)
    create_find_dialog(&find_dialog);

  gtk_window_position (GTK_WINDOW(find_dialog), GTK_WIN_POS_MOUSE);

  gtk_widget_show(find_dialog);
}

void replace_cb(GtkWidget *w) {
  if(replace_dialog == NULL)
    create_replace_dialog(&replace_dialog);

  gtk_window_position (GTK_WINDOW(replace_dialog), GTK_WIN_POS_MOUSE);

  gtk_widget_show(replace_dialog);
}

void jump_cb(GtkWidget *w) {
  if(jump_dialog == NULL)
    create_jump_dialog(&jump_dialog);

  gtk_window_position (GTK_WINDOW(jump_dialog), GTK_WIN_POS_MOUSE);

  gtk_widget_show(jump_dialog);
}

void prefs_cb(GtkWidget *w) {
  if(prefs_ui == NULL)
    create_prefs_dialog(&prefs_ui);

  gtk_window_position (GTK_WINDOW(prefs_ui->pbox), GTK_WIN_POS_MOUSE);

  gtk_widget_show(GTK_WIDGET(prefs_ui->pbox));
}

static gint get_search_string(gchar *str, gchar *buf) {
  gint len = strlen(str), shift;

  if(len > 0) {
    if(str[0] == '$') {
      if(len > 1) {
	if(str[1] == '$') {
	  /* a string beginning with '$$' == escaped '$' */
	  strcpy(buf, &str[1]);
	  len--;
	}
	else {
	  /* we convert the string from hex */
	  if(len % 2 == 0)
	    return 0;  /* the number of hex digits must be EVEN */
	  len = 0;     /* we'll store the returned string length in len */
	  str++;
	  shift = 4;
	  *buf = '\0';
	  while(*str != 0) {
	    if((*str >= '0') && (*str <= '9'))
	      *buf |= (*str - '0') << shift;
	    else if((*str >= 'A') && (*str <= 'F'))
	      *buf |= (*str - 'A') << shift;
	    else if((*str >= 'a') && (*str <= 'f'))
	      *buf |= (*str - 'a' + 10) << shift;
	    else
	      return 0;

	    if(shift > 0)
	      shift = 0;
	    else {
	      shift = 4;
	      buf++;
	      len++;
	      *buf = '\0';
	    }

	    str++;
	  }
	}
	return len;
      }
      return 0;
    }
    strcpy(buf, str);
    return len;
  }
  return 0;
}

void find_next_cb(GtkWidget *w, GtkEntry *data) {
  GtkHex *gh;
  guint offset, str_len;
  gchar str[256];

  if((str_len = get_search_string(gtk_entry_get_text(data), str)) == 0) {
    report_error(_("There seems to be no string to search for!"));
    return;
  }

  if(mdi->active_child == NULL) {
    report_error(_("There is no active buffer to search!"));
    return;
  }

  gh = GTK_HEX(mdi->active_view);

  if(find_string_forward(HEX_DOCUMENT(mdi->active_child), gh->cursor_pos+1, str, str_len, &offset) == 0)
    gtk_hex_set_cursor(gh, offset);
  else
    show_message(_("End Of File reached"));
}

void find_prev_cb(GtkWidget *w, GtkEntry *data) {
  GtkHex *gh;
  guint offset, str_len;
  gchar str[256];

  if((str_len = get_search_string(gtk_entry_get_text(data), str)) == 0) {
    report_error(_("There seems to be no string to search for!"));
    return;
  }

  if(mdi->active_child == NULL) {
    report_error(_("There is no active buffer to search!"));
    return;
  }

  gh = GTK_HEX(mdi->active_view);

  if(find_string_backward(HEX_DOCUMENT(mdi->active_child), gh->cursor_pos-1, str, str_len, &offset) == 0)
    gtk_hex_set_cursor(gh, offset);
  else
    show_message(_("Beginning Of File reached"));
}

void goto_byte_cb(GtkWidget *w, GtkEntry *data) {
  guint byte;
  gchar *byte_str = gtk_entry_get_text(data), *endptr;
  
  if(mdi->active_child == NULL) {
    report_error(_("There is no active buffer to move the cursor in!"));
    return;
  }

  if(strlen(byte_str) == 0) {
    report_error(_("No offset has been specified!"));
    return;
  }

  byte = strtoul(byte_str, &endptr, 10);

  if(*endptr != '\0') {
    report_error(_("The offset must be a positive integer value!"));
    return;
  }

  if(byte >= HEX_DOCUMENT(mdi->active_child)->buffer_size) {
    report_error(_("Can not position cursor beyond the End Of File!"));
    return;
  }

  gtk_hex_set_cursor(GTK_HEX(mdi->active_view), byte);
}

void replace_one_cb(GtkWidget *w, ReplaceCBData *data) {
  gchar find_str[256], rep_str[256];
  guint find_len, rep_len, offset;
  GtkHex *gh;
  HexDocument *doc;

  if(mdi->active_child == NULL) {
    report_error(_("There is no active buffer to replace data in!"));
    return;
  }

  gh = GTK_HEX(mdi->active_view);
  doc = HEX_DOCUMENT(mdi->active_child);

  if( ((find_len = get_search_string(gtk_entry_get_text(data->find), find_str)) == 0) ||
      ((rep_len = get_search_string(gtk_entry_get_text(data->replace), rep_str)) == 0)) {
    report_error(_("Erroneous find or replace string!"));
    return;
  }

  if(find_len != rep_len) {
    report_error(_("Both strings must be of the same length!"));
    return;
  }

  if(find_len > doc->buffer_size - gh->cursor_pos)
    return;

  if(compare_data(&doc->buffer[gh->cursor_pos], find_str, find_len) == 0)
    hex_document_set_data(doc, gh->cursor_pos, rep_len, rep_str);

  if(find_string_forward(doc, gh->cursor_pos+1, find_str, find_len, &offset) == 0)
    gtk_hex_set_cursor(gh, offset);
  else
    show_message(_("End Of File reached!"));
}

void replace_all_cb(GtkWidget *w, ReplaceCBData *data) {
  gchar find_str[256], rep_str[256];
  guint find_len, rep_len, offset, count;
  GtkHex *gh;
  HexDocument *doc;

  if(mdi->active_child == NULL) {
    report_error(_("There is no active buffer to replace data in!"));
    return;
  }

  gh = GTK_HEX(mdi->active_view);
  doc = HEX_DOCUMENT(mdi->active_child);

  if( ((find_len = get_search_string(gtk_entry_get_text(data->find), find_str)) == 0) ||
      ((rep_len = get_search_string(gtk_entry_get_text(data->replace), rep_str)) == 0)) {
    report_error(_("Erroneous find or replace string!"));
    return;
  }

  if(find_len != rep_len) {
    report_error(_("Both strings must be of the same length!"));
    return;
  }

  if(find_len > doc->buffer_size - gh->cursor_pos)
    return;

  count = 0;
  while(find_string_forward(doc, gh->cursor_pos, find_str, find_len, &offset) == 0) {
    hex_document_set_data(doc, offset, rep_len, rep_str);
    count++;
  }

  gtk_hex_set_cursor(gh, offset);  

  sprintf(find_str, _("Replaced %d occurencies."), count);
  show_message(find_str);
}

void select_font_cb(GtkWidget *w, GnomePropertyBox *pbox) {
  gchar *font_desc;
  GList *doc, *view;

  if((font_desc = gnome_font_select()) != NULL) {
    if(strcmp(font_desc, def_font_name) != 0) {
      gnome_property_box_changed(pbox);
      gtk_label_set(GTK_LABEL(GTK_BUTTON(w)->child), font_desc);
    }
    g_free(font_desc);
  }
}

void apply_changes_cb(GnomePropertyBox *pbox, gint page, PropertyUI *pui) {
  int i;
  GList *child, *view;
  GdkFont *new_font;

  if ( page != -1 ) return; /* Only do something on global apply */

  for(i = 0; i < 3; i++)
    if(GTK_TOGGLE_BUTTON(pui->group_type[i])->active) {
      def_group_type = group_type[i];
      break;
    }

  for(i = 0; i < 3; i++)
    if(GTK_TOGGLE_BUTTON(pui->mdi_type[i])->active) {
      mdi_mode = mdi_type[i];
      gnome_mdi_set_mode(mdi, mdi_mode);
      break;
    }

  if(strcmp(GTK_LABEL(pui->font_button->child)->label, def_font_name) != 0) {
    if((new_font = gdk_font_load(GTK_LABEL(pui->font_button->child)->label)) != NULL) {
      child = mdi->children;

      while(child) {
	view = GNOME_MDI_CHILD(child->data)->views;
	while(view) {
	  gtk_hex_set_font(GTK_HEX(view->data), new_font);
	  view = g_list_next(view);
	}
	child = g_list_next(child);
      }

      if(def_font)
        gdk_font_unref(def_font);

      def_font = new_font;

      free(def_font_name);

      def_font_name = g_strdup(GTK_LABEL(pui->font_button->child)->label);
    }
    else
      report_error(_("Can not open desired font!"));
  }
}
      
void add_view_cb(GtkWidget *w) {
  if(mdi->active_child)
    gnome_mdi_add_view(mdi, mdi->active_child);
}

void remove_view_cb(GtkWidget *w) {
  if(mdi->active_view)
    gnome_mdi_remove_view(mdi, mdi->active_view, FALSE);
}

gint remove_doc_cb(GnomeMDI *mdi, HexDocument *doc) {
  static char msg[512];
  GnomeMessageBox *mbox;
  gint reply;

  sprintf(msg, _("The document %s has changed since last save.\n"
                 "Do you want to save changes?"), GNOME_MDI_CHILD(doc)->name);

  if(hex_document_has_changed(doc)) {
    mbox = GNOME_MESSAGE_BOX(gnome_message_box_new( msg, GNOME_MESSAGE_BOX_QUESTION, GNOME_STOCK_BUTTON_YES,
						    GNOME_STOCK_BUTTON_NO, GNOME_STOCK_BUTTON_CANCEL, NULL));
    gnome_dialog_set_default(GNOME_DIALOG(mbox), 2);
    reply = ask_user(mbox);

    if(reply == 0)
      hex_document_write(doc);
    else if(reply == 2)
      return FALSE;
  }

  return TRUE;
}

void cleanup_cb(GnomeMDI *mdi) {
  save_configuration();
  gtk_main_quit();
}

void app_created_cb(GnomeMDI *mdi, GnomeApp *app) {
  GtkWidget *sb;

  sb = gtk_statusbar_new();
  gnome_app_set_statusbar(app, sb);
}









