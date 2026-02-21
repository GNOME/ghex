// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-highlight.h"

G_BEGIN_DECLS

struct _HexHighlight
{
	GObject parent_instance;

	gint64 start_offset;
	gint64 end_offset;
};

int _hex_highlight_compare_func (gconstpointer a, gconstpointer b, gpointer user_data);

G_END_DECLS
