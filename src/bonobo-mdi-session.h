/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * bonobo-mdi-session.h - session managament functions
 * written by Martin Baulig <martin@home-of-linux.org>
 */

#ifndef __BONOBO_MDI_SESSION_H__
#define __BONOBO_MDI_SESSION_H__

#include <string.h>

#include "bonobo-mdi.h"

/* This function should parse the config string and return a newly
 * created BonoboMDIChild. */
typedef BonoboMDIChild *(*BonoboMDIChildCreator) (const gchar *);

/* bonobo_mdi_restore_state(): call this with the BonoboMDI object, the
 * config section name and the function used to recreate the BonoboMDIChildren
 * from their config strings. */
gboolean	bonobo_mdi_restore_state	(BonoboMDI *mdi, const gchar *section,
					 BonoboMDIChildCreator child_create_func);

/* bonobo_mdi_save_state (): call this with the BonoboMDI object as the
 * first and the config section name as the second argument. */
void		bonobo_mdi_save_state	(BonoboMDI *mdi, const gchar *section);

#endif
