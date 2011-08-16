/*
 * Copyright 2002 Sun Microsystems Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef __ACCESSIBLE_GTK_HEX_H__
#define __ACCESSIBLE_GTK_HEX_H__

#include <gtk/gtk.h>
#include <libgail-util/gailtextutil.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ACCESSIBLE_TYPE_GTK_HEX                     (accessible_gtk_hex_get_type ())
#define ACCESSIBLE_GTK_HEX(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), ACCESSIBLE_TYPE_GTK_HEX, AccessibleGtkHex))

#define ACCESSIBLE_GTK_HEX_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), ACCESSIBLE_TYPE_GTK_HEX, AccessibleGtkHexClass))

#define ACCESSIBLE_IS_GTK_HEX(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ACCESSIBLE_TYPE_GTK_HEX))

#define ACCESSIBLE_IS_GTK_HEX_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), ACCESSIBLE_TYPE_GTK_HEX))

#define ACCESSIBLE_GTK_HEX_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), ACCESSIBLE_TYPE_GTK_HEX, AccessibleGtkHexClass))

typedef struct _AccessibleGtkHex                   AccessibleGtkHex;
typedef struct _AccessibleGtkHexClass              AccessibleGtkHexClass;

struct _AccessibleGtkHex
{
	GtkAccessible   parent;
	GailTextUtil *textutil;
};

struct _AccessibleGtkHexClass
{
	GtkAccessibleClass parent_class;
};

GType accessible_gtk_hex_get_type (void);

AtkObject* accessible_gtk_hex_new (GtkWidget *widget);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ACCESSIBLE_GTK_HEX_H__ */
