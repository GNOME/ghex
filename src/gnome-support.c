/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gnome-support.c - GNOMEificating code for ghex (actually only SM)

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

#include "gnome-support.h"
#include "ghex.h"

/* These are the arguments that our application supports.  */
static struct argp_option arguments[] =
{
#define DISCARD_KEY -1
	{ "discard-session", DISCARD_KEY, N_("ID"), 0, N_("Discard session"), 1 },
	{ NULL, 0, NULL, 0, NULL, 0 }
};

/* Forward declaration of the function that gets called when one of
   our arguments is recognized.  */
static error_t parse_an_arg (int key, char *arg, struct argp_state *state);

/* This structure defines our parser.  It can be used to specify some
   options for how our parsing function should be called.  */
struct argp parser =
{
	arguments,			/* Options.  */
	parse_an_arg,			/* The parser function.  */
	"[FILE]...",				/* Some docs.  */
	NULL,				/* Some more docs.  */
	NULL,				/* Child arguments -- gnome_init fills
						   this in for us.  */
	NULL,				/* Help filter.  */
	NULL				/* Translation domain; for the app it
						   can always be NULL.  */
};

int restarted = 0, just_exit = FALSE;

/* a list of files specifed on the command line */
GSList *cl_files = NULL;

static error_t
parse_an_arg (int key, char *arg, struct argp_state *state)
{
	switch(key) {
	case ARGP_KEY_ARG:
		cl_files = g_slist_append(cl_files, arg);
		break;
	case DISCARD_KEY:
		discard_session(arg);
		just_exit = 1;
		break;
	default:
		/* We didn't recognize it.  */
		return ARGP_ERR_UNKNOWN;
	}
	
	return 0;
}

/* Session management */

int save_state (GnomeClient        *client,
                gint                phase,
                GnomeRestartStyle   save_style,
                gint                shutdown,
                GnomeInteractStyle  interact_style,
                gint                fast,
                gpointer            client_data) {
	gchar *session_id;
	gchar files[2048];       /* I hope this does for most requirements, one could overflow it, however */
	gchar *argv[3];
	gint x, y, w, h;
	
	printf("saving session...\n");
	
	session_id = gnome_client_get_id (client);
	
	/* Save the state using gnome-config stuff. */
	gnome_config_push_prefix (gnome_client_get_config_prefix (client));
	
	gnome_mdi_save_state(mdi, "Session");
	
	gnome_config_pop_prefix();
	gnome_config_sync();
	
	/* Here is the real SM code. We set the argv to the parameters needed
	   to restart/discard the session that we've just saved and call
	   the gnome_session_set_*_command to tell the session manager it. */
	argv[0] = (char*) client_data;
	argv[1] = "--discard-session";
	argv[2] = gnome_client_get_config_prefix (client);
	gnome_client_set_discard_command (client, 3, argv);
	
	/* Set commands to clone and restart this application.  Note that we
	   use the same values for both -- the session management code will
	   automatically add whatever magic option is required to set the
	   session id on startup.  */
	gnome_client_set_clone_command (client, 1, argv);
	gnome_client_set_restart_command (client, 1, argv);
	
	return TRUE;
}

void
discard_session (gchar *arg)
{
    /* This discards the saved information about this client.  */
    gnome_config_clean_file (arg);
    gnome_config_sync ();
    
    /* We really need not connect, because we just exit after the
       gnome_init call.  */
    gnome_client_disable_master_connection ();
}
