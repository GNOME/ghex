/* Copyright 2002 Sun Microsystems Inc.
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

#include "factory.h"

static gboolean is_gail_loaded (GType derived_type);

void
setup_factory (void)
{
  AtkRegistry* default_registry;
  GType derived_type;

  /*
   * set up the factory only if GAIL is loaded.
   */
  derived_type = g_type_parent (GTK_TYPE_HEX);


  if (is_gail_loaded (derived_type))
    {
      /* create the factory */
      default_registry = atk_get_default_registry();
      atk_registry_set_factory_type (default_registry,
				     GTK_TYPE_HEX,
				     ACCESSIBLE_TYPE_GTK_HEX_FACTORY);
    }
}

/*
 * This function checks if GAIL is loaded or not,
 * given the parent type.
 * It should be called from the application only
 * after gnome_program_init() is called.
 */
static gboolean
is_gail_loaded (GType derived_type)
{
  AtkObjectFactory *factory;
  GType derived_atk_type;
  
  factory = atk_registry_get_factory (atk_get_default_registry(),
				      derived_type);
  
  derived_atk_type = atk_object_factory_get_accessible_type (factory);
  if (g_type_is_a (derived_atk_type, GTK_TYPE_ACCESSIBLE))
    return TRUE;
  
  return FALSE;
}
