/*
 * gnome-support.c - GNOMEificating code for ghex
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
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
  NULL,				/* Some docs.  */
  NULL,				/* Some more docs.  */
  NULL,				/* Child arguments -- gnome_init fills
				   this in for us.  */
  NULL,				/* Help filter.  */
  NULL				/* Translation domain; for the app it
				   can always be NULL.  */
};

int restarted = 0, just_exit = FALSE;

int os_x = 0,
    os_y = 0,
    os_w = 0,
    os_h = 0;

static error_t
parse_an_arg (int key, char *arg, struct argp_state *state)
{
  if (key == DISCARD_KEY)
    {
      discard_session (arg);
      just_exit = TRUE;
      return 0;
    }

  /* We didn't recognize it.  */
  return ARGP_ERR_UNKNOWN;
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
  gchar *sess;
  gchar *buf;
  gchar *argv[3];
  gint x, y, w, h;

  session_id = gnome_client_get_id (client);

  /* The only state that gnome-hello has is the window geometry. 
     Get it. */
  gdk_window_get_geometry (app->window, &x, &y, &w, &h, NULL);

  /* Save the state using gnome-config stuff. */
  sess = g_copy_strings ("/ghex/Saved-Session-",
                         session_id,
                         NULL);

  buf = g_copy_strings ( sess, "/x", NULL);
  gnome_config_set_int (buf, x);
  g_free(buf);
  buf = g_copy_strings ( sess, "/y", NULL);
  gnome_config_set_int (buf, y);
  g_free(buf);
  buf = g_copy_strings ( sess, "/w", NULL);
  gnome_config_set_int (buf, w);
  g_free(buf);
  buf = g_copy_strings ( sess, "/h", NULL);
  gnome_config_set_int (buf, h);
  g_free(buf);

  gnome_config_sync();
  g_free(sess);

  /* Here is the real SM code. We set the argv to the parameters needed
     to restart/discard the session that we've just saved and call
     the gnome_session_set_*_command to tell the session manager it. */
  argv[0] = (char*) client_data;
  argv[1] = "--discard-session";
  argv[2] = session_id;
  gnome_client_set_discard_command (client, 3, argv);

  /* Set commands to clone and restart this application.  Note that we
     use the same values for both -- the session management code will
     automatically add whatever magic option is required to set the
     session id on startup.  */
  gnome_client_set_clone_command (client, 1, argv);
  gnome_client_set_restart_command (client, 1, argv);

  return TRUE;
}

/* Connected to session manager. If restarted from a former session:
   reads the state of the previous session. Sets os_* (prepare_app
   uses them) */
void connect_client (GnomeClient *client, gint was_restarted, gpointer client_data) {
  gchar *session_id;

  /* Note that information is stored according to our *old*
     session id.  The id can change across sessions.  */
  session_id = gnome_client_get_previous_id (client);

  if (was_restarted && session_id != NULL)
    {
      gchar *sess;
      gchar *buf;

      restarted = 1;

      sess = g_copy_strings ("/ghex/Saved-Session-", session_id, NULL);

      buf = g_copy_strings ( sess, "/x", NULL);
      os_x = gnome_config_get_int (buf);
      g_free(buf);
      buf = g_copy_strings ( sess, "/y", NULL);
      os_y = gnome_config_get_int (buf);
      g_free(buf);
      buf = g_copy_strings ( sess, "/w", NULL);
      os_w = gnome_config_get_int (buf);
      g_free(buf);
      buf = g_copy_strings ( sess, "/h", NULL);
      os_h = gnome_config_get_int (buf);
      g_free(buf);
    }

  /* If we had an old session, we clean up after ourselves.  */
  if (session_id != NULL)
    discard_session (session_id);

  return;
}

void
discard_session (gchar *id)
{
  gchar *sess;

  sess = g_copy_strings ("/ghex/Saved-Session-", id, NULL);

  /* we use the gnome_config_get_* to work around a bug in gnome-config 
     (it's going under a redesign/rewrite, so i didn't correct it) */
  gnome_config_get_int ("/ghex/Bug/work-around=0");

  gnome_config_clean_section (sess);
  gnome_config_sync ();

  g_free (sess);
  return;
}
