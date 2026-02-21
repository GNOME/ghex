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

#include <config.h>

/* helper for common_set_gtkhex_font_from_settings.
 * 
 * This function was written by Matthias Clasen and is included somewhere in
 * the GTK source tree.. I believe it is also included in libdazzle, but I
 * didn't want to include a whole dependency just for one function. LGPL, but
 * credit where credit is due!
 */
static char *
pango_font_description_to_css (PangoFontDescription *desc,
		const char *selector)
{
	GString *s;
	PangoFontMask set;
	char *tmp;

	g_assert (selector);

	tmp = g_strdup_printf ("%s { ", selector);
	s = g_string_new (tmp);
	g_free (tmp);

	set = pango_font_description_get_set_fields (desc);
	if (set & PANGO_FONT_MASK_FAMILY)
	{
		g_string_append (s, "font-family: ");
		g_string_append (s, pango_font_description_get_family (desc));
		g_string_append (s, "; ");
	}
	if (set & PANGO_FONT_MASK_STYLE)
	{
		switch (pango_font_description_get_style (desc))
		{
			case PANGO_STYLE_NORMAL:
				g_string_append (s, "font-style: normal; ");
				break;
			case PANGO_STYLE_OBLIQUE:
				g_string_append (s, "font-style: oblique; ");
				break;
			case PANGO_STYLE_ITALIC:
				g_string_append (s, "font-style: italic; ");
				break;
		}
	}
	if (set & PANGO_FONT_MASK_VARIANT)
	{
		switch (pango_font_description_get_variant (desc))
		{
			case PANGO_VARIANT_NORMAL:
				g_string_append (s, "font-variant: normal; ");
				break;
			case PANGO_VARIANT_SMALL_CAPS:
				g_string_append (s, "font-variant: small-caps; ");
				break;
			default:
				break;
		}
	}
	if (set & PANGO_FONT_MASK_WEIGHT)
	{
		switch (pango_font_description_get_weight (desc))
		{
			case PANGO_WEIGHT_THIN:
				g_string_append (s, "font-weight: 100; ");
				break;
			case PANGO_WEIGHT_ULTRALIGHT:
				g_string_append (s, "font-weight: 200; ");
				break;
			case PANGO_WEIGHT_LIGHT:
			case PANGO_WEIGHT_SEMILIGHT:
				g_string_append (s, "font-weight: 300; ");
				break;
			case PANGO_WEIGHT_BOOK:
			case PANGO_WEIGHT_NORMAL:
				g_string_append (s, "font-weight: 400; ");
				break;
			case PANGO_WEIGHT_MEDIUM:
				g_string_append (s, "font-weight: 500; ");
				break;
			case PANGO_WEIGHT_SEMIBOLD:
				g_string_append (s, "font-weight: 600; ");
				break;
			case PANGO_WEIGHT_BOLD:
				g_string_append (s, "font-weight: 700; ");
				break;
			case PANGO_WEIGHT_ULTRABOLD:
				g_string_append (s, "font-weight: 800; ");
				break;
			case PANGO_WEIGHT_HEAVY:
			case PANGO_WEIGHT_ULTRAHEAVY:
				g_string_append (s, "font-weight: 900; ");
				break;
		}
	}
	if (set & PANGO_FONT_MASK_STRETCH)
	{
		switch (pango_font_description_get_stretch (desc))
		{
			case PANGO_STRETCH_ULTRA_CONDENSED:
				g_string_append (s, "font-stretch: ultra-condensed; ");
				break;
			case PANGO_STRETCH_EXTRA_CONDENSED:
				g_string_append (s, "font-stretch: extra-condensed; ");
				break;
			case PANGO_STRETCH_CONDENSED:
				g_string_append (s, "font-stretch: condensed; ");
				break;
			case PANGO_STRETCH_SEMI_CONDENSED:
				g_string_append (s, "font-stretch: semi-condensed; ");
				break;
			case PANGO_STRETCH_NORMAL:
				g_string_append (s, "font-stretch: normal; ");
				break;
			case PANGO_STRETCH_SEMI_EXPANDED:
				g_string_append (s, "font-stretch: semi-expanded; ");
				break;
			case PANGO_STRETCH_EXPANDED:
				g_string_append (s, "font-stretch: expanded; ");
				break;
			case PANGO_STRETCH_EXTRA_EXPANDED:
				g_string_append (s, "font-stretch: extra-expanded; ");
				break;
			case PANGO_STRETCH_ULTRA_EXPANDED:
				g_string_append (s, "font-stretch: ultra-expanded; ");
				break;
		}
	}
	if (set & PANGO_FONT_MASK_SIZE)
	{
		g_string_append_printf (s, "font-size: %dpt; ",
				pango_font_description_get_size (desc) / PANGO_SCALE);
	}

	g_string_append (s, "}");

	return g_string_free (s, FALSE);
}

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

void
common_help_cb (GtkWindow *parent)
{
	/* TODO: Move to gtk_uri_launcher_launch - requires gtk >= 4.10 */
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_show_uri (parent,
	              "help:ghex",
	              GDK_CURRENT_TIME);
	G_GNUC_END_IGNORE_DEPRECATIONS
}

void
common_about_cb (GtkWindow *parent)
{
	char *copyright;
	char *license_translated;
	char *version;

	g_return_if_fail (GTK_IS_WINDOW(parent));

	const char *authors[] = {
		"Jaka Mo\304\215nik",
		"Chema Celorio",
		"Shivram Upadhyayula",
		"Rodney Dawes",
		"Jonathon Jongsma",
		"Kalev Lember",
		"Logan Rathbone",
		NULL
	};

	const char *documentation_credits[] = {
		"Jaka Mo\304\215nik",
		"Sun GNOME Documentation Team",
		"Logan Rathbone",
		NULL
	};

	const char *license[] = {
		N_("This program is free software; you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation; either version 2 of the License, or "
		   "(at your option) any later version."),
		N_("This program is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License "
		   "along with this program; if not, write to the Free Software Foundation, Inc., "
		   "51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA")
	};
	license_translated = g_strjoin ("\n\n",
	                                _(license[0]),
	                                _(license[1]),
	                                _(license[2]),
	                                NULL);

	/* Translators: these two strings here indicate the copyright time span,
	   e.g. 1998-2018. */
	copyright = g_strdup_printf (_("Copyright © %d–%d The GHex authors"),
			1998, 2023);

	if (strstr (APP_ID, "Devel"))
		version = g_strdup_printf ("%s (Running against GTK %d.%d.%d)",
				PACKAGE_VERSION,
				gtk_get_major_version (),
				gtk_get_minor_version (),
				gtk_get_micro_version ());
	else
		version = PACKAGE_VERSION;

	adw_show_about_dialog (GTK_WIDGET (parent),
	                       "application-icon", APP_ID,
	                       "application-name", strstr (APP_ID, "Devel") ? "GHex (Development)" : "GHex",
	                       "developer-name", "Logan Rathbone",
	                       "version", version,
	                       "issue-url", "https://gitlab.gnome.org/GNOME/ghex/-/issues",
	                       "developers", authors,
	                       "documenters", documentation_credits,
	                       "translator-credits", _("translator-credits"),
	                       "copyright", copyright,
	                       "license-type", GTK_LICENSE_GPL_2_0,
	                       NULL);

	g_free (license_translated);
	g_free (copyright);
}


/* common_print
 *
 * Prints or previews the current document.
 *
 * @preview: Indicates whether to show only a print preview (TRUE) or to
 * display the print dialog.
 */
void
common_print (GtkWindow *parent, HexWidget *gh, gboolean preview)
{
	GHexPrintJobInfo *job;
	HexDocument *doc;
	GtkPrintOperationResult result;
	GError *error = NULL;
	char *basename;

	g_return_if_fail (HEX_IS_WIDGET (gh));

	doc = hex_widget_get_document (gh);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	basename = g_file_get_basename (hex_document_get_file (doc));

	job = ghex_print_job_info_new (doc, hex_widget_get_group_type (gh));
	job->master = gtk_print_operation_new ();
	job->config = gtk_print_settings_new ();
	gtk_print_settings_set (job->config, GTK_PRINT_SETTINGS_OUTPUT_BASENAME,
			basename);
	gtk_print_settings_set_paper_size (job->config,
			gtk_paper_size_new (NULL));	/* system default */
	gtk_print_operation_set_unit (job->master, GTK_UNIT_POINTS);
	gtk_print_operation_set_print_settings (job->master, job->config);
	gtk_print_operation_set_embed_page_setup (job->master, TRUE);
	gtk_print_operation_set_show_progress (job->master, TRUE);
	g_signal_connect (job->master, "draw-page",
	                  G_CALLBACK (print_page), job);
	g_signal_connect (job->master, "begin-print",
	                  G_CALLBACK (begin_print), job);

	if (!job)
		return;

	job->preview = preview;

	result = gtk_print_operation_run (job->master,
			(job->preview ?	GTK_PRINT_OPERATION_ACTION_PREVIEW :
							GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG),
			parent,
			&error);

	if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
		char *tmp;

		/* Translators: This is an error string for a print-related error
		 * dialog.  The %s is the error generated by GError. */
		tmp = g_strdup_printf (_("An error has occurred: %s"),
					error->message);

		display_dialog (parent, tmp);
		
		g_free (tmp);
		g_error_free (error);
	}
	ghex_print_job_info_destroy (job);
	g_free (basename);
}

void
display_dialog (GtkWindow *parent, const char *msg)
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
