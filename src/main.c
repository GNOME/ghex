/*
 * main.c - genesis of a GHex application
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * For more details see the file COPYING.
 */
#include <config.h>
#include <gnome.h>
#include <getopt.h>

#include "gnome-support.h"
#include "ghex.h"
#include "callbacks.h"

GnomeMDI *mdi;

gint mdi_mode = GNOME_MDI_DEFAULT_MODE;

int main(int argc, char **argv) {
  GnomeClient *client;

  argp_program_version = VERSION;

  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
  textdomain(PACKAGE);

  printf("starting ghex\n");

  gnome_init("ghex", &parser, argc, argv, 0, NULL);

  client = gnome_master_client();

  gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
		      GTK_SIGNAL_FUNC (save_state), (gpointer) argv[0]);

  if(!just_exit) {
    mdi = GNOME_MDI(gnome_mdi_new("ghex", "GHex"));

    /* set up MDI menus */
    gnome_mdi_set_menu_template(mdi, main_menu);

    /* and document menu and document list paths */
    gnome_mdi_set_child_menu_path(mdi, _("File"));
    gnome_mdi_set_child_list_path(mdi, _("Files/"));

    /* connect signals */
    gtk_signal_connect(GTK_OBJECT(mdi), "remove_child", GTK_SIGNAL_FUNC(remove_doc_cb), NULL);
    gtk_signal_connect(GTK_OBJECT(mdi), "destroy", GTK_SIGNAL_FUNC(cleanup_cb), NULL);

    /* load preferences */
    load_configuration();

    /* set MDI mode */
    gnome_mdi_set_mode(mdi, mdi_mode);

    /* open documents from previous session if there are any */
    if (GNOME_CLIENT_CONNECTED (client)) {
      /* Get the client, that may hold the configuration for this
         program.  */
      GnomeClient *cloned= gnome_cloned_client ();

      if (cloned) {
        restarted = 1;

        gnome_config_push_prefix (gnome_client_get_config_prefix (cloned));
        open_files = gnome_config_get_string("Files/files");
        gnome_config_pop_prefix ();

        printf("open: %s\n", open_files);
        if(open_files) {
          HexDocument *doc;
          gchar file_name[256], *c;
          int i;
          
          c = open_files;
          while(*c != '\0') {
            for(i = 0; *c != ':'; i++, c++)
              file_name[i] = *c;
            file_name[i] = '\0';
            c++;
            printf("file: %s\n", file_name);
            if((doc = hex_document_new(file_name)) != NULL) {
              gnome_mdi_add_child(mdi, GNOME_MDI_CHILD(doc));
              gnome_mdi_add_view(mdi, GNOME_MDI_CHILD(doc));
            }
            else
              report_error(_("Can not open file!"));
          }
          
          g_free(open_files);
          open_files = NULL;
        }
      }
    }

    /* and here we go... */
    gtk_main();
  }

  return 0;
}





