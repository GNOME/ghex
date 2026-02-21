/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex-paste-data.c - Paste data for HexWidget

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

struct _HexPasteData
{
	GObject parent_instance;

	char *doc_data;
	int elems;
};

G_DEFINE_TYPE (HexPasteData, hex_paste_data, G_TYPE_OBJECT)


/* Helper Functions */

/* Helper function for the copy and paste stuff, since the data returned by
 * hex_buffer_get_data is NOT null-temrinated.
 *
 * String returned should be freed with g_free.
 */
static char *
doc_data_to_string (const char *data, int len)
{
	char *str;

	str = g_malloc (len + 1);
	memcpy (str, data, len);
	str[len] = '\0';

	return str;
}

/* Constructors and Destructors */

static void
hex_paste_data_init (HexPasteData *self)
{
}

static void
hex_paste_data_class_init (HexPasteDataClass *klass)
{
}


/* Public Method Definitions */

HexPasteData *
hex_paste_data_new (char *doc_data, int elems)
{
	HexPasteData *self;

	g_return_val_if_fail (doc_data, NULL);
	g_return_val_if_fail (elems, NULL);

	self = g_object_new (HEX_TYPE_PASTE_DATA, NULL);

	self->doc_data = doc_data;
	self->elems = elems;

	return self;
}

/* String returned should be freed with g_free. */
char *
hex_paste_data_get_string (HexPasteData *self)
{
	char *string;

	g_return_val_if_fail (self->doc_data, NULL);
	g_return_val_if_fail (self->elems, NULL);

	string = doc_data_to_string (self->doc_data, self->elems);

	return string;
}

char *
hex_paste_data_get_doc_data (HexPasteData *self)
{
	g_return_val_if_fail (self->doc_data, NULL);

	return self->doc_data;
}

int
hex_paste_data_get_elems (HexPasteData *self)
{
	return self->elems;
}
