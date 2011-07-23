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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __ACCESSIBLE_GTK_HEX_FACTORY_H__
#define __ACCESSIBLE_GTK_HEX_FACTORY_H__

#include <atk/atk.h>
#include <gtkhex.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ACCESSIBLE_TYPE_GTK_HEX_FACTORY             (accessible_gtk_hex_factory_get_type())

#define ACCESSIBLE_GTK_HEX_FACTORY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), ACCESSIBLE_TYPE_GTK_HEX_FACTORY, AccessibleGtkHexFactory))

#define ACCESSIBLE_GTK_HEX_FACTORY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), ACCESSIBLE_TYPE_GTK_HEX_FACTORY, AccessibleGtkHexFactoryClass))

#define ACCESSIBLE_IS_GTK_HEX_FACTORY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ACCESSIBLE_TYPE_GTK_HEX_FACTORY))

#define ACCESSIBLE_IS_GTK_HEX_FACTORY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), ACCESSIBLE_TYPE_GTK_HEX_FACTORY))

#define ACCESSIBLE_GTK_HEX_FACTORY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), ACCESSIBLE_TYPE_GTK_HEX_FACTORY, AccessibleGtkHexFactoryClass))

typedef struct _AccessibleGtkHexFactory       AccessibleGtkHexFactory;
typedef struct _AccessibleGtkHexFactoryClass  AccessibleGtkHexFactoryClass;

struct _AccessibleGtkHexFactory
{
	AtkObjectFactory parent;
};

struct _AccessibleGtkHexFactoryClass
{
	AtkObjectFactoryClass parent_class;
};

GType accessible_gtk_hex_get_type (void);
AtkObjectFactory *accessible_gtk_hex_factory_new (void);

GType accessible_gtk_hex_factory_get_type (void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* __ACCESSIBLE_GTK_HEX_FACTORY_H__ */
