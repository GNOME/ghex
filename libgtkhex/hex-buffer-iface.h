/* vim: ts=4 sw=4 colorcolumn=80                                                
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- *
 */
/* hex-buffer-iface.h - Generic buffer interface intended for use with the
 * HexDocument API
 *
 * Copyright © 2021 Logan Rathbone
 *
 * GHex is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GHex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GHex; see the file COPYING.
 * If not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef HEX_BUFFER_IFACE_H
#define HEX_BUFFER_IFACE_H

#define _GNU_SOURCE

#include <gio/gio.h>
#include <glib-object.h>
#include <glib/gi18n.h>

G_BEGIN_DECLS

#define HEX_TYPE_BUFFER hex_buffer_get_type ()
G_DECLARE_INTERFACE (HexBuffer, hex_buffer, HEX, BUFFER, GObject)

/**
 * HexBufferNewFunc:
 * @file: #GFile to pass to the function
 *
 * Specifies the type of function which the `_new` method of a #HexBuffer
 * interface implementation must comply with.
 *
 * Returns: a pointer to a valid implementaion of a #HexBuffer interface,
 * pre-cast as the #HexBuffer type.
 */

typedef HexBuffer * (*HexBufferNewFunc) (GFile *file);

struct _HexBufferInterface
{
	GTypeInterface parent_iface;

	char * (*get_data) (HexBuffer *self,
			gint64 offset,
			size_t len);

	char (*get_byte) (HexBuffer *self,
			gint64 offset);

	gboolean (*set_data) (HexBuffer *self,
			gint64 offset,
			size_t len,
			size_t rep_len,
			char *data);

	GFile * (*get_file) (HexBuffer *self);

	gboolean (*set_file) (HexBuffer *self,
			GFile *file);

	gboolean (*read) (HexBuffer *self);

	void (*read_async) (HexBuffer *self,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data);

	gboolean (*read_finish) (HexBuffer *self,
			GAsyncResult *result,
			GError **error);

	gboolean (*write_to_file) (HexBuffer *self,
			GFile *file);

	void (*write_to_file_async) (HexBuffer *self,
			GFile *file,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data);

	gboolean (*write_to_file_finish) (HexBuffer *self,
			GAsyncResult *result,
			GError **error);

	gint64 (*get_payload_size) (HexBuffer *self);

	/* --- padding starts here -- started w/ 12 extra vfuncs --- */

	gpointer padding[12];
};

/* Interface functions */

char * hex_buffer_get_data (HexBuffer *self,
		gint64 offset,
		size_t len);

char hex_buffer_get_byte (HexBuffer *self,
		gint64 offset);

gboolean hex_buffer_set_data (HexBuffer *self,
		gint64 offset,
		size_t len,
		size_t rep_len,
		char *data);

gboolean hex_buffer_set_file (HexBuffer *self,
		GFile *file);

gboolean hex_buffer_read (HexBuffer *self);

void hex_buffer_read_async (HexBuffer *buf, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data);

gboolean hex_buffer_read_finish (HexBuffer *buf, GAsyncResult *result,
		GError **error);

gboolean hex_buffer_write_to_file (HexBuffer *self,
		GFile *file);

void hex_buffer_write_to_file_async (HexBuffer *self,
		GFile *file,
		GCancellable *cancellable,
		GAsyncReadyCallback callback,
		gpointer user_data);

gboolean hex_buffer_write_to_file_finish (HexBuffer *self,
		GAsyncResult *result,
		GError **error);

gint64 hex_buffer_get_payload_size (HexBuffer *self);

/* Common utility functions */

gint64 hex_buffer_util_get_file_size (GFile *file);
HexBuffer * hex_buffer_util_new (const char *plugin, GFile *file);

G_END_DECLS
#endif
