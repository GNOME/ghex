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

GSList *buffer_list;
FileEntry *active_fe;

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
    setup_ui();

    buffer_list = g_slist_alloc();

    active_fe = NULL;

    load_configuration();

    gtk_main();
    
    g_slist_free(buffer_list);
  }

  return 0;
}





