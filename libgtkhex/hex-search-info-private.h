// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include "hex-search-info.h"

/*
 * @found: whether the string was found
 * @start: start offset of the payload, in bytes
 * @what: (array length=len) (element-type gint8): a pointer to the data to
 *   search within the #HexDocument
 * @len: length in bytes of the data to be searched for
 * @flags: [flags@Hex.SearchFlags] search flags (Since: 4.2)
 * @offset: offset of the found string
 * @found_len: length of the found string (may be different from the search
 *   string when dealing with regular expressions, for example) (Since: 4.2)
 * @found_msg: message intended to be displayed by the client if the string
 *   is found
 * @not_found_msg: message intended to be displayed by the client if the string
 *   is not found
 *
 */

struct _HexSearchInfo
{
	GObject parent_instance;

	/* in */
	const guint8 *what;
	size_t len;
	gint64 start;
	HexSearchFlags flags;
	char *found_msg;
	char *not_found_msg;

	/* out */
	gboolean found;
	gint64 found_offset;
	size_t found_len;

	/* iterator */
	gint64 pos;
};

void _hex_search_info_found (HexSearchInfo *self, gint64 found_offset, size_t found_len);
void _hex_search_info_set_pos (HexSearchInfo *self, gint64 pos);
