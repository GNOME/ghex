/*
 * ui.c - GHex user interface
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */

#include <gnome.h>
#include "gtkhex.h"
#include "gnome-support.h"
#include "callbacks.h"
#include "ghex.h"

static void set_prefs(PropertyUI *);

GtkWidget *file_sel = NULL;
GtkWidget *find_dialog = NULL;
GtkWidget *replace_dialog = NULL;
GtkWidget *jump_dialog = NULL;
PropertyUI *prefs_ui = NULL;

GdkFont *def_font = NULL;
gchar *def_font_name = NULL;

guint mdi_type[3] = {
  GNOME_MDI_NOTEBOOK,
  GNOME_MDI_MODAL,
  GNOME_MDI_TOPLEVEL,
};

gchar *mdi_type_label[3] = {
  "Notebook",
  "Modal",
  "Toplevel",
};

void show_message(gchar *msg) {
  GtkWidget *message_box;

  message_box = gnome_message_box_new(msg, GNOME_MESSAGE_BOX_INFO, GNOME_STOCK_BUTTON_OK, NULL);
  gnome_message_box_set_modal(GNOME_MESSAGE_BOX(message_box));
  gtk_window_position (GTK_WINDOW (message_box), GTK_WIN_POS_MOUSE);
  gtk_widget_show(message_box);
}

void report_error(gchar *msg) {
  GtkWidget *message_box;

  message_box = gnome_message_box_new(msg, GNOME_MESSAGE_BOX_ERROR, GNOME_STOCK_BUTTON_OK, NULL);
  gnome_message_box_set_modal(GNOME_MESSAGE_BOX(message_box));
  gtk_window_position (GTK_WINDOW (message_box), GTK_WIN_POS_MOUSE);
  gtk_widget_show(message_box);
}

static void question_click_cb(GtkWidget *w, gint button, gint *reply) {
  *reply = button;
}  

static void question_destroy_cb(GtkObject *obj) {
  gtk_main_quit();
}

gint ask_user(GnomeMessageBox *message_box) {
  gint reply;

  gnome_dialog_set_destroy(GNOME_DIALOG(message_box), TRUE);
  gnome_dialog_set_modal(GNOME_DIALOG(message_box));
  gtk_signal_connect(GTK_OBJECT(message_box), "clicked",
		     GTK_SIGNAL_FUNC(question_click_cb), &reply);
  gtk_signal_connect(GTK_OBJECT(message_box), "destroy",
		     GTK_SIGNAL_FUNC(question_destroy_cb), NULL);
  gtk_window_position (GTK_WINDOW (message_box), GTK_WIN_POS_MOUSE);
  gtk_widget_show(GTK_WIDGET(message_box));
  /* I hope increasing main_level is the proper way to stop ghex until
     user had replied to this question... */
  gtk_main();

  return reply;
}

GtkWidget *create_button(GtkWidget *window, gchar *type, gchar *text) {
  GtkWidget *button, *pixmap, *label, *hbox;

  hbox = gtk_hbox_new(FALSE, 2);

  label = gtk_label_new(text);
  pixmap = gnome_stock_pixmap_widget(window, type);

  gtk_box_pack_start(GTK_BOX(hbox), pixmap, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 1);

  button = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(button), hbox);

  gtk_widget_show(label);
  gtk_widget_show(pixmap);
  gtk_widget_show(hbox);

  return button;
}

#if 0
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
#endif

GList *create_mdi_menus(GnomeMDI *mdi) {
  GList *menu_list;
  GtkWidget *w, *menu;
  GtkAcceleratorTable *accel = NULL;

  menu_list = NULL;

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

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_PREF, _("Preferences"));
  gtk_widget_show(w);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(prefs_cb), NULL);
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
  menu_list = g_list_append(menu_list, w);

  /* the View menu */
  menu = gtk_menu_new();

  w = gtk_menu_item_new_with_label(_("Add"));
  gtk_widget_show(w);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(add_view_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);
  
  w = gtk_menu_item_new_with_label(_("Remove"));
  gtk_widget_show(w);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(remove_view_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gtk_menu_item_new_with_label(_("View"));
  gtk_widget_show(w);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), menu);
  
  /* I'm looking for a nicer way to mark where document-specific menus should be inserted.
     this prevents one from using gnome-app-helper functions and casting some nonsense to
     gpointer looks really bad. any ideas? */
  gtk_object_set_data(GTK_OBJECT(w), "document_menu", (gpointer)TRUE);

  menu_list = g_list_append(menu_list, w);

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
  gtk_object_set_data(GTK_OBJECT(w), "document_list", (gpointer)TRUE);

  menu_list = g_list_append(menu_list, w);

  return menu_list;
}

void create_find_dialog(GtkWidget **dialog) {
  GtkWidget *window;

  GtkWidget *f_next, *f_prev, *f_close, *f_string;

  window = gtk_dialog_new();
  gtk_signal_connect(GTK_OBJECT(window), "delete_event",
		     GTK_SIGNAL_FUNC(delete_event_cb), dialog);

  gtk_window_set_title(GTK_WINDOW(window), _("Find Data"));

  f_string = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), f_string,
		     TRUE, TRUE, 0);
  gtk_widget_show(f_string);

  /*   f_next = gtk_button_new_with_label(_("Find Next")); */
  f_next = create_button(window, GNOME_STOCK_PIXMAP_FORWARD, _("Find Next"));
  gtk_signal_connect (GTK_OBJECT (f_next),
		      "clicked", GTK_SIGNAL_FUNC(find_next_cb),
		      f_string);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), f_next,
		     TRUE, TRUE, 0);
  gtk_widget_show(f_next);
  f_prev = create_button(window, GNOME_STOCK_PIXMAP_BACK, _("Find Previous"));
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
  gtk_signal_connect(GTK_OBJECT(window), "delete_event",
		     GTK_SIGNAL_FUNC(delete_event_cb), dialog);

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

  next = create_button(window, GNOME_STOCK_PIXMAP_FORWARD, _("Find next"));
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
  gtk_signal_connect(GTK_OBJECT(window), "delete_event",
		     GTK_SIGNAL_FUNC(delete_event_cb), dialog);

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

static void set_prefs(PropertyUI *pui) {
  int i;

  for(i = 0; i < 3; i++)
    if(def_group_type == group_type[i]) {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(pui->group_type[i]), TRUE);
      break;
    }
  
  for(i = 0; i < 3; i++)
    if(mdi_mode == mdi_type[i]) {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(pui->mdi_type[i]), TRUE);
      break;
    }
}

void create_prefs_dialog(PropertyUI **prop_data) {
  GtkWidget *vbox, *label, *frame, *box;
  GSList *group;
  PropertyUI *pui;
  
  int i;

  pui = g_malloc(sizeof(PropertyUI));

  pui->pbox = GNOME_PROPERTY_BOX(gnome_property_box_new());

  gtk_signal_connect(GTK_OBJECT(pui->pbox), "destroy",
		     GTK_SIGNAL_FUNC(prop_destroy_cb), prop_data);
  gtk_signal_connect(GTK_OBJECT(pui->pbox), "apply",
		     GTK_SIGNAL_FUNC(apply_changes_cb), pui);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vbox);

  frame = gtk_frame_new(_("Font"));
  gtk_container_border_width(GTK_CONTAINER(frame), 4);
  gtk_widget_show(frame);
  pui->font_button = GTK_BUTTON(gtk_button_new_with_label(def_font_name));
  gtk_signal_connect(GTK_OBJECT(pui->font_button), "clicked",
		     select_font_cb, pui->pbox);
  gtk_widget_show(GTK_WIDGET(pui->font_button));
  gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(pui->font_button));
  gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 2);

  frame = gtk_frame_new(_("Default Group Type"));
  gtk_container_border_width(GTK_CONTAINER(frame), 4);
  gtk_widget_show(frame);

  box = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(box);
  group = NULL;
  for(i = 0; i < 3; i++) {
    pui->group_type[i] = GTK_RADIO_BUTTON(gtk_radio_button_new_with_label(group, group_type_label[i]));
    gtk_widget_show(GTK_WIDGET(pui->group_type[i]));
    gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(pui->group_type[i]), TRUE, TRUE, 2);
    group = gtk_radio_button_group (pui->group_type[i]);
  }
  gtk_container_add(GTK_CONTAINER(frame), box);
  gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 2);
  
  label = gtk_label_new(_("Display"));
  gtk_widget_show(label);
  gnome_property_box_append_page(pui->pbox, vbox, label);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vbox);

  frame = gtk_frame_new(_("MDI Mode"));
  gtk_container_border_width(GTK_CONTAINER(frame), 4);
  gtk_widget_show(frame);

  box = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(box);
  group = NULL;
  for(i = 0; i < 3; i++) {
    pui->mdi_type[i] = GTK_RADIO_BUTTON(gtk_radio_button_new_with_label(group, mdi_type_label[i]));
    gtk_widget_show(GTK_WIDGET(pui->mdi_type[i]));
    gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(pui->mdi_type[i]), TRUE, TRUE, 2);
    group = gtk_radio_button_group (pui->mdi_type[i]);
  }

  gtk_container_add(GTK_CONTAINER(frame), box);
  gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 2);  

  label = gtk_label_new(_("MDI"));
  gtk_widget_show(label);
  gnome_property_box_append_page(pui->pbox, vbox, label);

  set_prefs(pui);

  /* signals have to be connected after set_prefs(), otherwise
     a gnome_property_box_changed() is called */

  for(i = 0; i < 3; i++) {
    gtk_signal_connect(GTK_OBJECT(pui->mdi_type[i]), "clicked",
		       properties_modified_cb, pui->pbox);
    gtk_signal_connect(GTK_OBJECT(pui->group_type[i]), "clicked",
		       properties_modified_cb, pui->pbox);
  } 

  *prop_data = pui;
}

