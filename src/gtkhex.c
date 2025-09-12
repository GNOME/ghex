/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

/* gtkhex.c - definition of a HexWidget widget

   Copyright © 1997 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021-2025 Logan Rathbone <poprocks@gmail.com>

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

#include "gtkhex.h"
#include "gtkhex-layout-manager.h"
#include "common-macros.h"

#include <string.h>

#include <config.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* Don't move these from the source file as they are not part of the public
 * header.
 */

/* DEFINES */

/* default minimum drawing area size (for ascii and hex widgets) in pixels. */
#define DEFAULT_DA_SIZE 50
#define SCROLL_TIMEOUT 100
#define STARTING_OFFSET 0

#define is_displayable(c) (((c) >= 0x20) && ((c) < 0x7f))
#define is_control_character(c) (((c) >= 0x00) && ((c) <= 0x1f))

#define PANGO_COLOR_FROM_FLOAT(C) (C * UINT16_MAX)

#define HEX_BUFFER_PAYLOAD(X)	\
	hex_buffer_get_payload_size (hex_document_get_buffer (X))

/* ENUMS */

enum {
	UPPER_NIBBLE,
	LOWER_NIBBLE
};

typedef enum {
	VIEW_HEX,
	VIEW_ASCII
} HexWidgetViewType;

/* ASCII control characters - lookup table */

static const gunichar control_characters_lookup_table[] = {
    U'\u2400', // U+2400: NUL
    U'\u2401', // U+2401: SOH
    U'\u2402', // U+2402: STX
    U'\u2403', // U+2403: ETX
    U'\u2404', // U+2404: EOT
    U'\u2405', // U+2405: ENQ
    U'\u2406', // U+2406: ACK
    U'\u2407', // U+2407: BEL
    U'\u2408', // U+2408: BS
    U'\u2409', // U+2409: HT
    U'\u240A', // U+240A: LF
    U'\u240B', // U+240B: VT
    U'\u240C', // U+240C: FF
    U'\u240D', // U+240D: CR
    U'\u240E', // U+240E: SO
    U'\u240F', // U+240F: SI
    U'\u2410', // U+2410: DLE
    U'\u2411', // U+2411: DC1
    U'\u2412', // U+2412: DC2
    U'\u2413', // U+2413: DC3
    U'\u2414', // U+2414: DC4
    U'\u2415', // U+2415: NAK
    U'\u2416', // U+2416: SYN
    U'\u2417', // U+2417: ETB
    U'\u2418', // U+2418: CAN
    U'\u2419', // U+2419: EM
    U'\u241A', // U+241A: SUB
    U'\u241B', // U+241B: ESC
    U'\u241C', // U+241C: FS
    U'\u241D', // U+241D: GS
    U'\u241E', // U+241E: RS
    U'\u241F'  // U+241F: US
};

/* highlighting information.
 */
typedef struct _HexWidget_Highlight HexWidget_Highlight;

struct _HexWidget_Highlight
{
	gint64 start, end;
	gint64 start_line, end_line;
};

/**
 * HexWidgetAutoHighlight:
 *
 * A structure used to automatically highlight all visible occurrences
 * of a given string.
 */
struct _HexWidgetAutoHighlight
{
	char *search_string;
	int search_len;

	HexSearchFlags search_flags;

	gint64 view_min;
	gint64 view_max;

	GPtrArray *highlights;
};

/* FIXME - only defined to create a boxed type and may be unreliable. */
static HexWidgetAutoHighlight *
hex_widget_autohighlight_copy (HexWidgetAutoHighlight *ahl)
{
	return ahl;
}

G_DEFINE_BOXED_TYPE (HexWidgetAutoHighlight, hex_widget_autohighlight,
		hex_widget_autohighlight_copy, g_free)

enum {
	MARK_HAVE_CUSTOM_COLOR = 1,
	MARK_CUSTOM_COLOR,
	MARK_N_PROPERTIES
};

static GParamSpec *mark_properties[MARK_N_PROPERTIES];

/**
 * HexWidgetMark:
 *
 * `HexWidgetMark` is a `GObject` which contains the metadata associated with a
 * mark for a hex document.
 *
 * To instantiate a `HexWidgetMark` object, use the [method@HexWidget.add_mark]
 * method.
 */
struct _HexWidgetMark
{
	GObject parent_instance;

	HexWidget_Highlight highlight;
	gboolean have_custom_color;
	GdkRGBA custom_color;
};

G_DEFINE_TYPE (HexWidgetMark, hex_widget_mark, G_TYPE_OBJECT)

/**
 * hex_widget_mark_get_have_custom_color:
 *
 * Returns whether the `HexWidgetMark` has a custom color associated with it.
 *
 * Returns: `TRUE` if the `HexWidgetMark` has a custom color associated with
 *   it; `FALSE` otherwise.
 *
 * Since: 4.8
 */
gboolean
hex_widget_mark_get_have_custom_color (HexWidgetMark *mark)
{
	g_return_val_if_fail (HEX_IS_WIDGET_MARK (mark), FALSE);

	return mark->have_custom_color;
}

/**
 * hex_widget_mark_get_custom_color:
 * @color: (out): A `GdkRGBA` structure to be set with the custom color
 *   associated with the `HexWidgetMark`, if applicable
 *
 * Obtains the custom color associated with a `HexWidgetMark` object, if
 * any.
 *
 * Since: 4.8
 */
void
hex_widget_mark_get_custom_color (HexWidgetMark *mark, GdkRGBA *color)
{
	g_return_if_fail (HEX_IS_WIDGET_MARK (mark));
	g_return_if_fail (color != NULL);

	*color = mark->custom_color;
}

static void
hex_widget_mark_set_custom_color (HexWidgetMark *mark, GdkRGBA *color)
{
	g_return_if_fail (HEX_IS_WIDGET_MARK (mark));
	g_return_if_fail (color != NULL);

	mark->have_custom_color = TRUE;
	mark->custom_color = *color;

	g_object_notify_by_pspec (G_OBJECT(mark), mark_properties[MARK_CUSTOM_COLOR]);
	g_object_notify_by_pspec (G_OBJECT(mark), mark_properties[MARK_HAVE_CUSTOM_COLOR]);
}

/**
 * hex_widget_mark_get_start_offset:
 *
 * Obtains the start offset of a `HexWidgetMark`.
 *
 * Returns: The start offset of the mark
 *
 * Since: 4.8
 */
gint64
hex_widget_mark_get_start_offset (HexWidgetMark *mark)
{
	g_return_val_if_fail (HEX_IS_WIDGET_MARK (mark), -1);

	return mark->highlight.start;
}

/**
 * hex_widget_mark_get_end_offset:
 *
 * Obtains the end offset of a `HexWidgetMark`.
 *
 * Returns: The end offset of the mark
 *
 * Since: 4.8
 */
gint64
hex_widget_mark_get_end_offset (HexWidgetMark *mark)
{
	g_return_val_if_fail (HEX_IS_WIDGET_MARK (mark), -1);

	return mark->highlight.end;
}

static void
hex_widget_mark_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexWidgetMark *mark = HEX_WIDGET_MARK(object);

	switch (property_id)
	{
		case MARK_CUSTOM_COLOR:
			hex_widget_mark_set_custom_color (mark, g_value_get_boxed (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_widget_mark_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexWidgetMark *mark = HEX_WIDGET_MARK(object);

	switch (property_id)
	{
		case MARK_HAVE_CUSTOM_COLOR:
			g_value_set_boolean (value, hex_widget_mark_get_have_custom_color (mark));
			break;

		case MARK_CUSTOM_COLOR:
		{
			GdkRGBA color;

			hex_widget_mark_get_custom_color (mark, &color);
			g_value_set_boxed (value, &color);
		}
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_widget_mark_init (HexWidgetMark *mark)
{
}

static void
hex_widget_mark_class_init (HexWidgetMarkClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->set_property = hex_widget_mark_set_property;
	object_class->get_property = hex_widget_mark_get_property;

	/**
	 * HexWidgetMark:have-custom-color:
	 *
	 * Whether the `HexWidgetMark` has a custom color.
	 */
	mark_properties[MARK_HAVE_CUSTOM_COLOR] = g_param_spec_boolean ("have-custom-color", NULL, NULL,
			FALSE,
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * HexWidgetMark:custom-color:
	 *
	 * The custom color of the `HexWidgetMark`, if applicable.
	 */
	mark_properties[MARK_CUSTOM_COLOR] = g_param_spec_boxed ("custom-color", NULL, NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, MARK_N_PROPERTIES, mark_properties);
}

static HexWidgetMark *
hex_widget_mark_new (void)
{
	return g_object_new (HEX_TYPE_WIDGET_MARK, NULL);
}

/* ------------------------------
 * Main HexWidget GObject definition
 * ------------------------------
 */

/* PROPERTIES */

enum
{
	DOCUMENT = 1,
	FADE_ZEROES,
	DISPLAY_CONTROL_CHARACTERS,
	INSERT_MODE,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

/* SIGNALS */

enum {
	CURSOR_MOVED_SIGNAL,
	DATA_CHANGED_SIGNAL,
	CUT_CLIPBOARD_SIGNAL,
	COPY_CLIPBOARD_SIGNAL,
	PASTE_CLIPBOARD_SIGNAL,
	DRAW_COMPLETE_SIGNAL,
	LAST_SIGNAL
};

static guint gtkhex_signals[LAST_SIGNAL] = { 0 };

/**
 * HexWidget:
 *
 * #HexWidget is a widget which can display #HexDocument data as a
 * side-by-side representation of offets, hexadecimal nibbles, and ASCII
 * characters.
 */
struct _HexWidget
{
	GtkWidget parent_instance;

	HexDocument *document;

	GtkLayoutManager *layout_manager;

	GtkCssProvider *provider;

	GtkWidget *xdisp, *adisp;	/* DrawingArea */
	GtkWidget *offsets;			/* DrawingArea */
	GtkWidget *scrollbar;

	GtkWidget *busy_spinner;

	PangoLayout *xlayout, *alayout, *olayout;
	PangoLayout *xlayout_top_line_cache;
	PangoLayout *alayout_top_line_cache;

	GtkWidget *context_menu;
	GtkWidget *geometry_popover;
	GtkWidget *auto_geometry_checkbtn, *cpl_spinbtn;

	GtkAdjustment *adj;

	HexWidgetViewType active_view;

	int char_width, char_height;

	/* Cache of the mouse button. guint to marry up with GtkGesture. */
	guint button;

	gint64 cursor_pos;
	HexWidget_Highlight selection;
	gboolean lower_nibble;

	HexWidgetGroupType group_type;

	/* cpl == `characters per line` */
	int cpl;
	/* number of lines visible in the display */
	int vis_lines;

	/* These are the lines in absolute terms, shown within the display. So they
	 * will be some fraction of `index`, but could still theoretically be quite
	 * large if dealing with a huge file.
	 */
	gint64 lines, top_line;
	gboolean cursor_shown;

	/* width of the hex display `xdisp` and ascii display `adisp` */
	int xdisp_width, adisp_width;

	GPtrArray *auto_highlights;
	GPtrArray *marks;

	/* scroll direction: 0 means no scrolling; a -ve number means we're
	 * scrolling up, and a +ve number means we're scrolling down. */
	int scroll_dir;
	guint scroll_timeout;

	gboolean show_offsets;
	gboolean insert;
	gboolean selecting;

	/* Buffer for storing formatted data for rendering;
	   dynamically adjusts its size to the display size */
	GArray *disp_buffer;	/* of gunichar */

	/* default characters per line and number of lines. */
	int default_cpl;
	int default_lines;

	GdkContentProvider *selection_content;

	gboolean fade_zeroes;
	gboolean display_control_characters;
};

G_DEFINE_TYPE (HexWidget, hex_widget, GTK_TYPE_WIDGET)

/* ----- */

/* HexContentProvider */

#define HEX_TYPE_CONTENT_PROVIDER (hex_content_provider_get_type ())
G_DECLARE_FINAL_TYPE (HexContentProvider, hex_content_provider, HEX, CONTENT_PROVIDER,
		GdkContentProvider)

struct _HexContentProvider
{
	GdkContentProvider parent_instance;

	HexWidget *owner;
};

G_DEFINE_TYPE (HexContentProvider, hex_content_provider, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
hex_content_provider_ref_formats (GdkContentProvider *provider)
{
	HexContentProvider *content = HEX_CONTENT_PROVIDER (provider);
	HexWidget *self = content->owner;
	GdkContentFormatsBuilder *builder = gdk_content_formats_builder_new ();

	gdk_content_formats_builder_add_gtype (builder, HEX_TYPE_PASTE_DATA);
	gdk_content_formats_builder_add_gtype (builder, G_TYPE_STRING);

	return gdk_content_formats_builder_free_to_formats (builder);
}

static void
hex_content_provider_detach (GdkContentProvider *provider,
		GdkClipboard *clipboard)
{
	HexContentProvider *content = HEX_CONTENT_PROVIDER (provider);
	HexWidget *self = content->owner;

	self->selecting = FALSE;
	hex_widget_set_selection (self, self->cursor_pos, self->cursor_pos);
}

static gboolean
hex_content_provider_get_value (GdkContentProvider *provider,
		GValue *value,
		GError **error)
{
	HexContentProvider *content = HEX_CONTENT_PROVIDER (provider);
	HexWidget *self = content->owner;
	HexPasteData *paste;
	gint64 start_pos, end_pos;
	size_t len;
	char *doc_data;

	/* cross-ref: hex_widget_real_copy_to_clipboard - similar initial code */

	start_pos = MIN(self->selection.start, self->selection.end);
	end_pos = MAX(self->selection.start, self->selection.end);

	len = end_pos - start_pos + 1;
	g_return_val_if_fail (len, FALSE);

	doc_data = hex_buffer_get_data (hex_document_get_buffer(self->document),
			start_pos, len);

	paste = hex_paste_data_new (doc_data, len);
	g_return_val_if_fail (HEX_IS_PASTE_DATA(paste), FALSE);

	if (G_VALUE_HOLDS (value, G_TYPE_STRING))
	{
		char *string;

		string = hex_paste_data_get_string (paste);
		g_value_take_string (value, string);
		g_object_unref (paste);

		return TRUE;
	}
	else if (G_VALUE_HOLDS (value, HEX_TYPE_PASTE_DATA))
	{
		g_value_take_object (value, paste);

		return TRUE;
	}

	/* chain up */
	return GDK_CONTENT_PROVIDER_CLASS (hex_content_provider_parent_class)->get_value (
			provider, value, error);
}

static void
hex_content_provider_class_init (HexContentProviderClass *klass)
{
	GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS(klass);

	provider_class->ref_formats = hex_content_provider_ref_formats;
	provider_class->get_value = hex_content_provider_get_value;
	provider_class->detach_clipboard = hex_content_provider_detach;
}

static void
hex_content_provider_init (HexContentProvider *content)
{
}

GdkContentProvider *
hex_content_provider_new (void)
{
	return g_object_new (HEX_TYPE_CONTENT_PROVIDER, NULL);
}

/* --- */


/* STATIC FORWARD DECLARATIONS */

static char *char_widths = NULL;

static void render_highlights (HexWidget *self, cairo_t *cr, gint64 cursor_line,
		HexWidgetViewType type);
static void render_lines (HexWidget *self, cairo_t *cr, int min_lines, int max_lines,
		HexWidgetViewType type);

static void hex_widget_update_all_auto_highlights (HexWidget *self);

static void hex_widget_update_auto_highlight (HexWidget *self, HexWidgetAutoHighlight *ahl,
		gboolean delete, gboolean add);

static void recalc_displays (HexWidget *self);

static void show_cursor (HexWidget *self, gboolean show);


/* PROPERTIES - GETTERS AND SETTERS */

static void
hex_widget_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexWidget *self = HEX_WIDGET(object);

	switch (property_id)
	{
		case DOCUMENT:
			self->document = g_value_get_object (value);
			g_object_notify_by_pspec (G_OBJECT(self), properties[DOCUMENT]);
			break;

		case FADE_ZEROES:
			hex_widget_set_fade_zeroes (self, g_value_get_boolean (value));
			break;

		case DISPLAY_CONTROL_CHARACTERS:
			hex_widget_set_display_control_characters (self, g_value_get_boolean (value));
			break;

		case INSERT_MODE:
			hex_widget_set_insert_mode (self, g_value_get_boolean (value));
			break;

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_widget_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexWidget *self = HEX_WIDGET(object);

	switch (property_id)
	{
		case DOCUMENT:
			g_value_set_object (value, self->document);
			break;

		case FADE_ZEROES:
			g_value_set_boolean (value, hex_widget_get_fade_zeroes (self));
			break;

		case DISPLAY_CONTROL_CHARACTERS:
			g_value_set_boolean (value, hex_widget_get_display_control_characters (self));
			break;

		case INSERT_MODE:
			g_value_set_boolean (value, hex_widget_get_insert_mode (self));
			break;

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/* HexWidget - Method Definitions */

/* ACTIONS */

static void
copy_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	HexWidget *self = HEX_WIDGET(widget);

	hex_widget_copy_to_clipboard (self);
}

static void
cut_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	HexWidget *self = HEX_WIDGET(widget);

	hex_widget_cut_to_clipboard (self);
}

static void
paste_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	HexWidget *self = HEX_WIDGET(widget);

	hex_widget_paste_from_clipboard (self);
}

static void
redo_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	HexWidget *self = HEX_WIDGET(widget);
	HexChangeData *cd;

	g_return_if_fail (HEX_IS_DOCUMENT (self->document));

	if (hex_document_can_redo (self->document))
	{
		hex_document_redo (self->document);

		cd = hex_document_get_undo_data (self->document);

		hex_widget_set_cursor (self, cd->start);
		hex_widget_set_nibble (self, cd->lower_nibble);
	}
}

static void
doc_undo_redo_cb (HexDocument *doc, gpointer user_data)
{
	HexWidget *self = HEX_WIDGET(user_data);
	
	gtk_widget_action_set_enabled (GTK_WIDGET(self),
			"gtkhex.undo", hex_document_can_undo (doc));
	gtk_widget_action_set_enabled (GTK_WIDGET(self),
			"gtkhex.redo", hex_document_can_redo (doc));
}

static void
undo_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	HexWidget *self = HEX_WIDGET(widget);
	HexChangeData *cd;

	g_return_if_fail (HEX_IS_DOCUMENT (self->document));

	if (hex_document_can_undo (self->document))
	{
		cd = hex_document_get_undo_data (self->document);

		hex_document_undo (self->document);

		hex_widget_set_cursor (self, cd->start);
		hex_widget_set_nibble (self, cd->lower_nibble);
	}
}

static void
toggle_hex_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	HexWidget *self = HEX_WIDGET (widget);

	if (gtk_widget_get_visible (self->xdisp))
	{
		self->active_view = VIEW_HEX;
		gtk_widget_queue_draw (widget);
	}
}

static void
toggle_ascii_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	HexWidget *self = HEX_WIDGET (widget);

	if (gtk_widget_get_visible (self->adisp))
	{
		self->active_view = VIEW_ASCII;
		gtk_widget_queue_draw (widget);
	}
}

static void
update_geometry (HexWidget *self, gpointer data)
{
	if (gtk_check_button_get_active (GTK_CHECK_BUTTON(self->auto_geometry_checkbtn)))
		hex_widget_set_geometry (self, 0, 0);
	else
		hex_widget_set_geometry (self, gtk_spin_button_get_value (GTK_SPIN_BUTTON(self->cpl_spinbtn)), 0);
}

static void
geometry_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	HexWidget *self = HEX_WIDGET (widget);

	gtk_popover_popup (GTK_POPOVER(self->geometry_popover));
}

static void
move_to_buffer_ends_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	HexWidget *self = HEX_WIDGET (widget);
	gboolean beginning, extend_selection;

	g_variant_get (parameter, "(bb)", &beginning, &extend_selection);

	self->selecting = extend_selection;
	hex_widget_set_cursor (self, beginning ? 0 : HEX_BUFFER_PAYLOAD (self->document));
}

/*
 * ?_to_pointer translates mouse coordinates in hex/ascii view
 * to cursor coordinates.
 */
static int
hex_layout_index_to_line_byte_offset (PangoLayout *layout,
		int hex_layout_index,
		gboolean *lower_nibble)
{
	const char *str = pango_layout_get_text (layout);

	g_assert (str);

	for (int i = 0, hex_char_ct = 0, retval = 0; i < strlen (str); ++i)
	{
		if (str[i] == ' ')
			continue;

		++hex_char_ct;

		if (i >= hex_layout_index)
		{
			if (lower_nibble != NULL)
				*lower_nibble = (hex_char_ct % 2 == 0);

			return retval;
		}

		if (hex_char_ct % 2 == 0)
			++retval;
	}

	return 0;
}

static void
hex_to_pointer (HexWidget *self, int mx, int my)
{
	const gint64 cy = self->top_line + my/self->char_height;
	PangoLayout *layout = self->xlayout_top_line_cache;
	int cx = 0;
	int index = 0;
	gboolean lower_nibble = FALSE;
	
	pango_layout_xy_to_index (layout, mx * PANGO_SCALE, 0, &index, NULL);

	cx = hex_layout_index_to_line_byte_offset (layout, index, &lower_nibble);

	hex_widget_set_cursor_by_row_and_col (self, cx, cy);
	hex_widget_set_nibble (self, lower_nibble);
}

static void
ascii_to_pointer (HexWidget *self, int mx, int my)
{
	const gint64 cy = self->top_line + my/self->char_height;
	int cx = 0;
	PangoLayout *layout = self->alayout_top_line_cache;

	/*cx = */ pango_layout_xy_to_index (layout, mx * PANGO_SCALE, 0, &cx, NULL);
	hex_widget_set_cursor_by_row_and_col (self, cx, cy);
}

static int
get_char_height (HexWidget *self)
{
	PangoContext *context;
	PangoFontMetrics *metrics;
	int height;

	context = gtk_widget_get_pango_context (GTK_WIDGET(self));
	metrics = pango_context_get_metrics (context, NULL, NULL);

	height =
		PANGO_PIXELS(pango_font_metrics_get_height (metrics));
	
	return height;
}

static int
get_char_width (HexWidget *self)
{
	PangoContext *context;
	PangoFontMetrics *metrics;
	int width;

	context = gtk_widget_get_pango_context (GTK_WIDGET(self));
	metrics = pango_context_get_metrics (context, NULL, NULL);

	/* generally the digit width returned will be bigger, but let's take
	 * the max for now and run with it.
	 */
	width = MAX (pango_font_metrics_get_approximate_digit_width (metrics),
			pango_font_metrics_get_approximate_char_width (metrics));

	/* scale down from pango units to pixels */
	width = PANGO_PIXELS(width);
	
	/* update layout manager */
	if (HEX_IS_WIDGET_LAYOUT (self->layout_manager)) {
		hex_widget_layout_set_char_width (HEX_WIDGET_LAYOUT(self->layout_manager),
				width);
	}
	return width;
}

/*
 * format_[x|a]block() formats contents of the buffer (out) into displayable
 * text in hex or ascii, respectively.
 * Returns: length of resulting number of bytes/characters in buffer.
 */
static int
format_xblock (HexWidget *self, gint64 start, gint64 end)
{
	int low, high;
	gunichar c;

	for (gint64 i = start + 1; i <= end; ++i)
	{
		c = hex_widget_get_byte (self, i - 1);

		low = c & 0x0F;
		low = (low < 10) ? (low + '0') : (low - 10 + 'A');

		high = (c & 0xF0) >> 4;
		high = (high < 10) ? (high + '0') : (high - 10 + 'A');

		g_array_append_val (self->disp_buffer, high);
		g_array_append_val (self->disp_buffer, low);

		if (i % self->group_type == 0)
		{
			gunichar spc = ' ';
			g_array_append_val (self->disp_buffer, spc);
		}
	}
	return self->disp_buffer->len;
}

static int
format_ablock (HexWidget *self, gint64 start, gint64 end)
{
	gint64 i;
	gunichar c;

	for (gint64 i = start; i < end; ++i)
	{
		c = hex_widget_get_byte (self, i);

		if (is_displayable(c))
			g_array_append_val (self->disp_buffer, c);
		else if (is_control_character(c) && self->display_control_characters) {
			c = control_characters_lookup_table[c];
			g_array_append_val (self->disp_buffer, c);
		}
		else {
			c = '.';
			g_array_append_val (self->disp_buffer, c);
		}
	}

	return end - start;
}

/*
 * get_[x|a]coords() translates offset from the beginning of
 * the block into x,y coordinates of the xdisp/adisp, respectively
 */
static gboolean
get_xcoords (HexWidget *self, gint64 pos, int *x, int *y)
{
	gint64 cx, cy, spaces;
	
	if (self->cpl == 0)
		return FALSE;
	
	cy = pos / self->cpl;
	cy -= self->top_line;

	if (cy < 0)
		return FALSE;
	
	cx = 2 * (pos % self->cpl);
	spaces = (pos % self->cpl) / self->group_type;
	
	cx *= self->char_width;
	cy *= self->char_height;
	spaces *= self->char_width;

	/* nb: Having this as an assertion warning is annoying. Happens a lot
	 * in that case if you scroll way up/down in a big file and suddenly start
	 * dragging. */
	if (cx + spaces > INT_MAX && cy > INT_MAX)
		return FALSE;

	*x = cx + spaces;
	*y = cy;
	
	return TRUE;
}

static gboolean
get_acoords (HexWidget *self, gint64 pos, int *x, int *y)
{
	gint64 cx, cy;
	
	if (self->cpl == 0)
		return FALSE;

	cy = pos / self->cpl;
	cy -= self->top_line;

	if (cy < 0)
		return FALSE;

	cy *= self->char_height;
	cx = self->char_width*(pos % self->cpl);
	
	/* nb: Having this as an assertion warning is annoying. Happens a lot
	 * in that case if you scroll way up/down in a big file and suddenly start
	 * dragging. */
	if (cy > INT_MAX && cx > INT_MAX)
		return FALSE;

	*x = cx;
	*y = cy;
	
	return TRUE;
}

static void
invalidate_xc (HexWidget *self)
{
	GtkWidget *widget = self->xdisp;
	int cx, cy;

	if (get_xcoords (self, self->cursor_pos, &cx, &cy))
	{
		if (self->lower_nibble)
			cx += self->char_width;

		gtk_widget_queue_draw (widget);
	}
}

static void
invalidate_ac (HexWidget *self)
{
	GtkWidget *widget = self->adisp;
	int cx, cy;

	if (get_acoords (self, self->cursor_pos, &cx, &cy))
	{
		gtk_widget_queue_draw (widget);
	}
}

inline static int
char_index_to_utf8_byte_index (const char *str, int index)
{
	return g_utf8_offset_to_pointer (str, index) - str;
}

static void
render_cursor (HexWidget *self,
		cairo_t *cr,
		GtkWidget *widget,
		PangoLayout *layout,
		int y,
		HexWidgetViewType cursor_type)
{
	GdkRGBA fg_color;
	gint64 index;
	int range[2];
	GtkStateFlags state;
	GtkStyleContext *context;
	cairo_region_t *region;
	double x1, x2, y1, y2;
	gboolean at_file_end = FALSE;
	gboolean at_new_row = FALSE;

	g_return_if_fail (gtk_widget_get_realized (widget));

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);
	gtk_style_context_get_color (context, &fg_color);

	/* Find out if we're at the end of the row and/or the end of the file,
	 * since this will make a difference when in insert mode
	 */
	if (self->cursor_pos >= HEX_BUFFER_PAYLOAD (self->document))
		at_file_end = TRUE;

	if (self->cursor_pos % self->cpl == 0)
		at_new_row = TRUE;

	if (cursor_type == VIEW_HEX)
	{
		int spaces;

		spaces = (self->cursor_pos % self->cpl) / self->group_type;
		index = 2 * (self->cursor_pos % self->cpl);
		index += spaces;

		if (self->lower_nibble) {
			range[0] = index + 1;
			range[1] = index + 2;
		} else {
			range[0] = index;
			range[1] = index + 1;
		}
	}
	else	/* VIEW_ASCII */
	{
		index = self->cursor_pos % self->cpl;
		range[0] = index;
		range[1] = index + 1;
	}

	if (self->insert && at_file_end)
	{
		if (at_new_row) {
			range[0] = 0;
			range[1] = 1;
		} else {
			--range[0];
			--range[1];
		}
	}

	range[0] = char_index_to_utf8_byte_index (pango_layout_get_text (layout), range[0]);
	range[1] = char_index_to_utf8_byte_index (pango_layout_get_text (layout), range[1]);

	region = gdk_pango_layout_get_clip_region (layout,
			0,	/* x */
			y,
			range,
			1);

	if (self->insert && at_file_end && !at_new_row)
	{
		cairo_rectangle_int_t tmp_rect = { 0 };

		cairo_region_get_extents (region, &tmp_rect);
		cairo_region_translate (region, tmp_rect.width, 0);
	}

	gdk_cairo_region (cr, region);
	cairo_save (cr);
	cairo_clip (cr);
	cairo_clip_extents (cr, &x1, &y1, &x2, &y2);

	/* - If don't have focus: draw dashed square.
	 * - If have focus, draw filled in square (colour defined w/ css using
	 *   `:checked` pseudoclass) if the pane we're drawing is selected;
	 *   otherwise, draw a solid square.
	 */
	if (! gtk_widget_has_focus (GTK_WIDGET(self)))
	{
		cairo_save (cr);
		cairo_set_source_rgba (cr,
				fg_color.red, fg_color.green, fg_color.blue, 0.75);

		cairo_set_line_width (cr, 1.5);
		cairo_set_dash (cr, (double[]){1.0}, 1, 0.0);

		cairo_rectangle (cr, x1, y1, ABS(x2-x1), ABS(y2-y1));

		cairo_stroke (cr);
		cairo_restore (cr);
	}
	else if (self->active_view == cursor_type)
	{
		gtk_style_context_save (context);

		state |= GTK_STATE_FLAG_CHECKED;
		gtk_style_context_set_state (context, state);

		gtk_render_background (context, cr,
				x1,
				y1,
				ABS(x2-x1), 
				ABS(y2-y1));

		/* Don't render layout if we're in insert mode and at file end, since
		 * the cursor should be blank at that point.
		 */
		if (at_file_end && self->insert)
			;
		else
			gtk_render_layout (context, cr, 0, y, layout);

		gtk_style_context_restore (context);
	}
	else
	{
		cairo_save (cr);
		cairo_set_source_rgba (cr,
				fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
		cairo_set_line_width (cr, 1.5);

		cairo_rectangle (cr, x1, y1, ABS(x2-x1), ABS(y2-y1));

		cairo_stroke (cr);
		cairo_restore (cr);
	}
	cairo_restore (cr);		/* from cairo_clip above */
}

static void
show_cursor (HexWidget *self, gboolean show)
{
	if (self->cursor_shown == show)
		return;
	
	if (gtk_widget_get_realized (self->xdisp) ||
			gtk_widget_get_realized (self->adisp))
	{
		invalidate_xc (self);
		invalidate_ac (self);
	}
	self->cursor_shown = show;
}

inline static int
get_hex_cpl (HexWidget *self)
{
	int hex_cpl;

	if (! self->default_cpl)
		hex_cpl = hex_widget_layout_get_hex_cpl (
				HEX_WIDGET_LAYOUT(self->layout_manager));
	else
		hex_cpl = hex_widget_layout_util_hex_cpl_from_ascii_cpl (
				self->default_cpl, self->group_type);

	return hex_cpl;
}

inline static void
update_highlight_start_and_end_lines (HexWidget *self, HexWidget_Highlight *hl)
{
	hl->start_line = MIN(hl->start, hl->end) / self->cpl - self->top_line;
	hl->end_line = MAX(hl->start, hl->end) / self->cpl - self->top_line;
}

inline static void
render_highlight (HexWidget *self,
		cairo_t *cr,
		HexWidgetViewType type,
		HexWidget_Highlight *highlight,
		HexWidgetAutoHighlight *auto_highlight,	/* can NULL */
		HexWidgetMark *mark,					/* can NULL */
		gint64 cursor_line)
{
	GtkWidget *widget;		/* shorthand for the hex or ascii drawing area */
	PangoLayout *layout;	/* shorthand for the hex or ascii pango layout */
	int hex_cpl = get_hex_cpl (self);
	int y = cursor_line * self->char_height;
	GtkStyleContext *context;
	GtkStateFlags state;
	gint64 cursor_off = 0;
	size_t len;
	int range[2];
	double x1, x2, y1, y2;
	cairo_region_t *region;

	/* Shorthands for readability of this loop */
	gint64 start, end, start_line, end_line;

	if (type == VIEW_HEX)
	{
		widget = self->xdisp;
		layout = self->xlayout;
	}
	else
	{
		widget = self->adisp;
		layout = self->alayout;
	}

	context = gtk_widget_get_style_context (widget);
	gtk_style_context_save (context);

	state = gtk_style_context_get_state (context);
	if (auto_highlight)
		state |= GTK_STATE_FLAG_LINK;
	else if (mark)
		state |= GTK_STATE_FLAG_VISITED;
	else
	{
		if (gtk_widget_has_focus (GTK_WIDGET(self)))
		{
			state |= GTK_STATE_FLAG_FOCUS_WITHIN;
		}
		state |= GTK_STATE_FLAG_SELECTED;
	}

	gtk_style_context_set_state (context, state);

	update_highlight_start_and_end_lines (self, highlight);

	start = MIN(highlight->start, highlight->end);
	end = MAX(highlight->start, highlight->end);
	start_line = highlight->start_line;
	end_line = highlight->end_line;

	if (start == end)
		goto out;

	if (cursor_line == start_line)
	{
		if (type == VIEW_HEX) {
			cursor_off = 2 * (start % self->cpl) +
				(start % self->cpl) / self->group_type;

			if (cursor_line == end_line)
				len = 2 * (end % self->cpl + 1) +
					(end % self->cpl) / self->group_type;
			else
				len = hex_cpl;

			len = len - cursor_off;
		}
		else {	/* VIEW_ASCII */
			cursor_off = start % self->cpl;

			if (cursor_line == end_line)
				len = end - start + 1;
			else
				len = self->cpl - cursor_off;
		}

		range[0] = cursor_off;
		range[1] = cursor_off + len;
	}
	else if (cursor_line == end_line)
	{
		if (type == VIEW_HEX) {
			cursor_off = 2 * (end % self->cpl + 1) +
				(end % self->cpl) / self->group_type;
		}
		else {	/* VIEW_ASCII */
			cursor_off = end % self->cpl + 1;
		}

		range[0] = 0;
		range[1] = cursor_off;
	}
	else if (cursor_line > start_line && cursor_line < end_line)
	{
		int cpl = (type == VIEW_HEX) ? hex_cpl : self->cpl;

		range[0] = 0;
		range[1] = cpl;
	}
	else
		goto out;

	range[0] = char_index_to_utf8_byte_index (pango_layout_get_text (layout), range[0]);
	range[1] = char_index_to_utf8_byte_index (pango_layout_get_text (layout), range[1]);
	
	cairo_save (cr);

	region = gdk_pango_layout_get_clip_region (layout,
			0,	/* x */
			y,
			range,
			1);

	gdk_cairo_region (cr, region);
	cairo_clip (cr);
	cairo_clip_extents (cr, &x1, &y1, &x2, &y2);

	if (mark && mark->have_custom_color)
	{
		cairo_save (cr);
		cairo_set_source_rgba (cr,
				mark->custom_color.red,
				mark->custom_color.green,
				mark->custom_color.blue,
				mark->custom_color.alpha);
		cairo_rectangle (cr, x1, y1, ABS(x2-x1), ABS(y2-y1));
		cairo_fill (cr);
		cairo_restore (cr);
	}
	else
	{
		gtk_render_background (context, cr,
				x1,
				y1,
				ABS(x2-x1),
				ABS(y2-y1));
	}

	cairo_restore (cr);

out:
	gtk_style_context_restore (context);
}

static void
render_highlights (HexWidget *self,
		cairo_t *cr,
		gint64 cursor_line,
		HexWidgetViewType type)
{
	/* Render primary highlighted selection */
	render_highlight (self, cr, type, &self->selection, NULL, NULL, cursor_line);

	/* Render auto highlights */
	for (guint i = 0; i < self->auto_highlights->len; ++i)
	{
		HexWidgetAutoHighlight *auto_highlight = g_ptr_array_index (self->auto_highlights, i);

		for (guint j = 0; j < auto_highlight->highlights->len; ++j)
		{
			HexWidget_Highlight *highlight = g_ptr_array_index (auto_highlight->highlights, j);

			render_highlight (self, cr, type, highlight, auto_highlight, NULL, cursor_line);
		}
	}

	/* Render marks */
	for (guint i = 0; i < self->marks->len; ++i)
	{
		HexWidgetMark *mark = g_ptr_array_index (self->marks, i);

		render_highlight (self, cr, type, &mark->highlight, NULL, mark, cursor_line);
	}
}

inline static void
fade_zeroes (HexWidget *self, PangoLayout *layout)
{
	GtkWidget *widget = GTK_WIDGET(self);
	GtkStyleContext *context = gtk_widget_get_style_context (widget);
	GtkStateFlags state = gtk_widget_get_state_flags (widget);
	const int color_modifier = UINT16_MAX * 0.4;
	const char *text = pango_layout_get_text (layout);
	PangoAttrList *list = pango_attr_list_new ();
	GdkRGBA fg_color;

	gtk_style_context_get_color (context, &fg_color);

	for (int i = 0; text[i]; ++i)
	{
		if (text[i] == '0' && text[i+1] == '0')
		{
			PangoAttribute *attr = pango_attr_foreground_new (
					PANGO_COLOR_FROM_FLOAT(fg_color.red) + color_modifier,
					PANGO_COLOR_FROM_FLOAT(fg_color.green) + color_modifier,
					PANGO_COLOR_FROM_FLOAT(fg_color.blue) + color_modifier);

			attr->start_index = i;
			attr->end_index = i+2;

			pango_attr_list_insert (list, attr);
		}
	}
	pango_layout_set_attributes (layout, list);
	pango_attr_list_unref (list);
}

/*
 * when calling render_lines() the min_lines and max_lines arguments are the
 * numbers of the first and last line TO BE DISPLAYED in the range
 * [0 .. self->vis_lines-1] AND NOT [0 .. self->lines]!
 */

#define DO_RENDER_CURSOR \
			render_cursor (self, cr, widget, layout,				\
					cursor_line * self->char_height,	/* y */		\
					type);
static void
render_lines (HexWidget *self,
		cairo_t *cr,
		int min_lines,
		int max_lines,
		HexWidgetViewType type)
{
	GtkWidget *widget;
	GtkStateFlags state;
	PangoLayout *layout;
	PangoLayout *layout_top_line_cache;
	int (*block_format_func) (HexWidget *self, gint64 start, gint64 end);
	int block_format_len;
	GtkAllocation allocation;
	GtkStyleContext *context;
	gint64 cursor_line;
	int cpl;
	gboolean cursor_drawn = FALSE;

	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET(self)));
	g_return_if_fail (self->cpl);

	/* Setup variables depending on widget type (hex or ascii) */

	if (type == VIEW_HEX)
	{
		widget = self->xdisp;
		layout = self->xlayout;
		layout_top_line_cache = self->xlayout_top_line_cache;
		block_format_func = format_xblock;

		if (! self->default_cpl)
			cpl = hex_widget_layout_get_hex_cpl (
					HEX_WIDGET_LAYOUT(self->layout_manager));
		else
			cpl = hex_widget_layout_util_hex_cpl_from_ascii_cpl (
					self->default_cpl, self->group_type);

	}
	else	/* VIEW_ASCII */
	{
		widget = self->adisp;
		layout = self->alayout;
		layout_top_line_cache = self->alayout_top_line_cache;
		block_format_func = format_ablock;
		cpl = self->cpl;
	}

	/* Grab styling and render the (blank) background. */

	context = gtk_widget_get_style_context (widget);
	cursor_line = self->cursor_pos / self->cpl - self->top_line;
	gtk_widget_get_allocation (widget, &allocation);

	gtk_render_background (context, cr,
			/* x: */		0,
			/* y: */		min_lines * self->char_height,
			/* width: */	allocation.width,
			/* height: */	(max_lines - min_lines + 1) * self->char_height);

	/* If it's a blank canvass, we know there'll be no lines to render. But we
	 * draw a single space in the pango layout as a workaround for the fact
	 * that it will have no context as to how large to draw the cursor.
	 */
	if (HEX_BUFFER_PAYLOAD (self->document) == 0 &&
			! hex_document_has_changed (self->document))
	{
		pango_layout_set_text (layout, " ", -1);
		pango_layout_set_text (layout_top_line_cache,
				pango_layout_get_text (layout), -1);
		goto no_more_lines_to_draw;
	}

	/* Work out how many lines need to be drawn, and draw them from the
	 * buffer.
	 */
	max_lines = MIN(max_lines, self->vis_lines);
	max_lines = MIN(max_lines, self->lines);

	block_format_len = block_format_func (self,
			(self->top_line + min_lines) * self->cpl,
			MIN( (self->top_line + max_lines + 1) * self->cpl,
				HEX_BUFFER_PAYLOAD (self->document) ));
	
	for (int i = min_lines; i <= max_lines; i++)
	{
		int tmp = block_format_len - ((i - min_lines) * cpl);
		char *disp_buffer_as_utf8 = NULL;

		if (tmp <= 0)
			break;

		/* Get current line in the buffer as a utf-8 string
		 */
		disp_buffer_as_utf8 = g_ucs4_to_utf8 (
				(gunichar *)self->disp_buffer->data + (i - min_lines) * cpl,
				CLAMP (MIN (cpl, tmp), 0, self->disp_buffer->len),
				NULL, NULL, NULL);

		pango_layout_set_text (layout, disp_buffer_as_utf8, -1);

		if (i == min_lines) {
			pango_layout_set_text (layout_top_line_cache,
					pango_layout_get_text (layout), -1);
		}

		g_free (disp_buffer_as_utf8);

		if (type == VIEW_HEX && self->fade_zeroes)
			fade_zeroes (self, layout);

		render_highlights (self, cr, i, type);

		gtk_render_layout (context, cr,
				/* x: */ 0,
				/* y: */ i * self->char_height,
				layout);

		if (i == cursor_line)
		{
			DO_RENDER_CURSOR
			cursor_drawn = TRUE;
		}
	}

no_more_lines_to_draw:
	if (! cursor_drawn && cursor_line <= self->vis_lines)
	{
		DO_RENDER_CURSOR
	}
}
#undef DO_RENDER_CURSOR

#define BUFLEN	32
#define MIN_CPL	8
static void
render_offsets (HexWidget *self,
                cairo_t *cr,
                int min_lines,
                int max_lines)
{
	GtkWidget *widget = self->offsets;
	GdkRGBA fg_color;
	GtkAllocation allocation;
	GtkStyleContext *context;
	char buf[BUFLEN] = {0};
	char fmt[BUFLEN] = {0};
	char off_str[BUFLEN];
	int off_cpl;

	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (self)));

	context = gtk_widget_get_style_context (widget);

	gtk_widget_get_allocation (widget, &allocation);

	gtk_render_background (context, cr,
			/* x: */		0,
			/* y: */		min_lines * self->char_height,
			/* width: */	allocation.width,
			/* height: */	(max_lines - min_lines + 1) * self->char_height);
  
	/* update max_lines and min_lines */
	max_lines = MIN(max_lines, self->vis_lines);
	max_lines = MIN(max_lines, self->lines - self->top_line - 1);

	/* find out how many chars wide our offset column should be based on
	 * how many chars will be in the last offset marker of the screen */
	
	snprintf (buf, BUFLEN-1, "%lX",
			MAX ((self->top_line + max_lines), 0) * self->cpl);
	off_cpl = MAX (MIN_CPL, strlen (buf));
	snprintf (fmt, BUFLEN-1, "%%0%dlX", off_cpl); 
	hex_widget_layout_set_offset_cpl (HEX_WIDGET_LAYOUT(self->layout_manager), off_cpl);

	for (int i = min_lines; i <= max_lines; i++)
	{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
		/* generate offset string and place in temporary buffer */
		sprintf(off_str, fmt,
				(self->top_line + i) * self->cpl + STARTING_OFFSET);
#pragma GCC diagnostic pop

		/* build pango layout for offset line; draw line with gtk. */
		pango_layout_set_text (self->olayout, off_str, off_cpl);
		gtk_render_layout (context, cr,
				/* x: */ 0,
				/* y: */ i * self->char_height,
				self->olayout);
	}
}
#undef BUFLEN
#undef MIN_CPL

static void
allocate_display_buffer (HexWidget *self, HexWidgetViewType type)
{
	GtkWidget *widget = GTK_WIDGET (self);

	g_clear_pointer (&self->disp_buffer, g_array_unref);

	self->disp_buffer = g_array_new (FALSE, FALSE, sizeof (gunichar));
}

/* draw_func for the hex drawing area
 */
static void
hex_draw (GtkDrawingArea *drawing_area,
                           cairo_t *cr,
                           int width,
                           int height,
                           gpointer user_data)
{
	HexWidget *self = HEX_WIDGET(user_data);

	/* Now that we have self->cpl defined, run this function to bump all
	 * required values:
	 */
	recalc_displays (self);
	allocate_display_buffer (self, VIEW_HEX);

	/* Finally, we can do what we wanted to do to begin with: draw our hex
	 * lines!
	 */
	render_lines (self, cr, 0, self->vis_lines, VIEW_HEX);
}

static void
ascii_draw (GtkDrawingArea *drawing_area,
                           cairo_t *cr,
                           int width,
                           int height,
                           gpointer user_data)
{
	HexWidget *self = HEX_WIDGET(user_data);

	recalc_displays (self);
	allocate_display_buffer (self, VIEW_ASCII);
	render_lines (self, cr, 0, self->vis_lines, VIEW_ASCII);
}

static void
offsets_draw (GtkDrawingArea *drawing_area,
                           cairo_t *cr,
                           int width,
                           int height,
                           gpointer user_data)
{
	HexWidget *self = HEX_WIDGET(user_data);

	recalc_displays (self);
	render_offsets (self, cr, 0, self->vis_lines);
}

/*
 * this calculates how many bytes we can stuff into one line and how many
 * lines we can display according to the current size of the widget
 */
static void
recalc_displays (HexWidget *self)
{
	GtkWidget *widget = GTK_WIDGET (self);
	gint64 payload_size;

	payload_size = HEX_BUFFER_PAYLOAD (self->document);

	/*
	 * Only change the value of the adjustment to put the cursor on screen
	 * if the cursor is currently within the displayed portion.
	 */
	if (payload_size == 0 || self->cpl == 0)
		self->lines = 1;
	else {
		self->lines = payload_size / self->cpl;
		if (payload_size % self->cpl)
			self->lines++;
	}
}

static void
recalc_scrolling (HexWidget *self)
{
	gboolean scroll_to_cursor;
	double value;

	if (self->cpl == 0)
		return;

	value = MIN (self->top_line, self->lines - self->vis_lines);
	/* clamp value */
	value = MAX (0, value);

	scroll_to_cursor = ((self->cursor_pos / self->cpl >= gtk_adjustment_get_value (self->adj)) &&
		(self->cursor_pos / self->cpl <= gtk_adjustment_get_value (self->adj) + self->vis_lines - 1) &&
	    ((self->cursor_pos / self->cpl < value) ||
			(self->cursor_pos / self->cpl > value + self->vis_lines - 1)));

	if (scroll_to_cursor)
	{
		value = MIN (self->cursor_pos / self->cpl, self->lines - self->vis_lines);
		value = MAX (0, value);
	}

	/* adjust the scrollbar and display position to new values */
	gtk_adjustment_configure (self->adj,
	                          value,             /* value */
	                          0,                 /* lower */
	                          self->lines,         /* upper */
	                          1,                 /* step increment */
	                          self->vis_lines - 1, /* page increment */
	                          self->vis_lines      /* page size */);
}

/*
 * takes care of xdisp and adisp scrolling
 * connected to value-changed signal of scrollbar's GtkAdjustment
 */
static void
adj_value_changed_cb (GtkAdjustment *adj, HexWidget *self)
{
	int dx, dy;
	
	if (! gtk_widget_is_drawable (self->xdisp) &&
			! gtk_widget_is_drawable (self->adisp))
		return;

	self->top_line = gtk_adjustment_get_value (adj);

	gtk_widget_queue_draw (GTK_WIDGET(self));
}

/*
 * mouse signal handlers (button 1 and motion) for both displays
 */
static gboolean
scroll_timeout_handler (HexWidget *self)
{
	if (self->scroll_dir < 0)
	{
		hex_widget_set_cursor (self,
				MAX (0, self->cursor_pos - self->cpl));
	}
	else if (self->scroll_dir > 0)
	{
		hex_widget_set_cursor (self,
				MIN (HEX_BUFFER_PAYLOAD (self->document) - 1,
						self->cursor_pos + self->cpl));
	}
	return G_SOURCE_CONTINUE;
}

static gboolean
scroll_cb (GtkEventControllerScroll *controller,
               double                    dx,
               double                    dy,
               gpointer                  user_data)
{
	HexWidget *self = HEX_WIDGET (user_data);
	double old_value, new_value;

	old_value = gtk_adjustment_get_value (self->adj);
	new_value = old_value + dy;

	gtk_adjustment_set_value (self->adj, new_value);

	/* TFM: returns true if scroll event was handled; false otherwise.
	 */
	return TRUE;
}

/* Helper function for *_pressed_cb 's
 */
static void
pressed_gesture_helper (HexWidget *self,
		GtkGestureClick *gesture,
		double x,
		double y,
		HexWidgetViewType type)
{
	GtkWidget *widget;
	guint button;

	if (type == VIEW_HEX)
		widget = self->xdisp;
	else
		widget = self->adisp;

	button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE(gesture));

	/* Single-press */
	if (button == GDK_BUTTON_PRIMARY)
	{
		if (! gtk_widget_has_focus (widget)) 
			gtk_widget_grab_focus (GTK_WIDGET(self));
		
		self->button = button;
		
		if (self->active_view == type)
		{
			if (type == VIEW_HEX)
				hex_to_pointer (self, x, y);
			else
				ascii_to_pointer (self, x, y);

			if (! self->selecting)
			{
				self->selecting = TRUE;
				hex_widget_set_selection (self, self->cursor_pos, self->cursor_pos);
			}
		}
		else
		{
			show_cursor (self, FALSE);
			self->active_view = type;
			show_cursor (self, TRUE);
			pressed_gesture_helper (self, gesture, x, y, type);
		}
	}
	else
	{
		self->button = 0;
	}
}

static void
update_primary_selection (HexWidget *self)
{
	GtkWidget *widget = GTK_WIDGET(self);
	GdkClipboard *clipboard;

	if (! gtk_widget_get_realized (widget))
		return;

	clipboard = gtk_widget_get_primary_clipboard (widget);

	if (self->selection.start != self->selection.end)
	{
		gdk_clipboard_set_content (clipboard, self->selection_content);
	}
	else
	{
		if (gdk_clipboard_get_content (clipboard) == self->selection_content)
			gdk_clipboard_set_content (clipboard, NULL);
	}
}

inline static size_t
clamp_rep_len (HexWidget *self, size_t start_pos, size_t rep_len)
{
	size_t payload_size = HEX_BUFFER_PAYLOAD (self->document);

	if (rep_len > payload_size - start_pos)
		rep_len = payload_size - start_pos;

	return rep_len;
}

static void
paste_set_data (HexWidget *self, char *data, size_t data_len)
{
	size_t start_pos = self->cursor_pos;
	size_t len = data_len;
	size_t rep_len = 0;

	if (self->selection.start != self->selection.end) {
		size_t selection_len, end_pos;

		start_pos = MIN(self->selection.start, self->selection.end);
		end_pos = MAX(self->selection.start, self->selection.end);
		selection_len = end_pos - start_pos + 1;

		if (self->insert) {
			rep_len = selection_len;
		} else {
			len = rep_len = MIN(selection_len, len);
		}
	}

	if (! self->insert)
		len = rep_len = clamp_rep_len (self, start_pos, len);

	hex_document_set_data (self->document,
			start_pos,
			len,
			/* rep_len is either:
			 * 0 (insert w/o replacing),
			 * len (delete and paste the same number of bytes),
			 * or a different number (delete and paste an arbitrary number
			 * of bytes)
			 */
			rep_len,
			data,
			TRUE);

	hex_widget_set_cursor (self, start_pos + len);
}

static void
plaintext_paste_received_cb (GObject *source_object,
		GAsyncResult *result,
		gpointer user_data)
{
	HexWidget *self = HEX_WIDGET(user_data);
	GdkClipboard *clipboard;
	char *text;
	GError *error = NULL;

	g_debug ("%s: We DON'T have HexPasteData. Falling back to plaintext paste",
			__func__);

	clipboard = GDK_CLIPBOARD (source_object);

	/* Get the resulting text of the read operation */
	text = gdk_clipboard_read_text_finish (clipboard, result, &error);

	if (text)
	{
		paste_set_data(self, text, strlen(text));
		g_free(text);
	}
	else
	{
		g_critical ("Error pasting text: %s",
				error->message);
		g_error_free (error);
	}
}

static void
paste_helper (HexWidget *self, GdkClipboard *clipboard)
{
	GdkContentProvider *content;
	GValue value = G_VALUE_INIT;
	HexPasteData *paste;
	gboolean have_hex_paste_data = FALSE;

	content = gdk_clipboard_get_content (clipboard);
	g_value_init (&value, HEX_TYPE_PASTE_DATA);

	/* If the clipboard contains our special HexPasteData, we'll use it.
	 * If not, just fall back to plaintext.
	 */
	have_hex_paste_data = content ?
		gdk_content_provider_get_value (content, &value, NULL) : FALSE;

	if (have_hex_paste_data)
	{
		char *doc_data;
		size_t start_pos, len, rep_len;

		g_debug("%s: We HAVE HexPasteData.", __func__);

		paste = HEX_PASTE_DATA(g_value_get_object (&value));
		doc_data = hex_paste_data_get_doc_data (paste);
		len = hex_paste_data_get_elems (paste);

		paste_set_data(self, doc_data, len);
	}
	else
	{
		gdk_clipboard_read_text_async (clipboard,
				NULL,	/* cancellable */
				plaintext_paste_received_cb,
				self);
	}
}

static void
released_gesture_helper (HexWidget *self,
		GtkGestureClick *gesture,
		int				n_press,
		double			x,
		double			y,
		HexWidgetViewType type)
{
	guint button = gtk_gesture_single_get_current_button (
			GTK_GESTURE_SINGLE(gesture));

	/* Single-click */
	if (button == GDK_BUTTON_PRIMARY && n_press == 1)
	{
		if (self->scroll_timeout != 0)
		{
			g_source_remove (self->scroll_timeout);
			self->scroll_timeout = 0;
			self->scroll_dir = 0;
		}

		update_primary_selection (self);
		self->selecting = FALSE;
		self->button = 0;
	}
	/* Single-click */
	else if (button == GDK_BUTTON_MIDDLE && n_press == 1)
	{
		GdkClipboard *primary = gtk_widget_get_primary_clipboard (GTK_WIDGET(self));
		g_debug ("%s: middle-click paste - TEST", __func__);
		paste_helper (self, primary);
	}
}

/* nb: this gesture is only associated with the right-click, so there is
 * no need to test for that. If the impl in _init changes, this will need
 * to change
 */
static void
gh_pressed_cb (GtkGestureClick *gesture,
	int			n_press,
	double		x,
	double		y,
	gpointer	user_data)
{
	HexWidget *self = HEX_WIDGET (user_data);

	hex_widget_layout_set_cursor_pos (HEX_WIDGET_LAYOUT(self->layout_manager), x, y);
	gtk_popover_popup (GTK_POPOVER(self->context_menu));
}

static void
hex_pressed_cb (GtkGestureClick *gesture,
	int			n_press,
	double		x,
	double		y,
	gpointer	user_data)
{
	HexWidget *self = HEX_WIDGET (user_data);

	pressed_gesture_helper (self, gesture, x, y, VIEW_HEX);
}

static void
hex_released_cb (GtkGestureClick *gesture,
               int              n_press,
               double           x,
               double           y,
               gpointer         user_data)
{
	HexWidget *self = HEX_WIDGET (user_data);

	released_gesture_helper (self, gesture, n_press, x, y, VIEW_HEX);
}

static void
drag_update_helper (HexWidget *self,
		GtkGestureDrag	*gesture,
		double			offset_x,
		double			offset_y,
		HexWidgetViewType	type)
{
	GtkWidget *widget;
	double start_x, start_y;
	double x, y;
	GtkAllocation allocation;

	if (type == VIEW_HEX)
		widget = self->xdisp;
	else
		widget = self->adisp;

	gtk_widget_get_allocation (widget, &allocation);
	gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);

	x = start_x + offset_x;
	y = start_y + offset_y;

	if (y < 0) {
		self->scroll_dir = -1;
	} else if (y >= allocation.height) {
		self->scroll_dir = 1;
	} else {
		self->scroll_dir = 0;
	}

	if (self->scroll_dir != 0) {
		if (! self->scroll_timeout) {
			self->scroll_timeout =
				g_timeout_add (SCROLL_TIMEOUT,
							  G_SOURCE_FUNC(scroll_timeout_handler),
							  self);
		}
		return;
	}
	else {
		if (self->scroll_timeout != 0) {
			g_source_remove (self->scroll_timeout);
			self->scroll_timeout = 0;
		}
	}
			
	if (self->active_view == type && self->button == GDK_BUTTON_PRIMARY)
	{
		if (type == VIEW_HEX)
			hex_to_pointer (self, x, y);
		else
			ascii_to_pointer (self, x, y);
	}
}

static void
hex_drag_update_cb (GtkGestureDrag *gesture,
               double          offset_x,
               double          offset_y,
               gpointer        user_data)
{
	HexWidget *self = HEX_WIDGET (user_data);

	drag_update_helper (self, gesture, offset_x, offset_y, VIEW_HEX);
}

/* ASCII Widget - click and drag callbacks. */

static void
ascii_pressed_cb (GtkGestureClick *gesture,
               int              n_press,
               double           x,
               double           y,
               gpointer         user_data)
{
	HexWidget *self = HEX_WIDGET (user_data);

	pressed_gesture_helper (self, gesture, x, y, VIEW_ASCII);
}

static void
ascii_released_cb (GtkGestureClick *gesture,
               int              n_press,
               double           x,
               double           y,
               gpointer         user_data)
{
	HexWidget *self = HEX_WIDGET (user_data);

	released_gesture_helper (self, gesture, n_press, x, y, VIEW_ASCII);
}

static void
ascii_drag_update_cb (GtkGestureDrag *gesture,
               double          offset_x,
               double          offset_y,
               gpointer        user_data)
{
	HexWidget *self = HEX_WIDGET (user_data);

	drag_update_helper (self, gesture, offset_x, offset_y, VIEW_ASCII);
}

static gboolean
key_press_cb (GtkEventControllerKey *controller,
               guint                  keyval,
               guint                  keycode,
               GdkModifierType        state,
               gpointer               user_data)
{
	HexWidget *self = HEX_WIDGET(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	gboolean ret = GDK_EVENT_PROPAGATE;
	size_t payload_size;

	g_return_val_if_fail (HEX_IS_DOCUMENT (self->document), FALSE);

	payload_size = HEX_BUFFER_PAYLOAD (self->document);

	show_cursor (self, FALSE);

	/* don't trample over Ctrl or Alt (reserved for actions) */
	if (state & (GDK_CONTROL_MASK | GDK_ALT_MASK)) {
		return FALSE;
	}

	if (state & GDK_SHIFT_MASK)
		self->selecting = TRUE;
	else
		self->selecting = FALSE;

	switch(keyval)
	{
		case GDK_KEY_BackSpace:
			if (self->cursor_pos > 0)
			{
				/* If we have only one character selected, what we want to do
				 * is delete the prior character. Otherwise, this should
				 * essentially work the same way as delete if we have a
				 * highlighted selection.
				 */
				if (! hex_widget_get_selection (self, NULL, NULL))
					hex_widget_set_cursor (self, self->cursor_pos - 1);

				if (self->insert)
					hex_widget_delete_selection (self);
				else
					hex_widget_zero_selection (self);

				if (self->selecting)
					self->selecting = FALSE;
				ret = GDK_EVENT_STOP;
			}
			break;

		case GDK_KEY_Delete:
			if (self->cursor_pos < payload_size)
			{
				if (self->insert)
					hex_widget_delete_selection (self);
				else
					hex_widget_zero_selection (self);

				hex_widget_set_cursor (self, self->cursor_pos);
				ret = GDK_EVENT_STOP;
			}
			break;

		case GDK_KEY_Up:
			hex_widget_set_cursor (self, self->cursor_pos - self->cpl);
			ret = GDK_EVENT_STOP;
			break;

		case GDK_KEY_Down:
			hex_widget_set_cursor (self, self->cursor_pos + self->cpl);
			ret = GDK_EVENT_STOP;
			break;

		case GDK_KEY_Page_Up:
			hex_widget_set_cursor (self, MAX (0, self->cursor_pos - self->vis_lines*self->cpl));
			ret = GDK_EVENT_STOP;
			break;

		case GDK_KEY_Page_Down:
			hex_widget_set_cursor(self, MIN (payload_size,
						self->cursor_pos + self->vis_lines*self->cpl));
			ret = GDK_EVENT_STOP;
			break;

		case GDK_KEY_Home:
		{
			gint64 line_beg = self->cursor_pos;

			while (line_beg % self->cpl != 0)
				--line_beg;

			hex_widget_set_cursor (self, line_beg);
			ret = GDK_EVENT_STOP;
		}
			break;

		case GDK_KEY_End:
		{
			gint64 line_end = self->cursor_pos;

			while (line_end % self->cpl != self->cpl - 1)
				++line_end;

			hex_widget_set_cursor (self, MIN (line_end, payload_size));
			ret = GDK_EVENT_STOP;
		}
			break;

		default:
			if (self->active_view == VIEW_HEX)
			{
				switch(keyval)
				{
					case GDK_KEY_Left:
						if (state & GDK_SHIFT_MASK) {
							hex_widget_set_cursor (self, self->cursor_pos - 1);
						}
						else {
							self->lower_nibble = !self->lower_nibble;
							if (self->lower_nibble)
								hex_widget_set_cursor (self, self->cursor_pos - 1);
						}
						ret = GDK_EVENT_STOP;
						break;

					case GDK_KEY_Right:
						if (self->cursor_pos >= payload_size) {
							ret = GDK_EVENT_STOP;
							break;
						}

						if (state & GDK_SHIFT_MASK) {
							hex_widget_set_cursor (self, self->cursor_pos + 1);
						}
						else {
							self->lower_nibble = !self->lower_nibble;
							if (!self->lower_nibble)
								hex_widget_set_cursor (self, self->cursor_pos + 1);
						}
						ret = GDK_EVENT_STOP;
						break;

					default:
						if (keyval >= '0' && keyval <= '9')
						{
							hex_document_set_nibble (self->document, keyval - '0',
									self->cursor_pos, self->lower_nibble,
									self->insert, TRUE);
							if (self->selecting)
								self->selecting = FALSE;
							self->lower_nibble = !self->lower_nibble;
							if (!self->lower_nibble)
								hex_widget_set_cursor (self, self->cursor_pos + 1);
							ret = GDK_EVENT_STOP;
						}
						else if (keyval >= 'A' && keyval <= 'F')
						{
							hex_document_set_nibble (self->document, keyval - 'A' + 10,
									self->cursor_pos, self->lower_nibble,
									self->insert, TRUE);
							if (self->selecting)
								self->selecting = FALSE;
							self->lower_nibble = !self->lower_nibble;
							if (!self->lower_nibble)
								hex_widget_set_cursor (self, self->cursor_pos + 1);
							ret = GDK_EVENT_STOP;
						}
						else if (keyval >= 'a' && keyval <= 'f')
						{
							hex_document_set_nibble (self->document, keyval - 'a' + 10,
									self->cursor_pos, self->lower_nibble,
									self->insert, TRUE);
							if (self->selecting)
								self->selecting = FALSE;
							self->lower_nibble = !self->lower_nibble;
							if (!self->lower_nibble)
								hex_widget_set_cursor (self, self->cursor_pos + 1);
							ret = GDK_EVENT_STOP;
						}
						else if (keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9)
						{
							hex_document_set_nibble (self->document, keyval - GDK_KEY_KP_0,
									self->cursor_pos, self->lower_nibble,
									self->insert, TRUE);
							if (self->selecting)
								self->selecting = FALSE;
							self->lower_nibble = !self->lower_nibble;
							if (!self->lower_nibble)
								hex_widget_set_cursor (self, self->cursor_pos + 1);
							ret = GDK_EVENT_STOP;
						}
						break;      
				}
			}
			else	/* VIEW_ASCII */
			{
				switch (keyval)
				{
					case GDK_KEY_Left:
						hex_widget_set_cursor (self, self->cursor_pos - 1);
						ret = GDK_EVENT_STOP;
						break;

					case GDK_KEY_Right:
						hex_widget_set_cursor (self, self->cursor_pos + 1);
						ret = GDK_EVENT_STOP;
						break;

					default:
						if (is_displayable (keyval))
						{
							hex_document_set_byte (self->document, keyval,
									self->cursor_pos, self->insert, TRUE);
							if (self->selecting)
								self->selecting = FALSE;
							hex_widget_set_cursor (self, self->cursor_pos + 1);
							ret = GDK_EVENT_STOP;
						}
						else if (keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9)
						{
							hex_document_set_byte (self->document, keyval - GDK_KEY_KP_0 + '0',
									self->cursor_pos, self->insert, TRUE);
							if (self->selecting)
								self->selecting = FALSE;
							hex_widget_set_cursor (self, self->cursor_pos + 1);
							ret = GDK_EVENT_STOP;
						}
						break;
				}
			}
			break;
	}
	show_cursor (self, TRUE);
	return ret;
}

static gboolean
key_release_cb (GtkEventControllerKey *controller,
               guint                  keyval,
               guint                  keycode,
               GdkModifierType        state,
               gpointer               user_data)
{
	HexWidget *self = HEX_WIDGET(user_data);
	GtkWidget *widget = GTK_WIDGET(user_data);
	gboolean ret = GDK_EVENT_PROPAGATE;

	/* avoid shift key getting 'stuck'
	 */
	if (state & GDK_SHIFT_MASK) {
		self->selecting = FALSE;
	}
	return ret;
}

/* FIXME - Reorganize/clean up. Mostly flown in from old code.
 */
/*
 * default data_changed signal handler
 */
static void
hex_widget_real_data_changed (HexWidget *self, gpointer data)
{
	HexChangeData *change_data = (HexChangeData *)data;
	gint64 start_line, end_line;
	gint64 lines;
	size_t payload_size;

	g_return_if_fail (HEX_IS_DOCUMENT (self->document));

	payload_size = HEX_BUFFER_PAYLOAD (self->document);

	if (self->cpl == 0)
		return;

	if (change_data->start - change_data->end + 1 != change_data->rep_len) {
		lines = payload_size / self->cpl;
		if (payload_size % self->cpl)
			lines++;
		if (lines != self->lines) {
			self->lines = lines;
			gtk_adjustment_set_value(self->adj, MIN(gtk_adjustment_get_value(self->adj), self->lines - self->vis_lines));
			gtk_adjustment_set_value(self->adj, MAX(0, gtk_adjustment_get_value(self->adj)));
			if((self->cursor_pos/self->cpl < gtk_adjustment_get_value(self->adj)) ||
			   (self->cursor_pos/self->cpl > gtk_adjustment_get_value(self->adj) + self->vis_lines - 1)) {
				gtk_adjustment_set_value(self->adj, MIN(self->cursor_pos/self->cpl, self->lines - self->vis_lines));
				gtk_adjustment_set_value(self->adj, MAX(0, gtk_adjustment_get_value(self->adj)));
			}
			gtk_adjustment_set_lower(self->adj, 0);
			gtk_adjustment_set_upper(self->adj, self->lines);
			gtk_adjustment_set_step_increment(self->adj, 1);
			gtk_adjustment_set_page_increment(self->adj, self->vis_lines - 1);
			gtk_adjustment_set_page_size(self->adj, self->vis_lines);
		}
	}

	start_line = change_data->start/self->cpl - self->top_line;
	end_line = change_data->end/self->cpl - self->top_line;

	if(end_line < 0 ||
	   start_line > self->vis_lines)
		return;

	start_line = MAX(start_line, 0);
	if(change_data->rep_len - change_data->end + change_data->start - 1 != 0)
		end_line = self->vis_lines;
	else
		end_line = MIN(end_line, self->vis_lines);

	gtk_widget_queue_draw (GTK_WIDGET(self));
}

static void
insert_highlight_into_auto_highlight (HexWidget *self,
		HexWidgetAutoHighlight *ahl,
		gint64 start,
		gint64 end)
{
	HexWidget_Highlight *new = NULL;
	size_t payload_size;

	/* Sanity check - make sure highlight attempting to add doesn't already
	 * exist in ahl - FIXME - this shouldn't have to exist at all.
	 */
	for (guint i = 0; i < ahl->highlights->len; ++i)
	{
		HexWidget_Highlight *hl = g_ptr_array_index (ahl->highlights, i);

		if (start == hl->start && end == hl->end) {
			return;
		}
	}

	new = g_new0 (HexWidget_Highlight, 1);
	payload_size = HEX_BUFFER_PAYLOAD (self->document);

	new->start = CLAMP(MIN(start, end), 0, payload_size);
	new->end = MIN(MAX(start, end), payload_size);

	g_ptr_array_add (ahl->highlights, new);

	gtk_widget_queue_draw (GTK_WIDGET(self));
}

static void
delete_highlight_from_auto_highlight (HexWidget *self, HexWidgetAutoHighlight *ahl,
		HexWidget_Highlight *hl)
{
	g_ptr_array_remove (ahl->highlights, hl);
	gtk_widget_queue_draw (GTK_WIDGET(self));
}

static gboolean
hex_widget_find_limited (HexWidget *self, char *find, int findlen,
		HexSearchFlags flags, gint64 lower, gint64 upper,
		gint64 *found, size_t *found_len)
{
	gboolean retval = FALSE;
	gint64 pos = lower;
	HexDocumentFindData *find_data = hex_document_find_data_new ();

	find_data->what = find;
	find_data->len = findlen;
	find_data->flags = flags;

	while (pos < upper)
	{
		if (hex_document_compare_data_full (self->document, find_data, pos)
				== 0)
		{
			*found = pos;
			*found_len = find_data->found_len;

			retval = TRUE;
			goto out;
		}
		pos++;
	}

out:
	g_free (find_data);
	return retval;
}

/* Helper for the autohighlight functions to set a reasonable default
 * view_min () and view_max () value.
 */
static inline void
ahl_set_view_min_max (HexWidget *self, HexWidgetAutoHighlight *ahl)
{
	ahl->view_min = self->top_line * self->cpl;
	ahl->view_max = (self->top_line + self->vis_lines) * self->cpl;
}

static void
hex_widget_update_auto_highlight (HexWidget *self, HexWidgetAutoHighlight *ahl,
		gboolean delete, gboolean add)
{
	gint64 del_min, del_max;
	gint64 add_min, add_max;
	size_t found_len;
	gint64 prev_min = ahl->view_min;
	gint64 prev_max = ahl->view_max;

	ahl_set_view_min_max (self, ahl);

	if (prev_min < ahl->view_min && prev_max < ahl->view_max)
	{
		del_min = prev_min - ahl->search_len;
		del_max = ahl->view_min - ahl->search_len;
		add_min = prev_max;
		add_max = ahl->view_max;
	}
	else if (prev_min > ahl->view_min && prev_max > ahl->view_max)
	{
		add_min = ahl->view_min - ahl->search_len;
		add_max = prev_min - ahl->search_len;
		del_min = ahl->view_max;
		del_max = prev_max;
	}
	else /* just refresh the lot */
	{
		del_min = 0;
		del_max = self->cpl * self->lines;
		add_min = ahl->view_min;
		add_max = ahl->view_max;
	}

	add_min = MAX(add_min, 0);
	del_min = MAX(del_min, 0);

	if (delete)
	{
		for (guint i = 0; i < ahl->highlights->len; ++i)
		{
			HexWidget_Highlight *highlight = g_ptr_array_index (ahl->highlights, i);

			if (highlight->start >= del_min && highlight->start <= del_max)
			{
				delete_highlight_from_auto_highlight (self, ahl, highlight);
			}
		}
	}

	if (add)
	{
		gint64 foundpos = -1;

		while (hex_widget_find_limited (self,
					ahl->search_string,
					ahl->search_len,
					ahl->search_flags,
					MAX(add_min, foundpos+1),		/* lower */
					add_max,						/* upper */
					&foundpos,
					&found_len))
		{
			insert_highlight_into_auto_highlight (self, ahl,
					foundpos,
					foundpos + found_len - 1);
		}
	}
}

static void
hex_widget_update_all_auto_highlights (HexWidget *self)
{
	int mult = 10;

	gint64 top_line_pos = self->top_line * self->cpl;
	gint64 bot_line_pos = (self->top_line + self->vis_lines) * self->cpl;

	for (guint i = 0; i < self->auto_highlights->len; ++i)
	{
		HexWidgetAutoHighlight *ahl = g_ptr_array_index (self->auto_highlights, i);

		/* only refresh possibilities within a certain number of line numbers
		 */
		if (top_line_pos - mult * self->vis_lines * self->cpl > ahl->view_min ||
			bot_line_pos + mult * self->vis_lines * self->cpl < ahl->view_max)
		{
			/* do nothing */
		}
		else
		{
			hex_widget_update_auto_highlight (self, ahl, TRUE, TRUE);
		}
	}
}

static void
hex_widget_real_copy_to_clipboard (HexWidget *self)
{
	GtkWidget *widget = GTK_WIDGET(self);
	GdkClipboard *clipboard;
	HexPasteData *paste;
	GdkContentProvider *provider_union;
	GdkContentProvider *provider_array[2];
	gint64 start_pos, end_pos;
	size_t len;
	char *doc_data;
	char *string;

	clipboard = gtk_widget_get_clipboard (widget);

	start_pos = MIN(self->selection.start, self->selection.end);
	end_pos = MAX(self->selection.start, self->selection.end);

	/* +1 because we're counting the number of characters to grab here.
	 * You have to actually include the first character in the range.
	 */
	len = end_pos - start_pos + 1;
	g_return_if_fail (len);

	/* Grab the raw data from the HexDocument. */
	doc_data = hex_buffer_get_data (hex_document_get_buffer(self->document),
			start_pos, len);

	/* Setup a union of HexPasteData and a plain C string */
	paste = hex_paste_data_new (doc_data, len);
	g_return_if_fail (HEX_IS_PASTE_DATA(paste));
	string = hex_paste_data_get_string (paste);

	provider_array[0] =
		gdk_content_provider_new_typed (HEX_TYPE_PASTE_DATA, paste);
	provider_array[1] =
		gdk_content_provider_new_typed (G_TYPE_STRING, string);

	provider_union = gdk_content_provider_new_union (provider_array, 2);

	/* Finally, set our content to our newly created union. */
	gdk_clipboard_set_content (clipboard, provider_union);

	g_free (string);
}

static void
hex_widget_real_cut_to_clipboard(HexWidget *self,
		gpointer user_data)
{
	if (self->selection.start != -1 && self->selection.end != -1) {
		hex_widget_real_copy_to_clipboard(self);
		if (self->insert)
			hex_widget_delete_selection(self);
		else
			hex_widget_zero_selection(self);
	}
}

static void
hex_widget_real_paste_from_clipboard (HexWidget *self,
		gpointer user_data)
{
	GtkWidget *widget = GTK_WIDGET(self);
	GdkClipboard *clipboard;

	clipboard = gtk_widget_get_clipboard (widget);

	paste_helper (self, clipboard);
}

static void
hex_widget_real_draw_complete (HexWidget *self,
		gpointer user_data)
{
	recalc_scrolling (self);
}

static void
hex_widget_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	HexWidget *self = HEX_WIDGET(widget);
	GtkWidget *child;
	float height;

	height = gtk_widget_get_allocated_height (widget);

	/* Update character width & height */
	self->char_width = get_char_width (self);
	self->char_height = get_char_height (self);

	/* Get cpl from layout manager or geometry (if set) */
	if (! self->default_cpl)
		self->cpl = hex_widget_layout_get_cpl (
				HEX_WIDGET_LAYOUT(self->layout_manager));
	else
		self->cpl = self->default_cpl;

	/* set visible lines based on widget height or geometry (if set) */
	if (! self->default_lines)
		self->vis_lines = height / self->char_height;
	else
		self->vis_lines = self->default_lines;

	hex_widget_update_all_auto_highlights (self);

	/* queue child draw functions
	 */

	/* manually specify these as sometimes _snapshot_child doesn't `think'
	 * they need to be redrawn. */
	gtk_widget_queue_draw (self->offsets);
	gtk_widget_queue_draw (self->xdisp);
	gtk_widget_queue_draw (self->adisp);

	for (child = gtk_widget_get_first_child (widget);
			child != NULL;
			child = gtk_widget_get_next_sibling (child))
	{
		if (gtk_widget_get_visible (child))
			gtk_widget_snapshot_child (widget, child, snapshot);
	}

	g_signal_emit_by_name(G_OBJECT(self), "draw-complete");
}

static void
set_busy_state (HexWidget *self, gboolean busy)
{
	GtkWidget *widget = GTK_WIDGET(self);

	if (busy)
	{
		gtk_widget_hide (self->offsets);
		gtk_widget_hide (self->xdisp);
		gtk_widget_hide (self->adisp);
		gtk_widget_hide (self->scrollbar);
		gtk_widget_show (self->busy_spinner);
	}
	else
	{
		gtk_widget_hide (self->busy_spinner);
		gtk_widget_set_visible (self->offsets, self->show_offsets);
		gtk_widget_show (self->xdisp);
		gtk_widget_show (self->adisp);
		gtk_widget_show (self->scrollbar);
	}
}

static void
file_read_started_cb (HexDocument *doc, gpointer data)
{
	HexWidget *self = HEX_WIDGET (data);

	set_busy_state (self, TRUE);
}

static void
file_loaded_cb (HexDocument *doc, gpointer data)
{
	HexWidget *self = HEX_WIDGET(data);

	gtk_widget_action_set_enabled (GTK_WIDGET(self),
			"gtkhex.undo", hex_document_can_undo (doc));
	gtk_widget_action_set_enabled (GTK_WIDGET(self),
			"gtkhex.redo", hex_document_can_redo (doc));

	set_busy_state (self, FALSE);

	gtk_widget_queue_draw (GTK_WIDGET(self));
}

static void
document_changed_cb (HexDocument* doc, gpointer change_data,
        gboolean push_undo, gpointer data)
{
	HexWidget *self = HEX_WIDGET(data);

    hex_widget_real_data_changed (self, change_data);

	gtk_widget_action_set_enabled (GTK_WIDGET(self),
			"gtkhex.undo", hex_document_can_undo (doc));
	gtk_widget_action_set_enabled (GTK_WIDGET(self),
			"gtkhex.redo", hex_document_can_redo (doc));
}

static void
file_save_started_cb (HexWidget *self, HexDocument *doc)
{
	set_busy_state (self, TRUE);
}

static void
file_saved_cb (HexWidget *self, HexDocument *doc)
{
	set_busy_state (self, FALSE);
}

static void
hex_widget_constructed (GObject *object)
{
	HexWidget *self = HEX_WIDGET(object);

	/* Setup document signals */

	g_signal_connect (self->document, "file-read-started",
			G_CALLBACK (file_read_started_cb), self);

	g_signal_connect_object (self->document, "file-loaded",
			G_CALLBACK (file_loaded_cb), self, 0);

	g_signal_connect (self->document, "document-changed",
			G_CALLBACK (document_changed_cb), self);

	g_signal_connect (self->document, "undo",
			G_CALLBACK (doc_undo_redo_cb), self);

	g_signal_connect (self->document, "redo",
			G_CALLBACK (doc_undo_redo_cb), self);

	g_signal_connect_swapped (self->document, "file-save-started",
			G_CALLBACK (file_save_started_cb), self);

	g_signal_connect_swapped (self->document, "file-saved",
			G_CALLBACK (file_saved_cb), self);

	/* Chain up */
	G_OBJECT_CLASS(hex_widget_parent_class)->constructed (object);
}

static void
hex_widget_dispose (GObject *object)
{
	HexWidget *self = HEX_WIDGET(object);
	GtkWidget *widget = GTK_WIDGET(self);
	GtkWidget *child;

	/* Unparent children
	 */
	g_clear_pointer (&self->xdisp, gtk_widget_unparent);
	g_clear_pointer (&self->adisp, gtk_widget_unparent);
	g_clear_pointer (&self->offsets, gtk_widget_unparent);
	g_clear_pointer (&self->scrollbar, gtk_widget_unparent);
	g_clear_pointer (&self->busy_spinner, gtk_widget_unparent);

	g_clear_pointer (&self->context_menu, gtk_widget_unparent);
	g_clear_pointer (&self->geometry_popover, gtk_widget_unparent);

	/* Clear pango layouts
	 */
	g_clear_object (&self->xlayout);
	g_clear_object (&self->alayout);
	g_clear_object (&self->olayout);

	g_clear_object (&self->xlayout_top_line_cache);
	g_clear_object (&self->alayout_top_line_cache);

	g_clear_object (&self->document);
	
	/* Chain up */
	G_OBJECT_CLASS(hex_widget_parent_class)->dispose(object);
}

static void
hex_widget_finalize (GObject *gobject)
{
	HexWidget *self = HEX_WIDGET(gobject);
	
	g_clear_pointer (&self->disp_buffer, g_array_unref);

	/* Chain up */
	G_OBJECT_CLASS(hex_widget_parent_class)->finalize(gobject);
}


static void
hex_widget_class_init (HexWidgetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GParamFlags prop_flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	/* vfuncs */

	object_class->constructed = hex_widget_constructed;
	object_class->dispose = hex_widget_dispose;
	object_class->finalize = hex_widget_finalize;
	object_class->set_property = hex_widget_set_property;
	object_class->get_property = hex_widget_get_property;

	widget_class->snapshot = hex_widget_snapshot;

	/* PROPERTIES */

	/**
	 * HexWidget:document:
	 *
	 * `HexDocument` affiliated with and owned by the `HexWidget`.
	 *
	 * Since: 4.2
	 */
	properties[DOCUMENT] = g_param_spec_object ("document", NULL, NULL,
			HEX_TYPE_DOCUMENT,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * HexWidget:fade-zeroes:
	 *
	 * Whether zeroes (`00`) will be faded on the hex side of the `HexWidget`.
	 *
	 * Since: 4.8
	 */
	properties[FADE_ZEROES] = g_param_spec_boolean ("fade-zeroes", NULL, NULL,
			FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * HexWidget:display-control-characters:
	 *
	 * Whether ASCII control characters (ASCII characters 0x0 through
	 * 0x1F) will be rendered as unicode symbols on the ASCII side of the
	 * `HexWidget`.
	 *
	 * Since: 4.10
	 */
	properties[DISPLAY_CONTROL_CHARACTERS] = g_param_spec_boolean ("display-control-characters", NULL, NULL,
			FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * HexWidget:insert-mode
	 *
	 * Whether insert-mode (versus overwrite) is currently engaged.
	 *
	 * Since: 4.10
	 */
	properties[INSERT_MODE] = g_param_spec_boolean ("insert-mode", NULL, NULL,
			FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	/* Layout manager */

	gtk_widget_class_set_layout_manager_type (widget_class,
			HEX_TYPE_WIDGET_LAYOUT);

	/* CSS name */

	gtk_widget_class_set_css_name (widget_class, "hexwidget");

	/* SIGNALS */

	/**
	 * HexWidget::cursor-moved:
	 * 
	 * Emitted when the cursor has moved.
	 */
	gtkhex_signals[CURSOR_MOVED_SIGNAL] =
		g_signal_new_class_handler ("cursor-moved",
				G_OBJECT_CLASS_TYPE(object_class),
			/* GSignalFlags signal_flags : */
				G_SIGNAL_RUN_FIRST,
			/* GCallback class_handler: */
				NULL,		/* no callback; plain signal. */
			/* no accumulator or accu_data */
				NULL, NULL,
			/* GSignalCMarshaller c_marshaller: */
				NULL,		/* use generic marshaller */
			/* GType return_type: */
				G_TYPE_NONE,
			/* guint n_params: */
				0);

	/**
	 * HexWidget::data-changed:
	 *
	 * Emitted when data has changed.
	 */
	gtkhex_signals[DATA_CHANGED_SIGNAL] = 
		g_signal_new_class_handler ("data-changed",
				G_OBJECT_CLASS_TYPE(object_class),
				G_SIGNAL_RUN_FIRST,
				G_CALLBACK(hex_widget_real_data_changed),
				NULL, NULL,
				NULL,
				G_TYPE_NONE,
				0);
	/**
	 * HexWidget::cut-clipboard:
	 *
	 * Emitted when a cut-to-clipboard operation has occurred.
	 */
	gtkhex_signals[CUT_CLIPBOARD_SIGNAL] =
		g_signal_new_class_handler ("cut-clipboard",
				G_OBJECT_CLASS_TYPE(object_class),
				G_SIGNAL_RUN_FIRST,
				G_CALLBACK(hex_widget_real_cut_to_clipboard),
				NULL, NULL,
				NULL,
				G_TYPE_NONE,
				0);

	/**
	 * HexWidget::copy-clipboard:
	 *
	 * Emitted when a copy-to-clipboard operation has occurred.
	 */
	gtkhex_signals[COPY_CLIPBOARD_SIGNAL] = 
		g_signal_new_class_handler ("copy-clipboard",
				G_OBJECT_CLASS_TYPE(object_class),
				G_SIGNAL_RUN_FIRST,
				G_CALLBACK(hex_widget_real_copy_to_clipboard),
				NULL, NULL,
				NULL,
				G_TYPE_NONE,
				0);

	/**
	 * HexWidget::paste-clipboard:
	 *
	 * Emitted when a paste-from-clipboard operation has occurred.
	 */
	gtkhex_signals[PASTE_CLIPBOARD_SIGNAL] = 
		g_signal_new_class_handler ("paste-clipboard",
				G_OBJECT_CLASS_TYPE(object_class),
				G_SIGNAL_RUN_FIRST,
				G_CALLBACK(hex_widget_real_paste_from_clipboard),
				NULL, NULL,
				NULL,
				G_TYPE_NONE,
				0);
	
	/**
	 * HexWidget:draw-complete:
	 *
	 * Emitted when the #HexWidget has been fully redrawn.
	 */
	gtkhex_signals[DRAW_COMPLETE_SIGNAL] = 
		g_signal_new_class_handler ("draw-complete",
				G_OBJECT_CLASS_TYPE(object_class),
				G_SIGNAL_RUN_FIRST,
				G_CALLBACK(hex_widget_real_draw_complete),
				NULL, NULL,
				NULL,
				G_TYPE_NONE,
				0);

	/* ACTIONS */

	gtk_widget_class_install_action (widget_class, "gtkhex.copy",
			NULL,   /* GVariant string param_type */
			copy_action);

	gtk_widget_class_install_action (widget_class, "gtkhex.cut",
			NULL,
			cut_action);

	gtk_widget_class_install_action (widget_class, "gtkhex.paste",
			NULL,
			paste_action);

	gtk_widget_class_install_action (widget_class, "gtkhex.undo",
			NULL,
			undo_action);

	gtk_widget_class_install_action (widget_class, "gtkhex.redo",
			NULL,
			redo_action);

	gtk_widget_class_install_action (widget_class, "gtkhex.toggle-hex",
			NULL,
			toggle_hex_action);

	gtk_widget_class_install_action (widget_class, "gtkhex.toggle-ascii",
			NULL,
			toggle_ascii_action);

	gtk_widget_class_install_action (widget_class, "gtkhex.geometry",
			NULL,
			geometry_action);

	gtk_widget_class_install_action (widget_class, "gtkhex.move-to-buffer-ends",
			"(bb)",
			move_to_buffer_ends_action);

	/* SHORTCUTS FOR ACTIONS (not to be confused with keybindings, which are
	 * set up in hex_widget_init) */

	/* Ctrl+c - copy */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_c,
			GDK_CONTROL_MASK,
			"gtkhex.copy",
			NULL);	/* no args. */

	/* Ctrl+x - cut */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_x,
			GDK_CONTROL_MASK,
			"gtkhex.cut",
			NULL);

	/* Ctrl+v - paste */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_v,
			GDK_CONTROL_MASK,
			"gtkhex.paste",
			NULL);

	/* Ctrl+z - undo */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_z,
			GDK_CONTROL_MASK,
			"gtkhex.undo",
			NULL);

	/* Ctrl+y - redo */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_y,
			GDK_CONTROL_MASK,
			"gtkhex.redo",
			NULL);

	/* Alt+Left - toggle hex display */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_Left,
			GDK_ALT_MASK,
			"gtkhex.toggle-hex",
			NULL);

	/* Alt+Right - toggle ascii display */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_Right,
			GDK_ALT_MASK,
			"gtkhex.toggle-ascii",
			NULL);

	/* Ctrl+Home - move to beginning of buffer */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_Home,
			GDK_CONTROL_MASK,
			"gtkhex.move-to-buffer-ends",
			"(bb)", TRUE, FALSE);

	/* Ctrl+End - move to end of buffer */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_End,
			GDK_CONTROL_MASK,
			"gtkhex.move-to-buffer-ends",
			"(bb)", FALSE, FALSE);

	/* Shift+Ctrl+Home - move to beginning of buffer with selection */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_Home,
			GDK_SHIFT_MASK | GDK_CONTROL_MASK,
			"gtkhex.move-to-buffer-ends",
			"(bb)", TRUE, TRUE);

	/* Shift+Ctrl+End - move to end of buffer with selection */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_End,
			GDK_SHIFT_MASK | GDK_CONTROL_MASK,
			"gtkhex.move-to-buffer-ends",
			"(bb)", FALSE, TRUE);

	/* Alt+Delete - dumb */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_Delete,
			GDK_ALT_MASK,
			"gtkhex.dumb",
			NULL);

	/* Shift+Alt+Delete - dumb2 */
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_Delete,
			GDK_ALT_MASK | GDK_SHIFT_MASK,
			"gtkhex.dumb2",
			NULL);
}

static void
hex_widget_init (HexWidget *self)
{
	GtkWidget *widget = GTK_WIDGET(self);

	HexWidgetLayoutChild *child_info;

	GtkStyleContext *context;

	GtkBuilder *builder;
	GMenuModel *menu;

	GtkGesture *gesture;
	GtkEventController *controller;

	self->layout_manager = gtk_widget_get_layout_manager (widget);

	self->scroll_timeout = 0;

	self->document = NULL;

	self->active_view = VIEW_HEX;
	self->group_type = HEX_WIDGET_GROUP_BYTE;
	self->lines = self->vis_lines = self->top_line = self->cpl = 0;
	self->cursor_pos = 0;
	self->lower_nibble = FALSE;
	self->cursor_shown = FALSE;
	self->button = 0;
	self->insert = FALSE; /* default to overwrite mode */
	self->selecting = FALSE;

	self->selection.start = self->selection.end = 0;

	self->auto_highlights = g_ptr_array_new_with_free_func (g_free);
	self->marks = g_ptr_array_new_with_free_func (g_object_unref);

	gtk_widget_set_can_focus (widget, TRUE);
	gtk_widget_set_focusable (widget, TRUE);

	/* Init CSS */

	context = gtk_widget_get_style_context (GTK_WIDGET (widget));

	/* Add common custom `.hex` style class */
	gtk_style_context_add_class (context, "hex");

	self->provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_resource (GTK_CSS_PROVIDER (self->provider),
		RESOURCE_BASE_PATH "/css/gtkhex.css");

	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (self->provider),
			GTK_STYLE_PROVIDER_PRIORITY_THEME);

	/* Setup offsets widget. */

	self->offsets = gtk_drawing_area_new();
	gtk_widget_set_parent (self->offsets, widget);
	child_info = HEX_WIDGET_LAYOUT_CHILD (gtk_layout_manager_get_layout_child
			(self->layout_manager, self->offsets));
	hex_widget_layout_child_set_column (child_info, OFFSETS_COLUMN);

	/* Create the pango layout for the widget */
	self->olayout = gtk_widget_create_pango_layout (self->offsets, "");

	gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self->offsets),
			offsets_draw,
			self,
			NULL);		/* GDestroyNotify destroy); */

	context = gtk_widget_get_style_context (GTK_WIDGET (self->offsets));

	gtk_style_context_add_class (context, "hex");

	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (self->provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_THEME);

	gtk_widget_set_name (self->offsets, "offsets");

	/* hide it by default. */
	gtk_widget_hide (self->offsets);


	/* Setup our Hex drawing area. */

	self->xdisp = gtk_drawing_area_new();
	gtk_widget_set_parent (self->xdisp, widget);
	child_info = HEX_WIDGET_LAYOUT_CHILD (gtk_layout_manager_get_layout_child
			(self->layout_manager, self->xdisp));
	hex_widget_layout_child_set_column (child_info, HEX_COLUMN);

	/* Create the pango layout for the widget */
	self->xlayout = gtk_widget_create_pango_layout (self->xdisp, "");
	self->xlayout_top_line_cache = pango_layout_copy (self->xlayout);

	gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self->xdisp),
			hex_draw,
			self,
			NULL);		/* GDestroyNotify destroy); */

	context = gtk_widget_get_style_context (GTK_WIDGET (self->xdisp));

	gtk_style_context_add_class (context, "hex");

	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (self->provider),
			GTK_STYLE_PROVIDER_PRIORITY_THEME);

	gtk_widget_set_name (self->xdisp, "hex-display");

	/* Setup our ASCII widget. */

	self->adisp = gtk_drawing_area_new();
	gtk_widget_set_parent (self->adisp, widget);
	child_info = HEX_WIDGET_LAYOUT_CHILD (gtk_layout_manager_get_layout_child
			(self->layout_manager, self->adisp));
	hex_widget_layout_child_set_column (child_info, ASCII_COLUMN);

	/* Create the pango layout for the widget */
	self->alayout = gtk_widget_create_pango_layout (self->adisp, "");
	self->alayout_top_line_cache = pango_layout_copy (self->alayout);

	gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self->adisp),
			ascii_draw,
			self,
			NULL);		/* GDestroyNotify destroy); */

	context = gtk_widget_get_style_context (GTK_WIDGET (self->adisp));
	gtk_style_context_add_class (context, "hex");
	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (self->provider),
			GTK_STYLE_PROVIDER_PRIORITY_THEME);

	gtk_widget_set_name (self->adisp, "ascii-display");

	/* Set a minimum size for hex/ascii drawing areas. */

	gtk_widget_set_size_request (self->adisp,
			DEFAULT_DA_SIZE, DEFAULT_DA_SIZE);
	gtk_widget_set_size_request (self->xdisp,
			DEFAULT_DA_SIZE, DEFAULT_DA_SIZE);

	/* Context Menu */

	builder = gtk_builder_new_from_resource (RESOURCE_BASE_PATH "/gtkhex.ui");
	menu = G_MENU_MODEL(gtk_builder_get_object (builder, "context-menu"));
	self->context_menu = gtk_popover_menu_new_from_model (menu);

	gtk_widget_set_parent (self->context_menu, widget);

	self->geometry_popover = GTK_WIDGET(gtk_builder_get_object (builder, "geometry_popover"));

	self->auto_geometry_checkbtn = GTK_WIDGET(gtk_builder_get_object (builder, "auto_geometry_checkbtn"));
	self->cpl_spinbtn = GTK_WIDGET(gtk_builder_get_object (builder, "cpl_spinbtn"));

	g_signal_connect_swapped (self->auto_geometry_checkbtn, "toggled", G_CALLBACK(update_geometry), self);
	g_signal_connect_swapped (self->cpl_spinbtn, "value-changed", G_CALLBACK(update_geometry), self);

	gtk_widget_set_parent (self->geometry_popover, widget);

	g_object_unref (builder);

	/* Initialize Adjustment */
	self->adj = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

	/* Setup scrollbar. */

	self->scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL,
			self->adj);

	context = gtk_widget_get_style_context (GTK_WIDGET (self->scrollbar));
	gtk_style_context_add_class (context, "hex");
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (self->provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_THEME);

	gtk_widget_set_parent (self->scrollbar, widget);
	child_info = HEX_WIDGET_LAYOUT_CHILD (gtk_layout_manager_get_layout_child
			(self->layout_manager, self->scrollbar));
	hex_widget_layout_child_set_column (child_info, SCROLLBAR_COLUMN);
	
	/* Setup busy spinner */

	self->busy_spinner = gtk_spinner_new ();
	gtk_spinner_start (GTK_SPINNER(self->busy_spinner));
	gtk_widget_set_visible (self->busy_spinner, FALSE);
	gtk_widget_set_parent (self->busy_spinner, widget);

	/* GESTURES */

	/* Whole HexWidget widget (for right-click context menu) */

	gesture = gtk_gesture_click_new ();

	/* listen for right-click only */
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE(gesture),
			GDK_BUTTON_SECONDARY);

	g_signal_connect (gesture, "pressed",
			G_CALLBACK(gh_pressed_cb),
			self);

	gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER(gesture));

	/* Hex widget: */

	/* click gestures */
	gesture = gtk_gesture_click_new ();

	/* listen for any button */
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE(gesture), 0);

	g_signal_connect (gesture, "pressed",
			G_CALLBACK(hex_pressed_cb),
			self);
	g_signal_connect (gesture, "released",
			G_CALLBACK(hex_released_cb),
			self);
	gtk_widget_add_controller (self->xdisp,
			GTK_EVENT_CONTROLLER(gesture));

	/* drag gestures */
	gesture = gtk_gesture_drag_new ();

	g_signal_connect (gesture, "drag-update",
			G_CALLBACK (hex_drag_update_cb),
			self);
	gtk_widget_add_controller (self->xdisp,
			GTK_EVENT_CONTROLLER(gesture));

	/* ASCII widget: */

	/* click gestures */
	gesture = gtk_gesture_click_new ();

	/* listen for any button */
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE(gesture), 0);

	g_signal_connect (gesture, "pressed",
			G_CALLBACK(ascii_pressed_cb),
			self);
	g_signal_connect (gesture, "released",
			G_CALLBACK(ascii_released_cb),
			self);
	gtk_widget_add_controller (self->adisp,
			GTK_EVENT_CONTROLLER(gesture));

	/* drag gestures */
	gesture = gtk_gesture_drag_new ();

	g_signal_connect (gesture, "drag-update",
			G_CALLBACK(ascii_drag_update_cb),
			self);
	gtk_widget_add_controller (self->adisp,
			GTK_EVENT_CONTROLLER(gesture));

	/* Event controller - scrolling */

	controller =
		gtk_event_controller_scroll_new
			(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
			 GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);

	g_signal_connect (controller, "scroll",
			G_CALLBACK(scroll_cb),
			self);
	gtk_widget_add_controller (widget,
			GTK_EVENT_CONTROLLER(controller));

	/* Event controller - keyboard - for the widget *as a whole* */
	
	controller = gtk_event_controller_key_new ();

	g_signal_connect(controller, "key-pressed",
			G_CALLBACK(key_press_cb),
			self);
	g_signal_connect(controller, "key-released",
			G_CALLBACK(key_release_cb),
			self);

	gtk_widget_add_controller (widget,
			GTK_EVENT_CONTROLLER(controller));

	/* Event controller - focus */

	controller = gtk_event_controller_focus_new ();

	g_signal_connect_swapped (controller, "enter",
			G_CALLBACK(gtk_widget_queue_draw), widget);
	g_signal_connect_swapped (controller, "leave",
			G_CALLBACK(gtk_widget_queue_draw), widget);

	gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER(controller));

	/* Connect signal for adjustment */

	g_signal_connect (self->adj, "value-changed",
					 G_CALLBACK (adj_value_changed_cb), self);

	/* ACTIONS - Undo / Redo should start out disabled. */

	gtk_widget_action_set_enabled (GTK_WIDGET(self),
			"gtkhex.undo", FALSE);
	gtk_widget_action_set_enabled (GTK_WIDGET(self),
			"gtkhex.redo", FALSE);

	/* PRIMARY selection content */

	self->selection_content = hex_content_provider_new ();
	HEX_CONTENT_PROVIDER (self->selection_content)->owner = self;
}

/*-------- public API starts here --------*/

/**
 * hex_widget_new:
 * @owner: (transfer full): the [class@Hex.Document] object to be associated
 *   with the newly created `HexWidget`
 *
 * Create a new `HexWidget` object.
 *
 * Returns: a newly created `HexWidget` object, or %NULL
 */
GtkWidget *
hex_widget_new (HexDocument *owner)
{
	return g_object_new (HEX_TYPE_WIDGET,
			"document", owner,
			NULL);
}

/**
 * hex_widget_copy_to_clipboard:
 *
 * Copy selection to clipboard.
 */
void
hex_widget_copy_to_clipboard (HexWidget *self)
{
	g_signal_emit_by_name(G_OBJECT(self), "copy-clipboard");
}

/**
 * hex_widget_cut_to_clipboard:
 *
 * Cut selection to clipboard.
 */
void
hex_widget_cut_to_clipboard (HexWidget *self)
{
	g_signal_emit_by_name(G_OBJECT(self), "cut-clipboard");
}

/**
 * hex_widget_paste_from_clipboard:
 *
 * Paste clipboard data to widget at position of cursor.
 *
 * Since 4.6, the behaviour of this method has changed. With 4.4 and earlier,
 * paste operations always inserted data into the payload, even if insert mode
 * was disabled.
 *
 * Commencing in 4.6, if insert mode is not enabled, data will be overwritten
 * by default with a paste operation, and possibly truncated in the event the
 * payload is not large enough to absorb the paste data. This is to avoid
 * increasing the payload size of a hex document when insert mode is disabled.
 */
void
hex_widget_paste_from_clipboard (HexWidget *self)
{
	g_signal_emit_by_name(G_OBJECT(self), "paste-clipboard");
}

/**
 * hex_widget_set_selection:
 * @start: starting offset by byte within the buffer
 * @end: ending offset by byte within the buffer
 *
 * Set the widget selection (highlights).
 */
void
hex_widget_set_selection (HexWidget *self, gint64 start, gint64 end)
{
	size_t payload_size;
	gint64 oe, os, ne, ns;

	g_return_if_fail (HEX_IS_DOCUMENT (self->document));

	payload_size = HEX_BUFFER_PAYLOAD (self->document);

	if (end < 0)
		end = payload_size;

	os = MIN(self->selection.start, self->selection.end);
	oe = MAX(self->selection.start, self->selection.end);

	self->selection.start = CLAMP(start, 0, payload_size);
	self->selection.end = MIN(end, payload_size);

	ns = MIN(self->selection.start, self->selection.end);
	ne = MAX(self->selection.start, self->selection.end);

	if (ns != os && ne != oe) {
		gtk_widget_queue_draw (GTK_WIDGET(self));
	}
	else if (ne != oe) {
		gtk_widget_queue_draw (GTK_WIDGET(self));
	}
	else if (ns != os) {
		gtk_widget_queue_draw (GTK_WIDGET(self));
	}
}

/**
 * hex_widget_get_selection:
 * @start: (nullable) (out): where to store the start of the current selection,
 *   as offset by byte within the buffer
 * @end: (nullable) (out): where to store the end of the current selection, as
 *   offset by byte within the buffer
 *
 * Get the current widget selection (highlights).
 *
 * Returns: %TRUE if there is an active selection (start and end are
 * different), and %FALSE if there is no selection (start and end are the
 * same).
 */
gboolean
hex_widget_get_selection (HexWidget *self, gint64 *start, gint64 *end)
{
	gint64 ss, se;

	if (self->selection.start > self->selection.end) {
		se = self->selection.start;
		ss = self->selection.end;
	}
	else {
		ss = self->selection.start;
		se = self->selection.end;
	}

	if (start)
		*start = ss;
	if (end)
		*end = se;

	return !(ss == se);
}

/**
 * hex_widget_clear_selection:
 *
 * Clear the selection (if any). Any autohighlights will remain intact.
 */
void
hex_widget_clear_selection (HexWidget *self)
{
	hex_widget_set_selection (self, 0, 0);
}

/**
 * hex_widget_delete_selection:
 *
 * Delete the current selection. The resulting action will be undoable. 
 */
void
hex_widget_delete_selection (HexWidget *self)
{
	gint64 start, end;
	size_t len;

	start = MIN(self->selection.start, self->selection.end);
	end = MAX(self->selection.start, self->selection.end);

	len = end - start + 1;
	g_return_if_fail (len);

	hex_widget_clear_selection (self);

	hex_document_delete_data (self->document,
			MIN(start, end), len, TRUE);

	hex_widget_set_cursor (self, start);
}

static char *zeros = NULL;

/**
 * hex_widget_zero_selection:
 *
 * Set the current selection to zero. The resulting action will be undoable.
 *
 * Since: 4.4
 */
void
hex_widget_zero_selection (HexWidget *self)
{
	gint64 start, end;
	size_t len;
	size_t written = 0;

	start = MIN(self->selection.start, self->selection.end);
	end = MAX(self->selection.start, self->selection.end);

	len = end - start + 1;
	g_return_if_fail (len);

	if (zeros == NULL) {
		zeros = g_malloc0 (512);
	}

	while (written < len) {
		size_t batch_size = len < 512 ? len : 512;
		hex_document_set_data (self->document,
				start, batch_size, batch_size,
				zeros, TRUE);
		written += batch_size;
	}
}

/**
 * hex_widget_set_nibble:
 * @lower_nibble: %TRUE if the lower nibble of the current byte should be
 *   selected; %FALSE for the upper nibble
 *
 * Move the cursor to upper nibble or lower nibble of the current byte.
 */
void
hex_widget_set_nibble (HexWidget *self, gboolean lower_nibble)
{
	g_return_if_fail (HEX_IS_WIDGET(self));

	if (self->selecting) {
		gtk_widget_queue_draw (GTK_WIDGET(self));
		self->lower_nibble = lower_nibble;
	}
	else if(self->selection.end != self->selection.start) {
		guint start = MIN(self->selection.start, self->selection.end);
		guint end = MAX(self->selection.start, self->selection.end);
		self->selection.end = self->selection.start = 0;
		gtk_widget_queue_draw (GTK_WIDGET(self));
		self->lower_nibble = lower_nibble;
	}
	else {
		show_cursor (self, FALSE);
		self->lower_nibble = lower_nibble;
		show_cursor(self, TRUE);
	}
}

/**
 * hex_widget_set_cursor:
 * @index: where the cursor should be moved to, as offset by byte within
 *   the buffer
 *
 * Move cursor to @index.
 */
void
hex_widget_set_cursor (HexWidget *self, gint64 index)
{
	gint64 y;
	size_t payload_size;

	g_return_if_fail (HEX_IS_WIDGET (self));

	payload_size = HEX_BUFFER_PAYLOAD (self->document);

	if ((index >= 0) && (index <= payload_size))
	{
		if (!self->insert && index == payload_size)
			index--;

		index = MAX(index, 0);

		show_cursor (self, FALSE);
		
		self->cursor_pos = index;

		if (self->cpl == 0)
			return;
		
		y = index / self->cpl;
		if (y >= self->top_line + self->vis_lines)
		{
			gtk_adjustment_set_value (self->adj,
					MIN (y - self->vis_lines + 1, self->lines - self->vis_lines));

			gtk_adjustment_set_value (self->adj,
					MAX (gtk_adjustment_get_value(self->adj), 0));
		}
		else if (y < self->top_line)
		{
			gtk_adjustment_set_value (self->adj, y);
		}      

		if (index == payload_size)
			self->lower_nibble = FALSE;

		if (self->selecting)
		{
			hex_widget_set_selection(self, self->selection.start, self->cursor_pos);

			gtk_widget_queue_draw (GTK_WIDGET(self));
		}
		else
		{
			gint64 start = MIN (self->selection.start, self->selection.end);
			gint64 end = MAX (self->selection.start, self->selection.end);

			gtk_widget_queue_draw (GTK_WIDGET(self));
			self->selection.end = self->selection.start = self->cursor_pos;
		}

		g_signal_emit_by_name (self, "cursor-moved");

		gtk_widget_queue_draw (GTK_WIDGET(self));
		show_cursor (self, TRUE);
	}
}

/**
 * hex_widget_set_cursor_by_row_and_col:
 * @col_x: column to which the cursor should be moved
 * @line_y: line to which the cursor should be moved, by absolute value, within
 *   the whole buffer (not just the currently visible part)
 *
 * Move the cursor by row and column, as absolute values.
 */
void
hex_widget_set_cursor_by_row_and_col (HexWidget *self, int col_x, gint64 line_y)
{
	gint64 tmp_cursor_pos;
	gint64 payload_size;

	g_return_if_fail (HEX_IS_WIDGET(self));

	tmp_cursor_pos = line_y * self->cpl + col_x;
	payload_size = HEX_BUFFER_PAYLOAD (self->document);

	if ((line_y >= 0) && (line_y < self->lines) && (col_x >= 0) &&
	   (col_x < self->cpl) && (tmp_cursor_pos <= payload_size))
	{
		if (!self->insert && tmp_cursor_pos == payload_size)
			tmp_cursor_pos--;

		tmp_cursor_pos = MAX(tmp_cursor_pos, 0);

		show_cursor (self, FALSE);
		
		self->cursor_pos = tmp_cursor_pos;
		
		if (line_y >= self->top_line + self->vis_lines) {
			gtk_adjustment_set_value (self->adj,
					MIN(line_y - self->vis_lines + 1, self->lines - self->vis_lines));
			gtk_adjustment_set_value (self->adj,
					MAX(0, gtk_adjustment_get_value(self->adj)));
		}
		else if (line_y < self->top_line) {
			gtk_adjustment_set_value(self->adj, line_y);
		}      
	
		g_signal_emit_by_name(G_OBJECT(self), "cursor-moved");
		
		if (self->selecting) {
			hex_widget_set_selection(self, self->selection.start, self->cursor_pos);
			gtk_widget_queue_draw (GTK_WIDGET(self));
		}
		else if (self->selection.end != self->selection.start) {
			gint64 start = MIN(self->selection.start, self->selection.end);
			gint64 end = MAX(self->selection.start, self->selection.end);
			self->selection.end = self->selection.start = 0;
			gtk_widget_queue_draw (GTK_WIDGET(self));
		}
		gtk_widget_queue_draw (GTK_WIDGET(self));
		show_cursor (self, TRUE);
	}
}

/**
 * hex_widget_get_cursor:
 *
 * Get the cursor position.
 *
 * Returns: the cursor position, as index within the whole of the buffer
 */
gint64
hex_widget_get_cursor (HexWidget *self)
{
	g_return_val_if_fail (self != NULL, -1);
	g_return_val_if_fail (HEX_IS_WIDGET(self), -1);

	return self->cursor_pos;
}

/**
 * hex_widget_get_byte:
 * @offset: index of the requested byte within the whole of the buffer
 *
 * Get the value of the byte at requested offset position.
 */
guchar
hex_widget_get_byte (HexWidget *self, gint64 offset)
{
	g_return_val_if_fail (HEX_IS_WIDGET(self), 0);

	if ((offset >= 0) && (offset < HEX_BUFFER_PAYLOAD (self->document)))
		return hex_buffer_get_byte (hex_document_get_buffer (self->document), offset);

	return 0;
}

/**
 * hex_widget_set_group_type:
 * @gt: group type
 *
 * Set the group type of the #HexWidget.
 */
void
hex_widget_set_group_type (HexWidget *self, HexWidgetGroupType gt)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(HEX_IS_WIDGET(self));

	show_cursor (self, FALSE);
	self->group_type = gt;

	hex_widget_layout_set_group_type (HEX_WIDGET_LAYOUT(self->layout_manager), gt);

	recalc_displays(self);
	gtk_widget_queue_resize (GTK_WIDGET(self));
	show_cursor (self, TRUE);
}

/**
 * hex_widget_show_offsets:
 * @show: %TRUE if the offsets column should be shown, %FALSE if it should
 *   be hidden
 *   
 * Set whether the offsets column of the widget should be shown.
 */
void
hex_widget_show_offsets (HexWidget *self, gboolean show)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	gtk_widget_set_visible (self->offsets, show);
	self->show_offsets = show;
}

/**
 * hex_widget_show_hex_column:
 * @show: %TRUE if the hex column should be shown, %FALSE if it should
 *   be hidden
 *
 * Set whether the hex column of the widget should be shown.
 *
 * Since: 4.2
 */
void
hex_widget_show_hex_column (HexWidget *self, gboolean show)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	if (!show && gtk_widget_get_visible (self->adisp))
		self->active_view = VIEW_ASCII;

	gtk_widget_set_visible (self->xdisp, show);
}

/**
 * hex_widget_show_ascii_column:
 * @show: %TRUE if the ASCII column should be shown, %FALSE if it should
 *   be hidden
 *
 * Set whether the ASCII column of the widget should be shown.
 *
 * Since: 4.2
 */
void
hex_widget_show_ascii_column (HexWidget *self, gboolean show)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	if (!show && gtk_widget_get_visible (self->xdisp))
		self->active_view = VIEW_HEX;

	gtk_widget_set_visible (self->adisp, show);
}

/**
 * hex_widget_set_insert_mode:
 * @insert: %TRUE if insert mode should be enabled, %FALSE if overwrite mode
 *   should be enabled
 *   
 * Set whether the #HexWidget should be in insert or overwrite mode.
 */
void
hex_widget_set_insert_mode (HexWidget *self, gboolean insert)
{
	size_t payload_size;

	g_return_if_fail (HEX_IS_DOCUMENT (self->document));

	payload_size = HEX_BUFFER_PAYLOAD (self->document);
	self->insert = insert;

	if (!self->insert && self->cursor_pos > 0 && self->cursor_pos >= payload_size)
		hex_widget_set_cursor (self, payload_size - 1);

	g_object_notify_by_pspec (G_OBJECT(self), properties[INSERT_MODE]);
}

/**
 * hex_widget_insert_autohighlight_full:
 * @search: (array length=len) (element-type gint8) (transfer full): search
 *   string to auto-highlight
 * @len: length of the @search string
 * @flags: #HexSearchFlags to specify match type
 *
 * Full version of [method@Hex.Widget.insert_autohighlight] which allows
 * for specifying string match types for auto-highlights over and above
 * exact byte-for-byte string matches.
 *
 * Returns: (transfer none): a newly created [struct@Hex.WidgetAutoHighlight]
 *   structure, owned by the `HexWidget`
 *
 * Since: 4.2
 */
HexWidgetAutoHighlight *
hex_widget_insert_autohighlight_full (HexWidget *self,
		const char *search,
		int len,
		HexSearchFlags flags)
{
	HexWidgetAutoHighlight *new = g_new0 (HexWidgetAutoHighlight, 1);

	new->search_string = g_memdup2 (search, len);
	new->search_len = len;
	new->search_flags = flags;

	new->highlights = g_ptr_array_new_with_free_func (g_free);

	g_ptr_array_add (self->auto_highlights, new);

	hex_widget_update_auto_highlight (self, new, FALSE, TRUE);

	return new;
}

/**
 * hex_widget_insert_autohighlight:
 * @search: (array length=len) (element-type gint8) (transfer full): search
 *   string to auto-highlight
 * @len: length of the @search string
 *
 * Insert an auto-highlight of a given search string.
 *
 * Returns: (transfer none): a newly created [struct@Hex.WidgetAutoHighlight]
 *   structure, owned by the `HexWidget`
 */
HexWidgetAutoHighlight *
hex_widget_insert_autohighlight (HexWidget *self,
		const char *search,
		int len)
{
	return hex_widget_insert_autohighlight_full (
			self, search, len, HEX_SEARCH_NONE);
}

/* FIXME - use _free func. */
/**
 * hex_widget_delete_autohighlight:
 * @ahl: the autohighlight to be deleted
 *
 * Delete the requested autohighlight.
 */
void hex_widget_delete_autohighlight (HexWidget *self,
		HexWidgetAutoHighlight *ahl)
{
	g_free (ahl->search_string);

	for (guint i = 0; i < ahl->highlights->len; ++i)
	{
		HexWidget_Highlight *highlight = g_ptr_array_index (ahl->highlights, i);

		delete_highlight_from_auto_highlight (self, ahl, highlight);
	}
	g_ptr_array_unref (ahl->highlights);

	g_ptr_array_remove (self->auto_highlights, ahl);
}

/**
 * hex_widget_set_mark_custom_color:
 * @mark: The `HexWidgetMark` for which the custom color will be set
 * @color: The custom color to be set for the mark
 *
 * Set a custom color for a `HexWidgetMark` object.
 *
 * Since: 4.8
 */
void
hex_widget_set_mark_custom_color (HexWidget *self, HexWidgetMark *mark, GdkRGBA *color)
{
	g_return_if_fail (HEX_IS_WIDGET (self));
	g_return_if_fail (HEX_IS_WIDGET_MARK (mark));
	g_return_if_fail (color != NULL);

	hex_widget_mark_set_custom_color (mark, color);

	gtk_widget_queue_draw (GTK_WIDGET(self));
}

/**
 * hex_widget_get_mark_custom_color:
 * @mark: The `HexWidgetMark` for which to obtain the associated custom color
 * @color: (out): A `GdkRGBA` structure to be set with the custom color
 *   associated with the `HexWidgetMark`, if applicable
 *
 * Obtains the custom color associated with a `HexWidgetMark` object, if
 * any.
 *
 * Returns: `TRUE` if the `HexWidgetMark` has a custom color associated with
 *   it; `FALSE` if it does not.
 *
 * Since: 4.8
 */
gboolean
hex_widget_get_mark_custom_color (HexWidget *self, HexWidgetMark *mark, GdkRGBA *color)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), FALSE);
	g_return_val_if_fail (HEX_IS_WIDGET_MARK (mark), FALSE);
	g_return_val_if_fail (color != NULL, FALSE);

	if (!mark->have_custom_color)
		return FALSE;
	else
	{
		*color = mark->custom_color;
		return TRUE;
	}
}

/**
 * hex_widget_add_mark:
 * @start: The start offset of the mark
 * @end: The start offset of the mark
 * @color: (nullable): A custom color to set for the mark, or `NULL` to use the
 *   default
 *
 * Add a mark for a `HexWidget` object at the specified absolute `start` and
 * `end` offsets.
 *
 * Although the mark obtains an index within the widget internally, this index
 * numeral is private and is not retrievable. As a result, it is recommended
 * that applications wishing to manipulate marks retain the pointer returned by
 * this function, and implement their own tracking mechanism for the marks.
 *
 * Returns: (transfer none): A pointer to a `HexWidgetMark` object, owned by
 * the `HexWidget`.
 *
 * Since: 4.8
 */
HexWidgetMark *
hex_widget_add_mark (HexWidget *self, gint64 start, gint64 end, GdkRGBA *color)
{
	HexWidgetMark *mark;

	g_return_val_if_fail (HEX_IS_WIDGET (self), NULL);

	mark = hex_widget_mark_new ();
	mark->highlight.start = start;
	mark->highlight.end = end;

	if (color)
		hex_widget_set_mark_custom_color (self, mark, color);

	g_ptr_array_add (self->marks, mark);
	gtk_widget_queue_draw (GTK_WIDGET(self));

	return mark;
}

/**
 * hex_widget_delete_mark:
 * @mark: The `HexWidgetMark` to delete
 *
 * Delete a `HexWidgetMark` from a `HexWidget`.
 *
 * Since: 4.8
 */
void
hex_widget_delete_mark (HexWidget *self, HexWidgetMark *mark)
{
	g_return_if_fail (HEX_IS_WIDGET (self));
	g_return_if_fail (HEX_IS_WIDGET_MARK (mark));

	g_ptr_array_remove (self->marks, mark);
	gtk_widget_queue_draw (GTK_WIDGET(self));
}

/**
 * hex_widget_goto_mark:
 * @mark: The mark to jump to
 *
 * Jump the cursor in the `HexWidget` specified to the mark in question.
 *
 * Since: 4.8
 */
void
hex_widget_goto_mark (HexWidget *self, HexWidgetMark *mark)
{
	g_return_if_fail (HEX_IS_WIDGET (self));
	g_return_if_fail (HEX_IS_WIDGET_MARK (mark));

	hex_widget_set_cursor (self, mark->highlight.start);
	hex_widget_set_nibble (self, FALSE);
}

/**
 * hex_widget_set_geometry:
 * @cpl: columns per line which should be displayed, or 0 for default
 * @vis_lines: number of lines which should be displayed, or 0 for default
 *
 * Set the geometry of the widget to specified dimensions.
 */
void hex_widget_set_geometry (HexWidget *self, int cpl, int vis_lines)
{
    self->default_cpl = cpl;
    self->default_lines = vis_lines;
}

/**
 * hex_widget_get_adjustment:
 * 
 * Get the [class@Gtk.Adjustment] of the #HexWidget.
 *
 * Returns: (transfer none): #GtkAdjustment of the widget.
 */
GtkAdjustment *
hex_widget_get_adjustment (HexWidget *self)
{
	g_return_val_if_fail (GTK_IS_ADJUSTMENT(self->adj), NULL);

	return self->adj;
}

/**
 * hex_widget_get_document:
 *
 * Get the [class@Hex.Document] owned by the #HexWidget.
 *
 * Returns: (transfer none): the #HexDocument owned by the #HexWidget, or
 *   %NULL.
 */
HexDocument *
hex_widget_get_document (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), NULL);
	g_return_val_if_fail (HEX_IS_DOCUMENT(self->document), NULL);

	return self->document;
}

/**
 * hex_widget_get_insert_mode:
 *
 * Get whether the widget is insert mode.
 *
 * Returns: %TRUE if the #HexWidget is in insert mode; %FALSE if it is in
 *   overwrite mode.
 */
gboolean
hex_widget_get_insert_mode (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), FALSE);

	return self->insert;
}

/**
 * hex_widget_get_group_type:
 *
 * Get the group type of the data of the #HexWidget.
 *
 * Returns: the group type of the data of the #HexWidget, by
 *   [enum@Hex.WidgetGroupType]
 */
HexWidgetGroupType
hex_widget_get_group_type (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), HEX_WIDGET_GROUP_BYTE);

	return self->group_type;
}

/**
 * hex_widget_get_fade_zeroes:
 *
 * Retrieve whether zeroes (`00`) are faded in the hex display.
 *
 * Returns: `TRUE` if zeroes are faded; `FALSE` otherwise
 *
 * Since: 4.8
 */
gboolean
hex_widget_get_fade_zeroes (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), FALSE);

	return self->fade_zeroes;
}

/**
 * hex_widget_set_fade_zeroes:
 * @fade: Whether zeroes (`00` in the hex display) should be faded
 *
 * Set whether zeroes (`00`) are faded in the hex display.
 *
 * Since: 4.8
 */
void
hex_widget_set_fade_zeroes (HexWidget *self, gboolean fade)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	self->fade_zeroes = fade;

	gtk_widget_queue_draw (GTK_WIDGET(self));
	g_object_notify_by_pspec (G_OBJECT(self), properties[FADE_ZEROES]);
}

/**
 * hex_widget_get_display_control_characters:
 *
 * Retrieve whether ASCII control characters are shown in the ASCII display.
 *
 * Returns: `TRUE` if control characters are displayed; `FALSE` otherwise
 *
 * Since: 4.10
 */
gboolean
hex_widget_get_display_control_characters (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), FALSE);

	return self->display_control_characters;
}

/**
 * hex_widget_set_display_control_characters
 * @display: Whether ASCII control characters should be displayed
 *
 * Set whether ASCII control characters are shown in the ASCII display.
 *
 * Since: 4.10
 */
void
hex_widget_set_display_control_characters (HexWidget *self, gboolean display)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	self->display_control_characters = display;

	gtk_widget_queue_draw (GTK_WIDGET(self));
	g_object_notify_by_pspec (G_OBJECT(self), properties[DISPLAY_CONTROL_CHARACTERS]);
}

G_GNUC_END_IGNORE_DEPRECATIONS
