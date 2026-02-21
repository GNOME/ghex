// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-auto-highlight.h"

G_BEGIN_DECLS

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
	GListStore *highlights;

	gint64 view_min;
	gint64 view_max;

	double search_progress;	/* use-setter */

	GWeakRef search_pending_wr;
	guint search_status_timeout_id;
};

GListModel * _hex_auto_highlight_build_1d_list (GListModel *list);

G_END_DECLS
