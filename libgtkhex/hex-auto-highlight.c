// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-auto-highlight"

#include "hex-auto-highlight-private.h"
#include "hex-highlight-private.h"
#include "hex-search-info-private.h"
#include "util.h"

/* Rate in number of seconds (as double) that the auto-highlight will report a progress update for a search operation.
 */
#define PROGRESS_REFRESH_RATE 3.0

/* Number of seconds (as integer) we're willing to wait for a threaded search to take before we give up.
 */
#define SEARCH_TIMEOUT 20

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
	N_SIGNALS
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (HexAutoHighlight, hex_auto_highlight, G_TYPE_OBJECT)

typedef struct
{
	HexAutoHighlight *ahl;
	gint64 start_offset;
	gint64 end_offset;
} HighlightCreationData;

static gboolean
add_highlight__threadsafe (gpointer user_data)
{
	g_autofree HighlightCreationData *data = user_data;
	g_autoptr(HexHighlight) highlight = hex_highlight_new ();

	g_assert (data && data->ahl);

	hex_highlight_update (highlight, data->start_offset, data->end_offset);
	hex_auto_highlight_add_highlight (data->ahl, highlight);

	return G_SOURCE_REMOVE;
}

static gboolean
emit_search_progress_update__threadsafe (gpointer user_data)
{
	HexAutoHighlight *self = user_data;

	g_signal_emit (self, signals[SIG_SEARCH_PROGRESS_UPDATE], 0, self->search_progress);

	return G_SOURCE_REMOVE;
}

static void
do_refresh (HexAutoHighlight *self, gboolean async)
{
	g_autoptr(GTimer) timer = NULL;

	g_return_if_fail (self->view_max > self->view_min && self->view_max != 0);

	g_list_store_remove_all (self->highlights);

	timer = g_timer_new ();

	for (gint64 i = self->search_info->start; i <= self->view_max; ++i)
	{
		i = CLAMP (i, self->view_min, self->view_max);
		self->search_info->pos = i;

		if (async)
		{
			GTask *task = g_weak_ref_get (&self->search_pending_wr);
			GCancellable *cancellable = NULL;

			if G_UNLIKELY (!task)
			{
				g_debug ("%s: We have no task. Unexpected! Breaking.", __func__);
				break;
			}

			cancellable = g_task_get_cancellable (task);
			if (g_cancellable_is_cancelled (cancellable))
				break;
		}

		if (hex_document_compare_data_full (self->document, self->search_info) == 0)
		{
			g_autofree HighlightCreationData *data = g_new0 (HighlightCreationData, 1);

			data->ahl = self;
			data->start_offset = self->search_info->pos;
			data->end_offset = self->search_info->pos + self->search_info->found_len - 1;

			g_main_context_invoke (NULL, add_highlight__threadsafe, g_steal_pointer (&data));
		}

		if (g_timer_elapsed (timer, NULL) >= PROGRESS_REFRESH_RATE)
		{
			double percent = CLAMP ((double) self->search_info->pos / (double) self->view_max, 0.0, 1.0);

			self->search_progress = percent;

			g_main_context_invoke (NULL, emit_search_progress_update__threadsafe, self);
			g_timer_start (timer);
		}
	}
}

void
hex_auto_highlight_refresh_sync (HexAutoHighlight *self)
{
	g_return_if_fail (HEX_IS_AUTO_HIGHLIGHT (self));
	g_return_if_fail (HEX_IS_SEARCH_INFO (self->search_info));
	g_return_if_fail (HEX_IS_DOCUMENT (self->document));

	do_refresh (self, FALSE);

	g_signal_emit (self, signals[SIG_REFRESH_COMPLETE], 0);
}

gboolean
hex_auto_highlight_refresh_finish (HexAutoHighlight *self, GAsyncResult *result)
{
	g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

	g_signal_emit (self, signals[SIG_REFRESH_COMPLETE], 0);

	return g_task_propagate_boolean (G_TASK(result), NULL);
}

static gboolean
search_status_func (gpointer data)
{
	HexAutoHighlight *self = data;
	GCancellable *cancellable = NULL;
	g_autoptr(GTask) search_pending = NULL;

	g_assert (HEX_IS_AUTO_HIGHLIGHT (self));

	self->search_status_timeout_id = 0;

	search_pending = g_weak_ref_get (&self->search_pending_wr);
	if (! search_pending)
	{
		g_debug ("%s: Search no longer pending. Will stop checking status.", __func__);
		return G_SOURCE_REMOVE;
	}

	g_return_val_if_fail (g_task_is_valid (search_pending, self), G_SOURCE_REMOVE);

	cancellable = g_task_get_cancellable (search_pending);
	g_debug ("%s: search has taken longer than %d seconds. Cancelling.", __func__, SEARCH_TIMEOUT);
	g_cancellable_cancel (cancellable);

	return G_SOURCE_REMOVE;
}

static void
refresh_task_func (GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
	HexAutoHighlight *self = source_object;

	g_assert (g_task_is_valid (task, source_object));

	g_clear_handle_id (&self->search_status_timeout_id, g_source_remove);
	
	self->search_status_timeout_id = g_timeout_add_seconds (SEARCH_TIMEOUT, search_status_func, self);

	do_refresh (self, TRUE);

	g_weak_ref_set (&self->search_pending_wr, NULL);

	if (g_cancellable_is_cancelled (cancellable))
		g_task_return_boolean (task, FALSE);
	else
		g_task_return_boolean (task, TRUE);
}

void
hex_auto_highlight_refresh_async (HexAutoHighlight *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_autoptr(GTask) task = NULL;
	g_autoptr(GTask) pending_task = NULL;

	g_return_if_fail (HEX_IS_AUTO_HIGHLIGHT (self));
	g_return_if_fail (HEX_IS_SEARCH_INFO (self->search_info));
	g_return_if_fail (HEX_IS_DOCUMENT (self->document));

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

	g_task_run_in_thread (task, refresh_task_func);
}

/* Transfer none */
void
hex_auto_highlight_add_highlight (HexAutoHighlight *self, HexHighlight *highlight)
{
	g_return_if_fail (HEX_IS_AUTO_HIGHLIGHT (self));
	g_return_if_fail (HEX_IS_HIGHLIGHT (highlight));

	for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL(self->highlights)); ++i)
	{
		g_autoptr(HexHighlight) existing_hl = g_list_model_get_item (G_LIST_MODEL(self->highlights), i);

		if (highlight->start_offset == existing_hl->start_offset && highlight->end_offset == existing_hl->end_offset)
		{
			return;
		}
	}

	g_list_store_append (self->highlights, highlight);
	g_list_store_sort (self->highlights, _hex_highlight_compare_func, NULL);
}

/* Transfer none */
GListModel *
hex_auto_highlight_get_highlights (HexAutoHighlight *self)
{
	g_return_val_if_fail (HEX_IS_AUTO_HIGHLIGHT (self), NULL);

	return G_LIST_MODEL (self->highlights);
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

	self->document = g_object_ref (document);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_DOCUMENT]);
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
hex_auto_highlight_constructed (GObject *object)
{
	HexAutoHighlight *self = HEX_AUTO_HIGHLIGHT(object);

	// TEST
	{
		HexBuffer *buf = hex_document_get_buffer (self->document);

		self->view_min = 0;
		self->view_max = hex_buffer_get_payload_size (buf);
	}
}

static void
hex_auto_highlight_init (HexAutoHighlight *self)
{
	self->highlights = g_list_store_new (HEX_TYPE_HIGHLIGHT);
	g_weak_ref_init (&self->search_pending_wr, NULL);
}

static void
hex_auto_highlight_dispose (GObject *object)
{
	HexAutoHighlight *self = HEX_AUTO_HIGHLIGHT(object);

	g_clear_handle_id (&self->search_status_timeout_id, g_source_remove);
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
	object_class->constructed = hex_auto_highlight_constructed;
	object_class->set_property = hex_auto_highlight_set_property;
	object_class->get_property = hex_auto_highlight_get_property;

	properties[PROP_DOCUMENT] = g_param_spec_object ("document", NULL, NULL,
			HEX_TYPE_DOCUMENT,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	properties[PROP_SEARCH_INFO] = g_param_spec_object ("search-info", NULL, NULL,
			HEX_TYPE_SEARCH_INFO,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_HIGHLIGHTS] = g_param_spec_object ("highlights", NULL, NULL,
			G_TYPE_LIST_MODEL,
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
}

HexAutoHighlight *
hex_auto_highlight_new (HexDocument *document, HexSearchInfo *search_info)
{
	g_return_val_if_fail (HEX_IS_DOCUMENT (document), NULL);
	g_return_val_if_fail (HEX_IS_SEARCH_INFO (search_info), NULL);

	return g_object_new (HEX_TYPE_AUTO_HIGHLIGHT,
			"document", document,
			"search-info", search_info,
			NULL);
}

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
