/* vim: ts=4 sw=4 colorcolumn=80                                                
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- *
 */
/* hex-buffer-iface.c - Generic buffer interface intended for use with the
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

#include "hex-buffer-iface.h"

/**
 * HexBuffer: 
 * 
 * #HexBuffer is an interface which can be implemented to act as a buffer
 * for [class@Hex.Document] data. This allows for a #HexDocument to be
 * manipulated at the backend by different backends.
 *
 * Once a file has been loaded into the buffer, it can be read, written
 * to file, etc.
 *
 * #HexBuffer makes reference to the "payload," which is the size of the
 * substantive data in the buffer, not counting items like padding, a gap,
 * etc. (all dependent upon the underlying implementation).
 */
G_DEFINE_INTERFACE (HexBuffer, hex_buffer, G_TYPE_OBJECT)

static void
hex_buffer_default_init (HexBufferInterface *iface)
{
	/**
	 * HexBuffer:file:
	 * This property is the file (as #GFile) being utilized by the buffer.
	 */
	g_object_interface_install_property (iface,
			g_param_spec_object ("file",
				"File",
				"File (as GFile) being utilized by the buffer",
				G_TYPE_FILE,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
}

/* PUBLIC INTERFACE FUNCTIONS */

/**
 * hex_buffer_get_data:
 * @offset: offset position of the data being requested within the payload
 * @len: size in bytes of the requested data
 *
 * Get data of a particular size at a particular offset within the buffer.
 *
 * Returns: (transfer full): a pointer to the data requested, to be freed
 * with g_free().
 */
char *
hex_buffer_get_data (HexBuffer *self,
		gint64 offset,
		size_t len)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), NULL);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->get_data != NULL, NULL);

	return iface->get_data (self, offset, len);
}

/**
 * hex_buffer_get_byte:
 * @offset: offset position of the data being requested within the payload
 * 
 * Get a single byte at a particular offset within the buffer.
 *
 * Returns: the 8-bit character located at `offset` within the payload, or
 * '\0'
 */
char
hex_buffer_get_byte (HexBuffer *self,
			gint64 offset)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), 0);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->get_byte != NULL, 0);

	return iface->get_byte (self, offset);
}

/**
 * hex_buffer_set_data:
 * @offset: offset position of the data being requested within the payload
 * @len: size in bytes of the input data being provided
 * @rep_len: amount of bytes to replace/overwrite (if any)
 * @data: (array length=len) (transfer full): a pointer to the data being
 *   provided
 *
 * Set data at of the buffer at a particular offset, replacing some, all or
 * none of the existing data in the buffer as desired.
 *
 * As `data` will be copied to the recipient, it should be freed with
 * g_free() after being passed to this method, to avoid a memory leak.
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
gboolean
hex_buffer_set_data (HexBuffer *self,
			gint64 offset,
			size_t len,
			size_t rep_len,
			char *data)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), FALSE);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->set_data != NULL, FALSE);

	return iface->set_data (self, offset, len, rep_len, data);
}

/**
 * hex_buffer_set_file:
 * @file: the file to be utilized by the buffer
 *
 * Set the #GFile to be utilized by the buffer. Once it has been set,
 * you can read it into the buffer with [method@Hex.Buffer.read] or
 * [method@Hex.Buffer.read_async].
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
gboolean
hex_buffer_set_file (HexBuffer *self, GFile *file)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), FALSE);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->set_file != NULL, FALSE);

	return iface->set_file (self, file);
}

/**
 * hex_buffer_read:
 *
 * Read the #GFile, previously set, into the buffer. This method will block
 * until the operation is complete. For a non-blocking version, use
 * [method@Hex.Buffer.read_async].
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
gboolean
hex_buffer_read (HexBuffer *self)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), FALSE);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->read != NULL, FALSE);

	return iface->read (self);
}

/**
 * hex_buffer_read_async:
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): function to be called when the operation is
 *   complete
 *
 * Read the #GFile, previously set, into the buffer. This is the non-blocking
 * version of [method@Hex.Buffer.read].
 */
void
hex_buffer_read_async (HexBuffer *self,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data)
{
	HexBufferInterface *iface;

	g_return_if_fail (HEX_IS_BUFFER (self));
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_if_fail (iface->read_async != NULL);

	iface->read_async (self, cancellable, callback, user_data);
}

/**
 * hex_buffer_read_finish:
 * @result: result of the task
 * @error: (nullable): optional pointer to a #GError object to populate with
 *   any error returned by the task
 *
 * Obtain the result of a completed file read operation.
 *
 * This method is typically called from the #GAsyncReadyCallback function
 * passed to [method@Hex.Buffer.read_async] to obtain the result of the
 * operation.
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
gboolean
hex_buffer_read_finish (HexBuffer *self,
		GAsyncResult *result,
		GError **error)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), FALSE);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->read_finish != NULL, FALSE);

	return iface->read_finish (self, result, error);
}

/**
 * hex_buffer_write_to_file:
 * @file: #GFile to write to
 *
 * Write the buffer to the #GFile specified. This operation will block.
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
/* FIXME - insert reference to async version if/when it is written. */
gboolean
hex_buffer_write_to_file (HexBuffer *self, GFile *file)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), FALSE);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->write_to_file != NULL, FALSE);

	return iface->write_to_file (self, file);
}

/**
 * hex_buffer_get_payload_size:
 * 
 * Get the size of the payload of the buffer, in bytes.
 *
 * Returns: the size in bytes of the payload of the buffer
 */
gint64
hex_buffer_get_payload_size (HexBuffer *self)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), 0);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->get_payload_size != NULL, 0);

	return iface->get_payload_size (self);
}

/* Utility functions */

/**
 * hex_buffer_util_get_file_size:
 * @file: file to obtain size of
 *
 * Utility function to obtain the size of a #GFile.
 *
 * Returns: the size of the file, in bytes
 */
gint64
hex_buffer_util_get_file_size (GFile *file)
{
	GFileInfo *info;

	info = g_file_query_info (file,
			G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);

	if (! info)
		return 0;

	return g_file_info_get_size (info);
}
