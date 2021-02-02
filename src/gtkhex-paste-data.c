/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex-paste-data.c - Paste data for GtkHex

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

#include "gtkhex-paste-data.h"


/* GObject Definition */

struct _GtkHexPasteData
{
	GObject parent_instance;

	guchar *doc_data;
	guint elems;
};

G_DEFINE_TYPE (GtkHexPasteData, gtk_hex_paste_data, G_TYPE_OBJECT)


/* Helper Functions */

/* Helper function for the copy and paste stuff, since the data returned by
 * hex_document_get_data is NOT null-temrinated.
 *
 * String returned should be freed with g_free.
 */
static char *
doc_data_to_string (const guchar *data, guint len)
{
	char *str;

	str = g_malloc (len + 1);
	memcpy (str, data, len);
	str[len] = '\0';

	return str;
}


/* FIXME/TODO - this transforms certain problematic characters for copy/paste
 * to a '?'. Maybe find a home for this guy at some point.
 */
#if 0
{
	char *cp;
		for (cp = text; *cp != '\0'; ++cp)
		{
			if (! is_copyable(*cp))
				*cp = '?';
		}
}
#endif
		

/* Constructors and Destructors */

static void
gtk_hex_paste_data_init (GtkHexPasteData *self)
{
}

static void
gtk_hex_paste_data_class_init (GtkHexPasteDataClass *klass)
{
}


/* Public Method Definitions */

GtkHexPasteData *
gtk_hex_paste_data_new (guchar *doc_data, guint elems)
{
	GtkHexPasteData *self;

	g_return_val_if_fail (doc_data, NULL);
	g_return_val_if_fail (elems, NULL);

	self = g_object_new (GTK_TYPE_HEX_PASTE_DATA, NULL);

	self->doc_data = doc_data;
	self->elems = elems;

	return self;
}

/* String returned should be freed with g_free. */
char *
gtk_hex_paste_data_get_string (GtkHexPasteData *self)
{
	char *string;

	g_return_val_if_fail (self->doc_data, NULL);
	g_return_val_if_fail (self->elems, NULL);

	string = doc_data_to_string (self->doc_data, self->elems);

	return string;
}

guchar *
gtk_hex_paste_data_get_doc_data (GtkHexPasteData *self)
{
	g_return_val_if_fail (self->doc_data, NULL);

	return self->doc_data;
}

guint
gtk_hex_paste_data_get_elems (GtkHexPasteData *self)
{
	return self->elems;
}
