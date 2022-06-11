#include "hex-buffer-direct-child.h"

static int FD = -1;
static GVariantType *TYPE_BASE;

/* nb: retval of FALSE here implies a *parsing* error; if the parsing was
 * handled successfully but the *operation* failed (eg, 'write' command was
 * properly formed, but the write operation itself failed), we still return
 * TRUE.
 */
static gboolean
parse_input (const char *input)
{
	enum Command cmd;
	GVariant *cmd_gvar = NULL;
	GVariant *params = NULL;
	GVariant *ret_gvar = NULL;
	gboolean retval = TRUE;
	gboolean op_ret = TRUE;
	char *gvar_ret_str = NULL;
	char *errmsg = NULL;

	cmd_gvar = g_variant_parse (TYPE_BASE, input, NULL, NULL, NULL);
	if (! cmd_gvar)
	{
		g_warning ("Failed to parse command.");
		retval = FALSE;
		goto out;
	}

	g_variant_get (cmd_gvar, FMT_CMD, &cmd, &params);

	if (! params)
	{
		g_warning ("No parameters provided or failed to parse parameters.");
		retval = FALSE;
		goto out;
	}

	switch (cmd)
	{
		case NEW: case OPEN:
		{
			const char *path = g_variant_get_string (params, NULL);

			if (FD != -1)
			{
				errmsg = _("Trying to work on multiple file descriptors at once. "
						"You should not have received this error; please report a bug.");
				op_ret = FALSE;
				goto new_open_out;
			}

			if (cmd == NEW)
			{
				errno = 0;
				FD = open (path, O_CREAT|O_TRUNC|O_RDWR,
						S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

				if (FD < 0)
				{
					errmsg = _("Unable to create file");
					op_ret = FALSE;
				}
			}
			else	/* OPEN */
			{
				errno = 0;
				FD = open (path, O_RDWR);

				if (FD < 0)
				{
					errno = 0;
					FD = open (path, O_RDONLY);
				}
				if (FD < 0)
				{
					errmsg = _("Unable to open file for reading");
					op_ret = FALSE;
				}
			}

			new_open_out:
			ret_gvar = g_variant_new (FMT_CMD, cmd,
					g_variant_new (RET_FMT_NEW_OPEN, FD, !op_ret, errno, errmsg));
			gvar_ret_str = g_variant_print (ret_gvar, TRUE);
			g_print ("%s\n", gvar_ret_str);
		}
			break;

		case CLOSE:
		{
			int close_retval;

			if (FD == -1)
			{
				errmsg = _("Cannot close a file without opening first. "
						"You should not have received this error; please report a bug.");
				op_ret = FALSE;
				goto close_out;
			}

			errno = 0;
			close_retval = close (FD);
			if (close_retval != 0)
			{
				errmsg = _("Failed to close file");
				op_ret = FALSE;
			}
		
			close_out:
			ret_gvar = g_variant_new (FMT_CMD, cmd,
					g_variant_new (RET_FMT_CLOSE, op_ret, !op_ret, errno, errmsg));
			gvar_ret_str = g_variant_print (ret_gvar, TRUE);
			g_print ("%s\n", gvar_ret_str);
		}
			break;

		case READ:
		{
			gint64 off;
			char b = 0;
			ssize_t nread;

			if (FD == -1)
			{
				errmsg = _("Cannot read from a file without opening first. "
						"You should not have received this error; please report a bug.");
				op_ret = FALSE;
				goto read_out;
			}

			g_variant_get (params, FMT_READ, &off);

			/* FIXME/TODO - check retval */
			lseek (FD, off, SEEK_SET);

			errno = 0;
			nread = read (FD, &b, 1);

			if (nread != 1)
			{
				errmsg = _("Failed to read data from file.");
				op_ret = FALSE;
			}

			read_out:
			ret_gvar = g_variant_new (FMT_CMD, cmd,
					g_variant_new (RET_FMT_READ,
						op_ret, op_ret, b, op_ret, off, !op_ret, errno, errmsg));
			gvar_ret_str = g_variant_print (ret_gvar, TRUE);
			g_print ("%s\n", gvar_ret_str);
		}
			break;

		case WRITE:
		{
			gint64 off;
			char b;
			ssize_t nwritten;
			off_t new_off;

			if (FD == -1)
			{
				errmsg = _("Cannot write to a file without opening first. "
						"You should not have received this error; please report a bug.");
				op_ret = FALSE;
				goto write_out;
			}

			g_variant_get (params, FMT_WRITE, &off, &b);

			errno = 0;
			new_off = lseek (FD, off, SEEK_SET);
			if (off != new_off)
			{
				errmsg = _("Critical error: read and write offsets do not agree");
				op_ret = FALSE;
			}

			errno = 0;
			nwritten = write (FD, &b, 1);
			if (nwritten != 1)
			{
				errmsg = _("Error writing changes to file");
				op_ret = FALSE;
			}

			write_out:
			ret_gvar = g_variant_new (FMT_CMD, cmd,
					g_variant_new (RET_FMT_WRITE, op_ret, !op_ret, errno, errmsg));
			gvar_ret_str = g_variant_print (ret_gvar, TRUE);
			g_print ("%s\n", gvar_ret_str);
		}
			break;

		default:
			g_warning ("Invalid command. Ignoring.");
			break;
	}
out:
	g_clear_pointer (&cmd_gvar, g_variant_unref);
	g_clear_pointer (&params, g_variant_unref);
	g_clear_pointer (&ret_gvar, g_variant_unref);
	g_free (gvar_ret_str);

	return retval;
}
#undef FD_SANITY_CHECK

int main (void)
{
	char buf[BUFSIZ];

	TYPE_BASE = g_variant_type_new (FMT_CMD);

	while (fgets (buf, BUFSIZ, stdin) != NULL)
	{
		parse_input (g_strchomp (buf));
	}

	g_variant_type_free (TYPE_BASE);

	return 0;
}
