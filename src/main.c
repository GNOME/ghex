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

GnomeMDI *mdi;

gint mdi_mode = GNOME_MDI_NOTEBOOK;

static void cleanup(GtkObject *obj) {
  save_configuration();
}

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
    mdi = gnome_mdi_new("ghex", "GNOME hex editor");

    gtk_signal_connect(GTK_OBJECT(mdi), "create_menus", GTK_SIGNAL_FUNC(create_mdi_menus), NULL);  
    gtk_signal_connect(GTK_OBJECT(mdi), "destroy", GTK_SIGNAL_FUNC(cleanup), NULL);

    load_configuration();

    gnome_mdi_set_mode(mdi, mdi_mode);

    gtk_main();
  }

  return 0;
}





