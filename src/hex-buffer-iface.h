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

#ifndef HEX_BUFFER_IFACE_H
#define HEX_BUFFER_IFACE_H

#define _GNU_SOURCE

#include <gio/gio.h>
#include <glib-object.h>
#include <glib/gi18n.h>

G_BEGIN_DECLS

#define HEX_TYPE_BUFFER hex_buffer_get_type ()
G_DECLARE_INTERFACE (HexBuffer, hex_buffer, HEX, BUFFER, GObject)

struct _HexBufferInterface
{
	GTypeInterface parent_iface;

	char * (*get_data) (HexBuffer *self,
			size_t offset,
			size_t len);

	char (*get_byte) (HexBuffer *self,
			size_t offset);

	gboolean (*set_data) (HexBuffer *self,
			size_t offset,
			size_t len,
			size_t rep_len,
			char *data);

	gboolean (*set_file) (HexBuffer *self,
			GFile *file);

	gboolean (*read) (HexBuffer *self);

	gboolean (*write_to_file) (HexBuffer *self,
			GFile *file);

	size_t (*get_payload_size) (HexBuffer *self);

	/* --- padding starts here -- started w/ 12 extra vfuncs --- */

	gpointer padding[12];
};



char * hex_buffer_get_data (HexBuffer *self,
		size_t offset,
		size_t len);

char hex_buffer_get_byte (HexBuffer *self,
		size_t offset);

gboolean hex_buffer_set_data (HexBuffer *self,
		size_t offset,
		size_t len,
		size_t rep_len,
		char *data);

gboolean hex_buffer_set_file (HexBuffer *self,
		GFile *file);

gboolean hex_buffer_read (HexBuffer *self);

gboolean hex_buffer_write_to_file (HexBuffer *self,
		GFile *file);

size_t hex_buffer_get_payload_size (HexBuffer *self);


G_END_DECLS
#endif
