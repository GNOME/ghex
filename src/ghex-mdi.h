/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * ghex-mdi.h
 * This file is part of ghex
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001, 2002 Chema Celorio, Paolo Maggi, Jaka Mocnik
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA. * *
 */
 
/*
 * Modified by the ghex Team, 1998-2001. See the AUTHORS file for a 
 * list of people on the ghex Team.  
 * See the ChangeLog files for a list of changes. 
 */

#ifndef __GHEX_MDI_H__
#define __GHEX_MDI_H__

#include <bonobo-mdi.h>

#define GHEX_TYPE_MDI			(ghex_mdi_get_type ())
#define GHEX_MDI(obj)			(GTK_CHECK_CAST ((obj), GHEX_TYPE_MDI, GhexMDI))
#define GHEX_MDI_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GHEX_TYPE_MDI, GhexMDIClass))
#define GHEX_IS_MDI(obj)		(GTK_CHECK_TYPE ((obj), GHEX_TYPE_MDI))
#define GHEX_IS_MDI_CLASS(klass)  	(GTK_CHECK_CLASS_TYPE ((klass), GHEX_TYPE_MDI))
#define GHEX_MDI_GET_CLASS(obj)  	(GTK_CHECK_GET_CLASS ((obj), GHEX_TYPE_MDI, GhexMdiClass))


typedef struct _GhexMDI		GhexMDI;
typedef struct _GhexMDIClass		GhexMDIClass;

typedef struct _GhexMDIPrivate		GhexMDIPrivate;

struct _GhexMDI
{
	BonoboMDI mdi;
	
	GhexMDIPrivate *priv;
};

struct _GhexMDIClass
{
	BonoboMDIClass parent_class;
};


GtkType        	ghex_mdi_get_type 	(void) G_GNUC_CONST;

GhexMDI*	ghex_mdi_new		(void);

void		ghex_mdi_set_active_window_title (BonoboMDI *mdi);

#ifdef SNM /* Not reqd for ghex2. Used by gedit2 -- SnM */
void		ghex_mdi_update_ui_according_to_preferences (GhexMDI *mdi);
#endif

/* FIXME: should be static ??? */
void 		ghex_mdi_set_active_window_verbs_sensitivity (BonoboMDI *mdi);
void            ghex_mdi_set_active_window_group_type (GhexMDI *mdi);
void            ghex_mdi_set_active_window_insert_state (GhexMDI *mdi);

void            ghex_menus_set_verb_list_sensitive (BonoboUIComponent *uic, gboolean allmenus);

#endif /* __GHEX_MDI_H__ */

