// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-auto-highlight.h"

/**
 * HexAutoHighlight:
 *
 * An object used to automatically highlight all visible occurrences
 * of a given string.
 */
struct _HexAutoHighlight
{
	GObject parent_instance;

	HexDocument *document;
	HexSearchInfo *search_info;
	HexHighlightList *highlights;

	gboolean freeze_sorting;
	gboolean sort_queued;

	gint64 view_min;
	gint64 view_max;

	double search_progress;	/* use-setter */

	GCancellable *cancellable;
	GWeakRef search_pending_wr;
};

/**
 * HEX_AUTO_HIGHLIGHT_DEFAULT_COLOR:
 *
 * Provides the default auto-highlight color by way of a compound literal of
 * type `GdkRGBA`.
 */
#define HEX_AUTO_HIGHLIGHT_DEFAULT_COLOR \
	((GdkRGBA){ \
		.red = 1.0f, \
		.green = 1.0f, \
		.blue = 0.5f, \
		.alpha = 0.75f \
	})

//GListModel * _hex_auto_highlight_build_1d_list (GListModel *list);
