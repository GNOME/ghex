/*
 * callbacks.c - callbacks for GHex widgets
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */
#include <libgnome/gnome-help.h> 
#include <string.h>
#include "ghex.h"
#include "callbacks.h"

void about_cb (GtkWidget *widget) {
  GtkWidget *about;

  gchar *authors[] = {
	  "Jaka Mocnik",
          NULL
          };

  about = gnome_about_new ( _("GHex, a binary file editor"), VERSION,
			    "(C) 1998 Jaka Mocnik", authors,
			    _("A small contribution to the GNOME project.\n"
			      "Send bug reports (patches preferred ;)) to:\n"
			      "<jaka.mocnik@kiss.uni-lj.si>"), NULL);

  gtk_widget_show (about);
}

void show_help_cb (GtkWidget *widget) {
  static GnomeHelpMenuEntry entry = {
    "ghex",
    "index.html"
  };

  gnome_help_display(NULL, &entry);
}

void set_group_type_cb(GtkWidget *w, guint *type) {
  if(active_fe)
    gtk_hex_set_group_type(GTK_HEX(active_fe->hexedit), *type);
}

void select_buffer_cb(GtkWidget *w, FileEntry *fe) {
  if(active_fe != fe) {
    remove_view(active_fe);
    active_fe = fe;
    add_view(fe);
  }
} 

void quit_app_cb(GtkWidget *w, void *data) {  
  if(active_fe)
    remove_view(active_fe);

  g_slist_foreach(buffer_list, (GFunc)close_file, NULL);

  if(save_config_on_exit)
    save_configuration();

  gtk_main_quit();
}

void save_cb(GtkWidget *w) {
  if(active_fe)
    if(write_file(active_fe))
      report_error(_("Error saving file!")); 
}

void revert_cb(GtkWidget *w) {
  if(active_fe) {
    read_file(active_fe);
    gtk_signal_emit_by_name(GTK_OBJECT(active_fe->hexedit), "data_changed",
			    0, active_fe->len - 1);
  }
}

void open_selected_file(GtkWidget *w) {
  FileEntry *new_fe, *fe;

  fe = active_fe;
  if((new_fe = open_file(gtk_file_selection_get_filename (GTK_FILE_SELECTION (file_sel)))) != NULL) {
    active_fe = new_fe;
    if(fe)
      remove_view(fe);
    add_view(new_fe);
  }
  else
    report_error(_("Can not open file!"));

  gtk_widget_destroy(GTK_WIDGET(file_sel));
  file_sel = NULL;
}

void save_selected_file(GtkWidget *w) {
  gchar *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_sel));
  int i;

  if((active_fe->file = fopen(filename, "w")) != NULL) {
    if(fwrite(active_fe->contents, active_fe->len, 1, active_fe->file) == 1) {
      if(active_fe->file_name)
	free(active_fe->file_name);
      active_fe->file_name = strdup(filename);

      for(i = strlen(active_fe->file_name);
	  (i >= 0) && (active_fe->file_name[i] != '/');
	  i--)
	;
      if(active_fe->file_name[i] == '/')
	active_fe->path_end = &active_fe->file_name[i+1];
      else
	active_fe->path_end = active_fe->file_name;

      remake_buffer_menu();
    }
    else
      report_error(_("Error saving file!"));
    fclose(active_fe->file);
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
  if(active_fe == NULL)
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
  GSList *first;

  if(active_fe == NULL)
    return;

  remove_view(active_fe);
  buffer_list = g_slist_remove(buffer_list, active_fe);
  close_file(active_fe);
  if((first = g_slist_nth(buffer_list, 1)) != NULL) {
    active_fe = (FileEntry *)first->data;
    add_view(active_fe);
  }
  else
    active_fe = NULL;
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

  if(active_fe == NULL) {
    report_error(_("There is no active buffer to search!"));
    return;
  }

  gh = GTK_HEX(active_fe->hexedit);

  if(find_string_forward(active_fe, gh->cursor_pos+1, str, str_len, &offset) == 0)
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

  if(active_fe == NULL) {
    report_error(_("There is no active buffer to search!"));
    return;
  }

  gh = GTK_HEX(active_fe->hexedit);

  if(find_string_backward(active_fe, gh->cursor_pos-1, str, str_len, &offset) == 0)
    gtk_hex_set_cursor(gh, offset);
  else
    show_message(_("Beginning Of File reached"));
}

void goto_byte_cb(GtkWidget *w, GtkEntry *data) {
  guint byte;
  gchar *byte_str = gtk_entry_get_text(data), *endptr;
  
  if(active_fe == NULL) {
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

  if(byte >= active_fe->len) {
    report_error(_("Can not position cursor beyond the End Of File!"));
    return;
  }

  gtk_hex_set_cursor(GTK_HEX(active_fe->hexedit), byte);
}

void replace_one_cb(GtkWidget *w, ReplaceCBData *data) {
  gchar find_str[256], rep_str[256];
  guint find_len, rep_len, offset;
  GtkHex *gh;

  if(active_fe == NULL) {
    report_error(_("There is no active buffer to replace data in!"));
    return;
  }

  gh = GTK_HEX(active_fe->hexedit);

  if( ((find_len = get_search_string(gtk_entry_get_text(data->find), find_str)) == 0) ||
      ((rep_len = get_search_string(gtk_entry_get_text(data->replace), rep_str)) == 0)) {
    report_error(_("Erroneous find or replace string!"));
    return;
  }

  if(find_len != rep_len) {
    report_error(_("Both strings must be of the same length!"));
    return;
  }

  if(find_len > gh->buffer_size - gh->cursor_pos)
    return;

  if(compare_data(&gh->buffer[gh->cursor_pos], find_str, find_len) == 0)
    gtk_hex_set_data(gh, gh->cursor_pos, rep_len, rep_str);

  if(find_string_forward(active_fe, gh->cursor_pos+1, find_str, find_len, &offset) == 0)
    gtk_hex_set_cursor(GTK_HEX(active_fe->hexedit), offset);
  else
    show_message(_("End Of File reached!"));
}

void replace_all_cb(GtkWidget *w, ReplaceCBData *data) {
  gchar find_str[256], rep_str[256];
  guint find_len, rep_len, offset, count;
  GtkHex *gh;

  if(active_fe == NULL) {
    report_error(_("There is no active buffer to replace data in!"));
    return;
  }

  gh = GTK_HEX(active_fe->hexedit);

  if( ((find_len = get_search_string(gtk_entry_get_text(data->find), find_str)) == 0) ||
      ((rep_len = get_search_string(gtk_entry_get_text(data->replace), rep_str)) == 0)) {
    report_error(_("Erroneous find or replace string!"));
    return;
  }

  if(find_len != rep_len) {
    report_error(_("Both strings must be of the same length!"));
    return;
  }

  if(find_len > gh->buffer_size - gh->cursor_pos)
    return;

  count = 0;
  while(find_string_forward(active_fe, gh->cursor_pos, find_str, find_len, &offset) == 0) {
    gtk_hex_set_data(gh, offset, rep_len, rep_str);
    count++;
  }

  gtk_hex_set_cursor(GTK_HEX(active_fe->hexedit), offset);  

  sprintf(find_str, _("Replaced %d occurencies."), count);
  show_message(find_str);
}

void select_font_cb(GtkWidget *w) {
  gchar *font_desc;
  GdkFont *new_font;

  if((font_desc = gnome_font_select()) != NULL) {
    if((new_font = gdk_font_load(font_desc)) != NULL) {
      if(active_fe)
	gtk_hex_set_font(GTK_HEX(active_fe->hexedit), new_font);
      if(def_font)
        gdk_font_unref(def_font);
      def_font = new_font;
      strcpy(def_font_name, font_desc);
    }
    else
      report_error(_("Can not open desired font!"));
    free(font_desc);
  }
}

void save_on_exit_cb(GtkWidget *w) {
  if(GTK_CHECK_MENU_ITEM(w)->active)
    save_config_on_exit = TRUE;
  else
    save_config_on_exit = FALSE;
}

      
