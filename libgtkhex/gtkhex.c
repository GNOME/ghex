/* gtkhex.c
 *
 * Copyright © 2026 Logan Rathbone
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gtkhex.h"

#include "config.h"

/**
 * hex_init:
 *
 * Initializes the libgtkhex library (e.g. for internationalization).
 *
 * This function can be called several times, but is meant to be called at the
 * beginning of `main()`, before any other libgtkhex function call.
 */
void
hex_init (void)
{
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	initialized = TRUE;
}
