/*
 * ui.c - GHex user interface
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */

#include "gtkhex.h"
#include "gnome-support.h"
#include "callbacks.h"
#include "ghex.h"

GtkWidget *app, *table;
GtkWidget *tbar, *mbar;
GtkWidget *name_label, *offset_label, *data_label;
GtkAcceleratorTable *accel;

GtkWidget *file_sel = NULL;
GtkWidget *find_dialog = NULL;
GtkWidget *replace_dialog = NULL;
GtkWidget *jump_dialog = NULL;

/* the "important" menus and menu items */
GtkMenuItem *group_menu, *buffers_menu;
GtkCheckMenuItem *save_config_item;

GdkFont *def_font = NULL;
gchar def_font_name[256];

guint group_type[3] = {
  GROUP_BYTE,
  GROUP_WORD,
  GROUP_LONG,
};

gchar *group_type_label[3] = {
  _("Single Bytes"),
  _("Words"),
  _("Longwords"),
};

void cursor_moved(GtkWidget *w) {
  static gchar info_msg[64];

  sprintf(info_msg, "%9d / %d", GTK_HEX(w)->cursor_pos, GTK_HEX(w)->buffer_size-1);
  gtk_label_set(GTK_LABEL(offset_label), info_msg);
  data_changed(w, GTK_HEX(w)->cursor_pos, GTK_HEX(w)->cursor_pos);
}

void data_changed(GtkWidget *w, guint start, guint end) {
  GtkHex *gh = GTK_HEX(w);
  static gchar data_msg[64];
  guint word, longword, pos;

  if((gh->cursor_pos >= start) && (gh->cursor_pos <= end)) {
    pos = gh->cursor_pos;
    while(pos % 2 != 0)
      pos--;
    if(pos > gh->buffer_size-2)
      word = 0;
    else
      word = (gh->buffer[pos] << 8) | gh->buffer[pos+1];
    while(pos % 4 != 0)
      pos--;
    if(pos > gh->buffer_size-4)
      longword = 0;
    else
      longword = (((((gh->buffer[pos] << 8) | gh->buffer[pos+1]) << 8) |
	       gh->buffer[pos+2]) << 8) | gh->buffer[pos+3];

    sprintf(data_msg, "B:%03u W:%05lu L:%010lu", gh->buffer[gh->cursor_pos], word, longword);
    gtk_label_set(GTK_LABEL(data_label), data_msg);
  }
}

void show_message(gchar *msg) {
  GtkWidget *message_box;

  message_box = gnome_message_box_new(msg, GNOME_MESSAGE_BOX_INFO, _("OK"), NULL);
  gnome_message_box_set_modal(GNOME_MESSAGE_BOX(message_box));
  gtk_window_position (GTK_WINDOW (message_box), GTK_WIN_POS_MOUSE);
  gtk_widget_show(message_box);
}

void report_error(gchar *msg) {
  GtkWidget *message_box;

  message_box = gnome_message_box_new(msg, GNOME_MESSAGE_BOX_ERROR, _("OK"), NULL);
  gnome_message_box_set_modal(GNOME_MESSAGE_BOX(message_box));
  gtk_window_position (GTK_WINDOW (message_box), GTK_WIN_POS_MOUSE);
  gtk_widget_show(message_box);
}

guint get_desired_group_type() {
  GtkMenuShell *menu;
  GList *children;
  int i;

  menu = GTK_MENU_SHELL(group_menu->submenu);

  for(children = menu->children, i = 0; children != NULL; children = g_list_next(children), i++) {
    if(GTK_CHECK_MENU_ITEM(children->data)->active)
      return (1L << i);
  }

  return 1;
}

void set_desired_group_type(gint type) {
  GtkMenuShell *menu;
  GList *children;
  int i;

  menu = GTK_MENU_SHELL(group_menu->submenu);

  for(children = menu->children, i = 0; children != NULL; children = g_list_next(children), i++) {
    if((1L << i) == type)
      gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(children->data), TRUE);
  }
}

int add_view(FileEntry *fe) {

  fe->hexedit = gtk_hex_new();

  gtk_signal_connect(GTK_OBJECT(fe->hexedit), "cursor_moved",
		     GTK_SIGNAL_FUNC(cursor_moved), NULL);
  gtk_signal_connect(GTK_OBJECT(fe->hexedit), "data_changed",
		     GTK_SIGNAL_FUNC(data_changed), NULL);

  gtk_hex_set_buffer(GTK_HEX(fe->hexedit), fe->contents, fe->len);
  GTK_HEX(fe->hexedit)->group_type = get_desired_group_type();
  gtk_hex_set_cursor(GTK_HEX(fe->hexedit), fe->cursor_pos);

  gtk_table_attach(GTK_TABLE(table), fe->hexedit, 0, 3, 0, 1,
		   GTK_FILL|GTK_SHRINK|GTK_EXPAND,
		   GTK_FILL|GTK_SHRINK|GTK_EXPAND, 0, 0);
  if(def_font)
    gtk_hex_set_font(GTK_HEX(fe->hexedit), def_font);

  gtk_widget_show(fe->hexedit);

  gtk_label_set(GTK_LABEL(name_label), fe->path_end);

  gtk_signal_emit_by_name(GTK_OBJECT(fe->hexedit), "cursor_moved");
  return 0;
}

void remove_view(FileEntry *fe) {
  if(fe == NULL)
    return;

  fe->cursor_pos = gtk_hex_get_cursor(GTK_HEX(fe->hexedit));

  gtk_label_set(GTK_LABEL(name_label), NO_BUFFER_LABEL);
  gtk_label_set(GTK_LABEL(offset_label), "");
  gtk_label_set(GTK_LABEL(data_label), "");

  gtk_widget_destroy(fe->hexedit);

  fe->hexedit = NULL;
}

void add_buffer_entry(FileEntry *fe) {
  GtkWidget *menu, *menuitem;

  if(fe == NULL)
    return;

  menu = buffers_menu->submenu;

  if(menu == NULL) {
    menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(buffers_menu), menu);
  }

  menuitem = gtk_menu_item_new_with_label(fe->file_name);

  gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(select_buffer_cb), fe);

  gtk_menu_append(GTK_MENU(menu), menuitem);

  gtk_widget_show(menuitem);
}

void remake_buffer_menu() {
  /* uh-huh... how to dynamically remove menuitems? should I directly manipulate the
     gtkmenu's children GList?
     for now, we will just remake the whole menu... :(
     fe must first be removed from buffer_list for this to work!!!
     */
  GtkWidget *menu;

  menu = buffers_menu->submenu;

  if(menu) {
#if 0
    gtk_menu_detach(GTK_MENU(menu));
    gtk_widget_destroy(menu);
#endif
  }

  if(g_slist_length(buffer_list) > 1) {
    menu = gtk_menu_new();
    gtk_menu_item_set_submenu(buffers_menu, menu);
    
    g_slist_foreach(buffer_list, (GFunc)add_buffer_entry, NULL);
  }
}

GtkWidget *create_group_type_menu() {
  GtkWidget *menu;
  GtkWidget *menuitem;
  GSList *group;
  int i;

  menu = gtk_menu_new ();
  group = NULL;
  
  for(i = 0; i < 3; i++) {
    menuitem = gtk_radio_menu_item_new_with_label (group, group_type_label[i]);
    gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(set_group_type_cb), &group_type[i]);
    group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM(menuitem));
    gtk_menu_append (GTK_MENU (menu), menuitem);
    gtk_widget_show (menuitem);
  }

  return menu;
}

GtkWidget *create_button(gchar *type, gchar *text) {
  GtkWidget *button, *pixmap, *label, *hbox;

  hbox = gtk_hbox_new(FALSE, 2);

  label = gtk_label_new(text);
  pixmap = gnome_stock_pixmap_widget(app, type);

  gtk_box_pack_start(GTK_BOX(hbox), pixmap, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 1);

  button = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(button), hbox);

  gtk_widget_show(label);
  gtk_widget_show(pixmap);
  gtk_widget_show(hbox);

  return button;
}

void create_toolbar() {
  tbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
  gtk_container_border_width(GTK_CONTAINER(tbar), 2);
  gtk_widget_show(tbar);

  gtk_toolbar_append_item (GTK_TOOLBAR (tbar),
			   _("Open"), _("Open a file"), NULL,
			   gnome_stock_pixmap_widget(app, GNOME_STOCK_PIXMAP_OPEN),
			   GTK_SIGNAL_FUNC(open_cb), NULL);
  gtk_toolbar_append_item (GTK_TOOLBAR (tbar),
			   _("Save"), _("Save file"), NULL,
			   gnome_stock_pixmap_widget(app, GNOME_STOCK_PIXMAP_SAVE),
			   GTK_SIGNAL_FUNC(save_cb), NULL);
  gtk_toolbar_append_item (GTK_TOOLBAR (tbar),
			   _("Revert"), _("Revert file"), NULL,
			   NULL, GTK_SIGNAL_FUNC(revert_cb), NULL);
  gtk_toolbar_append_item (GTK_TOOLBAR (tbar),
			   _("Close"), _("Close file"), NULL,
			   NULL, GTK_SIGNAL_FUNC(close_cb), NULL);

  gtk_toolbar_append_space(GTK_TOOLBAR(tbar));
}

void create_menus() {
  GtkWidget *w, *menu;

  accel = gtk_accelerator_table_new();
  mbar = gtk_menu_bar_new();
  gtk_widget_show(mbar);
  
  menu = gtk_menu_new();

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_NEW, _("Open..."));
  gtk_widget_show(w);
  gtk_widget_install_accelerator(w, accel, "activate",
                                       'O', GDK_CONTROL_MASK);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(open_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_OPEN, _("Save"));
  gtk_widget_show(w);
  gtk_widget_install_accelerator(w, accel, "activate",
				 'S', GDK_CONTROL_MASK);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(save_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_SAVE_AS, _("Save as..."));
  gtk_widget_show(w);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(save_as_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_BLANK, _("Revert"));
  gtk_widget_show(w);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(revert_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_BLANK, _("Close"));
  gtk_widget_show(w);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(close_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gtk_menu_item_new();
  gtk_widget_show(w);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_QUIT, _("Quit"));
  gtk_widget_show(w);
  gtk_widget_install_accelerator(w, accel, "activate",
				 'Q', GDK_CONTROL_MASK);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(quit_app_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gtk_menu_item_new_with_label(_("File"));
  gtk_widget_show(w);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), menu);
  gtk_menu_bar_append(GTK_MENU_BAR(mbar), w);

  /* the Edit menu */
  menu = gtk_menu_new();

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_SEARCH, _("Find..."));
  gtk_widget_show(w);
  gtk_widget_install_accelerator(w, accel, "activate",
				 'F', GDK_CONTROL_MASK);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(find_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_BLANK,_("Replace..."));
  gtk_widget_show(w);
  gtk_widget_install_accelerator(w, accel, "activate",
				 'R', GDK_CONTROL_MASK);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(replace_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gtk_menu_item_new();
  gtk_widget_show(w);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_BLANK,_("Goto byte"));
  gtk_widget_show(w);
  gtk_widget_install_accelerator(w, accel, "activate",
				 'G', GDK_CONTROL_MASK);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(jump_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gtk_menu_item_new_with_label(_("Edit"));
  gtk_widget_show(w);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), menu);
  gtk_menu_bar_append(GTK_MENU_BAR(mbar), w);

  /* the Options menu */
  menu = gtk_menu_new();

  w = gtk_menu_item_new_with_label(_("Group Data As"));
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), create_group_type_menu());
  group_menu = GTK_MENU_ITEM(w);
  gtk_widget_show(w);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gtk_menu_item_new_with_label(_("Select Font..."));
  gtk_widget_show(w);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(select_font_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gtk_menu_item_new();
  gtk_widget_show(w);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gtk_check_menu_item_new_with_label(_("Save Configuration On Exit"));
  gtk_widget_show(w);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(save_on_exit_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);  
  save_config_item = GTK_CHECK_MENU_ITEM(w);

  w = gtk_menu_item_new_with_label(_("Options"));
  gtk_widget_show(w);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), menu);
  gtk_menu_bar_append(GTK_MENU_BAR(mbar), w);

  /* the Buffers menu */
  w = gtk_menu_item_new_with_label(_("Buffers"));
  buffers_menu = GTK_MENU_ITEM(w);
  gtk_widget_show(w);
  gtk_menu_bar_append(GTK_MENU_BAR(mbar), w);

  /* the Help menu */
  menu = gtk_menu_new();

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_ABOUT, _("About..."));
  gtk_widget_show(w);
  gtk_widget_install_accelerator(w, accel, "activate",
				 'A', GDK_CONTROL_MASK);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(about_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gtk_menu_item_new();
  gtk_widget_show(w);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_BLANK, _("Help..."));
  gtk_widget_show(w);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(show_help_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);  

  w = gtk_menu_item_new_with_label(_("Help"));
  gtk_widget_show(w);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), menu);
  gtk_menu_item_right_justify(GTK_MENU_ITEM(w));
  gtk_menu_bar_append(GTK_MENU_BAR(mbar), w);


}

void create_find_dialog(GtkWidget **dialog) {
  GtkWidget *window;

  GtkWidget *f_next, *f_prev, *f_close, *f_string;

  window = gtk_dialog_new();

  gtk_window_set_title(GTK_WINDOW(window), _("Find Data"));

  f_string = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), f_string,
		     TRUE, TRUE, 0);
  gtk_widget_show(f_string);

  /*   f_next = gtk_button_new_with_label(_("Find Next")); */
  f_next = create_button(GNOME_STOCK_PIXMAP_FORWARD, _("Find Next"));
  gtk_signal_connect (GTK_OBJECT (f_next),
		      "clicked", GTK_SIGNAL_FUNC(find_next_cb),
		      f_string);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), f_next,
		     TRUE, TRUE, 0);
  gtk_widget_show(f_next);
  f_prev = create_button(GNOME_STOCK_PIXMAP_BACK, _("Find Previous"));
  gtk_signal_connect (GTK_OBJECT (f_prev),
		      "clicked", GTK_SIGNAL_FUNC(find_prev_cb),
		      f_string);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), f_prev,
		     TRUE, TRUE, 0);
  gtk_widget_show(f_prev);
  f_close = gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);
  gtk_signal_connect (GTK_OBJECT (f_close),
		      "clicked", GTK_SIGNAL_FUNC(cancel_cb),
		      dialog);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), f_close,
		     TRUE, TRUE, 0);
  gtk_widget_show(f_close);

  gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(window)->vbox), 2);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(window)->vbox), 2);

  *dialog = window;
}

void create_replace_dialog(GtkWidget **dialog) {
  GtkWidget *window;

  GtkWidget *replace, *replace_all, *next, *close;
  GtkWidget *f_string, *r_string;

  static ReplaceCBData cbdata;

  window = gtk_dialog_new();

  gtk_window_set_title(GTK_WINDOW(window), _("Find & Replace Data"));

  f_string = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), f_string,
		     TRUE, TRUE, 0);
  gtk_widget_show(f_string);

  r_string = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), r_string,
		     TRUE, TRUE, 0);
  gtk_widget_show(r_string);

  cbdata.find = GTK_ENTRY(f_string);
  cbdata.replace = GTK_ENTRY(r_string);

  next = create_button(GNOME_STOCK_PIXMAP_FORWARD, _("Find next"));
  gtk_signal_connect (GTK_OBJECT (next),
		      "clicked", GTK_SIGNAL_FUNC(find_next_cb),
		      f_string);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), next,
		     TRUE, TRUE, 0);
  gtk_widget_show(next);
  replace = gtk_button_new_with_label(_("Replace"));
  gtk_signal_connect (GTK_OBJECT (replace),
		      "clicked", GTK_SIGNAL_FUNC(replace_one_cb),
		      &cbdata);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), replace,
		     TRUE, TRUE, 0);
  gtk_widget_show(replace);
  replace_all= gtk_button_new_with_label(_("Replace All"));
  gtk_signal_connect (GTK_OBJECT (replace_all),
		      "clicked", GTK_SIGNAL_FUNC(replace_all_cb),
		      &cbdata);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), replace_all,
		     TRUE, TRUE, 0);
  gtk_widget_show(replace_all);
  close = gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);
  gtk_signal_connect (GTK_OBJECT (close),
		      "clicked", GTK_SIGNAL_FUNC(cancel_cb),
		      dialog);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), close,
		     TRUE, TRUE, 0);
  gtk_widget_show(close);

  gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(window)->vbox), 2);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(window)->vbox), 2);

  *dialog = window;
}

void create_jump_dialog(GtkWidget **dialog) {
  GtkWidget *window;

  GtkWidget *int_entry;
  GtkWidget *ok, *cancel;

  window = gtk_dialog_new();

  gtk_window_set_title(GTK_WINDOW(window), _("Jump To Byte"));

  int_entry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), int_entry,
		     TRUE, TRUE, 0);
  gtk_widget_show(int_entry);

  ok = gnome_stock_button(GNOME_STOCK_BUTTON_OK);
  gtk_signal_connect (GTK_OBJECT (ok),
		      "clicked", GTK_SIGNAL_FUNC(goto_byte_cb),
		      int_entry);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), ok,
		     TRUE, TRUE, 0);
  gtk_widget_show(ok);
  cancel = gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);
  gtk_signal_connect (GTK_OBJECT (cancel),
		      "clicked", GTK_SIGNAL_FUNC(cancel_cb),
		      dialog);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), cancel,
		     TRUE, TRUE, 0);
  gtk_widget_show(cancel);

  gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(window)->vbox), 2);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(window)->vbox), 2);

  *dialog = window;
}

void setup_ui() {
  GtkWidget *w;

  app = gnome_app_new("ghex", "GNOME hex editor");
  gtk_widget_realize(app);

  gtk_signal_connect(GTK_OBJECT(app), "delete_event", GTK_SIGNAL_FUNC(quit_app_cb), NULL);
  gtk_window_set_policy(GTK_WINDOW(app), TRUE, TRUE, FALSE);

  if (restarted) {
    gtk_widget_set_uposition (app, os_x, os_y);
    gtk_widget_set_usize     (app, os_w, os_h);
  }

  create_menus();
  create_toolbar();
  table = gtk_table_new(2, 3, FALSE);
  gtk_widget_show(table);

  w = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(w), GTK_SHADOW_IN);
  gtk_widget_show(w);
  name_label = gtk_label_new(NO_BUFFER_LABEL);
  gtk_label_set_justify(GTK_LABEL(name_label), GTK_JUSTIFY_RIGHT);
  gtk_container_add(GTK_CONTAINER(w), name_label);
  gtk_widget_show(name_label); 
  gtk_table_attach(GTK_TABLE(table), w, 0, 1, 1, 2,
		   GTK_FILL|GTK_SHRINK|GTK_EXPAND,
		   0, 0, 0); 

  w = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(w), GTK_SHADOW_IN);
  gtk_widget_show(w);
  offset_label = gtk_label_new("");
  gtk_label_set_justify(GTK_LABEL(offset_label), GTK_JUSTIFY_RIGHT);
  gtk_container_add(GTK_CONTAINER(w), offset_label);
  gtk_widget_show(offset_label);
  gtk_table_attach(GTK_TABLE(table), w, 1, 2, 1, 2,
		   GTK_FILL|GTK_SHRINK|GTK_EXPAND,
		   0, 0, 0); 

  w = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(w), GTK_SHADOW_IN);
  gtk_widget_show(w);
  data_label = gtk_label_new("");
  gtk_label_set_justify(GTK_LABEL(data_label), GTK_JUSTIFY_RIGHT);
  gtk_container_add(GTK_CONTAINER(w), data_label);
  gtk_widget_show(data_label);
  gtk_table_attach(GTK_TABLE(table), w, 2, 3, 1, 2,
		   GTK_FILL|GTK_SHRINK|GTK_EXPAND,
		   0, 0, 0); 

  gnome_app_set_menus(GNOME_APP(app), GTK_MENU_BAR(mbar));
  gnome_app_set_toolbar(GNOME_APP(app), GTK_TOOLBAR(tbar));
  gnome_app_set_contents(GNOME_APP(app), table);

  gtk_window_add_accelerator_table(GTK_WINDOW(app), accel);

  gtk_widget_show(app);
}
