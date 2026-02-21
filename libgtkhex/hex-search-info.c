#include "hex-search-info.h"
#include "hex-search-info-private.h"

#include <stdint.h>

enum
{
	PROP_0,
	PROP_WHAT,
	PROP_LEN,
	PROP_START,
	PROP_FLAGS,
	PROP_FOUND_MSG,
	PROP_NOT_FOUND_MSG,
	PROP_POS,
	PROP_FOUND,
	PROP_FOUND_OFFSET,
	PROP_FOUND_LEN,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE (HexSearchInfo, hex_search_info, G_TYPE_OBJECT)

static void
hex_search_info_set_what (HexSearchInfo *self, const guint8 *what)
{
	g_return_if_fail (HEX_IS_SEARCH_INFO (self));

	self->what = what;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_WHAT]);
}

const guint8 *
hex_search_info_get_what (HexSearchInfo *self)
{
	g_return_val_if_fail (HEX_IS_SEARCH_INFO (self), NULL);

	return self->what;
}

static void
hex_search_info_set_len (HexSearchInfo *self, size_t len)
{
	g_return_if_fail (HEX_IS_SEARCH_INFO (self));

	self->len = len;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_LEN]);
}

size_t
hex_search_info_get_len (HexSearchInfo *self)
{
	g_return_val_if_fail (HEX_IS_SEARCH_INFO (self), 0);

	return self->len;
}

static void
hex_search_info_set_start (HexSearchInfo *self, gint64 start)
{
	g_return_if_fail (HEX_SEARCH_INFO (self));

	self->start = start;
	self->pos = start;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_START]);
}

gint64
hex_search_info_get_start (HexSearchInfo *self)
{
	g_return_val_if_fail (HEX_IS_SEARCH_INFO (self), 0);

	return self->start;
}

static void
hex_search_info_set_flags (HexSearchInfo *self, HexSearchFlags flags)
{
	g_return_if_fail (HEX_SEARCH_INFO (self));

	self->flags = flags;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_FLAGS]);
}

HexSearchFlags
hex_search_info_get_flags (HexSearchInfo *self)
{
	g_return_val_if_fail (HEX_IS_SEARCH_INFO (self), HEX_SEARCH_NONE);

	return self->flags;
}

void
hex_search_info_set_found_msg (HexSearchInfo *self, const char *found_msg)
{
	g_return_if_fail (HEX_SEARCH_INFO (self));

	g_clear_pointer (&self->found_msg, g_free);

	if (found_msg)
		self->found_msg = g_strdup (found_msg);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_FOUND_MSG]);
}

const char *
hex_search_info_get_found_msg (HexSearchInfo *self)
{
	g_return_val_if_fail (HEX_IS_SEARCH_INFO (self), NULL);

	return self->found_msg;
}

void
hex_search_info_set_not_found_msg (HexSearchInfo *self, const char *not_found_msg)
{
	g_return_if_fail (HEX_SEARCH_INFO (self));

	g_clear_pointer (&self->not_found_msg, g_free);

	if (not_found_msg)
		self->not_found_msg = g_strdup (not_found_msg);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_NOT_FOUND_MSG]);
}

const char *
hex_search_info_get_not_found_msg (HexSearchInfo *self)
{
	g_return_val_if_fail (HEX_IS_SEARCH_INFO (self), NULL);

	return self->not_found_msg;
}

gboolean
hex_search_info_get_found (HexSearchInfo *self)
{
	g_return_val_if_fail (HEX_IS_SEARCH_INFO (self), FALSE);

	return self->found;
}

gint64
hex_search_info_get_found_offset (HexSearchInfo *self)
{
	g_return_val_if_fail (HEX_IS_SEARCH_INFO (self), 0);

	return self->found_offset;
}

size_t
hex_search_info_get_found_len (HexSearchInfo *self)
{
	g_return_val_if_fail (HEX_IS_SEARCH_INFO (self), 0);

	return self->found_len;
}

void
_hex_search_info_set_pos (HexSearchInfo *self, gint64 pos)
{
	g_return_if_fail (HEX_IS_SEARCH_INFO (self));

	self->pos = pos;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_POS]);
}

gint64
hex_search_info_get_pos (HexSearchInfo *self)
{
	g_return_val_if_fail (HEX_IS_SEARCH_INFO (self), 0);

	return self->pos;
}

static void
hex_search_info_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexSearchInfo *self = HEX_SEARCH_INFO(object);

	switch (property_id)
	{
		case PROP_WHAT:
			hex_search_info_set_what (self, g_value_get_pointer (value));
			break;

		case PROP_LEN:
			hex_search_info_set_len (self, g_value_get_ulong (value));
			break;

		case PROP_START:
			hex_search_info_set_start (self, g_value_get_int64 (value));
			break;

		case PROP_FLAGS:
			// FIXME enum
			hex_search_info_set_flags (self, g_value_get_int (value));
			break;

		case PROP_FOUND_MSG:
			hex_search_info_set_found_msg (self, g_value_get_string (value));
			break;

		case PROP_NOT_FOUND_MSG:
			hex_search_info_set_not_found_msg (self, g_value_get_string (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_search_info_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexSearchInfo *self = HEX_SEARCH_INFO(object);

	switch (property_id)
	{
		case PROP_WHAT:
			g_value_set_pointer (value, (gpointer) hex_search_info_get_what (self));
			break;

		case PROP_LEN:
			g_value_set_ulong (value, hex_search_info_get_len (self));
			break;

		case PROP_START:
			g_value_set_int64 (value, hex_search_info_get_start (self));
			break;

		case PROP_FLAGS:
			// FIXME - enum
			g_value_set_int (value, hex_search_info_get_flags (self));
			break;

		case PROP_FOUND_MSG:
			g_value_set_string (value, hex_search_info_get_found_msg (self));
			break;

		case PROP_NOT_FOUND_MSG:
			g_value_set_string (value, hex_search_info_get_not_found_msg (self));
			break;

		case PROP_POS:
			g_value_set_int64 (value, hex_search_info_get_pos (self));
			break;

		case PROP_FOUND:
			g_value_set_boolean (value, hex_search_info_get_found (self));
			break;

		case PROP_FOUND_OFFSET:
			g_value_set_int64 (value, hex_search_info_get_found_offset (self));
			break;

		case PROP_FOUND_LEN:
			g_value_set_ulong (value, hex_search_info_get_found_len (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_search_info_init (HexSearchInfo *self)
{
}

static void
hex_search_info_dispose (GObject *object)
{
	HexSearchInfo *self = HEX_SEARCH_INFO(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_search_info_parent_class)->dispose (object);
}

static void
hex_search_info_finalize (GObject *object)
{
	HexSearchInfo *self = HEX_SEARCH_INFO(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_search_info_parent_class)->finalize (object);
}

static void
hex_search_info_class_init (HexSearchInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose =  hex_search_info_dispose;
	object_class->finalize = hex_search_info_finalize;

	object_class->set_property = hex_search_info_set_property;
	object_class->get_property = hex_search_info_get_property;

	/* PROPERTIES */

	properties[PROP_WHAT] = g_param_spec_pointer ("what", NULL, NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | default_flags);

	properties[PROP_LEN] = g_param_spec_ulong ("len", NULL, NULL,
			0, SIZE_MAX, 0,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | default_flags);

	properties[PROP_START] = g_param_spec_int64 ("start", NULL, NULL,
			0, INT64_MAX, 0,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | default_flags);

	// FIXME - enum
	properties[PROP_FLAGS] = g_param_spec_int ("flags", NULL, NULL,
			HEX_SEARCH_NONE, HEX_SEARCH_IGNORE_CASE, HEX_SEARCH_NONE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | default_flags);

	properties[PROP_FOUND_MSG] = g_param_spec_string ("found-msg", NULL, NULL,
			NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | default_flags);

	properties[PROP_NOT_FOUND_MSG] = g_param_spec_string ("not-found-msg", NULL, NULL,
			NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | default_flags);

	properties[PROP_POS] = g_param_spec_int64 ("pos", NULL, NULL,
			0, INT64_MAX, 0,
			G_PARAM_READABLE | default_flags);

	properties[PROP_FOUND] = g_param_spec_boolean ("found", NULL, NULL,
			FALSE,
			G_PARAM_READABLE | default_flags);

	properties[PROP_FOUND_OFFSET] = g_param_spec_int64 ("found-offset", NULL, NULL,
			0, INT64_MAX, 0,
			G_PARAM_READABLE | default_flags);

	properties[PROP_FOUND_LEN] = g_param_spec_ulong ("found-len", NULL, NULL,
			0, SIZE_MAX, 0,
			G_PARAM_READABLE | default_flags);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

HexSearchInfo *
hex_search_info_new (void)
{
	return g_object_new (HEX_TYPE_SEARCH_INFO, NULL);
}

void
_hex_search_info_found (HexSearchInfo *self, gint64 found_offset, size_t found_len)
{
	g_return_if_fail (HEX_IS_SEARCH_INFO (self));
	g_return_if_fail (found_offset >= 0);
	g_return_if_fail (found_len > 0);

	self->found = TRUE;
	self->found_offset = found_offset;
	self->found_len = found_len;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_FOUND]);
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_FOUND_OFFSET]);
	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_FOUND_LEN]);
}
