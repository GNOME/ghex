// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * HexSearchFlags:
 * @HEX_SEARCH_NONE: no search flags (byte-for-byte match)
 * @HEX_SEARCH_REGEX: regular expression search
 * @HEX_SEARCH_IGNORE_CASE: case-insensitive search
 *
 * Bitwise flags for search options that can be combined as desired.
 */
typedef enum
{
	HEX_SEARCH_NONE				= 0,
	HEX_SEARCH_REGEX			= 1 << 0,
	HEX_SEARCH_IGNORE_CASE		= 1 << 1,
} HexSearchFlags;

#define HEX_TYPE_SEARCH_INFO (hex_search_info_get_type ())
G_DECLARE_FINAL_TYPE (HexSearchInfo, hex_search_info, HEX, SEARCH_INFO, GObject)

HexSearchInfo *	hex_search_info_new (void);

const guint8 * hex_search_info_get_what (HexSearchInfo *self);
size_t hex_search_info_get_len (HexSearchInfo *self);
gint64 hex_search_info_get_start (HexSearchInfo *self);
HexSearchFlags hex_search_info_get_flags (HexSearchInfo *self);
void hex_search_info_set_found_msg (HexSearchInfo *self, const char *found_msg);
const char * hex_search_info_get_found_msg (HexSearchInfo *self);
void hex_search_info_set_not_found_msg (HexSearchInfo *self, const char *not_found_msg);
const char * hex_search_info_get_not_found_msg (HexSearchInfo *self);
gboolean hex_search_info_get_found (HexSearchInfo *self);
gint64 hex_search_info_get_found_offset (HexSearchInfo *self);
size_t hex_search_info_get_found_len (HexSearchInfo *self);

G_END_DECLS
