/*
 * config.c - configuration loading/saving via gnome-config routines
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */
#include <string.h>

#include "ghex.h"

gint def_group_type = GROUP_BYTE;

void save_configuration() {
  if(def_font)
    gnome_config_set_string("/ghex/Display/Font", def_font_name);
  else
    gnome_config_clean_key("/ghex/Display/Font");

  gnome_config_set_int("/ghex/Display/Group", def_group_type);

  gnome_config_set_int("/ghex/MDI/Mode", mdi_mode);

  gnome_config_sync();
}

void load_configuration() {
  gchar *font_desc;
  gint group;
  GdkFont *new_font;

  if((font_desc = gnome_config_get_string("/ghex/Display/Font=" DEFAULT_FONT)) != NULL) {
    if((new_font = gdk_font_load(font_desc)) != NULL) {
      if(def_font)
        gdk_font_unref(def_font);
      def_font = new_font;
      if(def_font_name)
	free(def_font_name);

      def_font_name = strdup(font_desc);
    }
    else
      report_error(_("Can not open configured font!"));
  }

  def_group_type = gnome_config_get_int("/ghex/Display/Group=0");

  mdi_mode = gnome_config_get_int("/ghex/MDI/Mode=2");
}

  



