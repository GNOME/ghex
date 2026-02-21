/* vim: ts=4 sw=4 colorcolumn=80                                                
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- *
 */
/* hex-document-malloc.c - `malloc` implementation of the HexBuffer iface.
 *
 * Copyright © 2021 Logan Rathbone
 *
 * Adapted from code originally contained in hex-document.c (see copyright
 * notices therein for prior authorship).
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

#include "hex-buffer-malloc.h"

/* PROPERTIES */

enum
{
	PROP_FILE = 1,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

/**
 * HexBufferMalloc:
 * 
 * #HexBufferMalloc is an object implementing the [iface@Hex.Buffer] interface,
 * allowing it to be used as a #HexBuffer backend to be used with
 * [class@Hex.Document].
 *
 * #HexBufferMalloc replicates the legacy backend of GHex, reading files
 * directly into memory as a buffer. This operation tends to be very fast,
 * but unreliable with larger files. It is not recommended except as a
 * fallback for systems that are not compatible with other backends.
 */
struct _HexBufferMalloc
{
	GObject parent_instance;

	GFile *file;
	char *buffer;			/* data buffer */
	char *gap_pos;			/* pointer to the start of insertion gap */

	size_t gap_size;		/* insertion gap size */
	gint64 buffer_size;		/* buffer size = file size + gap size */
	gint64 payload_size;
};

static void hex_buffer_malloc_iface_init (HexBufferInterface *iface);

G_DEFINE_TYPE_WITH_CODE (HexBufferMalloc, hex_buffer_malloc, G_TYPE_OBJECT,
		G_IMPLEMENT_INTERFACE (HEX_TYPE_BUFFER, hex_buffer_malloc_iface_init))

/* FORWARD DECLARATIONS */
	
static gboolean	hex_buffer_malloc_set_file (HexBuffer *buf, GFile *file);
static GFile *	hex_buffer_malloc_get_file (HexBuffer *buf);

/* PROPERTIES - GETTERS AND SETTERS */

static void
hex_buffer_malloc_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC(object);
	HexBuffer *buf = HEX_BUFFER(object);

	switch (property_id)
	{
		case PROP_FILE:
			hex_buffer_malloc_set_file (buf, g_value_get_object (value));
			break;

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_buffer_malloc_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC(object);
	HexBuffer *buf = HEX_BUFFER(object);

	switch (property_id)
	{
		case PROP_FILE:
			g_value_set_object (value, self->file);
			break;

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/* PRIVATE FUNCTIONS */

static gboolean
update_payload_size_from_file (HexBufferMalloc *self)
{
	gint64 file_size = hex_buffer_util_get_file_size (self->file);

	if (file_size < 0)
		return FALSE;

	self->payload_size = file_size;
	return TRUE;
}

/* transfer: none */
static GFile *
hex_buffer_malloc_get_file (HexBuffer *buf)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (buf);

	return self->file;
}

static gboolean
hex_buffer_malloc_set_file (HexBuffer *buf, GFile *file)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (buf);

	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	self->file = file;
	if (! update_payload_size_from_file (self))
	{
		self->file = NULL;
		return FALSE;
	}
	g_object_notify (G_OBJECT(self), "file");
	return TRUE;
}

static char
hex_buffer_malloc_get_byte (HexBuffer *buf, gint64 offset)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (buf);

	if (offset < self->payload_size)
	{
		if (self->gap_pos <= self->buffer + offset)
			offset += self->gap_size;

		return self->buffer[offset];
	}
	else
		return 0;
}

static char *
hex_buffer_malloc_get_data (HexBuffer *buf, gint64 offset, size_t len)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (buf);
	char *ptr, *data, *dptr;

	ptr = self->buffer + offset;

	if (ptr >= self->gap_pos)
		ptr += self->gap_size;

	dptr = data = g_malloc (len);

	for (size_t i = 0; i < len; ++i)
	{
		if (ptr >= self->gap_pos  &&  ptr < self->gap_pos + self->gap_size)
			ptr += self->gap_size;

		*dptr++ = *ptr++;
	}

	return data;
}

static void
hex_buffer_malloc_place_gap (HexBuffer *buf, gint64 offset, size_t min_size)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (buf);
	char *tmp, *buf_ptr, *tmp_ptr;

	if (self->gap_size < min_size)
	{
		tmp = g_malloc (self->payload_size);
		buf_ptr = self->buffer;
		tmp_ptr = tmp;

		while (buf_ptr < self->gap_pos)
			*tmp_ptr++ = *buf_ptr++;

		buf_ptr += self->gap_size;
		while (buf_ptr < self->buffer + self->buffer_size)
			*tmp_ptr++ = *buf_ptr++;

		self->gap_size = MAX (min_size, 32);
		self->buffer_size = self->payload_size + self->gap_size;
		self->buffer = g_realloc (self->buffer, self->buffer_size);
		self->gap_pos = self->buffer + offset;

		buf_ptr = self->buffer;
		tmp_ptr = tmp;
		
		while (buf_ptr < self->gap_pos)
			*buf_ptr++ = *tmp_ptr++;

		buf_ptr += self->gap_size;
		while (buf_ptr < self->buffer + self->buffer_size)
			*buf_ptr++ = *tmp_ptr++;

		g_free(tmp);
	}
	else
	{
		if (self->buffer + offset < self->gap_pos)
		{
			buf_ptr = self->gap_pos + self->gap_size - 1;

			while (self->gap_pos > self->buffer + offset)
				*buf_ptr-- = *(--self->gap_pos);
		}
		else if (self->buffer + offset > self->gap_pos)
		{
			buf_ptr = self->gap_pos + self->gap_size;

			while (self->gap_pos < self->buffer + offset)
				*self->gap_pos++ = *buf_ptr++;
		}
	}
}

static gboolean
hex_buffer_malloc_set_data (HexBuffer *buf, gint64 offset, size_t len,
					  size_t rep_len, char *data)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (buf);
	gint64 i;
	char *ptr;

	if (offset > self->payload_size)
	{
		g_debug ("%s: offset greater than payload size; returning.", __func__);
		return FALSE;
	}

	if (rep_len == len) {
		if (self->buffer + offset >= self->gap_pos)
			offset += self->gap_size;
	}
	else {
		if (rep_len > len) {
			hex_buffer_malloc_place_gap (buf, offset + rep_len, 1);
		}
		else if (rep_len < len) {
			hex_buffer_malloc_place_gap (buf, offset + rep_len, len - rep_len);
		}
		self->gap_pos -= rep_len - len;
		self->gap_size += rep_len - len;
		self->payload_size += len - rep_len;
	}

	ptr = &self->buffer[offset];
	i = 0;
	while (offset + i < self->buffer_size && i < len) {
		if (ptr >= self->gap_pos && ptr < self->gap_pos + self->gap_size)
			ptr += self->gap_size;
		*ptr++ = *data++;
		i++;
	}

	return TRUE;
}

static gboolean
hex_buffer_malloc_read (HexBuffer *buf)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (buf);
	char *path = NULL;
	FILE *file = NULL;
	gint64 fread_ret;
	gboolean retval = FALSE;

	if (! G_IS_FILE (self->file))
		goto out;

	path = g_file_get_path (self->file);
	if (! path)
		goto out;

	if (! update_payload_size_from_file (self))
		goto out;

	if ((file = fopen(path, "r")) == NULL)
		goto out;

	self->buffer_size = self->payload_size + self->gap_size;
	self->buffer = g_malloc (self->buffer_size);                               

	/* FIXME - I believe this will crap out after 4GB on a 32-bit machine
	 */
	fread_ret = fread (
			self->buffer + self->gap_size, 1, self->payload_size, file);
	if (fread_ret != self->payload_size)
		goto out;

	self->gap_pos = self->buffer;
	retval = TRUE;

out:
	if (file)
		fclose (file);
	g_free (path);
	return retval;
}


static gboolean
hex_buffer_malloc_read_finish (HexBuffer *buf,
		GAsyncResult *result,
		GError **error)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (buf);

	g_return_val_if_fail (g_task_is_valid (result, G_OBJECT(self)), FALSE);

	return g_task_propagate_boolean (G_TASK(result), error);
}

static void
hex_buffer_malloc_thread (GTask *task,
		gpointer source_object,
		gpointer task_data,
		GCancellable *cancellable)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (source_object);
	gboolean success;

	success = hex_buffer_malloc_read (HEX_BUFFER(self));

	g_task_return_boolean (task, success);
}

static void
hex_buffer_malloc_read_async (HexBuffer *buf,
		GCancellable *cancellable,
		GAsyncReadyCallback callback,
		gpointer user_data)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (buf);
	GTask *task;

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_run_in_thread (task, hex_buffer_malloc_thread);
	g_object_unref (task);	/* _run_in_thread takes a ref */
}

static gboolean
hex_buffer_malloc_write_to_file (HexBuffer *buf, GFile *file)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (buf);
	char *path = NULL;
	FILE *fp = NULL;
	gboolean ret = FALSE;
	gint64 exp_len;
	gint64 len;

	path = g_file_get_path (file);
	if (! path)
		goto out;

	/* TODO/FIXME - Actually use the GFile functions to write to file. */

	if ((fp = fopen(path, "wb")) == NULL)
		goto out;

	ret = TRUE;

	if (self->gap_pos > self->buffer)
	{
		exp_len = MIN (self->payload_size,
				(gint64)(self->gap_pos - self->buffer));
		len = fwrite (self->buffer, 1, exp_len, fp);
		ret = (ret && (len == exp_len)) ? TRUE : FALSE;
	}

	if (self->gap_pos < self->buffer + self->payload_size)
	{
		exp_len = self->payload_size - (self->gap_pos - self->buffer);
		len = fwrite (self->gap_pos + self->gap_size, 1, exp_len, fp);
		ret = (ret && (len == exp_len)) ? TRUE : FALSE;
	}

out:
	g_free (path);
	if (fp) fclose(fp);
	return ret;
}

static gboolean
hex_buffer_malloc_write_to_file_finish (HexBuffer *buf,
		GAsyncResult *result,
		GError **error)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (buf);

	g_return_val_if_fail (g_task_is_valid (result, G_OBJECT(self)), FALSE);

	return g_task_propagate_boolean (G_TASK(result), error);
}

static void
hex_buffer_malloc_write_thread (GTask *task,
		gpointer source_object,
		gpointer task_data,
		GCancellable *cancellable)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (source_object);
	GFile *file = G_FILE (task_data);
	gboolean success;

	success = hex_buffer_malloc_write_to_file (HEX_BUFFER(self), file);

	g_task_return_boolean (task, success);
}

static void
hex_buffer_malloc_write_to_file_async (HexBuffer *buf,
		GFile *file,
		GCancellable *cancellable,
		GAsyncReadyCallback callback,
		gpointer user_data)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (buf);
	GTask *task;

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_task_data (task, file, NULL);
	g_task_run_in_thread (task, hex_buffer_malloc_write_thread);
	g_object_unref (task);	/* _run_in_thread takes a ref */
}

static gint64
hex_buffer_malloc_get_payload_size (HexBuffer *buf)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (buf);

	return self->payload_size;
}

/* CONSTRUCTORS AND DESTRUCTORS */

static void
hex_buffer_malloc_init (HexBufferMalloc *self)
{
	self->gap_size = 100;
	self->buffer_size = self->gap_size;
	self->buffer = g_malloc (self->buffer_size);
	self->gap_pos = self->buffer;
}

static void
hex_buffer_malloc_dispose (GObject *gobject)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (gobject);

	/* chain up */
	G_OBJECT_CLASS(hex_buffer_malloc_parent_class)->dispose (gobject);
}

static void
hex_buffer_malloc_finalize (GObject *gobject)
{
	HexBufferMalloc *self = HEX_BUFFER_MALLOC (gobject);

	g_clear_pointer (&self->buffer, g_free);

	/* chain up */
	G_OBJECT_CLASS(hex_buffer_malloc_parent_class)->finalize (gobject);
}

static void
hex_buffer_malloc_class_init (HexBufferMallocClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	
	gobject_class->finalize = hex_buffer_malloc_finalize;
	gobject_class->dispose = hex_buffer_malloc_dispose;

	gobject_class->set_property = hex_buffer_malloc_set_property;
	gobject_class->get_property = hex_buffer_malloc_get_property;

	g_object_class_override_property (gobject_class, PROP_FILE, "file");
}


/* PUBLIC FUNCTIONS */

/**
 * hex_buffer_malloc_new:
 * @file: a #GFile pointing to a valid file on the system
 *
 * Create a new #HexBufferMalloc object.
 *
 * Returns: a new #HexBufferMalloc object, automatically cast to a #HexBuffer
 * type, or %NULL if the operation failed.
 */
HexBuffer *
hex_buffer_malloc_new (GFile *file)
{
	HexBufferMalloc *self = g_object_new (HEX_TYPE_BUFFER_MALLOC, NULL);

	if (file)
	{
		/* If a path is provided but it can't be set, nullify the object */
		if (! hex_buffer_malloc_set_file (HEX_BUFFER(self), file))
			g_clear_object (&self);
	}

	if (self)
		return HEX_BUFFER(self);
	else
		return NULL;
}


/* INTERFACE IMPLEMENTATION FUNCTIONS */

static void
hex_buffer_malloc_iface_init (HexBufferInterface *iface)
{
	iface->get_data = hex_buffer_malloc_get_data;
	iface->get_byte = hex_buffer_malloc_get_byte;
	iface->set_data = hex_buffer_malloc_set_data;
	iface->get_file = hex_buffer_malloc_get_file;
	iface->set_file = hex_buffer_malloc_set_file;
	iface->read = hex_buffer_malloc_read;
	iface->read_async = hex_buffer_malloc_read_async;
	iface->read_finish = hex_buffer_malloc_read_finish;
	iface->write_to_file = hex_buffer_malloc_write_to_file;
	iface->write_to_file_async = hex_buffer_malloc_write_to_file_async;
	iface->write_to_file_finish = hex_buffer_malloc_write_to_file_finish;
	iface->get_payload_size = hex_buffer_malloc_get_payload_size;
}
