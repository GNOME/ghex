/*
 * gnome-support.c - GNOMEificating code for ghex (actually only SM)
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

gchar *open_files = NULL;

static error_t
parse_an_arg (int key, char *arg, struct argp_state *state)
{
  if (key == DISCARD_KEY) {
    discard_session(arg);
    just_exit = 1;
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
  gchar files[2048];       /* I hope this does for most requirements, one could overflow it, however */
  gchar *argv[3];
  gint x, y, w, h;

  GList *child;
  HexDocument *doc;

  printf("saving session...\n");

  session_id = gnome_client_get_id (client);

  /* Save the state using gnome-config stuff. */
  gnome_config_push_prefix (gnome_client_get_config_prefix (client));

  files[0] = '\0';
  child = mdi->children;
  while(child) {
    doc = HEX_DOCUMENT(child->data);
    strcat(files, doc->file_name);
    strcat(files, ":");
    child = g_list_next(child);
  }

  if(mdi->children)
    gnome_config_set_string("Files/files", files);
  else
    gnome_config_clean_key("Files/files");

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
