/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* session.c - SM code for ghex

   Copyright (C) 1998 - 2004 Free Software Foundation

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

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#include "ghex.h"

gchar *geometry = NULL;
const struct poptOption options[] = {
  { "geometry", '\0', POPT_ARG_STRING, &geometry, 0,
    N_("X geometry specification (see \"X\" man page)."),
    N_("GEOMETRY")
  },
  {NULL, '\0', 0, NULL, 0}
};

/* Session management */
void
client_die(GnomeClient *client, gpointer data)
{
	bonobo_main_quit ();
}

static void
interaction_function (GnomeClient *client, gint key, GnomeDialogType dialog_type, gpointer shutdown)
{

	if (GPOINTER_TO_INT (shutdown)) {
		const GList *doc_node;
		GHexWindow *win;
		HexDocument *doc;

		doc_node = hex_document_get_list();
		while(doc_node) {
			doc = HEX_DOCUMENT(doc_node->data);
			win = ghex_window_find_for_doc(doc);
			if(!ghex_window_ok_to_close(win)) {
				gnome_interaction_key_return (key, TRUE);
				return;
			}
			doc_node = doc_node->next;
		}
	}
	gnome_interaction_key_return (key, FALSE);
}



gint
save_session(GnomeClient        *client,
             gint                phase,
             GnomeRestartStyle   save_style,
             gint                shutdown,
             GnomeInteractStyle  interact_style,
             gint                fast,
             gpointer            client_data)
{
	gchar *argv[128];
	gint argc;
	const GList *node;
	GHexWindow *win;

	argv[0] = (gchar *)client_data;
	argc = 1;
	gnome_client_request_interaction (client,
                                          GNOME_DIALOG_NORMAL,
                                          interaction_function,
                                          GINT_TO_POINTER (shutdown));

	node = ghex_window_get_list();
	while(node) {
		win = GHEX_WINDOW(node->data);
		if(win->gh)
			argv[argc++] = win->gh->document->file_name;
		node = node->next;
	}
	gnome_client_set_clone_command(client, argc, argv);
	gnome_client_set_restart_command(client, argc, argv);
	
	return TRUE;
}
