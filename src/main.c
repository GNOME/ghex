/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* main.c - genesis of a GHex application

   Copyright (C) 1997, 1998 Free Software Foundation

   GHex is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   GHex is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GHex; see the file COPYING.
   If not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
*/

#include <config.h>
#include <gnome.h>
#include "gnome-support.h"
#include "ghex.h"
#include "callbacks.h"

GnomeMDI *mdi;

gint mdi_mode = GNOME_MDI_DEFAULT_MODE;

int main(int argc, char **argv) {
	GnomeClient *client;
	HexDocument *doc;
	char **cl_files;
	poptContext ctx;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	gnome_init_with_popt_table("ghex", VERSION, argc, argv, options, 0, &ctx);

	client = gnome_master_client();

	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
						GTK_SIGNAL_FUNC (save_state), (gpointer) argv[0]);
	gtk_signal_connect (GTK_OBJECT (client), "die",
						GTK_SIGNAL_FUNC (client_die), NULL);

    mdi = GNOME_MDI(gnome_mdi_new("ghex", "GHex"));

    /* set up MDI menus */
    gnome_mdi_set_menubar_template(mdi, main_menu);

    /* and document menu and document list paths */
    gnome_mdi_set_child_menu_path(mdi, _(CHILD_MENU_PATH));
    gnome_mdi_set_child_list_path(mdi, _(CHILD_LIST_PATH));

    /* connect signals */
    gtk_signal_connect(GTK_OBJECT(mdi), "remove_child", GTK_SIGNAL_FUNC(remove_doc_cb), NULL);
    gtk_signal_connect(GTK_OBJECT(mdi), "destroy", GTK_SIGNAL_FUNC(cleanup_cb), NULL);
    gtk_signal_connect(GTK_OBJECT(mdi), "view_changed", GTK_SIGNAL_FUNC(view_changed_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(mdi), "child_changed", GTK_SIGNAL_FUNC(child_changed_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(mdi), "app_created", GTK_SIGNAL_FUNC(customize_app_cb), NULL);

    /* load preferences */
    load_configuration();
    /* set MDI mode */
    gnome_mdi_set_mode(mdi, mdi_mode);

    /* restore state from previous session */
    if (gnome_client_get_flags (client) & GNOME_CLIENT_RESTORED) {

		gnome_config_push_prefix (gnome_client_get_config_prefix (client));
		
		restarted= gnome_mdi_restore_state (mdi, "Session", (GnomeMDIChildCreator)hex_document_new_from_config);		
		
		gnome_config_pop_prefix ();
	}

	if (!restarted)
		gnome_mdi_open_toplevel(mdi);
	
    cl_files = poptGetArgs(ctx);
	
    while(cl_files && *cl_files) {
		doc = hex_document_new(*cl_files);
		if(doc) {
			gnome_mdi_add_child(mdi, GNOME_MDI_CHILD(doc));
			gnome_mdi_add_view(mdi, GNOME_MDI_CHILD(doc));
		}
		cl_files++;
    }
    poptFreeContext(ctx);
	
    /* and here we go... */
    gtk_main();

	return 0;
}
