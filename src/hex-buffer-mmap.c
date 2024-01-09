/* vim: ts=4 sw=4 colorcolumn=80                                                
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- *
 */
/* hex-document-mmap.c - `mmap` implementation of the HexBuffer iface.
 *
 * Based on code from aoeui, Copyright © 2007, 2008 Peter Klausler,
 * licensed by the author/copyright-holder under GPLv2 only.
 *
 * Source as adapted herein licensed for GHex to GPLv2+ with written
 * permission from Peter Klausler dated December 13, 2021 (see
 * associated git log).
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

#include "hex-buffer-mmap.h"

#define HEX_BUFFER_MMAP_ERROR hex_buffer_mmap_error_quark ()
GQuark
hex_buffer_mmap_error_quark (void)
{
  return g_quark_from_static_string ("hex-buffer-mmap-error-quark");
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
 * HexBufferMmap:
 * 
 * #HexBufferMmap is an object implementing the [iface@Hex.Buffer] interface,
 * allowing it to be used as a #HexBuffer backend to be used with
 * [class@Hex.Document].
 *
 * Unlike the [class@Hex.BufferMalloc] object, which replicates the legacy
 * backend of GHex, #HexBufferMmap allows for files to be memory-mapped
 * by the operating system when being read. This can make files take a bit
 * longer to load, but once loaded will work much faster and more reliably
 * with very large files.
 *
 * #HexBufferMmap uses the POSIX mmap() function at the backend, which
 * requires a POSIX system, and also depends on the mremap() function
 * being present. If the required headers and functions are not found at
 * compile-time, this backend will not be built, and #HexBufferMalloc can
 * be used as a fallback.
 */
struct _HexBufferMmap
{
	GObject parent_instance;

	GFile *file;
	GError *error;		/* no custom codes; use codes from errno */
	int last_errno;		/* cache in case we need to re-report errno error. */

	char *data;			/* buffer for modification and info */
	gint64 payload;
	gint64 mapped;
	size_t gap;
	char *tmpfile_path;	/* path to buffer tmpfile in mkstemp format */
	int fd;				/* file descriptor of tmpfile. */

	char *clean;		/* unmodified content, mmap'ed */
	gint64 clean_bytes;
	int clean_fd;

	size_t pagesize;	/* is only fetched once and cached. */
};

static void hex_buffer_mmap_iface_init (HexBufferInterface *iface);

G_DEFINE_TYPE_WITH_CODE (HexBufferMmap, hex_buffer_mmap, G_TYPE_OBJECT,
		G_IMPLEMENT_INTERFACE (HEX_TYPE_BUFFER, hex_buffer_mmap_iface_init))

/* FORWARD DECLARATIONS */
	
static gboolean	hex_buffer_mmap_set_file (HexBuffer *buf, GFile *file);
static GFile *	hex_buffer_mmap_get_file (HexBuffer *buf);

/* PROPERTIES - GETTERS AND SETTERS */

static void
hex_buffer_mmap_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP(object);
	HexBuffer *buf = HEX_BUFFER(object);

	switch (property_id)
	{
		case PROP_FILE:
			hex_buffer_mmap_set_file (buf, g_value_get_object (value));
			break;

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_buffer_mmap_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP(object);
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

static void
clear_tmpfile_path (HexBufferMmap *self)
{
	if (! self->tmpfile_path)
		return;

	unlink (self->tmpfile_path);
	g_clear_pointer (&self->tmpfile_path, g_free);
}

/* Helper wrapper for g_set_error and to cache errno */
static void
set_error (HexBufferMmap *self, const char *blurb)
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
			HEX_BUFFER_MMAP_ERROR,
			errno,
			"%s",
			message);

	if (errno)
		self->last_errno = errno;

	g_free (message);
}

static inline size_t
buffer_gap_bytes (HexBufferMmap *self)
{
	return self->mapped - self->payload;
}

/* CONSTRUCTORS AND DESTRUCTORS */

static void
hex_buffer_mmap_init (HexBufferMmap *self)
{
	self->pagesize = getpagesize ();
	self->fd = -1;
}

static void
hex_buffer_mmap_dispose (GObject *gobject)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (gobject);

	/* chain up */
	G_OBJECT_CLASS(hex_buffer_mmap_parent_class)->dispose (gobject);
}

static void
hex_buffer_mmap_finalize (GObject *gobject)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (gobject);

	munmap (self->data, self->mapped);
	munmap (self->clean, self->clean_bytes);

	if (self->fd >= 0)
	{
		close (self->fd);
		/* this should happen previously, but it's harmless to run it again
		 * in finalize in case it didn't happen for some reason
		 */
		clear_tmpfile_path (self);
	}

	/* This will either be spuriously still allocated and non-null (highly
	 * unlikely or null, so safe to call this in any event. */
	g_free (self->tmpfile_path);

	/* chain up */
	G_OBJECT_CLASS(hex_buffer_mmap_parent_class)->finalize (gobject);
}

static void
hex_buffer_mmap_class_init (HexBufferMmapClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	
	gobject_class->finalize = hex_buffer_mmap_finalize;
	gobject_class->dispose = hex_buffer_mmap_dispose;

	gobject_class->set_property = hex_buffer_mmap_set_property;
	gobject_class->get_property = hex_buffer_mmap_get_property;

	g_object_class_override_property (gobject_class, PROP_FILE, "file");
}

static void
hex_buffer_mmap_place_gap (HexBufferMmap *self, gint64 offset)
{
	g_return_if_fail (HEX_IS_BUFFER_MMAP (self));

	size_t gapsize = buffer_gap_bytes (self);

	if (offset > self->payload)
		offset = self->payload;

	if (offset <= self->gap)
		memmove (self->data + offset + gapsize,
				self->data + offset,
				self->gap - offset);
	else
		memmove (self->data + self->gap,
			self->data + self->gap + gapsize,
			offset - self->gap);

	self->gap = offset;

	if (self->fd >= 0 && gapsize)
		memset (self->data + self->gap, ' ', gapsize);
}

static void
hex_buffer_mmap_resize (HexBufferMmap *self, gint64 payload_bytes)
{
	void *p;
	char *old = self->data;
	int fd;
	int mapflags = 0;
	gint64 map_bytes = payload_bytes;

	g_return_if_fail (HEX_IS_BUFFER_MMAP (self));

	/* Whole pages, with extras as size increases */
	map_bytes += self->pagesize - 1;
	map_bytes /= self->pagesize;
	map_bytes *= 11;
	map_bytes /= 10;
	map_bytes *= self->pagesize;

	if (map_bytes < self->mapped)
		munmap (old + map_bytes, self->mapped - map_bytes);

	if (self->fd >= 0  &&  map_bytes != self->mapped)
	{
		errno = 0;
		if (ftruncate (self->fd, map_bytes))
		{
			char *errmsg = g_strdup_printf (
					_("Could not adjust %s from %lu to %lu bytes"),
					self->tmpfile_path, (long)self->mapped, (long)map_bytes);

			set_error (self, errmsg);
			g_free (errmsg);
			return;
		}
	}

	if (map_bytes <= self->mapped)
	{
		self->mapped = map_bytes;
		return;
	}

	if (old)
	{
		/* attempt extension */
		errno = 0;
		p = mremap (old, self->mapped, map_bytes, MREMAP_MAYMOVE);
		if (p != MAP_FAILED)
			goto done;
	}

	/* new/replacement allocation */

	if ((fd = self->fd) >= 0)
	{
		mapflags |= MAP_SHARED;
		if (old) {
			munmap(old, self->mapped);
			old = NULL;
		}
	}
	else
	{
#ifdef MAP_ANONYMOUS
		mapflags |= MAP_ANONYMOUS;
#else
		mapflags |= MAP_ANON;
#endif
		mapflags |= MAP_PRIVATE;
	}

	errno = 0;
	p = mmap (0, map_bytes, PROT_READ|PROT_WRITE, mapflags, fd, 0);
	if (p == MAP_FAILED)
	{
		char *errmsg = g_strdup_printf (
			_("Fatal error: Memory mapping of file (%lu bytes, fd %d) failed"),
				(long)map_bytes, fd);

		set_error (self, errmsg);
		g_free (errmsg);
		return;
	}

	if (old)
	{
		memcpy(p, old, self->payload);
		munmap(old, self->mapped);
	}

done:
	self->data = p;
	self->mapped = map_bytes;
}

#define ADJUST_OFFSET_AND_BYTES				\
	if (offset >= self->payload)			\
		offset = self->payload;				\
	if (offset + bytes > self->payload)		\
		bytes = self->payload - offset;		\

size_t
hex_buffer_mmap_raw (HexBufferMmap *self,
		char **out, gint64 offset, size_t bytes)
{
	g_assert (HEX_IS_BUFFER_MMAP (self));
	
	ADJUST_OFFSET_AND_BYTES

	if (!bytes) {
		*out = NULL;
		return 0;
	}

	if (offset < self->gap  &&  offset + bytes > self->gap)
		hex_buffer_mmap_place_gap (self, offset + bytes);

	*out = self->data + offset;
	if (offset >= self->gap)
		*out += buffer_gap_bytes (self);

	return bytes;
}

size_t
hex_buffer_mmap_copy_data (HexBufferMmap *self,
		void *out, gint64 offset, size_t bytes)
{
	size_t left;

	g_assert (HEX_IS_BUFFER_MMAP (self));

	ADJUST_OFFSET_AND_BYTES

	left = bytes;
	if (offset < self->gap)
	{
		unsigned int before = self->gap - offset;

		if (before > bytes)
			before = bytes;

		memcpy (out, self->data + offset, before);

		out = (char *)out + before;
		offset += before;
		left -= before;

		if (!left)
			return bytes;
	}
	offset += buffer_gap_bytes (self);

	memcpy (out, self->data + offset, left);

	return bytes;
}

size_t
hex_buffer_mmap_delete (HexBufferMmap *self,
		     gint64 offset, size_t bytes)
{
	g_assert (HEX_IS_BUFFER_MMAP (self));

	ADJUST_OFFSET_AND_BYTES

	hex_buffer_mmap_place_gap (self, offset);
	self->payload -= bytes;

	return bytes;
}
#undef ADJUST_OFFSET_AND_BYTES

static size_t
hex_buffer_mmap_insert (HexBufferMmap *self,
		const void *in, gint64 offset, size_t bytes)
{
	g_assert (HEX_IS_BUFFER_MMAP (self));

	if (offset > self->payload)
		offset = self->payload;

	if (bytes > buffer_gap_bytes (self)) {
		hex_buffer_mmap_place_gap (self, self->payload);
		hex_buffer_mmap_resize (self, self->payload + bytes);
	}

	hex_buffer_mmap_place_gap (self, offset);

	if (in)
		memcpy (self->data + offset, in, bytes);
	else
		memset (self->data + offset, 0, bytes);

	self->gap += bytes;
	self->payload += bytes;

	return bytes;
}

size_t
hex_buffer_mmap_move (HexBufferMmap *to,
		gint64 to_offset,
		HexBufferMmap *from,
		gint64 from_offset,
		size_t bytes)
{
	char *raw = NULL;

	bytes = hex_buffer_mmap_raw (from, &raw, from_offset, bytes);
	hex_buffer_mmap_insert (to, raw, to_offset, bytes);

	return hex_buffer_mmap_delete (from, from_offset, bytes);
}

void 
hex_buffer_mmap_snap (HexBufferMmap *self)
{
	g_return_if_fail (HEX_IS_BUFFER_MMAP (self));

	if (self->fd >= 0)
	{
		hex_buffer_mmap_place_gap (self, self->payload);
		if (ftruncate (self->fd, self->payload)) {
			/* don't care */
		}
	}
}

char * hex_buffer_mmap_get_data (HexBuffer *buf,
		gint64 offset,
		size_t len)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (buf);
	char *data;

	data = g_malloc (len);
	hex_buffer_mmap_copy_data (self, data, offset, len);

	return data;
}

char hex_buffer_mmap_get_byte (HexBuffer *buf,
		gint64 offset)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (buf);
	char *cp;

	if (hex_buffer_mmap_raw (self, &cp, offset, 1))
		return cp[0];
	else
		return 0;
}

static gint64
hex_buffer_mmap_get_payload_size (HexBuffer *buf)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (buf);

	return self->payload;
}

/* transfer: none */
static GFile *
hex_buffer_mmap_get_file (HexBuffer *buf)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (buf);

	return self->file;
}

static gboolean
hex_buffer_mmap_set_file (HexBuffer *buf, GFile *file)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (buf);
	const char *file_path;

	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	file_path = g_file_peek_path (file);
	if (! file_path)
	{
		set_error (self, _(invalid_path_msg));
		return FALSE;
	}

	self->file = file;
	g_object_notify (G_OBJECT(self), "file");
	return TRUE;
}

/* helper */
static int
create_fd_from_path (HexBufferMmap *self, const char *path)
{
	int fd = -1;
	struct stat statbuf;

	errno = 0;

	if (stat (path, &statbuf))
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
		/* Non-regular files should be using a different backend, eg, direct.
		 */
		if (!S_ISREG(statbuf.st_mode)) {
			set_error (self, _("Not a regular file"));
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

static void
clear_buffer (HexBufferMmap *self)
{
	if (self->fd)
	{
		close (self->fd);
		clear_tmpfile_path (self);
	}
	if (self->data)
		munmap (self->data, self->mapped);

	self->payload = self->mapped = self->gap = 0;
}

static gboolean
create_buffer (HexBufferMmap *self)
{
	self->tmpfile_path = g_strdup ("hexmmapbufXXXXXX");
	errno = 0;
	self->fd = mkstemp (self->tmpfile_path);
	clear_tmpfile_path (self);

	if (self->fd < 0) {
		set_error (self, _("Failed to open temporary file."));
		return FALSE;
	}

	return TRUE;
}

static gboolean
hex_buffer_mmap_read (HexBuffer *buf)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (buf);
	void *p;
	gint64 bytes = 0;
	gint64 pages;
	const char *file_path;
	int tmp_clean_fd;

	g_return_val_if_fail (G_IS_FILE (self->file), FALSE);

	file_path = g_file_peek_path (self->file);
	if (! file_path)
	{
		set_error (self, _(invalid_path_msg));
		return FALSE;
	}

	bytes = hex_buffer_util_get_file_size (self->file);
	pages = (bytes + self->pagesize - 1) / self->pagesize;

	/* Set up a clean buffer (read-only memory mapped version of O.G. file)
	 */
	if (self->clean)
		munmap (self->clean, self->clean_bytes);

	self->clean_bytes = bytes;
	self->clean = NULL;

	if (! pages)
	{
		set_error (self, _("Error reading file"));
		return FALSE;
	}

	tmp_clean_fd = create_fd_from_path (self, file_path);
	if (tmp_clean_fd < 0)
		return FALSE;

	self->clean_fd = tmp_clean_fd;

	errno = 0;
	p = mmap (0, pages * self->pagesize, PROT_READ, MAP_SHARED,
			self->clean_fd, 0);

	if (p == MAP_FAILED)
	{
		set_error (self, _("An error has occurred"));
		return FALSE;
	}

	self->clean = p;

	/* Create dirty buffer for writing etc. */
	clear_buffer (self);
	create_buffer (self);

	/* FIXME/TODO - sanity check against # of bytes read? */
	hex_buffer_mmap_insert (self, self->clean, 0, self->clean_bytes);

	return TRUE;
}

static gboolean
hex_buffer_mmap_read_finish (HexBuffer *buf,
		GAsyncResult *result,
		GError **error)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (buf);

	g_return_val_if_fail (g_task_is_valid (result, G_OBJECT(self)), FALSE);

	return g_task_propagate_boolean (G_TASK(result), error);
}

static void
hex_buffer_mmap_read_thread (GTask *task,
		gpointer source_object,
		gpointer task_data,
		GCancellable *cancellable)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (source_object);
	gboolean success;

	success = hex_buffer_mmap_read (HEX_BUFFER(self));
	if (success)
		g_task_return_boolean (task, TRUE);
	else
		g_task_return_error (task, self->error);
}

static void
hex_buffer_mmap_read_async (HexBuffer *buf,
		GCancellable *cancellable,
		GAsyncReadyCallback callback,
		gpointer user_data)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (buf);
	GTask *task;

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_run_in_thread (task, hex_buffer_mmap_read_thread);
	g_object_unref (task);	/* _run_in_thread takes a ref */
}

static gboolean hex_buffer_mmap_set_data (HexBuffer *buf,
		gint64 offset,
		size_t len,
		size_t rep_len,
		char *data)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (buf);

	if (offset > self->payload)
	{
		g_debug ("%s: offset greater than payload size; returning.", __func__);
		return FALSE;
	}

	hex_buffer_mmap_insert (self, data, offset, len);
	hex_buffer_mmap_delete (self, offset + len, rep_len);

	return TRUE;
}

static gboolean
hex_buffer_mmap_write_to_file (HexBuffer *buf,
		GFile *file)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (buf);
	char *raw;
	gboolean retval;

	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	hex_buffer_mmap_raw (self, &raw, 0, self->payload);

	g_clear_error (&self->error);
	retval = g_file_replace_contents (file,
		/* const char* contents, */			raw,	
		/* gsize length, */					self->payload,
		/* const char* etag, */				NULL,
		/* gboolean make_backup, */			FALSE,
		/* GFileCreateFlags flags, */		G_FILE_CREATE_NONE,
		/* char** new_etag, */				NULL,
		/* GCancellable* cancellable, */	NULL,
		/* GError** error */				&self->error);

	return retval;
}

static gboolean
hex_buffer_mmap_write_to_file_finish (HexBuffer *buf,
		GAsyncResult *result,
		GError **error)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (buf);

	g_return_val_if_fail (g_task_is_valid (result, G_OBJECT(self)), FALSE);

	return g_task_propagate_boolean (G_TASK(result), error);
}

static void
hex_buffer_mmap_write_thread (GTask *task,
		gpointer source_object,
		gpointer task_data,
		GCancellable *cancellable)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (source_object);
	GFile *file = G_FILE (task_data);
	gboolean success;

	success = hex_buffer_mmap_write_to_file (HEX_BUFFER(self), file);
	if (success)
		g_task_return_boolean (task, TRUE);
	else
		g_task_return_error (task, self->error);
}

static void
hex_buffer_mmap_write_to_file_async (HexBuffer *buf,
		GFile *file,
		GCancellable *cancellable,
		GAsyncReadyCallback callback,
		gpointer user_data)
{
	HexBufferMmap *self = HEX_BUFFER_MMAP (buf);
	GTask *task;

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_task_data (task, file, NULL);
	g_task_run_in_thread (task, hex_buffer_mmap_write_thread);
	g_object_unref (task);	/* _run_in_thread takes a ref */
}

/* PUBLIC FUNCTIONS */

/**
 * hex_buffer_mmap_new:
 * @file: a #GFile pointing to a valid file on the system
 *
 * Create a new #HexBufferMmap object.
 *
 * Returns: a new #HexBufferMmap object, automatically cast to a #HexBuffer
 * type, or %NULL if the operation failed.
 */
HexBuffer *
hex_buffer_mmap_new (GFile *file)
{
	HexBufferMmap *self = g_object_new (HEX_TYPE_BUFFER_MMAP, NULL);

	if (file)
	{
		/* If a path is provided but it can't be set, nullify the object */
		if (! hex_buffer_mmap_set_file (HEX_BUFFER(self), file))
			g_clear_object (&self);
	}

	if (self)
		return HEX_BUFFER(self);
	else
		return NULL;
}

/* INTERFACE IMPLEMENTATION FUNCTIONS */

static void
hex_buffer_mmap_iface_init (HexBufferInterface *iface)
{
	iface->get_data = hex_buffer_mmap_get_data;
	iface->get_byte = hex_buffer_mmap_get_byte;
	iface->set_data = hex_buffer_mmap_set_data;
	iface->get_file = hex_buffer_mmap_get_file;
	iface->set_file = hex_buffer_mmap_set_file;
	iface->read = hex_buffer_mmap_read;
	iface->read_async = hex_buffer_mmap_read_async;
	iface->read_finish = hex_buffer_mmap_read_finish;
	iface->write_to_file = hex_buffer_mmap_write_to_file;
	iface->write_to_file_async = hex_buffer_mmap_write_to_file_async;
	iface->write_to_file_finish = hex_buffer_mmap_write_to_file_finish;
	iface->get_payload_size = hex_buffer_mmap_get_payload_size;
}
