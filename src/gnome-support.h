/*
 * gnome-support.h - forward decls of GNOMEificating functions
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */

#ifndef GNOME_SUPPORT_H
#define GNOME_SUPPORT_H

#include <gnome.h>
#include <getopt.h>

extern int restarted, just_exit;
extern int os_x, os_y, os_w, os_h;
extern struct argp parser;

int save_state      (GnomeClient        *client,
		     gint                phase,
		     GnomeRestartStyle   save_style,
		     gint                shutdown,
		     GnomeInteractStyle  interact_style,
		     gint                fast,
		     gpointer            client_data);
void connect_client (GnomeClient *client, 
		     gint         was_restarted, 
		     gpointer     client_data);

void discard_session (gchar *id);

#endif
