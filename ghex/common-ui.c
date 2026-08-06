/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* common-ui.c - Common UI utility functions

   Copyright © 1998 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021-2022 Logan Rathbone <poprocks@gmail.com>

   GHex is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   GHex is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GHex; see the file COPYING.
   If not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Original Author: Jaka Mocnik <jaka@gnu.org>
*/

#include "common-ui.h"

#include "config.h"

#if 0
/* helper for common_set_gtkhex_font_from_settings.
 */
static void
set_css_provider_font_from_settings (void)
{
	PangoFontDescription *desc;
	char *css_str;

	desc = pango_font_description_from_string (def_font_name);
	css_str = pango_font_description_to_css (desc, ".hex");

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_css_provider_load_from_data (global_provider,
			css_str, -1);
	G_GNUC_END_IGNORE_DEPRECATIONS

	g_free (css_str);
}

void
common_set_gtkhex_font_from_settings (HexWidget *gh)
{
	g_return_if_fail (HEX_IS_WIDGET(gh));
	g_return_if_fail (GTK_IS_STYLE_PROVIDER(global_provider));

	/* Ensure global provider and settings are in sync font-wise. */
	set_css_provider_font_from_settings ();

	gtk_style_context_add_provider_for_display (gdk_display_get_default (),
			GTK_STYLE_PROVIDER (global_provider),
			GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);
}
#endif

void
ghex_display_dialog (GtkWindow *parent, const char *msg)
{
	AdwDialog *dialog;

	g_return_if_fail (GTK_IS_WINDOW(parent));
	g_return_if_fail (msg);

	dialog = adw_alert_dialog_new (NULL, msg);
	adw_alert_dialog_add_response (ADW_ALERT_DIALOG(dialog), "close", _("Close"));
	adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG(dialog), "close");
	adw_dialog_present (dialog, GTK_WIDGET(parent));
}

/* transfer full */
char *
common_get_ui_basename (HexDocument *doc)
{
	char *retval = NULL;
	GFile *gfile = NULL;
	GFileInfo *info = NULL;
	GError *local_error = NULL;

	g_return_val_if_fail (HEX_IS_DOCUMENT (doc), NULL);

	gfile = hex_document_get_file (doc);

	if (!gfile)
		return NULL;

	info = g_file_query_info (gfile, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
			G_FILE_QUERY_INFO_NONE, NULL, &local_error);

	if (local_error)
	{
		g_debug ("%s: Returning null due to error: %s",
				__func__, local_error->message);
		g_assert (!retval);
		goto out;
	}

	retval = g_strdup (g_file_info_get_display_name (info));

out:
	g_clear_error (&local_error);
	g_object_unref (info);
	return retval;
}
