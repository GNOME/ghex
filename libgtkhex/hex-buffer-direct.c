/* vim: ts=4 sw=4 colorcolumn=80                                                
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- *
 */
/* hex-document-direct.c - `direct` implementation of the HexBuffer iface.
 *
 * Copyright © 2022 Logan Rathbone
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

#include "hex-buffer-direct.h"

#define HEX_BUFFER_DIRECT_ERROR hex_buffer_direct_error_quark ()
GQuark
hex_buffer_direct_error_quark (void)
{
  return g_quark_from_static_string ("hex-buffer-direct-error-quark");
}

/* PROPERTIES */

enum
{
	PROP_FILE = 1,
	N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES];

static char *invalid_path_msg = N_("The file appears to have an invalid path.");

/**
 * HexBufferDirect:
 * 
 * #HexBufferDirect is an object implementing the [iface@Hex.Buffer] interface,
 * allowing it to be used as a #HexBuffer backend to be used with
 * [class@Hex.Document].
 *
 * #HexBufferDirect allows for direct reading/writing of files, and is
 * primarily intended to be used with block devices.
 *
 * #HexBufferDirect depends upon UNIX/POSIX system calls; as such, it will
 * be unlikely to be available under WIN32.
 */
struct _HexBufferDirect
{
	GObject parent_instance;

	GFile *file;
	GError *error;		/* no custom codes; use codes from errno */
	int last_errno;		/* cache in case we need to re-report errno error. */

	char *path;
	int fd;				/* file descriptor for direct read/write */
	gint64 payload;		/* size of the payload */

	gint64 clean_bytes;	/* 'clean' size of the file (no additions or deletions */
	GHashTable *changes;
};

static void hex_buffer_direct_iface_init (HexBufferInterface *iface);

G_DEFINE_TYPE_WITH_CODE (HexBufferDirect, hex_buffer_direct, G_TYPE_OBJECT,
		G_IMPLEMENT_INTERFACE (HEX_TYPE_BUFFER, hex_buffer_direct_iface_init))

/* FORWARD DECLARATIONS */
	
static gboolean	hex_buffer_direct_set_file (HexBuffer *buf, GFile *file);
static GFile *	hex_buffer_direct_get_file (HexBuffer *buf);

/* PROPERTIES - GETTERS AND SETTERS */

static void
hex_buffer_direct_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT(object);
	HexBuffer *buf = HEX_BUFFER(object);

	switch (property_id)
	{
		case PROP_FILE:
			hex_buffer_direct_set_file (buf, g_value_get_object (value));
			break;

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_buffer_direct_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT(object);
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

/* PRIVATE FUNCTIONS AND HELPERS */

/* Helper wrapper for g_set_error and to cache errno */
static void
set_error (HexBufferDirect *self, const char *blurb)
{
	char *message = NULL;

	if (errno) {
	/* Translators:  the first '%s' is the blurb indicating some kind of an
	 * error has occurred (eg, 'An error has occurred', and the the 2nd '%s'
	 * is the standard error message that will be reported from the system
	 * (eg, 'No such file or directory').
	 */
		message = g_strdup_printf (_("%s: %s"), blurb, g_strerror (errno));
	}
	else
	{
		message = g_strdup (blurb);
	}
	g_debug ("%s: %s", __func__, message);

	g_clear_error (&self->error);

	g_set_error (&self->error,
			HEX_BUFFER_DIRECT_ERROR,
			errno,
			"%s",
			message);

	if (errno)
		self->last_errno = errno;

	g_free (message);
}

/* mostly a helper for _get_data and _set_data */
static char *
get_file_data (HexBufferDirect *self,
		gint64 offset,
		size_t len)
{
	char *data = NULL;
	off_t new_offset;
	ssize_t nread;

	if (offset + len > self->payload)
	{
		g_critical ("%s: Programmer error - length is past payload. Reducing. "
				"Some garbage may be displayed in the hex widget.", __func__);

		len = self->payload - offset;
	}

	data = g_malloc (len);
	new_offset = lseek (self->fd, offset, SEEK_SET);

	g_assert (offset == new_offset);

	errno = 0;
	nread = read (self->fd, data, len);

	if (nread == -1)
	{
		set_error (self, _("Failed to read data from file."));
		g_clear_pointer (&data, g_free);
	}

	return data;
}

static int
create_fd_from_path (HexBufferDirect *self, const char *path)
{
	int fd = -1;
	struct stat statbuf;

	errno = 0;

	if (stat (path, &statbuf) != 0)
	{
		if (errno != ENOENT) {
			set_error (self,
				_("Unable to retrieve file or directory information"));
			return -1;
		}

		errno = 0;
		fd = open(path, O_CREAT|O_TRUNC|O_RDWR,
				S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

		if (fd < 0) {
			set_error (self, _("Unable to create file"));
			return -1;
		}
	} 
	else
	{
		/* 	We only support regular files and block devices. */
		if (! S_ISREG(statbuf.st_mode) && ! S_ISBLK(statbuf.st_mode))
		{
			set_error (self, _("Not a regular file or block device"));
			return -1;
		}

		fd = open (path, O_RDWR);

		if (fd < 0) {
			errno = 0;
			fd = open (path, O_RDONLY);
			if (fd < 0) {
				set_error (self, _("Unable to open file for reading"));
				return -1;
			}
		}
	}
	return fd;
}

/* CONSTRUCTORS AND DESTRUCTORS */

static void
hex_buffer_direct_init (HexBufferDirect *self)
{
	self->fd = -1;
	self->changes = g_hash_table_new_full (g_int64_hash, g_int64_equal,
			g_free, g_free);
}

static void
hex_buffer_direct_dispose (GObject *gobject)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT(gobject);

	/* chain up */
	G_OBJECT_CLASS(hex_buffer_direct_parent_class)->dispose (gobject);
}

static void
hex_buffer_direct_finalize (GObject *gobject)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT(gobject);

	if (self->fd >= 0)
	{
		close (self->fd);
	}
	g_free (self->path);

	/* chain up */
	G_OBJECT_CLASS(hex_buffer_direct_parent_class)->finalize (gobject);
}

static void
hex_buffer_direct_class_init (HexBufferDirectClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	
	gobject_class->finalize = hex_buffer_direct_finalize;
	gobject_class->dispose = hex_buffer_direct_dispose;

	gobject_class->set_property = hex_buffer_direct_set_property;
	gobject_class->get_property = hex_buffer_direct_get_property;

	g_object_class_override_property (gobject_class, PROP_FILE, "file");
}

/* INTERFACE - VFUNC IMPLEMENTATIONS */

static gint64
hex_buffer_direct_get_payload_size (HexBuffer *buf)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (buf);

	return self->payload;
}

static char *
hex_buffer_direct_get_data (HexBuffer *buf,
		gint64 offset,
		size_t len)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (buf);
	char *data;

	g_return_val_if_fail (self->fd != -1, NULL);

	data = get_file_data (self, offset, len);

	if (! data)
	{
		return NULL;
	}

	for (size_t i = 0; i < len; ++i)
	{
		char *cp;
		gint64 loc = offset + i;

		cp = g_hash_table_lookup (self->changes, &loc);
		if (cp)
		{
			data[i] = *cp;
		}
	}

	return data;
}

static char
hex_buffer_direct_get_byte (HexBuffer *buf,
		gint64 offset)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (buf);
	char *cp = NULL;

	cp = hex_buffer_direct_get_data (buf, offset, 1);

	if (cp)
		return cp[0];
	else
		return 0;
}

/* transfer: none */
static GFile *
hex_buffer_direct_get_file (HexBuffer *buf)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (buf);

	return self->file;
}


static gboolean
hex_buffer_direct_set_file (HexBuffer *buf, GFile *file)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (buf);
	const char *file_path;

	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	file_path = g_file_peek_path (file);
	if (! file_path)
	{
		set_error (self, _(invalid_path_msg));
		return FALSE;
	}

	self->file = file;
	self->path = g_strdup (file_path);
	g_object_notify (G_OBJECT(self), "file");
	return TRUE;
}

static gboolean
hex_buffer_direct_read (HexBuffer *buf)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (buf);
	gint64 bytes = 0;
	int tmp_fd;

	g_return_val_if_fail (G_IS_FILE (self->file), FALSE);

	const char *file_path = g_file_peek_path (self->file);

	if (! file_path)
	{
		set_error (self, _(invalid_path_msg));
		return FALSE;
	}

	tmp_fd = create_fd_from_path (self, file_path);
	if (tmp_fd < 0)
	{
		set_error (self, _("Unable to read file"));
		return FALSE;
	}

	/* will only return > 0 for a regular file. */
	bytes = hex_buffer_util_get_file_size (self->file);

	if (! bytes)	/* block device */
	{
		gint64 block_file_size;

		if (ioctl (tmp_fd, BLKGETSIZE64, &block_file_size) != 0)
		{
			set_error (self, _("Error attempting to read block device"));
			return FALSE;
		}
		bytes = block_file_size;
	}

	self->payload = self->clean_bytes = bytes;

	self->fd = tmp_fd;

	return TRUE;
}

static gboolean
hex_buffer_direct_read_finish (HexBuffer *buf,
		GAsyncResult *result,
		GError **error)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (buf);

	g_return_val_if_fail (g_task_is_valid (result, G_OBJECT(self)), FALSE);

	return g_task_propagate_boolean (G_TASK(result), error);
}

static void
hex_buffer_direct_read_thread (GTask *task,
		gpointer source_object,
		gpointer task_data,
		GCancellable *cancellable)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (source_object);
	gboolean success;

	success = hex_buffer_direct_read (HEX_BUFFER(self));
	if (success)
		g_task_return_boolean (task, TRUE);
	else
		g_task_return_error (task, self->error);
}

static void
hex_buffer_direct_read_async (HexBuffer *buf,
		GCancellable *cancellable,
		GAsyncReadyCallback callback,
		gpointer user_data)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (buf);
	GTask *task;

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_run_in_thread (task, hex_buffer_direct_read_thread);
	g_object_unref (task);	/* _run_in_thread takes a ref */
}

static gboolean
hex_buffer_direct_set_data (HexBuffer *buf,
		gint64 offset,
		size_t len,
		size_t rep_len,
		char *data)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (buf);

	if (rep_len != len)
	{
		g_debug ("%s: rep_len != len; returning false", __func__);
		return FALSE;
	}

	for (size_t i = 0; i < len; ++i)
	{
		gboolean retval;
		gint64 *ip = g_new (gint64, 1);
		char *cp = g_new (char, 1);

		*ip = offset + i;
		*cp = data[i];

		retval = g_hash_table_replace (self->changes, ip, cp);

		if (! retval)	/* key already existed; replace */
		{
			char *tmp = NULL;

			tmp = get_file_data (self, offset, 1);

			if (*tmp == *cp) {
				g_hash_table_remove (self->changes, ip);
			}
			g_free (tmp);
		}
	}
	return TRUE;
}

static gboolean
hex_buffer_direct_write_to_file (HexBuffer *buf,
		GFile *file)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (buf);
	guint len;
	gint64 **keys;

	g_return_val_if_fail (self->fd != -1, FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	errno = 0;
	if (g_strcmp0 (self->path, g_file_peek_path (file)) != 0)
	{
		set_error (self, _("With direct-write mode, you cannot save a file "
					"to a path other than its originating path"));
		return FALSE;
	}

	keys = (gint64 **)g_hash_table_get_keys_as_array (self->changes, &len);

	/* FIXME - very inefficient - we should at least implement a sorter
	 * function that puts the changes in order, and merges changes that
	 * are adjacent to one another.
	 */
	for (guint i = 0; i < len; ++i)
	{
		ssize_t nwritten;
		char *cp = g_hash_table_lookup (self->changes, keys[i]);
		off_t offset, new_offset;

		offset = *keys[i];
		new_offset = lseek (self->fd, offset, SEEK_SET);
		g_assert (offset == new_offset);

		errno = 0;
		nwritten = write (self->fd, cp, 1);
		if (nwritten != 1)
		{
			set_error (self, _("Error writing changes to file"));
			return FALSE;
		}
	}
	g_hash_table_remove_all (self->changes);
	return TRUE;
}

static gboolean
hex_buffer_direct_write_to_file_finish (HexBuffer *buf,
		GAsyncResult *result,
		GError **error)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (buf);

	g_return_val_if_fail (g_task_is_valid (result, G_OBJECT(self)), FALSE);

	return g_task_propagate_boolean (G_TASK(result), error);
}

static void
hex_buffer_direct_write_thread (GTask *task,
		gpointer source_object,
		gpointer task_data,
		GCancellable *cancellable)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (source_object);
	GFile *file = G_FILE (task_data);
	gboolean success;

	success = hex_buffer_direct_write_to_file (HEX_BUFFER(self), file);
	if (success)
		g_task_return_boolean (task, TRUE);
	else
		g_task_return_error (task, self->error);
}

static void
hex_buffer_direct_write_to_file_async (HexBuffer *buf,
		GFile *file,
		GCancellable *cancellable,
		GAsyncReadyCallback callback,
		gpointer user_data)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT (buf);
	GTask *task;

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_task_data (task, file, NULL);
	g_task_run_in_thread (task, hex_buffer_direct_write_thread);
	g_object_unref (task);	/* _run_in_thread takes a ref */
}

/* PUBLIC FUNCTIONS */

/**
 * hex_buffer_direct_new:
 * @file: a #GFile pointing to a valid file on the system
 *
 * Create a new #HexBufferDirect object.
 *
 * Returns: a new #HexBufferDirect object, automatically cast to a #HexBuffer
 * type, or %NULL if the operation failed.
 */
HexBuffer *
hex_buffer_direct_new (GFile *file)
{
	HexBufferDirect *self = g_object_new (HEX_TYPE_BUFFER_DIRECT, NULL);

	if (file)
	{
		/* If a path is provided but it can't be set, nullify the object */
		if (! hex_buffer_direct_set_file (HEX_BUFFER(self), file))
			g_clear_object (&self);
	}

	if (self)
		return HEX_BUFFER(self);
	else
		return NULL;
}

/* INTERFACE IMPLEMENTATION FUNCTIONS */

static void
hex_buffer_direct_iface_init (HexBufferInterface *iface)
{
	iface->get_data = hex_buffer_direct_get_data;
	iface->get_byte = hex_buffer_direct_get_byte;
	iface->set_data = hex_buffer_direct_set_data;
	iface->get_file = hex_buffer_direct_get_file;
	iface->set_file = hex_buffer_direct_set_file;
	iface->read = hex_buffer_direct_read;
	iface->read_async = hex_buffer_direct_read_async;
	iface->read_finish = hex_buffer_direct_read_finish;
	iface->write_to_file = hex_buffer_direct_write_to_file;
	iface->write_to_file_async = hex_buffer_direct_write_to_file_async;
	iface->write_to_file_finish = hex_buffer_direct_write_to_file_finish;
	iface->get_payload_size = hex_buffer_direct_get_payload_size;
}
