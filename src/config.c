/*
 * config.c - configuration loading/saving via gnome-config routines
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */

#include "ghex.h"

gint save_config_on_exit = FALSE;

void save_configuration() {
  if(def_font)
    gnome_config_set_string("/ghex/Display/Font", def_font_name);
  else
    gnome_config_clean_key("/ghex/Display/Font");

  gnome_config_set_int("/ghex/Display/Group", get_desired_group_type());

  gnome_config_set_bool("/ghex/Misc/SaveConfigOnExit", save_config_on_exit);

  gnome_config_sync();
}

void load_configuration() {
  gchar *font_desc;
  gint group;
  GdkFont *new_font;

  if(font_desc = gnome_config_get_string("/ghex/Display/Font")) {
    if(new_font = gdk_font_load(font_desc)) {
      if(active_fe)
	gtk_hex_set_font(GTK_HEX(active_fe->hexedit), new_font);
      if(def_font)
        gdk_font_unref(def_font);
      def_font = new_font;
      strcpy(def_font_name, font_desc);
    }
    else
      report_error(_("Can not open configured font!"));
  }

  if(group = gnome_config_get_int("/ghex/Display/Group")) {
    set_desired_group_type(group);
    if(active_fe)
      gtk_hex_set_group_type(GTK_HEX(active_fe->hexedit), group);
  }

  save_config_on_exit = gnome_config_get_bool("/ghex/Misc/SaveConfigOnExit=0");
  if(save_config_item)
    gtk_check_menu_item_set_state(save_config_item, save_config_on_exit);
}

  



