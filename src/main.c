/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* main.c - genesis of a GHex application

   Copyright (C) 1998 - 2002 Free Software Foundation

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

#include <config.h>
#include <gnome.h>
#include "ghex.h"

int
main(int argc, char **argv)
{
	GnomeClient *client;
	GValue value = { 0, };
	GnomeProgram *program;
	poptContext ctx;
	char **args;
	GtkWidget *win;

	char **cl_files;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Initialize gnome program */
	program = gnome_program_init ("ghex2", VERSION,
				LIBGNOMEUI_MODULE, argc, argv,
				GNOME_PARAM_POPT_TABLE, options,
				GNOME_PARAM_HUMAN_READABLE_NAME,
				_("The gnome binary editor"),
				GNOME_PARAM_APP_DATADIR, DATADIR,
				NULL);

	/* FIXME - Couldnt find gnome-ghex.png in the sources directory */
	/* Set default window icon */
	gnome_window_icon_set_default_from_file (GNOMEICONDIR "/gnome-ghex.png");

	/* load preferences */
	ghex_prefs_init();
	load_configuration();
	/* accessibility setup */
	setup_factory();

	if (bonobo_ui_init ("Gnome Binary Editor", VERSION, &argc, argv) == FALSE)
		g_error (_("Could not initialize Bonobo!\n"));

	client = gnome_master_client();

	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
						GTK_SIGNAL_FUNC (save_session), (gpointer) argv[0]);
	gtk_signal_connect (GTK_OBJECT (client), "die",
						GTK_SIGNAL_FUNC (client_die), NULL);

	/* Parse args and build the list of files to be loaded at startup */
	g_value_init (&value, G_TYPE_POINTER);
	g_object_get_property (G_OBJECT (program), GNOME_PARAM_POPT_CONTEXT, &value);
	ctx = g_value_get_pointer (&value);
	g_value_unset (&value);

	args = (char**) poptGetArgs(ctx);

	cl_files = (char **)poptGetArgs(ctx);

	while(cl_files && *cl_files) {
		win = ghex_window_new_from_file(*cl_files);
		if(geometry) {
			if(!gtk_window_parse_geometry(GTK_WINDOW(win), geometry))
				g_warning(_("Invalid geometry string \"%s\"\n"), geometry);
			geometry = NULL;
		}
		gtk_widget_show(win);
		cl_files++;
	}
	poptFreeContext(ctx);

	if(ghex_window_get_list() == NULL) {
		win = ghex_window_new();
		if(geometry) {
			if(!gtk_window_parse_geometry(GTK_WINDOW(win), geometry))
				g_warning(_("Invalid geometry string \"%s\"\n"), geometry);
			geometry = NULL;
		}
		gtk_widget_show(win);
	}
	else win = GTK_WIDGET(ghex_window_get_list()->data);


	bonobo_main();

	return 0;
}
