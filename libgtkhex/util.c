// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-util"

#include "util.h"

#include "hex-selection.h"

/* <UtilTuple> */

UtilTuple *
util_tuple_new (void)
{
	UtilTuple *self = g_new0 (UtilTuple, 1);
	return self;
}

void
util_tuple_destroy (UtilTuple *self)
{
	for (guint i = 0; i < G_N_ELEMENTS (self->data); ++i)
	{
		if (self->data[i] && self->destroy[i]) {
			g_clear_pointer (&self->data[i], self->destroy[i]);
		}
	}

	g_free (self);
}

/* </UtilTuple> */

void
util_warn_invalid_enum (const char *desc, GType enum_type, int enum_value)
{
	g_autofree char *enum_prettyname = g_enum_to_string (enum_type, enum_value);
	g_autofree char *warning_msg = g_strdup_printf ("Invalid %s: %s",
			desc ? desc : "parameter",
			enum_prettyname);

	g_warning ("%s", warning_msg);
}

/* All-purpose GObject tranform_to function that converts the non-NULL value of
 * an object to 'TRUE'.
 */
gboolean
util_have_object_transform_to (GBinding *binding,
		const GValue *from_value /* source = GObject */,
		GValue *to_value /* target = gboolean */,
		gpointer data) /* ignored */
{
	GObject *from = g_value_get_object (from_value);
	g_value_set_boolean (to_value, from ? TRUE : FALSE);
	return TRUE;
}

/* Transform a GFile to a document title string */
gboolean
file_title_transform_to (GBinding *binding, const GValue *from_value, GValue *to_value, gpointer data)
{
	GFile *file = g_value_get_object (from_value);
	g_autofree char *str = NULL;

	if (file)
		str = g_file_get_basename (file);
	else
		str = g_strdup (_("Untitled"));

	g_value_set_string (to_value, str);

	return TRUE;
}

/* transfer: full */
char *
util_gdk_rgba_to_hex_color (const GdkRGBA *color)
{
	char *retval = NULL;

	g_assert (color != NULL);

	retval = g_strdup_printf ("#%02X%02X%02X",
			(guchar) (color->red	* 255),
			(guchar) (color->green	* 255),
			(guchar) (color->blue	* 255)
			);

	return retval;
}

/* helper for common_set_gtkhex_font_from_settings.
 *
 * This function was written by Matthias Clasen and is included somewhere in
 * the GTK source tree.. I believe it is also included in libdazzle, but I
 * didn't want to include a whole dependency just for one function. LGPL, but
 * credit where credit is due!
 */
char *
util_pango_font_description_to_css (PangoFontDescription *desc,
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

int
util_hex_cpl_from_ascii_cpl (int ascii_cpl, HexGroupType group_type)
{
	return ascii_cpl * 2 + ascii_cpl / group_type;
}

int
util_ascii_cpl_from_hex_cpl (int hex_cpl, HexGroupType group_type)
{
	return hex_cpl * group_type / (2 * group_type + 1);
}

/* Sanity check to make sure font string is valid */
gboolean
util_validate_font_str (const char *font)
{
	g_autofree char *try_font_tag = g_strdup_printf ("<span font=\"%s\"></span>", font);

	return pango_parse_markup (try_font_tag, -1, 0, NULL, NULL, NULL, NULL);
}
