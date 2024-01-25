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
#include "hex-buffer-malloc.h"
#include <config.h>

/**
 * HexBuffer: 
 * 
 * #HexBuffer is an interface which can be implemented to act as a buffer
 * for [class@Hex.Document] data. This allows for a #HexDocument to be
 * manipulated by different backends.
 *
 * Once a file has been loaded into the buffer, it can be read, written
 * to file, etc.
 *
 * #HexBuffer makes reference to the "payload," which is the size of the
 * substantive data in the buffer, not counting items like padding, a gap,
 * etc. (all dependent upon the underlying implementation).
 *
 * Most clients who just want to create an spin up a #HexBuffer object should
 * look to the [func@Hex.Buffer.util_new] utility function as a starting
 * point, and then manipulate the returned #HexBuffer object with the methods
 * documented herein.
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
 * @data: (array length=len) (element-type gint8): a pointer to
 *   the data being provided
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
 * @file: (transfer full): the file to be utilized by the buffer
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
 * Write the buffer to the #GFile specified. This operation will block. For a
 * non-blocking version, use [method@Hex.Buffer.write_to_file_async].
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
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
 * hex_buffer_write_to_file_async:
 * @file: #GFile to write to
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): function to be called when the operation is
 *   complete
 *
 * Write the buffer to the #GFile specified. This is the non-blocking
 * version of [method@Hex.Buffer.write_to_file].
 */
void
hex_buffer_write_to_file_async (HexBuffer *self,
		GFile *file,
		GCancellable *cancellable,
		GAsyncReadyCallback callback,
		gpointer user_data)
{
	HexBufferInterface *iface;

	g_return_if_fail (HEX_IS_BUFFER (self));
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_if_fail (iface->write_to_file_async != NULL);

	iface->write_to_file_async (self, file, cancellable, callback, user_data);
}

/**
 * hex_buffer_write_to_file_finish:
 * @result: result of the task
 * @error: (nullable): optional pointer to a #GError object to populate with
 *   any error returned by the task
 *
 * Obtain the result of a completed write-to-file operation.
 *
 * This method is typically called from the #GAsyncReadyCallback function
 * passed to [method@Hex.Buffer.write_to_file_async] to obtain the result of
 * the operation.
 *
 * Returns: %TRUE if the operation was successful; %FALSE otherwise.
 */
gboolean
hex_buffer_write_to_file_finish (HexBuffer *self,
		GAsyncResult *result,
		GError **error)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), FALSE);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->write_to_file_finish != NULL, FALSE);

	return iface->write_to_file_finish (self, result, error);
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
 * hex_buffer_util_new:
 * @plugin: (nullable): the name of the plugin, or %NULL
 * @file: (nullable): file to initialize the buffer with, or %NULL
 *
 * Utility function to create an on object which implements the HexBuffer
 * interface.
 *
 * The `plugin` parameter will be the unique part of the plugin file name (eg,
 * if the file name is libhex-buffer-mmap.so, you would specify "mmap"). If
 * `NULL` is passed, the fallback (presently the "malloc" backend, but this is
 * an implementation detail and may be subject to change) will be used.
 *
 * The `file` parameter is a valid #GFile if you would like the buffer
 * pre-loaded, or %NULL for an empty buffer.
 *
 * Returns: (transfer full): a pointer to a valid implementation of a
 * [iface@Hex.Buffer] interface, pre-cast as type #HexBuffer, or %NULL if
 * the operation failed. Starting with 4.2, if a specific backend is requested,
 * and the system supports plugins as a whole but cannot load that specified
 * plugin, %NULL will be returned as though the operation failed, so as to
 * customize the fallback scheme programmatically.
 */
HexBuffer *
hex_buffer_util_new (const char *plugin, GFile *file)
{
	GModule *module;
	HexBufferNewFunc func = NULL;
	char *plugin_soname = NULL;
	char *plugin_path = NULL;
	char *symbol_name = NULL;

	/* If the malloc backend is specifically requested, or if dynamic loading
	 * of plugins isn't even supported on the platform, or if NULL is passed,
	 * fall back to `malloc` since it's the only one baked in.
	 */
	if (g_strcmp0 (plugin, "malloc") == 0)
	{
		g_debug ("malloc buffer explicitly specified; no need to load any plugin.");
		return hex_buffer_malloc_new (file);
	}
	if (! plugin)
	{
		g_debug ("No plugin specified; falling back to the `malloc` backend.");
		return hex_buffer_malloc_new (file);
	}
	if (! g_module_supported () || !plugin)
	{
		g_warning ("Modules not supported on this system; falling back to `malloc` backend.");
		return hex_buffer_malloc_new (file);
	}

	/* g_module_build_path has been deprecated as of glib 2.76, but the behaviour
	 * of g_module_open has also been changed^W improved. Since it's unclear at
	 * this point whether these improvements are backwards-compatible, we use the
	 * legacy functions for now until the changes have had some time to cook for a
	 * while.
	 */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	plugin_soname = g_strdup_printf ("hex-buffer-%s", plugin);
	plugin_path = g_module_build_path (PACKAGE_PLUGINDIR, plugin_soname);
	symbol_name = g_strdup_printf ("hex_buffer_%s_new", plugin);
	module = g_module_open (plugin_path, G_MODULE_BIND_LAZY);
G_GNUC_END_IGNORE_DEPRECATIONS

	if (! module)
	{
		g_warning ("Unable to locate/load plugin at %s", plugin_path);
		func = NULL;
	}
	else if (! g_module_symbol (module, symbol_name, (gpointer *)&func) ||
			func == NULL)
	{
		g_warning ("Plugin found at %s - but unable to locate symbol: %s - "
				"falling back to `malloc` backend.",
				plugin_path, symbol_name);
		func = hex_buffer_malloc_new;
	}
	else
	{
		g_debug ("Loaded plugin: %s", plugin_path);
		g_module_make_resident (module);
	}

/* out: */
	g_free (plugin_soname);
	g_free (plugin_path);
	g_free (symbol_name);

	if (func)
		return func (file);
	else
		return NULL;
}

/**
 * hex_buffer_util_get_file_size:
 * @file: file to obtain size of
 *
 * Utility function to obtain the size of a #GFile.
 *
 * Since 4.6, this function will return an unspecified negative value if the
 * file size was unable to be obtained, as opposed to 0 as it previously did.
 * This is to distinguish between valid zero-length files and files for which
 * the size was not able to be obtained (eg, if it was unreadable). In the
 * future, these negative values may be defined as specific enums which have a
 * more specific meaning. But presently and going forward, testing for a
 * negative value is sufficient to determine that the file size was
 * unobtainable.
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
		return -1;

	return g_file_info_get_size (info);
}
