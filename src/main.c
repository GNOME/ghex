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

gint mdi_mode = GNOME_MDI_NOTEBOOK;

int main(int argc, char **argv) {
  GnomeClient *client;

  argp_program_version = VERSION;

  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
  textdomain(PACKAGE);

  client = gnome_client_new_default();

  gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
		      GTK_SIGNAL_FUNC (save_state), (gpointer) argv[0]);
  gtk_signal_connect (GTK_OBJECT (client), "connect",
		      GTK_SIGNAL_FUNC (connect_client), NULL);

  gnome_init("ghex", &parser, argc, argv, 0, NULL);

  if(!just_exit) {
    mdi = GNOME_MDI(gnome_mdi_new("ghex", "GNOME hex editor"));

    /* set up MDI menus */
    gnome_mdi_set_menu_template(mdi, main_menu);

    /* and document menu and document list paths */
    gnome_mdi_set_child_menu_path(mdi, "File");
    gnome_mdi_set_child_list_path(mdi, "View");

    /* connect signals */
    gtk_signal_connect(GTK_OBJECT(mdi), "remove_child", GTK_SIGNAL_FUNC(remove_doc_cb), NULL);
    gtk_signal_connect(GTK_OBJECT(mdi), "destroy", GTK_SIGNAL_FUNC(cleanup_cb), NULL);

    /* load preferences */
    load_configuration();

    /* set MDI mode */
    gnome_mdi_set_mode(mdi, mdi_mode);

    /* and here we go... */
    gtk_main();
  }

  return 0;
}





