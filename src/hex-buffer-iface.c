/* vim: ts=4 sw=4 colorcolumn=80                                                
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- *
 */
/* hex-buffer-iface.h - Generic buffer interface intended for use with the
 * HexDocument API
 *
 * Copyright © 2021 Logan Rathbone
 *
 * Original GHex author: Jaka Mocnik
 */

#include "hex-buffer-iface.h"

G_DEFINE_INTERFACE (HexBuffer, hex_buffer, G_TYPE_OBJECT)

static void
hex_buffer_default_init (HexBufferInterface *iface)
{
    /* add properties and signals to the interface here */
}

/* PUBLIC INTERFACE FUNCTIONS */

char *
hex_buffer_get_data (HexBuffer *self,
		size_t offset,
		size_t len)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), NULL);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->get_data != NULL, NULL);

	return iface->get_data (self, offset, len);
}

char
hex_buffer_get_byte (HexBuffer *self,
			size_t offset)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), 0);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->get_byte != NULL, 0);

	return iface->get_byte (self, offset);
}

gboolean
hex_buffer_set_data (HexBuffer *self,
			size_t offset,
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

gboolean
hex_buffer_set_file (HexBuffer *self, GFile *file)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), FALSE);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->set_file != NULL, FALSE);

	return iface->set_file (self, file);
}


gboolean
hex_buffer_read (HexBuffer *self)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), FALSE);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->read != NULL, FALSE);

	return iface->read (self);
}

gboolean
hex_buffer_write_to_file (HexBuffer *self, GFile *file)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), FALSE);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->write_to_file != NULL, FALSE);

	return iface->write_to_file (self, file);
}

size_t
hex_buffer_get_payload_size (HexBuffer *self)
{
	HexBufferInterface *iface;

	g_return_val_if_fail (HEX_IS_BUFFER (self), 0);
	iface = HEX_BUFFER_GET_IFACE (self);
	g_return_val_if_fail (iface->get_payload_size != NULL, 0);

	return iface->get_payload_size (self);
}
