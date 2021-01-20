/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* FIXME.c - a window with a character table

   Copyright © 1998 - 2004 Free Software Foundation
   Copyright © 2021 Logan Rathbone

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include "preferences.h"

/* provides ``settings'' and def_* globals, as well as GHEX_PREF_* defines. */
#include "configuration.h"

#define PREFS_RESOURCE "/org/gnome/ghex/preferences.ui"

#define GET_WIDGET(X) \
	X = GTK_WIDGET(gtk_builder_get_object (builder, #X)); \
	g_assert (GTK_IS_WIDGET (X))

/* PRIVATE DATATYPES */

typedef enum {
	GUI_FONT,
	DATA_FONT,
	HEADER_FONT
} FontType;

/* STATIC GLOBALS */

static GtkBuilder *builder;

/* use GET_WIDGET(X) macro to set these from builder, where
 * X == var name == builder id name. Don't include quotation marks.
 */
/* main widget */
static GtkWidget *prefs_dialog;

/* for css stuff */
static GtkWidget *content_area_box;
static GtkWidget *font_frame, *group_type_frame, *print_font_frame;

/* widgets that interact with settings */
static GtkWidget *font_button;
static GtkWidget *data_font_button;
static GtkWidget *header_font_button;
static GtkWidget *show_offsets_chkbtn;
static GtkWidget *bytes_chkbtn;
static GtkWidget *words_chkbtn;
static GtkWidget *long_chkbtn;
static GtkWidget *shaded_box_chkbtn;
static GtkWidget *shaded_box_spinbtn;
static GtkWidget *shaded_box_box;

/* PRIVATE DECLARATIONS */



/* PRIVATE FUNCTIONS */

#define APPLY_PROVIDER_TO(PROVIDER, WIDGET)									\
	context = gtk_widget_get_style_context (WIDGET);						\
	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER(PROVIDER),	\
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION)
static void
do_css_stuff(void)
{
	GtkStyleContext *context;
	GtkCssProvider *box_provider, *frame_provider;

	/* Grab layout-oriented widgets and set CSS styling. */
	GET_WIDGET (content_area_box);
	GET_WIDGET (font_frame);
	GET_WIDGET (group_type_frame);
	GET_WIDGET (print_font_frame);

	
	/* overall padding for content area: */
	box_provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (box_provider,
										"* {\n"
										"  padding-left: 24px;\n"
										"  padding-right: 24px;\n"
										"}\n", -1);

	APPLY_PROVIDER_TO(box_provider, content_area_box);

	/* padding for our frames (they look god-awful without a bit) */
	frame_provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (frame_provider,
										"* {\n"
										"  padding: 12px;\n"
										"}\n",	-1);

	APPLY_PROVIDER_TO(frame_provider, font_frame);
	APPLY_PROVIDER_TO(frame_provider, group_type_frame);
	APPLY_PROVIDER_TO(frame_provider, print_font_frame);
}
#undef APPLY_PROVIDER_TO

/* note the lack of const and the ugly cast below. This is to silence a
 * warning about incompatible types.
 */
static gboolean
monospace_font_filter (/* const */ PangoFontFamily *family,
		const PangoFontFace *face,
		gpointer data)
{
	(void)face, (void)data;

	if (pango_font_family_is_monospace (family))
		return TRUE;
	else
		return FALSE;
}

static void
font_set_cb (GtkFontButton *widget,
		gpointer user_data)
{
	FontType type = GPOINTER_TO_INT(user_data);
	char *tmp;
	char **global;
	char *pref;
	GtkFontChooser *chooser = GTK_FONT_CHOOSER(widget);

	if (type == GUI_FONT) {
		global = &def_font_name;
		pref = GHEX_PREF_FONT;
	} else if (type == DATA_FONT) {
		global = &data_font_name;
		pref = GHEX_PREF_DATA_FONT;
	} else if (type == HEADER_FONT) {
		global = &header_font_name;
		pref = GHEX_PREF_HEADER_FONT;
	} else {
		g_error ("%s: Programmer error - invalid enum passed to function.",
				__func__);
	}

	tmp = gtk_font_chooser_get_font (chooser);

	if (tmp) {
		if (*global)
			g_free (*global);
		*global = tmp;
		g_settings_set_string (settings,
				pref,
				*global);
	} else {
		g_warning ("%s: No chosen font detected. Doing nothing.", __func__);
	}

	printf("TEST - def_font_name: %s - data_font_name: %s - header_font_name: %s\n", def_font_name, data_font_name, header_font_name);
}

static void
setup_signals (void)
{
	/* font_button */

	/* Make font chooser only allow monospace fonts. */
	gtk_font_chooser_set_filter_func (GTK_FONT_CHOOSER(font_button),
			(GtkFontFilterFunc)monospace_font_filter,
			NULL, NULL);	/* no user data, no destroy func for same. */

	g_signal_connect (font_button, "font-set",
			G_CALLBACK(font_set_cb), GINT_TO_POINTER(GUI_FONT));

	g_signal_connect (data_font_button, "font-set",
			G_CALLBACK(font_set_cb), GINT_TO_POINTER(DATA_FONT));

	g_signal_connect (header_font_button, "font-set",
			G_CALLBACK(font_set_cb), GINT_TO_POINTER(HEADER_FONT));

}

/* put all of your GET_WIDGET calls other than the main prefs_dialog widget
 * and CSS-only stuff in here, please.
 */
static void
grab_widget_values_from_settings (void)
{
	/* font_button */
	gtk_font_chooser_set_font (GTK_FONT_CHOOSER(font_button),
			def_font_name);

	/* data_font_button */
	gtk_font_chooser_set_font (GTK_FONT_CHOOSER(data_font_button),
			data_font_name);

	/* header_font_button */
	gtk_font_chooser_set_font (GTK_FONT_CHOOSER(header_font_button),
			header_font_name);

	/* show_offsets_chkbtn */
	gtk_check_button_set_active (GTK_CHECK_BUTTON(show_offsets_chkbtn),
			show_offsets_column);

	/* group_type radio buttons
	 */
	if (def_group_type == GROUP_BYTE) {
		gtk_check_button_set_active (GTK_CHECK_BUTTON(bytes_chkbtn), TRUE);
	} else if (def_group_type == GROUP_WORD) {
		gtk_check_button_set_active (GTK_CHECK_BUTTON(words_chkbtn), TRUE);
	} else if (def_group_type == GROUP_LONG) {
		gtk_check_button_set_active (GTK_CHECK_BUTTON(long_chkbtn), TRUE);
	} else {
		g_warning ("group_type option invalid; falling back to BYTES.");
		gtk_check_button_set_active (GTK_CHECK_BUTTON(bytes_chkbtn), TRUE);
	}

	/* shaded_box_* */
	gtk_check_button_set_active (GTK_CHECK_BUTTON(shaded_box_chkbtn),
			shaded_box_size > 0 ? TRUE : FALSE);
	gtk_widget_set_sensitive (shaded_box_box,
			shaded_box_size > 0 ? TRUE : FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(shaded_box_spinbtn),
			shaded_box_size);
}

static void
init_widgets (void)
{
	GET_WIDGET (font_button);
	GET_WIDGET (data_font_button);
	GET_WIDGET (header_font_button);
	GET_WIDGET (show_offsets_chkbtn);
	GET_WIDGET (bytes_chkbtn);
	GET_WIDGET (words_chkbtn);
	GET_WIDGET (long_chkbtn);
	GET_WIDGET (shaded_box_chkbtn);
	GET_WIDGET (shaded_box_spinbtn);
	GET_WIDGET (shaded_box_box);
}

/* PUBLIC FUNCTIONS */

GtkWidget *
create_preferences_dialog (void)
{
	builder = gtk_builder_new_from_resource (PREFS_RESOURCE);

	do_css_stuff ();
	init_widgets ();
	grab_widget_values_from_settings ();
	setup_signals ();

	GET_WIDGET (prefs_dialog);

	return prefs_dialog;
}
