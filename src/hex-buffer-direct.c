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
// FIXME - TEST
#include "hex-buffer-direct-child.h"

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

	int fd;				/* file descriptor for direct read/write */
	gint64 payload;		/* size of the payload */

	gint64 clean_bytes;	/* 'clean' size of the file (no additions or deletions */
	GHashTable *changes;
	
	GSubprocess *sub;
//	GOutputStream *stdin_pipe;
//	GInputStream *stdout_pipe;
//	GDataInputStream *stdout_stream;
//	size_t nread;
};

static void hex_buffer_direct_iface_init (HexBufferInterface *iface);

G_DEFINE_TYPE_WITH_CODE (HexBufferDirect, hex_buffer_direct, G_TYPE_OBJECT,
		G_IMPLEMENT_INTERFACE (HEX_TYPE_BUFFER, hex_buffer_direct_iface_init))

/* FORWARD DECLARATIONS */
	
static gboolean	hex_buffer_direct_set_file (HexBuffer *buf, GFile *file);
static GFile *	hex_buffer_direct_get_file (HexBuffer *buf);
static void		set_error (HexBufferDirect *self, const char *blurb);
static void		setup_subprocess (HexBufferDirect *self);

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

/* < Commands to send to child process >
 * We use the templates below, and set `cmd_gvar = ...` to a GVariant
 * representing the proper command format.
 */

static void
child_terminated_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT(user_data);
	GSubprocess *sub = G_SUBPROCESS(source_object);
	GError *local_error = NULL;
	gboolean retval;

	retval = g_subprocess_wait_finish (sub, res, &self->error);

#if 0
	if (retval)
		g_print ("%s: child terminated successfully\n", __func__);
	else
		g_print ("%s: child terminated with error: %s\n", __func__,
				self->error->message);
#endif

}


#define SEND_CMD_TEMPLATE_PARAMS \
	GVariant *cmd_gvar; \
	GVariant *ret_base_gvar; \
	GVariant *ret_params_gvar; \
	char *cmd_str = NULL; \
	GBytes *cmd_bytes = NULL; \
	gboolean com_retval; \
	GBytes *stdout_buf; \
	char *line; \
	gsize stdout_len; \
	int err_num = 0; \
	char *err_msg = NULL; \
	gboolean was_err = FALSE;

#define SEND_CMD_TEMPLATE_BOILERPLATE \
	cmd_bytes = g_bytes_new (cmd_str, strlen(cmd_str)); \
 \
	setup_subprocess (self); \
/*	g_subprocess_wait_async (self->sub, NULL, child_terminated_cb, self); */ \
 \
	g_clear_error (&self->error); \
	com_retval = g_subprocess_communicate (self->sub, \
                          cmd_bytes, \
						  NULL,	/* cancellable */ \
						  &stdout_buf, \
						  NULL,	/* stderr_buf */ \
                          &self->error); \
	if (! com_retval) \
	{ \
		g_warning ("g_subprocess_communicate failed: %s", self->error->message); \
		goto out; \
	} \
 \
	line = g_bytes_unref_to_data (stdout_buf, &stdout_len); \
	line[stdout_len - 1] =  0; \
 \
	ret_base_gvar = g_variant_parse (NULL, line, NULL, NULL, NULL); \
	if (! ret_base_gvar) \
	{ \
		g_warning ("Failed to parse retval."); \
		goto out; \
	} \
 \
	g_variant_get (ret_base_gvar, FMT_CMD, NULL, &ret_params_gvar); \
	if (! ret_params_gvar) \
	{ \
		g_warning ("No parameters provided or failed to parse parameters."); \
		goto out; \
	}

#define SEND_CMD_TEMPLATE_HANDLE_ERROR \
	if (was_err) \
		set_error (self, err_msg);

#define SEND_CMD_TEMPLATE_BOILERPLATE_CLEANUP \
	g_variant_unref (cmd_gvar); \
	g_free (cmd_str); \
	g_bytes_unref (cmd_bytes);


static gboolean
send_close_cmd (HexBufferDirect *self)
{
	SEND_CMD_TEMPLATE_PARAMS

	gboolean retval;

	cmd_gvar = g_variant_new (FMT_CMD, CLOSE, g_variant_new_boolean (TRUE));
	cmd_str = g_strdup_printf ("%s\n", g_variant_print (cmd_gvar, TRUE));

	SEND_CMD_TEMPLATE_BOILERPLATE

	g_variant_get (ret_params_gvar, RET_FMT_CLOSE,
			&retval, &was_err, &err_num, &err_msg);

	SEND_CMD_TEMPLATE_HANDLE_ERROR

out:
	SEND_CMD_TEMPLATE_BOILERPLATE_CLEANUP

	return retval;
}

static char *
send_read_cmd (HexBufferDirect *self, gint64 off_start, gint64 off_end)
{
	GVariant *cmd_gvar;
	GVariant *ret_base_gvar;
	GVariant *ret_params_gvar;
	char *cmd_str = NULL;
	GBytes *cmd_bytes = NULL;
	gboolean com_retval;
	GBytes *stdout_buf;
	char *line;
	gsize stdout_len;
	int err_num = 0;
	char *err_msg = NULL;
	gboolean was_err = FALSE;

	char *ret_data = NULL;
//	char b = 0;
	gboolean op_ret;
	goffset ret_off;
	GString *gstr = g_string_new (NULL);
	char *stdout_str;
	char **stdout_lines;
//	char *tmp;
	gint64 i;

	g_assert (off_end >= off_start);

	cmd_gvar = g_variant_new (FMT_CMD, OPEN, g_variant_new_string (g_file_peek_path (self->file)));
	g_string_append_printf (gstr, "%s\n", g_variant_print (cmd_gvar, TRUE));
	g_variant_unref (cmd_gvar);

	for (i = 0; i <= off_end - off_start; ++i)
	{
		cmd_gvar = g_variant_new (FMT_CMD, READ, g_variant_new_int64 (off_start + i));
		g_string_append_printf (gstr, "%s\n", g_variant_print (cmd_gvar, TRUE));
		g_variant_unref (cmd_gvar);
	}

	cmd_gvar = g_variant_new (FMT_CMD, CLOSE, g_variant_new_boolean (TRUE));
	g_string_append_printf (gstr, "%s\n", g_variant_print (cmd_gvar, TRUE));
	g_variant_unref (cmd_gvar);

//	cmd_str = g_strdup_printf ("%s\n", g_variant_print (cmd_gvar, TRUE));

	cmd_bytes = g_bytes_new (gstr->str, gstr->len);

	setup_subprocess (self);

	g_clear_error (&self->error);
	com_retval = g_subprocess_communicate (self->sub,
                          cmd_bytes,
						  NULL,	/* cancellable */
						  &stdout_buf,
						  NULL,	/* stderr_buf */
                          &self->error);
	if (! com_retval)
	{
		g_warning ("g_subprocess_communicate failed: %s", self->error->message);
		goto out;
	}

//	cmd_str = g_strdup (gstr->str);

	stdout_str = g_bytes_unref_to_data (stdout_buf, &stdout_len);

	if (!stdout_str)
		goto out;

	stdout_lines = g_strsplit (stdout_str, "\n", -1);

	for (gint j = 0; j < i; ++j)
	{
		char b;

		/* skip first retval for OPEN */
		ret_base_gvar = g_variant_parse (NULL, stdout_lines[j+1], NULL, NULL, NULL);
		if (! ret_base_gvar)
		{
			g_warning ("Failed to parse retval.");
			goto out;
		}

		g_variant_get (ret_base_gvar, FMT_CMD, NULL, &ret_params_gvar);
		if (! ret_params_gvar)
		{
			g_warning ("No parameters provided or failed to parse parameters.");
			goto out;
		}

		g_variant_get (ret_params_gvar, RET_FMT_READ,
				&op_ret, &op_ret, &b, &op_ret, &ret_off, &was_err, &err_num, &err_msg);

		/* Build array to return */
		if (op_ret)
		{
			if (! ret_data)
				ret_data = g_malloc (i);

			ret_data[j] = b;
		}
		else if (was_err)
		{
			set_error (self, err_msg);
		}

		g_clear_pointer (&ret_base_gvar, g_variant_unref);
		g_clear_pointer (&ret_params_gvar, g_variant_unref);
	}

	// TEST
//	g_variant_get (ret_params_gvar, RET_FMT_READ,
//			&op_ret, &op_ret, &retval, &op_ret, &ret_off, &was_err, &err_num, &err_msg);

//	g_variant_get (ret_params_gvar, RET_FMT_READ,
//			&op_ret, &op_ret, &b, &op_ret, &ret_off, &was_err, &err_num, &err_msg);

//	g_debug ("operation: READ - retval: %d - byte: %c - offset: %ld - "
//			"was_err: %d - err_num: %d - err_msg: %s\n",
//			op_ret, b, ret_off, was_err, err_num, err_msg);


out:
	/* free stuff */

	return ret_data;
}


static int
send_open_cmd (HexBufferDirect *self, const char *path)
{
	SEND_CMD_TEMPLATE_PARAMS

	int fd_retval = -1;

	cmd_gvar = g_variant_new (FMT_CMD, OPEN, g_variant_new_string (path));
	cmd_str = g_strdup_printf ("%s\n", g_variant_print (cmd_gvar, TRUE));

	cmd_bytes = g_bytes_new (cmd_str, strlen(cmd_str));

	setup_subprocess (self);
/*	g_subprocess_wait_async (self->sub, NULL, child_terminated_cb, self); */

	g_clear_error (&self->error);
	com_retval = g_subprocess_communicate (self->sub,
                          cmd_bytes,
						  NULL,	/* cancellable */
						  &stdout_buf,
						  NULL,	/* stderr_buf */
                          &self->error);
	if (! com_retval)
	{
		g_warning ("g_subprocess_communicate failed: %s", self->error->message);
		goto out;
	}

	line = g_bytes_unref_to_data (stdout_buf, &stdout_len);
	if (! line)
		goto out;

	line[stdout_len - 1] =  0;

	ret_base_gvar = g_variant_parse (NULL, line, NULL, NULL, NULL);
	if (! ret_base_gvar)
	{
		g_warning ("Failed to parse retval.");
		goto out;
	}

	g_variant_get (ret_base_gvar, FMT_CMD, NULL, &ret_params_gvar);
	if (! ret_params_gvar)
	{
		g_warning ("No parameters provided or failed to parse parameters.");
		goto out;
	}
 


	g_variant_get (ret_params_gvar, RET_FMT_NEW_OPEN,
			&fd_retval, &was_err, &err_num, &err_msg);

	g_debug ("operation: OPEN - fd: %d - was_err: %d - err_num: %d - err_msg: %s\n",
			fd_retval, was_err, err_num, err_msg);

	SEND_CMD_TEMPLATE_HANDLE_ERROR

out:
	SEND_CMD_TEMPLATE_BOILERPLATE_CLEANUP

	return fd_retval;
}

static gboolean
send_write_cmd (HexBufferDirect *self, gint64 offset, char byte)
{
	SEND_CMD_TEMPLATE_PARAMS

	gboolean retval;

	cmd_gvar = g_variant_new (FMT_CMD, WRITE,
			g_variant_new (FMT_WRITE, offset, byte));
	cmd_str = g_strdup_printf ("%s\n", g_variant_print (cmd_gvar, TRUE));

	SEND_CMD_TEMPLATE_BOILERPLATE

	g_variant_get (ret_params_gvar, RET_FMT_WRITE,
			&retval, &was_err, &err_num, &err_msg);

	SEND_CMD_TEMPLATE_HANDLE_ERROR

out:
	SEND_CMD_TEMPLATE_BOILERPLATE_CLEANUP

	return retval;
}

static int 
send_new_cmd (HexBufferDirect *self, const char *path)
{
	SEND_CMD_TEMPLATE_PARAMS

	int fd_retval = -1;

	cmd_gvar = g_variant_new (FMT_CMD, NEW, g_variant_new_string (path));
	cmd_str = g_strdup_printf ("%s\n", g_variant_print (cmd_gvar, TRUE));

	SEND_CMD_TEMPLATE_BOILERPLATE

	g_variant_get (ret_params_gvar, RET_FMT_NEW_OPEN,
			&fd_retval, &was_err, &err_num, &err_msg);

	g_debug ("operation: NEW - fd: %d - was_err: %d - err_num: %d - err_msg: %s\n",
			fd_retval, was_err, err_num, err_msg);

	SEND_CMD_TEMPLATE_HANDLE_ERROR

out:
	SEND_CMD_TEMPLATE_BOILERPLATE_CLEANUP

	return fd_retval;
}

/* </ Commands to send to child process > */

#if 0
static void
stdout_read_line_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT(user_data);
	GDataInputStream *stdout_stream = G_DATA_INPUT_STREAM(source_object);
	char *line;
	GVariant *ret_gvar = NULL;
	GVariant *params = NULL;
	enum Command cmd;

	line = g_data_input_stream_read_line_finish (stdout_stream, res, NULL, NULL);

	ret_gvar = g_variant_parse (NULL, line, NULL, NULL, NULL);

	if (! ret_gvar)
	{
		g_warning ("Failed to parse retval.");
		goto out;
	}

	g_variant_get (ret_gvar, FMT_CMD, &cmd, &params);
	if (! params)
	{
		g_warning ("No parameters provided or failed to parse parameters.");
		goto out;
	}

	switch (cmd)
	{
		case NEW:
		{
			int fd = -1;
			int err_num = 0;
			char *err_msg = NULL;
			gboolean was_err = FALSE;

			g_variant_get (params, RET_FMT_NEW_OPEN,
					&fd, &was_err, &err_num, &err_msg);

			g_debug ("operation: NEW - fd: %d - was_err: %d - err_num: %d - err_msg: %s\n",
					fd, was_err, err_num, err_msg);

			if (was_err)
			{
				/* report error */
			}
			else
			{
				/* do stuff */
			}

		}
			break;

		case OPEN:
		{
			int fd = -1;
			int err_num = 0;
			char *err_msg = NULL;
			gboolean was_err = FALSE;

			g_variant_get (params, RET_FMT_NEW_OPEN,
					&fd, &was_err, &err_num, &err_msg);

			g_debug ("operation: OPEN - fd: %d - was_err: %d - err_num: %d - err_msg: %s\n",
					fd, was_err, err_num, err_msg);

			if (was_err)
			{
				/* report error */
			}
			else
			{
				/* do stuff */
			}
		}
			break;

		case CLOSE:
		{
			gboolean op_ret = FALSE;
			int err_num = 0;
			char *err_msg = NULL;
			gboolean was_err = FALSE;

			g_variant_get (params, RET_FMT_CLOSE,
					&op_ret, &was_err, &err_num, &err_msg);

			g_debug ("operation: CLOSE - retval: %d - was_err: %d - err_num: %d - err_msg: %s\n",
					op_ret, was_err, err_num, err_msg);

			if (! op_ret || was_err)
			{
				/* report error */
			}
			else
			{
				/* do stuff */
			}

		}
			break;

		case READ:
		{
			gboolean op_ret = FALSE;
			char b;
			gint64 off;
			gboolean was_err = FALSE;
			int err_num = 0;
			char *err_msg = NULL;

			g_variant_get (params, RET_FMT_READ,
					&op_ret, &op_ret, &b, &op_ret, &off, &was_err, &err_num, &err_msg);

			g_debug ("operation: READ - retval: %d - byte: %c - offset: %ld - was_err: %d - err_num: %d - err_msg: %s\n",
					op_ret, b, off, was_err, err_num, err_msg);

			if (was_err)
			{
				/* report error */
			}
			else
			{
				/* do stuff */
			}
		}
			break;

		case WRITE:
		{
			gboolean op_ret = FALSE;
			gboolean was_err = FALSE;
			int err_num = 0;
			char *err_msg = NULL;

			g_variant_get (params, RET_FMT_WRITE, &op_ret, &was_err, &err_num, &err_msg);

			g_debug ("operation: WRITE - retval: %d - was_err: %d - err_num: %d - err_msg: %s\n",
					op_ret, was_err, err_num, err_msg);

			if (was_err)
			{
				/* report error */
			}
			else
			{
				/* do stuff */
			}
		}
			break;

		default:
			g_warning ("Invalid retval. Ignoring.");
			break;
	}

out:
	g_variant_unref (ret_gvar);

	g_data_input_stream_read_line_async (stdout_stream,
			G_PRIORITY_DEFAULT,
			NULL, /* cancellable */
			stdout_read_line_cb,
			self); /* data */
}
#endif

#if 0
/* mostly a helper for _get_data and _set_data */
static char *
get_file_data (HexBufferDirect *self,
		gint64 offset,
		size_t len)
{
	char *data = NULL;
	off_t new_offset;
	ssize_t nread;

	data = g_malloc (len);
	new_offset = lseek (self->fd, offset, SEEK_SET);

	g_assert (offset == new_offset);

	errno = 0;
	nread = read (self->fd, data, len);

	/* FIXME/TODO - test that if nread is less than amount requested, that it
	 * marries up with amount left in payload */
	if (nread == -1)
	{
		set_error (self, _("Failed to read data from file."));
		g_clear_pointer (&data, g_free);
	}

	return data;
}
#endif

static int
create_fd_from_path (HexBufferDirect *self, const char *path)
{
	int fd = -1;
	struct stat statbuf;

	errno = 0;

	if (stat (path, &statbuf) != 0)		/* new file */
	{
		if (errno != ENOENT) {
			set_error (self,
				_("Unable to retrieve file or directory information"));
			return -1;
		}

		fd = send_new_cmd (self, path);
	} 
	else	/* open */
	{
		/* 	We only support regular files and block devices. */
		if (! S_ISREG(statbuf.st_mode) && ! S_ISBLK(statbuf.st_mode))
		{
			set_error (self, _("Not a regular file or block device"));
			return -1;
		}

		fd = send_open_cmd (self, path);
	}

	return fd;
}

static void
setup_subprocess (HexBufferDirect *self)
{
	g_clear_object (&self->sub);
	g_clear_error (&self->error);

	// TEST
#define PACKAGE_LIBEXECDIR "/home/logan/git/ghex/src"
	self->sub = g_subprocess_new (
			G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE |
			G_SUBPROCESS_FLAGS_STDERR_PIPE | G_SUBPROCESS_FLAGS_INHERIT_FDS,
			&self->error,
			"pkexec",
			PACKAGE_LIBEXECDIR "/gvar",	// FIXME - name, path, etc.
			NULL);

	// FIXME - shouldn't gerror-out here.
	if (! self->sub)
		g_error ("%s: Initializing subprocess object failed!", __func__);
}

/* CONSTRUCTORS AND DESTRUCTORS */

static void
hex_buffer_direct_init (HexBufferDirect *self)
{
	GInputStream *stdout_pipe;

	self->fd = -1;
	self->changes = g_hash_table_new_full (g_int64_hash, g_int64_equal,
			g_free, g_free);

	setup_subprocess (self);
}

static void
hex_buffer_direct_dispose (GObject *gobject)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT(gobject);

	g_clear_object (&self->sub);

	/* chain up */
	G_OBJECT_CLASS(hex_buffer_direct_parent_class)->dispose (gobject);
}

static void
hex_buffer_direct_finalize (GObject *gobject)
{
	HexBufferDirect *self = HEX_BUFFER_DIRECT(gobject);

	if (self->fd >= 0)
	{
		send_close_cmd (self);
	}

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

	data = send_read_cmd (self, offset, offset + len-1);
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
			g_debug ("found change - swapping byte at: %ld", loc);
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
	char *data;
	char b = 0;

	data = send_read_cmd (self, offset, offset);

	if (data)
	{
		b = data[0];
		g_free (data);
	}

	return b;
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

//	tmp_fd = create_fd_from_path (self, file_path);
//	if (tmp_fd < 0)
//	{
//		set_error (self, _("Unable to read file"));
//		return FALSE;
//	}

	/* will only return > 0 for a regular file. */
	bytes = hex_buffer_util_get_file_size (self->file);

	if (! bytes)	/* block device */
	{
		gint64 block_file_size;

//		if (ioctl (tmp_fd, BLKGETSIZE64, &block_file_size) != 0)
//		{
//			set_error (self, _("Error attempting to read block device"));
//			return FALSE;
//		}
		g_debug ("DOING DUMB TEST NOW"); block_file_size = 100;
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

		// TEST
		if (retval) /* key did not exist yet */
		{
			g_debug ("key did not exist yet");
		}
		else
		{
			char *tmp = NULL;

			g_debug ("key already existed; replaced");

			// TEST
			tmp = send_read_cmd (self, offset, offset);

			if (*tmp == *cp)
			{
				g_debug ("key value back to what O.G. file was. Removing hash entry.");
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

	keys = (gint64 **)g_hash_table_get_keys_as_array (self->changes, &len);

	/* FIXME - very inefficient - we should at least implement a sorter
	 * function that puts the changes in order, and merges changes that
	 * are adjacent to one another.
	 */
	for (guint i = 0; i < len; ++i)
	{
		gboolean retval;
		char *cp = g_hash_table_lookup (self->changes, keys[i]);
		off_t offset = *keys[i];

		retval = send_write_cmd (self, offset, *cp);

		if (! retval)
			return FALSE;
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
