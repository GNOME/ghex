// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-mark.h"

G_BEGIN_DECLS

/**
 * HexMark:
 *
 * `HexMark` is a `GObject` which contains the metadata associated with a
 * mark for a hex document.
 *
 * To instantiate a `HexMark` object, use the [method@Hex.View.add_mark]
 * method.
 */
struct _HexMark
{
	GObject parent_instance;

	HexHighlight *highlight;
	gboolean have_custom_color;
	GdkRGBA custom_color;
};

#define HEX_MARK_DEFAULT_COLOR \
	((GdkRGBA){ \
		.red = 1.0f, \
		.green = 1.0f, \
		.blue = 0.0f, \
		.alpha = 0.5f \
	})

G_END_DECLS
