/* vim: colorcolumn=80 ts=4 sw=4
 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* preferences.c - Preferences dialog for GHex

   Copyright © 1998 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021 Logan Rathbone <poprocks@gmail.com>

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

#include "preferences.h"

#include <config.h>

/* CONSTANTS */

#define SHADED_BOX_MAX		CONFIG_H_SHADED_BOX_MAX
#define PREFS_RESOURCE		RESOURCE_BASE_PATH "/preferences.ui"

/* PRIVATE DATATYPES */

/* The types of fonts that can be set via font choosers. I suppose we could
 * just compare the pointer values since they're global, but that would break
 * if the structure were ever changed to de-global-ify them.
 */
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

/* for spinbtn */
static GtkAdjustment *shaded_box_adj;

/* widgets that interact with settings */
static GtkWidget *font_button;
static GtkWidget *data_font_button;
static GtkWidget *header_font_button;
static GtkWidget *show_offsets_chkbtn;
static GtkWidget *bytes_chkbtn;
static GtkWidget *words_chkbtn;
static GtkWidget *long_chkbtn;
static GtkWidget *quad_chkbtn;
static GtkWidget *offsethex_chkbtn;
static GtkWidget *offsetdec_chkbtn;
static GtkWidget *offsetboth_chkbtn;
static GtkWidget *shaded_box_chkbtn;
static GtkWidget *shaded_box_spinbtn;
static GtkWidget *shaded_box_row;
static GtkWidget *dark_mode_switch;
static GtkWidget *system_default_chkbtn;
static GtkWidget *show_control_chars_checkbtn;

/* PRIVATE FUNCTIONS */

/* wee helper */
static void
sync_shaded_box_size_with_spinbtn (void)
{
	GtkSpinButton *spin_button = GTK_SPIN_BUTTON(shaded_box_spinbtn);
	/* we _want_ implicit conversion here. */
	guint tmp = gtk_spin_button_get_value_as_int (spin_button);

	if (tmp != shaded_box_size) {
		g_settings_set_uint (settings,
				GHEX_PREF_BOX_SIZE,
				tmp);
	}
}

static void
shaded_box_spinbtn_value_changed_cb (GtkSpinButton *spin_button,
		gpointer user_data)
{
	sync_shaded_box_size_with_spinbtn ();
}

static void
shaded_box_chkbtn_toggled_cb (GtkCheckButton *checkbutton,
		gpointer user_data)
{
	gboolean checked;

	checked = gtk_check_button_get_active (checkbutton);

	gtk_widget_set_sensitive (shaded_box_row,
			checked ? TRUE : FALSE);

	if (checked) {
		sync_shaded_box_size_with_spinbtn ();
	} else if (shaded_box_size) {
		g_settings_set_uint (settings,
				GHEX_PREF_BOX_SIZE,
				0);
	}
}

static void
show_offsets_set_cb (GtkCheckButton *checkbutton,
		gpointer user_data)
{
	gboolean show_or_hide;

	show_or_hide = gtk_check_button_get_active (checkbutton);

	g_settings_set_boolean (settings,
			GHEX_PREF_OFFSETS_COLUMN,
			show_or_hide);
}

static void
show_control_chars_set_cb (GtkCheckButton *checkbutton,
		gpointer user_data)
{
	gboolean show_or_hide;

	show_or_hide = gtk_check_button_get_active (checkbutton);

	g_settings_set_boolean (settings,
			GHEX_PREF_CONTROL_CHARS,
			show_or_hide);
}

static void
group_type_set_cb (GtkCheckButton *checkbutton,
		gpointer user_data)
{
	int group_type = GPOINTER_TO_INT(user_data);

	/* this signal activate when the state *changes*, so we still need to see
	 * whether or not the button associated with our enum is *checked or not.
	 */
	if (gtk_check_button_get_active (checkbutton))
	{
		g_settings_set_enum (settings,
				GHEX_PREF_GROUP,
				group_type);
	}
}

static void
offset_format_set_cb (GtkCheckButton *checkbutton,
		gpointer user_data)
{
	int sb_format = GPOINTER_TO_INT(user_data);

	/* this signal activate when the state *changes*, so we still need to see
	 * whether or not the button associated with our enum is *checked or not.
	 */
	if (gtk_check_button_get_active (checkbutton))
	{
		g_settings_set_enum (settings,
				GHEX_PREF_SB_OFFSET_FORMAT,
				sb_format);
	}
}

/* note the lack of const and the ugly cast below. This is to silence a
 * warning about incompatible types.
 */
static gboolean
monospace_font_filter (/* const */ PangoFontFamily *family,
		const PangoFontFace *face,
		gpointer data)
{
	if (pango_font_family_is_monospace (family))
		return TRUE;
	else
		return FALSE;
}

static void
font_set_cb (GtkFontButton *widget,
		gpointer user_data)
{
	GtkFontChooser *chooser = GTK_FONT_CHOOSER(widget);
	FontType type = GPOINTER_TO_INT(user_data);
	char *tmp;
	char *pref;

	switch (type)
	{
		case GUI_FONT:
			pref = GHEX_PREF_FONT;
			break;

		case DATA_FONT:
			pref = GHEX_PREF_DATA_FONT;
			break;

		case HEADER_FONT:
			pref = GHEX_PREF_HEADER_FONT;
			break;

		default:
			g_error ("%s: Programmer error - invalid enum passed to function.",
					__func__);
			break;
	}
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	tmp = gtk_font_chooser_get_font (chooser);
	G_GNUC_END_IGNORE_DEPRECATIONS

	if (tmp) {
		g_settings_set_string (settings,
				pref,
				tmp);
		g_free (tmp);
	}
	else {
		g_warning ("%s: No chosen font detected. Doing nothing.",
				__func__);
	}
}

/* Quick helper function for font buttons */
static void
monospace_only (GtkWidget *font_button)
{
	GtkFontChooser *chooser = GTK_FONT_CHOOSER(font_button);

	g_return_if_fail (GTK_IS_FONT_CHOOSER (chooser));

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_font_chooser_set_filter_func (chooser,
			(GtkFontFilterFunc)monospace_font_filter,
			NULL, NULL);	/* no user data, no destroy func for same. */
	G_GNUC_END_IGNORE_DEPRECATIONS
}

static gboolean
dark_mode_set_cb (GtkSwitch *widget,
		gboolean state,
		gpointer user_data)
{
	int dark_mode;

	if (state)
		dark_mode = DARK_MODE_ON;
	else
		dark_mode = DARK_MODE_OFF;

	g_settings_set_enum (settings,
			GHEX_PREF_DARK_MODE,
			dark_mode);

	return GDK_EVENT_PROPAGATE;
}

static void
system_default_set_cb (GtkCheckButton *checkbutton,
		gpointer user_data)
{
	gboolean checked;
	int dark_mode;

	checked = gtk_check_button_get_active (checkbutton);

	gtk_widget_set_sensitive (dark_mode_switch,
			checked ? FALSE : TRUE);

	if (checked) {
		dark_mode = DARK_MODE_SYSTEM;
	} else {
		dark_mode = gtk_switch_get_active (GTK_SWITCH(dark_mode_switch)) ?
			DARK_MODE_ON : DARK_MODE_OFF;
	}
	g_settings_set_enum (settings,
			GHEX_PREF_DARK_MODE,
			dark_mode);
}

static void
setup_signals (void)
{
	/* font_buttons */

	g_signal_connect (font_button, "font-set",
			G_CALLBACK(font_set_cb), GINT_TO_POINTER(GUI_FONT));

	g_signal_connect (data_font_button, "font-set",
			G_CALLBACK(font_set_cb), GINT_TO_POINTER(DATA_FONT));

	g_signal_connect (header_font_button, "font-set",
			G_CALLBACK(font_set_cb), GINT_TO_POINTER(HEADER_FONT));

	/* dark mode */

	g_signal_connect (dark_mode_switch, "state-set",
			G_CALLBACK(dark_mode_set_cb), NULL);

	g_signal_connect (system_default_chkbtn, "toggled",
			G_CALLBACK(system_default_set_cb), NULL);

	/* group type checkbuttons */

	g_signal_connect (bytes_chkbtn, "toggled",
			G_CALLBACK(group_type_set_cb), GINT_TO_POINTER(HEX_WIDGET_GROUP_BYTE));

	g_signal_connect (words_chkbtn, "toggled",
			G_CALLBACK(group_type_set_cb), GINT_TO_POINTER(HEX_WIDGET_GROUP_WORD));

	g_signal_connect (long_chkbtn, "toggled",
			G_CALLBACK(group_type_set_cb), GINT_TO_POINTER(HEX_WIDGET_GROUP_LONG));

	g_signal_connect (quad_chkbtn, "toggled",
			G_CALLBACK(group_type_set_cb), GINT_TO_POINTER(HEX_WIDGET_GROUP_QUAD));

	/* status bar offset format checkbuttons */

	g_signal_connect (offsethex_chkbtn, "toggled",
			G_CALLBACK(offset_format_set_cb), GINT_TO_POINTER(HEX_WIDGET_STATUS_BAR_OFFSET_HEX));

	g_signal_connect (offsetdec_chkbtn, "toggled",
			G_CALLBACK(offset_format_set_cb), GINT_TO_POINTER(HEX_WIDGET_STATUS_BAR_OFFSET_DEC));

	g_signal_connect (offsetboth_chkbtn, "toggled",
			G_CALLBACK(offset_format_set_cb), GINT_TO_POINTER(HEX_WIDGET_STATUS_BAR_OFFSET_HEX | HEX_WIDGET_STATUS_BAR_OFFSET_DEC));

	/* show offsets checkbutton */

	g_signal_connect (show_offsets_chkbtn, "toggled",
			G_CALLBACK(show_offsets_set_cb), NULL);

	/* show control chars checkbutton */

	g_signal_connect (show_control_chars_checkbtn, "toggled",
			G_CALLBACK(show_control_chars_set_cb), NULL);

	/* shaded box for printing */

	g_signal_connect (shaded_box_chkbtn, "toggled",
			G_CALLBACK(shaded_box_chkbtn_toggled_cb), NULL);

	g_signal_connect (shaded_box_spinbtn, "value-changed",
			G_CALLBACK(shaded_box_spinbtn_value_changed_cb), NULL);
}

static void
grab_widget_values_from_settings (void)
{
	AdwStyleManager *manager = adw_style_manager_get_default ();

	/* font_button */
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_font_chooser_set_font (GTK_FONT_CHOOSER(font_button),
			def_font_name);
	G_GNUC_END_IGNORE_DEPRECATIONS

	/* dark mode stuff */

	/* Set switch to appropriate position and grey out if system default */
	if (def_dark_mode == DARK_MODE_SYSTEM)
	{
		gtk_check_button_set_active (GTK_CHECK_BUTTON(system_default_chkbtn),
				TRUE);
		gtk_widget_set_sensitive (dark_mode_switch, FALSE);
		gtk_switch_set_state (GTK_SWITCH(dark_mode_switch),
				adw_style_manager_get_dark (manager));
	}
	else
	{
		gtk_check_button_set_active (GTK_CHECK_BUTTON(system_default_chkbtn),
				FALSE);
		gtk_widget_set_sensitive (dark_mode_switch, TRUE);
		gtk_switch_set_state (GTK_SWITCH(dark_mode_switch),
				def_dark_mode == DARK_MODE_ON ? TRUE : FALSE);
	}

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	/* data_font_button */
	gtk_font_chooser_set_font (GTK_FONT_CHOOSER(data_font_button),
			data_font_name);

	/* header_font_button */
	gtk_font_chooser_set_font (GTK_FONT_CHOOSER(header_font_button),
			header_font_name);
	G_GNUC_END_IGNORE_DEPRECATIONS

	/* show_offsets_chkbtn */
	gtk_check_button_set_active (GTK_CHECK_BUTTON(show_offsets_chkbtn),
			show_offsets_column);

	/* show_control_chars_checkbtn */
	gtk_check_button_set_active (GTK_CHECK_BUTTON(show_control_chars_checkbtn),
			def_display_control_characters);

	/* group_type radio buttons
	 */
	switch (def_group_type) {
		case HEX_WIDGET_GROUP_BYTE:
			gtk_check_button_set_active (GTK_CHECK_BUTTON(bytes_chkbtn),
					TRUE);
			break;

		case HEX_WIDGET_GROUP_WORD:
			gtk_check_button_set_active (GTK_CHECK_BUTTON(words_chkbtn),
					TRUE);
			break;

		case HEX_WIDGET_GROUP_LONG:
			gtk_check_button_set_active (GTK_CHECK_BUTTON(long_chkbtn),
					TRUE);
			break;

		case HEX_WIDGET_GROUP_QUAD:
			gtk_check_button_set_active (GTK_CHECK_BUTTON(quad_chkbtn),
					TRUE);
			break;

		default:
			g_warning ("group_type option invalid; falling back to BYTES.");
			gtk_check_button_set_active (GTK_CHECK_BUTTON(bytes_chkbtn),
					TRUE);
			break;
	}

	/* sb_offset_format radio buttons
	 */
	switch (def_sb_offset_format) {
		case HEX_WIDGET_STATUS_BAR_OFFSET_HEX | HEX_WIDGET_STATUS_BAR_OFFSET_DEC:
			gtk_check_button_set_active (GTK_CHECK_BUTTON(offsetboth_chkbtn),
					TRUE);
			break;

		case HEX_WIDGET_STATUS_BAR_OFFSET_HEX:
			gtk_check_button_set_active (GTK_CHECK_BUTTON(offsethex_chkbtn),
					TRUE);
			break;

		case HEX_WIDGET_STATUS_BAR_OFFSET_DEC:
			gtk_check_button_set_active (GTK_CHECK_BUTTON(offsetdec_chkbtn),
					TRUE);
			break;

		default:
			g_warning ("sb_offset_format option invalid; falling back to HEX.");
			gtk_check_button_set_active (GTK_CHECK_BUTTON(offsethex_chkbtn),
					TRUE);
			break;
	}

	/* shaded_box_* */
	gtk_check_button_set_active (GTK_CHECK_BUTTON(shaded_box_chkbtn),
			shaded_box_size > 0 ? TRUE : FALSE);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON(shaded_box_spinbtn),
			shaded_box_size);

	shaded_box_chkbtn_toggled_cb (GTK_CHECK_BUTTON(shaded_box_chkbtn), NULL);
}

static void
init_widgets (void)
{
	GET_WIDGET (prefs_dialog);

	GET_WIDGET (font_button);
	GET_WIDGET (data_font_button);
	GET_WIDGET (header_font_button);
	GET_WIDGET (show_offsets_chkbtn);
	GET_WIDGET (bytes_chkbtn);
	GET_WIDGET (words_chkbtn);
	GET_WIDGET (long_chkbtn);
	GET_WIDGET (quad_chkbtn);
	GET_WIDGET (offsethex_chkbtn);
	GET_WIDGET (offsetdec_chkbtn);
	GET_WIDGET (offsetboth_chkbtn);
	GET_WIDGET (shaded_box_chkbtn);
	GET_WIDGET (shaded_box_spinbtn);
	GET_WIDGET (shaded_box_row);
	GET_WIDGET (dark_mode_switch);
	GET_WIDGET (system_default_chkbtn);
	GET_WIDGET (show_control_chars_checkbtn);

	/* Make certain font choosers only allow monospace fonts. */
	monospace_only (font_button);
	monospace_only (data_font_button);

	/* shaded box entry */
	shaded_box_adj = GTK_ADJUSTMENT(gtk_adjustment_new(1,
				1,				/* min; no point in having 0 if ineffective */
				SHADED_BOX_MAX,
				1,				/* step incr */
				10,				/* page incr */
				0));			/* page size */
	gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON(shaded_box_spinbtn),
			shaded_box_adj);
}

/* PUBLIC FUNCTIONS */

GtkWidget *
create_preferences_dialog (GtkWindow *parent)
{
	builder = gtk_builder_new_from_resource (PREFS_RESOURCE);

	init_widgets ();
	grab_widget_values_from_settings ();
	setup_signals ();

	if (parent) {
		g_assert (GTK_IS_WINDOW (parent));

		gtk_window_set_transient_for (GTK_WINDOW(prefs_dialog), parent);
	}
	return prefs_dialog;
}
