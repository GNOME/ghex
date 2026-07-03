// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-auto-highlight"

#include "hex-auto-highlight-private.h"
#include "hex-highlight-private.h"
#include "hex-search-info-private.h"
#include "util.h"

/* Rate in number of seconds (as double) that the auto-highlight will report a progress update for a search operation.
 */
#define PROGRESS_REFRESH_RATE 1.0

enum
{
	PROP_0,
	PROP_DOCUMENT,
	PROP_SEARCH_INFO,
	PROP_HIGHLIGHTS,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

enum
{
	SIG_SEARCH_PROGRESS_UPDATE,
	SIG_REFRESH_COMPLETE,
	SIG_REFRESH_CANCELLED,
	SIG_HIGHLIGHTS_CHANGED,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (HexAutoHighlight, hex_auto_highlight, G_TYPE_OBJECT)

/* <HighlightAdditionData> - helper for threadsafe addition of highlight to auto_highlight */

typedef struct
{
	HexAutoHighlight *auto_highlight;
	HexHighlight *highlight;
} HighlightAdditionData;

static HighlightAdditionData *
highlight_addition_data_new (HexAutoHighlight *auto_highlight, HexHighlight *highlight)
{
	HighlightAdditionData *data;

	g_assert (HEX_IS_AUTO_HIGHLIGHT (auto_highlight));
	g_assert (HEX_IS_HIGHLIGHT (highlight));

	data = g_new0 (HighlightAdditionData, 1);
	data->auto_highlight = g_object_ref (auto_highlight);
	data->highlight = g_object_ref (highlight);

	return data;
}

static void
highlight_addition_data_destroy (HighlightAdditionData *data)
{
	if (!data)
		return;

	g_clear_object (&data->auto_highlight);
	g_clear_object (&data->highlight);
	g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (HighlightAdditionData, highlight_addition_data_destroy)

/* </HighlightAdditionData> */

#if 0
static void
_hex_auto_highlight_sort (HexAutoHighlight *self)
{
	g_return_if_fail (HEX_IS_AUTO_HIGHLIGHT (self));

	if (self->freeze_sorting)
		self->sort_queued = TRUE;
	else
		g_list_store_sort (self->highlights, _hex_highlight_compare_func, NULL);
}

static void
_hex_auto_highlight_freeze_sorting (HexAutoHighlight *self)
{
	g_return_if_fail (HEX_IS_AUTO_HIGHLIGHT (self));

	self->freeze_sorting = TRUE;
}

static void
_hex_auto_highlight_thaw_sorting (HexAutoHighlight *self)
{
	g_return_if_fail (HEX_IS_AUTO_HIGHLIGHT (self));

	if (self->freeze_sorting && self->sort_queued)
		_hex_auto_highlight_sort (self);

	self->freeze_sorting = FALSE;
	self->sort_queued = FALSE;
}
#endif

static gboolean
add_highlight__threadsafe (gpointer user_data)
{
	HighlightAdditionData *addition_data = user_data;

	g_assert (addition_data != NULL);
	g_assert (HEX_IS_AUTO_HIGHLIGHT (addition_data->auto_highlight));
	g_assert (HEX_IS_HIGHLIGHT (addition_data->highlight));
	g_assert (g_main_context_is_owner (g_main_context_default ()));

	hex_auto_highlight_add_highlight (addition_data->auto_highlight, addition_data->highlight);

	return G_SOURCE_REMOVE;
}

static gboolean
emit_highlights_changed__threadsafe (gpointer user_data)
{
	HexAutoHighlight *self = user_data;

	g_assert (HEX_IS_AUTO_HIGHLIGHT (self));
	g_assert (g_main_context_is_owner (g_main_context_default ()));

	g_signal_emit (self, signals[SIG_HIGHLIGHTS_CHANGED], 0);

	return G_SOURCE_REMOVE;
}

static gboolean
emit_search_progress_update__threadsafe (gpointer user_data)
{
	HexAutoHighlight *self = user_data;

	g_assert (HEX_IS_AUTO_HIGHLIGHT (self));
	g_assert (g_main_context_is_owner (g_main_context_default ()));

	g_debug ("%s: search progress: %.4f%%", __func__, 100.0 * self->search_progress);

	g_signal_emit (self, signals[SIG_SEARCH_PROGRESS_UPDATE], 0, self->search_progress);
	
	return G_SOURCE_REMOVE;
}

static gboolean
emit_refresh_complete__threadsafe (gpointer user_data)
{
	HexAutoHighlight *self = user_data;

	g_assert (HEX_IS_AUTO_HIGHLIGHT (self));
	g_assert (g_main_context_is_owner (g_main_context_default ()));

	g_signal_emit (self, signals[SIG_REFRESH_COMPLETE], 0);

	return G_SOURCE_REMOVE;
}

static gboolean
emit_refresh_cancelled__threadsafe (gpointer user_data)
{
	HexAutoHighlight *self = user_data;

	g_assert (HEX_IS_AUTO_HIGHLIGHT (self));
	g_assert (g_main_context_is_owner (g_main_context_default ()));

	g_signal_emit (self, signals[SIG_REFRESH_CANCELLED], 0);

	return G_SOURCE_REMOVE;
}

static void
reset_view_min_and_max (HexAutoHighlight *self)
{
	g_assert (HEX_IS_AUTO_HIGHLIGHT (self));

	self->view_min = 0;

	if (!self->document)
	{
		self->view_max = 0;
	}
	else
	{
		HexBuffer *buf = hex_document_get_buffer (self->document);

		self->view_max = hex_buffer_get_payload_size (buf);
	}
}

static void
do_refresh (HexAutoHighlight *self, GCancellable *cancellable, gboolean async)
{
	g_autoptr(GTimer) timer = NULL;

	g_return_if_fail (self->view_max > self->view_min && self->view_max != 0);

	hex_highlight_list_remove_all (self->highlights);

	timer = g_timer_new ();

//	_hex_auto_highlight_freeze_sorting (self);

	for (gint64 i = self->search_info->start; i <= self->view_max; ++i)
	{
		if (g_cancellable_is_cancelled (cancellable))
			break;

		i = CLAMP (i, self->view_min, self->view_max);
		self->search_info->pos = i;

		if (hex_document_compare_data_full (self->document, self->search_info) == 0)
		{
			g_autoptr(HexHighlight) highlight = NULL;
			const gint64 start_offset = self->search_info->pos;
			const gint64 end_offset = self->search_info->pos + self->search_info->found_len - 1;

			highlight = hex_highlight_new ();
			hex_highlight_update (highlight, start_offset, end_offset);

			if (async)
			{
				g_autoptr(HighlightAdditionData) addition_data = highlight_addition_data_new (self, highlight);

				g_main_context_invoke_full (NULL, G_PRIORITY_DEFAULT, add_highlight__threadsafe, g_steal_pointer (&addition_data), (GDestroyNotify) highlight_addition_data_destroy);
			}
			else
				hex_auto_highlight_add_highlight (self, highlight);
		}

		if (g_timer_elapsed (timer, NULL) >= PROGRESS_REFRESH_RATE)
		{
			double percent = CLAMP ((double) self->search_info->pos / (double) self->view_max, 0.0, 1.0);

			self->search_progress = percent;

			g_main_context_invoke_full (NULL, G_PRIORITY_DEFAULT, emit_search_progress_update__threadsafe, g_object_ref (self), g_object_unref);

			g_timer_start (timer);
		}
	}

//	_hex_auto_highlight_thaw_sorting (self);

	g_main_context_invoke_full (NULL, G_PRIORITY_DEFAULT, emit_refresh_complete__threadsafe, g_object_ref (self), g_object_unref);
}

void
hex_auto_highlight_refresh_sync (HexAutoHighlight *self, GCancellable *cancellable)
{
	g_return_if_fail (HEX_IS_AUTO_HIGHLIGHT (self));
	g_return_if_fail (HEX_IS_SEARCH_INFO (self->search_info));
	g_return_if_fail (HEX_IS_DOCUMENT (self->document));

	do_refresh (self, cancellable, FALSE);
}

gboolean
hex_auto_highlight_refresh_finish (HexAutoHighlight *self, GAsyncResult *result)
{
	g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

	g_object_thaw_notify (G_OBJECT(self->highlights));

	return g_task_propagate_boolean (G_TASK(result), NULL);
}

static void
refresh_task_thread_func (GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
	HexAutoHighlight *self = source_object;

	g_assert (g_task_is_valid (task, source_object));

	do_refresh (self, cancellable, TRUE);

	g_weak_ref_set (&self->search_pending_wr, NULL);

	if (g_cancellable_is_cancelled (cancellable))
		g_task_return_boolean (task, FALSE);
	else
		g_task_return_boolean (task, TRUE);
}

static void
cancellable_cancelled_cb (GCancellable *cancellable, HexAutoHighlight *self)
{
	g_assert (HEX_IS_AUTO_HIGHLIGHT (self));
	g_assert (G_IS_CANCELLABLE (cancellable));

	g_main_context_invoke_full (NULL, G_PRIORITY_DEFAULT, emit_refresh_cancelled__threadsafe, g_object_ref (self), g_object_unref);
}

void
hex_auto_highlight_refresh_async (HexAutoHighlight *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_autoptr(GTask) task = NULL;
	g_autoptr(GTask) pending_task = NULL;

	g_return_if_fail (HEX_IS_AUTO_HIGHLIGHT (self));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	g_assert (HEX_IS_SEARCH_INFO (self->search_info));
	g_assert (HEX_IS_DOCUMENT (self->document));

	task = g_task_new (self, cancellable, callback, user_data);

	pending_task = g_weak_ref_get (&self->search_pending_wr);
	if (pending_task)
	{
		g_debug ("%s: Search already pending for HexAutoHighlight %p - refresh not permitted.", __func__, self);

		// FIXME - g_task_return_error ?
		g_task_return_boolean (task, FALSE);
		return;
	}

	g_weak_ref_set (&self->search_pending_wr, task);

	if (cancellable)
	{
		g_cancellable_connect (cancellable, G_CALLBACK(cancellable_cancelled_cb), g_object_ref (self), g_object_unref);
	}

	g_object_freeze_notify (G_OBJECT(self->highlights));

	g_task_run_in_thread (task, refresh_task_thread_func);
}

/* Transfer none */
void
hex_auto_highlight_add_highlight (HexAutoHighlight *self, HexHighlight *highlight)
{
	g_return_if_fail (HEX_IS_AUTO_HIGHLIGHT (self));
	g_return_if_fail (HEX_IS_HIGHLIGHT (highlight));

	// FIXME - disable checking for dupes for now - seems too slow.
#if 0
	for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL(self->highlights)); ++i)
	{
		g_autoptr(HexHighlight) existing_hl = g_list_model_get_item (G_LIST_MODEL(self->highlights), i);

		if (highlight->start_offset == existing_hl->start_offset && highlight->end_offset == existing_hl->end_offset)
		{
			return;
		}
	}
#endif

	hex_highlight_list_append (self->highlights, highlight);

//	_hex_auto_highlight_sort (self);

	g_signal_emit (self, signals[SIG_HIGHLIGHTS_CHANGED], 0);

//	g_main_context_invoke_full (NULL, G_PRIORITY_DEFAULT, emit_highlights_changed__threadsafe, g_object_ref (self), g_object_unref);
}

/* Transfer none */
HexHighlightList *
hex_auto_highlight_get_highlights (HexAutoHighlight *self)
{
	g_return_val_if_fail (HEX_IS_AUTO_HIGHLIGHT (self), NULL);

	return self->highlights;
}

/* Transfer none */
void
hex_auto_highlight_set_search_info (HexAutoHighlight *self, HexSearchInfo *search_info)
{
	g_return_if_fail (HEX_IS_AUTO_HIGHLIGHT (self));
	g_return_if_fail (HEX_IS_SEARCH_INFO (search_info));

	self->search_info = g_object_ref (search_info);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_SEARCH_INFO]);
}

/* Transfer none */
HexSearchInfo *
hex_auto_highlight_get_search_info (HexAutoHighlight *self)
{
	g_return_val_if_fail (HEX_IS_AUTO_HIGHLIGHT (self), NULL);

	return self->search_info;
}

/* Transfer none */
static void
hex_auto_highlight_set_document (HexAutoHighlight *self, HexDocument *document)
{
	g_return_if_fail (HEX_IS_AUTO_HIGHLIGHT (self));
	g_return_if_fail (HEX_IS_DOCUMENT (document));

	if (g_set_object (&self->document, document))
	{
		reset_view_min_and_max (self);

		g_signal_handlers_disconnect_by_data (self->document, self);

		g_signal_connect_object (self->document, "file-loaded", G_CALLBACK(reset_view_min_and_max), self, G_CONNECT_SWAPPED);
		g_signal_connect_object (self->document, "document-changed", G_CALLBACK(reset_view_min_and_max), self, G_CONNECT_SWAPPED);
		g_signal_connect_object (self->document, "file-name-changed", G_CALLBACK(reset_view_min_and_max), self, G_CONNECT_SWAPPED);

		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_DOCUMENT]);
	}
}

/* Transfer none */
HexDocument *
hex_auto_highlight_get_document (HexAutoHighlight *self)
{
	g_return_val_if_fail (HEX_IS_AUTO_HIGHLIGHT (self), NULL);

	return self->document;
}

static void
hex_auto_highlight_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexAutoHighlight *self = HEX_AUTO_HIGHLIGHT(object);

	switch (property_id)
	{
		case PROP_DOCUMENT:
			hex_auto_highlight_set_document (self, g_value_get_object (value));
			break;

		case PROP_SEARCH_INFO:
			hex_auto_highlight_set_search_info (self, g_value_get_object (value));
			break;
			
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_auto_highlight_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexAutoHighlight *self = HEX_AUTO_HIGHLIGHT(object);

	switch (property_id)
	{
		case PROP_DOCUMENT:
			g_value_set_object (value, hex_auto_highlight_get_document (self));
			break;

		case PROP_SEARCH_INFO:
			g_value_set_object (value, hex_auto_highlight_get_search_info (self));
			break;

		case PROP_HIGHLIGHTS:
			g_value_set_object (value, hex_auto_highlight_get_highlights (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_auto_highlight_init (HexAutoHighlight *self)
{
	self->highlights = hex_highlight_list_new ();
	g_weak_ref_init (&self->search_pending_wr, NULL);
}

static void
hex_auto_highlight_dispose (GObject *object)
{
	HexAutoHighlight *self = HEX_AUTO_HIGHLIGHT(object);

	{
		g_autoptr(GTask) task = g_weak_ref_get (&self->search_pending_wr);

		if (task)
			g_cancellable_cancel (g_task_get_cancellable (task));

		g_weak_ref_set (&self->search_pending_wr, NULL);
	}

	g_clear_object (&self->document);
	g_clear_object (&self->highlights); 
	g_clear_object (&self->search_info);

	/* Chain up */
	G_OBJECT_CLASS(hex_auto_highlight_parent_class)->dispose (object);
}

static void
hex_auto_highlight_finalize (GObject *object)
{
	HexAutoHighlight *self = HEX_AUTO_HIGHLIGHT(object);

	g_weak_ref_clear (&self->search_pending_wr);

	/* Chain up */
	G_OBJECT_CLASS(hex_auto_highlight_parent_class)->finalize (object);
}

static void
hex_auto_highlight_class_init (HexAutoHighlightClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose =  hex_auto_highlight_dispose;
	object_class->finalize = hex_auto_highlight_finalize;
	object_class->set_property = hex_auto_highlight_set_property;
	object_class->get_property = hex_auto_highlight_get_property;

	properties[PROP_DOCUMENT] = g_param_spec_object ("document", NULL, NULL,
			HEX_TYPE_DOCUMENT,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_SEARCH_INFO] = g_param_spec_object ("search-info", NULL, NULL,
			HEX_TYPE_SEARCH_INFO,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_HIGHLIGHTS] = g_param_spec_object ("highlights", NULL, NULL,
			HEX_TYPE_HIGHLIGHT_LIST,
			default_flags | G_PARAM_READABLE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	/* Signals */

	signals[SIG_SEARCH_PROGRESS_UPDATE] = g_signal_new_class_handler (
			"search-progress-update",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			NULL,
			NULL, NULL, NULL,
			G_TYPE_NONE,
			1,
			G_TYPE_DOUBLE
			);

	signals[SIG_REFRESH_COMPLETE] = g_signal_new_class_handler (
			"refresh-complete",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			NULL,
			NULL, NULL, NULL,
			G_TYPE_NONE,
			0);

	signals[SIG_REFRESH_CANCELLED] = g_signal_new_class_handler (
			"refresh-cancelled",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			NULL,
			NULL, NULL, NULL,
			G_TYPE_NONE,
			0);

	signals[SIG_HIGHLIGHTS_CHANGED] = g_signal_new_class_handler (
			"highlights-changed",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			NULL,
			NULL, NULL, NULL,
			G_TYPE_NONE,
			0);
}

HexAutoHighlight *
hex_auto_highlight_new (HexDocument *document, HexSearchInfo *search_info)
{
	g_return_val_if_fail (HEX_IS_DOCUMENT (document), NULL);
	g_return_val_if_fail (search_info == NULL || HEX_IS_SEARCH_INFO (search_info), NULL);

	return g_object_new (HEX_TYPE_AUTO_HIGHLIGHT,
			"document", document,
			"search-info", search_info,
			NULL);
}

#if 0
//FIXME- publicize??
GListModel *
_hex_auto_highlight_build_1d_list (GListModel *auto_highlights)
{
	g_autoptr(GListStore) retval = NULL;

	g_assert (g_list_model_get_item_type (auto_highlights) == HEX_TYPE_AUTO_HIGHLIGHT);

	retval = g_list_store_new (HEX_TYPE_HIGHLIGHT);

	for (guint i = 0; i < g_list_model_get_n_items (auto_highlights); ++i)
	{
		g_autoptr(HexAutoHighlight) ahl = g_list_model_get_item (auto_highlights, i);

		for (guint j = 0; j < g_list_model_get_n_items (G_LIST_MODEL(ahl->highlights)); ++j)
		{
			g_autoptr(HexHighlight) hl = g_list_model_get_item (G_LIST_MODEL(ahl->highlights), j);

			g_list_store_append (retval, hl);
		}
	}

	g_list_store_sort (retval, _hex_highlight_compare_func, NULL);

	return (GListModel *) g_steal_pointer (&retval);
}
#endif

/*
 * returns the cancellable set by hex_auto_highlight_refresh_async, or `NULL`
 *
 * transfer none
 */
GCancellable *
hex_auto_highlight_get_cancellable (HexAutoHighlight *self)
{
	g_autoptr(GTask) task = NULL;
	GCancellable *cancellable = NULL;

	g_return_val_if_fail (HEX_IS_AUTO_HIGHLIGHT (self), NULL);

	task = g_weak_ref_get (&self->search_pending_wr);

	if (task)
		cancellable = g_task_get_cancellable (task);

	return cancellable;
}
