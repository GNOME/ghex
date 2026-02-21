/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* common-macros.h - Common macros for GHex

   Copyright © 2021 Logan Rathbone <poprocks@gmail.com>

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
   If not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Original GHex Author: Jaka Mocnik <jaka@gnu.org>
*/

#define GET_WIDGET(X) 														\
	X = GTK_WIDGET(gtk_builder_get_object (builder, #X));					\
	g_assert (GTK_IS_WIDGET (X));

/* Note - only use this macro for application-specific (as opposed to theme/
 * widget-specific) operations.
 */
#define APPLY_PROVIDER_TO(PROVIDER, WIDGET)									\
{																			\
	GtkStyleContext *_context;												\
	_context = gtk_widget_get_style_context (GTK_WIDGET(WIDGET));			\
	gtk_style_context_add_provider (_context, GTK_STYLE_PROVIDER(PROVIDER),	\
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);						\
}
