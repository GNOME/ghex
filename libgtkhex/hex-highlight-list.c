// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-highlight-list"

#include "hex-highlight-list.h"

#include "hex-highlight-private.h"

static void list_model_iface_init (GListModelInterface *iface);

enum
{
	PROP_ONE = 1,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

enum signal_types {
	SIGNAL_ONE,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

struct _HexHighlightList
{
	GObject parent_instance;

	GHashTable *ht;
	GPtrArray *arr;
};

/* <GListModelInterface> */

G_DEFINE_FINAL_TYPE_WITH_CODE (HexHighlightList, hex_highlight_list, G_TYPE_OBJECT,
		G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static guint
hex_highlight_list_get_n_items (GListModel *list)
{
	HexHighlightList *self = HEX_HIGHLIGHT_LIST (list);

	return self->arr->len;
}

/* This will be slow with a large number of items. Not recommended for general usage. */

static gpointer
hex_highlight_list_get_item (GListModel *list, guint position)
{
	HexHighlightList *self = HEX_HIGHLIGHT_LIST (list);
	HexHighlight *retval;

	if (position >= self->arr->len)
		return NULL;

	retval = g_ptr_array_index (self->arr, position);

	g_assert (HEX_IS_HIGHLIGHT (retval));

	return g_object_ref (retval);
}

static GType
hex_highlight_list_get_item_type (GListModel *list)
{
	return HEX_TYPE_HIGHLIGHT;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
	iface->get_n_items = hex_highlight_list_get_n_items;
	iface->get_item = hex_highlight_list_get_item;
	iface->get_item_type = hex_highlight_list_get_item_type;
}

/* </GListModelInterface> */

static void
hex_highlight_list_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexHighlightList *self = HEX_HIGHLIGHT_LIST(object);

	switch (property_id)
	{
		case PROP_ONE:
			/* --- */
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_highlight_list_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexHighlightList *self = HEX_HIGHLIGHT_LIST(object);

	switch (property_id)
	{
		case PROP_ONE:
			/* --- */
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_highlight_list_init (HexHighlightList *self)
{
	self->ht = g_hash_table_new_full (g_int64_hash, g_int64_equal, g_free, (GDestroyNotify)g_ptr_array_unref);

	self->arr = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
hex_highlight_list_dispose (GObject *object)
{
	HexHighlightList *self = HEX_HIGHLIGHT_LIST(object);

	g_clear_pointer (&self->ht, g_hash_table_unref);
	g_clear_pointer (&self->arr, g_ptr_array_unref);

	G_OBJECT_CLASS(hex_highlight_list_parent_class)->dispose (object);
}

static void
hex_highlight_list_finalize (GObject *object)
{
	HexHighlightList *self = HEX_HIGHLIGHT_LIST(object);

	G_OBJECT_CLASS(hex_highlight_list_parent_class)->finalize (object);
}

static void
hex_highlight_list_class_init (HexHighlightListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->dispose =  hex_highlight_list_dispose;
	object_class->finalize = hex_highlight_list_finalize;

	object_class->set_property = hex_highlight_list_set_property;
	object_class->get_property = hex_highlight_list_get_property;

	properties[PROP_ONE] = g_param_spec_string ("property-one", NULL, NULL,
			/* default: */	"Hello, world!",
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	signals[SIGNAL_ONE] = g_signal_new_class_handler ("signal-one",
			G_OBJECT_CLASS_TYPE (object_class),
			G_SIGNAL_RUN_LAST,
		/* no default C function */
			NULL,
		/* defaults for accumulator, marshaller &c. */
			NULL, NULL, NULL,	
		/* No return type or params. */
			G_TYPE_NONE, 0);
}

HexHighlightList *
hex_highlight_list_new (void)
{
	return g_object_new (HEX_TYPE_HIGHLIGHT_LIST, NULL);
}

/* Transfer: container */

HexHighlight **
hex_highlight_list_get_highlights_for_range (HexHighlightList *self, gint64 start, gint64 end, guint *n_found)
{
	g_autoptr(GPtrArray) ret_arr = NULL;

	g_return_val_if_fail (HEX_IS_HIGHLIGHT_LIST (self), NULL);
	g_return_val_if_fail (end >= start, NULL);

	ret_arr = g_ptr_array_new_null_terminated (0, NULL, TRUE);

	for (gint64 i = start; i <= end; ++i)
	{
		GPtrArray *hl_arr = g_hash_table_lookup (self->ht, &i);

		if (!hl_arr)
			continue;

		for (guint j = 0; j < hl_arr->len; ++j)
		{
			HexHighlight *highlight = g_ptr_array_index (hl_arr, j);

			g_ptr_array_add (ret_arr, highlight);
		}
	}

	if (n_found)
		*n_found = ret_arr->len;

	return (HexHighlight **) g_steal_pointer (&ret_arr->pdata);
}

/* append() helpers */

static void
append_ht (HexHighlightList *self, HexHighlight *highlight)
{
	GPtrArray *existing_val_arr = g_hash_table_lookup (self->ht, &highlight->start_offset);

	if (existing_val_arr)
	{
		g_assert (existing_val_arr->len > 0);

		g_ptr_array_add (existing_val_arr, g_object_ref (highlight));
	}
	else
	{
		g_autofree gint64 *keyp = g_new0 (gint64, 1);
		g_autoptr(GPtrArray) new_val_arr = g_ptr_array_new_with_free_func (g_object_unref);

		*keyp = highlight->start_offset;
		g_ptr_array_add (new_val_arr, g_object_ref (highlight));

		g_hash_table_insert (self->ht, g_steal_pointer (&keyp), g_steal_pointer (&new_val_arr));
	}
}

static void
append_arr (HexHighlightList *self, HexHighlight *highlight)
{
	g_ptr_array_add (self->arr, g_object_ref (highlight));

	g_list_model_items_changed (G_LIST_MODEL(self), self->arr->len - 1, 0, 1);
}

/* item = transfer none */

void
hex_highlight_list_append (HexHighlightList *self, gpointer item)
{
	HexHighlight *highlight = item;

	g_return_if_fail (HEX_IS_HIGHLIGHT_LIST (self));
	g_return_if_fail (HEX_IS_HIGHLIGHT (highlight));

	append_ht (self, highlight);
	append_arr (self, highlight);
}

/* remove() helpers */

static gboolean
remove_ht (HexHighlightList *self, HexHighlight *highlight)
{
	GPtrArray *values_arr = g_hash_table_lookup (self->ht, &highlight->start_offset);

	if (!values_arr)
	{
		g_debug ("%s: No highlights found in list %p at offset %ld", __func__, self, highlight->start_offset);

		return FALSE;
	}

	for (guint i = 0; i < values_arr->len; ++i)
	{
		HexHighlight *hl = g_ptr_array_index (values_arr, i);

		if (highlight == hl)
		{
			g_debug ("%s: Removing highlight %p from list %p", __func__, highlight, self);

			g_ptr_array_remove_index (values_arr, i);

			if (values_arr->len == 0)
			{
				g_hash_table_remove (self->ht, &highlight->start_offset);
				g_assert (g_hash_table_lookup (self->ht, &highlight->start_offset) == NULL);
			}

			return TRUE;
		}
	}
	
	return FALSE;
}

static gboolean
remove_arr (HexHighlightList *self, HexHighlight *highlight)
{
	guint index;

	if (g_ptr_array_find (self->arr, highlight, &index))
	{
		g_ptr_array_remove_index (self->arr, index);

		g_list_model_items_changed (G_LIST_MODEL(self), index, 1, 0);

		return TRUE;
	}

	return FALSE;
}

void
hex_highlight_list_remove (HexHighlightList *self, HexHighlight *highlight)
{
	gboolean ht_ret, arr_ret;

	g_return_if_fail (HEX_IS_HIGHLIGHT_LIST (self));
	g_return_if_fail (HEX_IS_HIGHLIGHT (highlight));

	ht_ret = remove_ht (self, highlight);
	arr_ret = remove_arr (self, highlight);

	g_assert (ht_ret == arr_ret);
}

void
hex_highlight_list_remove_all (HexHighlightList *self)
{
	guint n_items_before;

	g_return_if_fail (HEX_IS_HIGHLIGHT_LIST (self));

	n_items_before = self->arr->len;

	g_hash_table_remove_all (self->ht);
	g_ptr_array_set_size (self->arr, 0);

	g_list_model_items_changed (G_LIST_MODEL(self), 0, n_items_before, 0);
}
