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

int save_state (GnomeClient        *client,
                gint                phase,
                GnomeRestartStyle   save_style,
                gint                shutdown,
                GnomeInteractStyle  interact_style,
                gint                fast,
                gpointer            client_data) {
	gchar *prefix= gnome_client_get_config_prefix (client);
	gchar *argv[]= { "rm", "-r", NULL };
	
	/* Save the state using gnome-config stuff. */
	gnome_config_push_prefix (prefix);
	
	gnome_mdi_save_state(mdi, "Session");
	
	gnome_config_pop_prefix();
	gnome_config_sync();
	
	/* Here is the real SM code. We set the argv to the parameters needed
	   to restart/discard the session that we've just saved and call
	   the gnome_session_set_*_command to tell the session manager it. */
	argv[2] = gnome_config_get_real_path (prefix);
	gnome_client_set_discard_command (client, 3, argv);
	
	/* Set commands to clone and restart this application.  Note that we
	   use the same values for both -- the session management code will
	   automatically add whatever magic option is required to set the
	   session id on startup.  */
	argv[0] = (char*) client_data;
	gnome_client_set_clone_command (client, 1, argv);
	gnome_client_set_restart_command (client, 1, argv);
	
	return TRUE;
}

gint
client_die (GnomeClient *client, gpointer client_data)
{
	gtk_exit (0);
	
	return FALSE;
}
