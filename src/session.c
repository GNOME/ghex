/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* session.c - SM code for ghex

   Copyright (C) 1998, 1999, 2000 Free Software Foundation

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

int restarted = 0;
const struct poptOption options[] = {
  {NULL, '\0', 0, NULL, 0}
};

/* Session management */
void
client_die(GnomeClient *client, gpointer data)
{
	bonobo_main_quit ();
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
