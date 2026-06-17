#include "ghex-search-bar.h"

#include "hex-highlight-list.h"
#include "hex-highlight-private.h"
#include "hex-auto-highlight-private.h"
#include "libgtkhex-enums.h"
#include "util.h"

#include "config.h"

/* time (in ms) that we wait before processing a typeahead search query. */

#define TYPEAHEAD_DELAY_TIME	500

enum
{
	PROP_AUTO_HIGHLIGHT = 1,
	PROP_SELECTED_HIGHLIGHT,
	PROP_WRAPAROUND,
	PROP_TYPEAHEAD,
	PROP_REPLACE_MODE,
	PROP_REGEX_ENABLED,
	PROP_IGNORE_CASE,
	PROP_SEARCH_FLAGS,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _GHexSearchBar
{
	GHexPane parent_instance;

	HexAutoHighlight *auto_highlight;
	guint selected_highlight;

	gboolean wraparound;
	gboolean replace_mode;

	gboolean typeahead;
	gulong typeahead_handler;

	gboolean regex_enabled;
	gboolean ignore_case;
	HexSearchFlags search_flags;

	guint search_entry_refresh_timeout_id;

	/* From template: */

	/* direct parent; otherwise unused */
	GtkWidget *breakpoint_bin;
	GtkButton *replace_all_button;
	GtkButton *replace_button;
	GtkButton *replace_entry;
	HexView *search_entry;
	GtkRevealer *search_progress_revealer;
	GtkProgressBar *search_progress_bar;
	GtkButton *search_progress_cancel_button;
	GtkLabel *num_matches_label;
};

G_DEFINE_FINAL_TYPE (GHexSearchBar, ghex_search_bar, GHEX_TYPE_PANE)

static void _ghex_search_bar_set_auto_highlight (GHexSearchBar *self, HexAutoHighlight *auto_highlight);

static void
show_search_progress (GHexSearchBar *self)
{
	g_assert (GHEX_IS_SEARCH_BAR (self));

	gtk_widget_set_sensitive (GTK_WIDGET(self->search_progress_cancel_button), TRUE);
	gtk_revealer_set_reveal_child (self->search_progress_revealer, TRUE);
}

static void
clear_search_progress (GHexSearchBar *self)
{
	g_assert (GHEX_IS_SEARCH_BAR (self));

	gtk_widget_set_sensitive (GTK_WIDGET(self->search_progress_cancel_button), FALSE);
	gtk_revealer_set_reveal_child (self->search_progress_revealer, FALSE);
	gtk_progress_bar_set_fraction (self->search_progress_bar, 0.0);
}

static void
_ghex_search_bar_cancel_query (GHexSearchBar *self)
{
	g_assert (GHEX_IS_SEARCH_BAR (self));

	clear_search_progress (self);

	_ghex_search_bar_set_auto_highlight (self, NULL);
	ghex_search_bar_set_selected_highlight (self, 0);
}

static void
auto_highlight_search_progress_update_cb (GHexSearchBar *self, double progress, HexAutoHighlight *auto_highlight)
{
	g_assert (GHEX_IS_SEARCH_BAR (self));
	g_assert (HEX_IS_AUTO_HIGHLIGHT (auto_highlight));

	if (auto_highlight != self->auto_highlight)
		return;

	show_search_progress (self);

	gtk_progress_bar_set_fraction (self->search_progress_bar, progress);
}

static void
auto_highlight_refresh_complete_cb (GHexSearchBar *self, HexAutoHighlight *auto_highlight)
{
	g_assert (GHEX_IS_SEARCH_BAR (self));
	g_assert (HEX_IS_AUTO_HIGHLIGHT (auto_highlight));

	if (auto_highlight != self->auto_highlight)
		return;

	clear_search_progress (self);

	ghex_search_bar_set_selected_highlight (self, 0);
}

static void
auto_highlight_refresh_cancelled_cb (GHexSearchBar *self, HexAutoHighlight *auto_highlight)
{
	g_assert (GHEX_IS_SEARCH_BAR (self));
	g_assert (HEX_IS_AUTO_HIGHLIGHT (auto_highlight));

	if (auto_highlight != self->auto_highlight)
		return;

	_ghex_search_bar_cancel_query (self);
}

static gboolean
refresh_search_query_source_func (gpointer data)
{
	GHexSearchBar *self = data;

	g_assert (GHEX_IS_SEARCH_BAR (self));

	ghex_search_bar_refresh_query (self);

	self->search_entry_refresh_timeout_id = 0;
	return G_SOURCE_REMOVE;
}

static void
search_entry_doc_changed_cb (GHexSearchBar *self)
{
	g_assert (GHEX_IS_SEARCH_BAR (self));

	g_clear_handle_id (&self->search_entry_refresh_timeout_id, g_source_remove);

	self->search_entry_refresh_timeout_id = g_timeout_add_full (G_PRIORITY_LOW, TYPEAHEAD_DELAY_TIME, refresh_search_query_source_func, g_object_ref (self), g_object_unref);
}

/* transfer none */
static void
_ghex_search_bar_set_auto_highlight (GHexSearchBar *self, HexAutoHighlight *auto_highlight)
{
	HexView *substantive_view = NULL;
	HexAutoHighlight *old_auto_highlight = NULL;

	g_return_if_fail (GHEX_IS_SEARCH_BAR (self));
	g_return_if_fail (auto_highlight == NULL || HEX_IS_AUTO_HIGHLIGHT (auto_highlight));

	if (auto_highlight == self->auto_highlight)
		return;

	ghex_search_bar_set_selected_highlight (self, 0);

	/* self->auto_highlight needs to be cleared right at the beginning, because
	 * otherwise the semi-recursive nature of this function won't work.
	 */
	if (self->auto_highlight)
		old_auto_highlight = g_steal_pointer (&self->auto_highlight);

	substantive_view = ghex_pane_get_hex (GHEX_PANE(self));
	g_return_if_fail (HEX_IS_VIEW (substantive_view));

	if (old_auto_highlight)
	{
		hex_view_remove_auto_highlight (substantive_view, old_auto_highlight);

		g_cancellable_cancel (hex_auto_highlight_get_cancellable (old_auto_highlight));
	}
	g_clear_object (&old_auto_highlight);

	if (auto_highlight)
	{
		self->auto_highlight = g_object_ref (g_steal_pointer (&auto_highlight));

		g_signal_connect_object (self->auto_highlight, "search-progress-update", G_CALLBACK(auto_highlight_search_progress_update_cb), self, G_CONNECT_SWAPPED);

		g_signal_connect_object (self->auto_highlight, "refresh-complete", G_CALLBACK(auto_highlight_refresh_complete_cb), self, G_CONNECT_SWAPPED);

		g_signal_connect_object (self->auto_highlight, "refresh-cancelled", G_CALLBACK(auto_highlight_refresh_cancelled_cb), self, G_CONNECT_SWAPPED);

		hex_view_insert_auto_highlight (substantive_view, self->auto_highlight);
	}

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_AUTO_HIGHLIGHT]);
}

/* transfer none */
HexAutoHighlight *
ghex_search_bar_get_auto_highlight (GHexSearchBar *self)
{
	g_return_val_if_fail (GHEX_IS_SEARCH_BAR (self), NULL);

	return self->auto_highlight;
}

void
ghex_search_bar_set_replace_mode (GHexSearchBar *self, gboolean replace_mode)
{
	g_return_if_fail (GHEX_IS_SEARCH_BAR (self));

	gpointer replace_widgets[] = {self->replace_all_button, self->replace_button, self->replace_entry};

	if (replace_mode == self->replace_mode)
		return;

	for (guint i = 0; i < G_N_ELEMENTS (replace_widgets); ++i)
		gtk_widget_set_visible (replace_widgets[i], replace_mode);

	self->replace_mode = replace_mode;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_REPLACE_MODE]);
}

gboolean
ghex_search_bar_get_replace_mode (GHexSearchBar *self)
{
	g_return_val_if_fail (GHEX_IS_SEARCH_BAR (self), FALSE);

	return self->replace_mode;
}

void
ghex_search_bar_set_wraparound (GHexSearchBar *self, gboolean wraparound)
{
	g_return_if_fail (GHEX_IS_SEARCH_BAR (self));

	if (self->wraparound == wraparound)
		return;

	self->wraparound = wraparound;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_WRAPAROUND]);
}

gboolean
ghex_search_bar_get_wraparound (GHexSearchBar *self)
{
	g_return_val_if_fail (GHEX_IS_SEARCH_BAR (self), TRUE);

	return self->wraparound;
}

void
ghex_search_bar_set_typeahead (GHexSearchBar *self, gboolean typeahead)
{
	HexDocument *search_entry_doc;

	g_return_if_fail (GHEX_IS_SEARCH_BAR (self));

	if (self->typeahead == typeahead)
		return;

	self->typeahead = typeahead;

	search_entry_doc = hex_view_get_document (self->search_entry);

	g_clear_signal_handler (&self->typeahead_handler, search_entry_doc);

	if (self->typeahead)
	{
		self->typeahead_handler = g_signal_connect_object (search_entry_doc, "document-changed", G_CALLBACK(search_entry_doc_changed_cb), self, G_CONNECT_SWAPPED);
	}

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_TYPEAHEAD]);
}

gboolean
ghex_search_bar_get_typeahead (GHexSearchBar *self)
{
	g_return_val_if_fail (GHEX_IS_SEARCH_BAR (self), TRUE);

	return self->typeahead;
}

void
ghex_search_bar_refresh_query (GHexSearchBar *self)
{
	HexDocument *search_entry_doc;
	HexBuffer *search_entry_buf;
	HexView *substantive_view;
	HexDocument *substantive_doc;

	g_autofree char *contents = NULL;
	g_autoptr(HexSearchInfo) search_info = NULL;
	g_autoptr(HexAutoHighlight) auto_highlight = NULL;

	g_assert (GHEX_IS_SEARCH_BAR (self));

	search_entry_doc = hex_view_get_document (self->search_entry);
	search_entry_buf = hex_document_get_buffer (search_entry_doc);

	const gint64 payload_size = hex_buffer_get_payload_size (search_entry_buf);

	if (payload_size == 0)
	{
		_ghex_search_bar_set_auto_highlight (self, NULL);
		return;
	}

	contents = hex_buffer_get_data (search_entry_buf, 0, payload_size);

	search_info = g_object_new (HEX_TYPE_SEARCH_INFO,
			"what", g_steal_pointer (&contents),
			"len", payload_size,
			"flags", self->search_flags,
			"found-msg", "Found",	// TEST
			"not-found-msg", "Not found",	// TEST
			NULL);

	substantive_view = ghex_pane_get_hex (GHEX_PANE(self));
	substantive_doc = hex_view_get_document (substantive_view);

	auto_highlight = hex_auto_highlight_new (substantive_doc, search_info);

	_ghex_search_bar_set_auto_highlight (self, auto_highlight);
}

static void
refresh_search_flags (GHexSearchBar *self)
{
	self->search_flags = 0;

	if (self->regex_enabled)
		self->search_flags |= HEX_SEARCH_REGEX;

	if (self->ignore_case)
		self->search_flags |= HEX_SEARCH_IGNORE_CASE;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_SEARCH_FLAGS]);
}

static void
_ghex_search_bar_set_regex_enabled (GHexSearchBar *self, gboolean regex_enabled)
{
	g_return_if_fail (GHEX_IS_SEARCH_BAR (self));

	self->regex_enabled = regex_enabled;

	refresh_search_flags (self);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_REGEX_ENABLED]);
}

static gboolean
_ghex_search_bar_get_regex_enabled (GHexSearchBar *self)
{
	g_return_val_if_fail (GHEX_IS_SEARCH_BAR (self), FALSE);

	return self->regex_enabled;
}

static void
_ghex_search_bar_set_ignore_case (GHexSearchBar *self, gboolean ignore_case)
{
	g_return_if_fail (GHEX_IS_SEARCH_BAR (self));

	self->ignore_case = ignore_case;

	refresh_search_flags (self);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_IGNORE_CASE]);
}

static gboolean
_ghex_search_bar_get_ignore_case (GHexSearchBar *self)
{
	g_return_val_if_fail (GHEX_IS_SEARCH_BAR (self), FALSE);

	return self->ignore_case;
}

static HexSearchFlags
_ghex_search_bar_get_search_flags (GHexSearchBar *self)
{
	g_return_val_if_fail (GHEX_IS_SEARCH_BAR (self), HEX_SEARCH_NONE);

	return self->search_flags;
}

void
ghex_search_bar_set_selected_highlight (GHexSearchBar *self, guint selected_highlight)
{
	g_autofree char *label = NULL;

	g_return_if_fail (GHEX_IS_SEARCH_BAR (self));

	if (self->auto_highlight == NULL)
	{
		if G_UNLIKELY (selected_highlight != 0)
		{
			g_warning ("%s: %s %p has no auto_highlight set, so the only valid value for selected_highlight is 0; setting selected_highlight to 0.", __func__, G_OBJECT_TYPE_NAME (self), self);
		}

		self->selected_highlight = 0;
	}
	else
	{
		HexHighlightList *hl_list = hex_auto_highlight_get_highlights (self->auto_highlight);

		if (hl_list)
		{
			guint n_highlights = g_list_model_get_n_items (G_LIST_MODEL(hl_list));

			self->selected_highlight = MIN (selected_highlight, n_highlights);

			if (self->selected_highlight == 0)
			{
				/* Translators: This is the number of total search results. */
				label = g_strdup_printf (_("%u matches"), n_highlights);
			}
			else
			{
				HexView *view;
				g_autoptr(HexHighlight) highlight = NULL;

				highlight = g_list_model_get_item (G_LIST_MODEL(hl_list), self->selected_highlight - 1);

				view = ghex_pane_get_hex (GHEX_PANE(self));
				if (view)
				{
					HexSelection *selection = hex_view_get_selection (view);

					hex_selection_collapse (selection, highlight->start_offset);
				}

				/* Translators: This is the selected search result out of the
				 * number of total search results.
				*/
				label = g_strdup_printf (_("%u of %u"), self->selected_highlight, n_highlights);
			}
		}
	}

	gtk_label_set_label (self->num_matches_label, label);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_SELECTED_HIGHLIGHT]);
}

guint
ghex_search_bar_get_selected_highlight (GHexSearchBar *self)
{
	g_return_val_if_fail (GHEX_IS_SEARCH_BAR (self), 0);

	return self->selected_highlight;
}

static void
ghex_search_bar_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GHexSearchBar *self = GHEX_SEARCH_BAR(object);

	switch (property_id)
	{
		case PROP_SELECTED_HIGHLIGHT:
			ghex_search_bar_set_selected_highlight (self, g_value_get_uint (value));
			break;

		case PROP_WRAPAROUND:
			ghex_search_bar_set_wraparound (self, g_value_get_boolean (value));
			break;

		case PROP_TYPEAHEAD:
			ghex_search_bar_set_typeahead (self, g_value_get_boolean (value));
			break;

		case PROP_REPLACE_MODE:
			ghex_search_bar_set_replace_mode (self, g_value_get_boolean (value));
			break;

		case PROP_REGEX_ENABLED:
			_ghex_search_bar_set_regex_enabled (self, g_value_get_boolean (value));
			break;

		case PROP_IGNORE_CASE:
			_ghex_search_bar_set_ignore_case (self, g_value_get_boolean (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_search_bar_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	GHexSearchBar *self = GHEX_SEARCH_BAR(object);

	switch (property_id)
	{
		case PROP_AUTO_HIGHLIGHT:
			g_value_set_object (value, ghex_search_bar_get_auto_highlight (self));
			break;

		case PROP_SELECTED_HIGHLIGHT:
			g_value_set_uint (value, ghex_search_bar_get_selected_highlight (self));
			break;

		case PROP_WRAPAROUND:
			g_value_set_boolean (value, ghex_search_bar_get_wraparound (self));
			break;

		case PROP_TYPEAHEAD:
			g_value_set_boolean (value, ghex_search_bar_get_typeahead (self));
			break;

		case PROP_REPLACE_MODE:
			g_value_set_boolean (value, ghex_search_bar_get_replace_mode (self));
			break;

		case PROP_REGEX_ENABLED:
			g_value_set_boolean (value, _ghex_search_bar_get_regex_enabled (self));
			break;

		case PROP_IGNORE_CASE:
			g_value_set_boolean (value, _ghex_search_bar_get_ignore_case (self));
			break;
			
		case PROP_SEARCH_FLAGS:
			g_value_set_flags (value, _ghex_search_bar_get_search_flags (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
next_highlight_found_cb (GObject *source_object, GAsyncResult *res, gpointer data)
{
	GHexSearchBar *self = data;
	HexView *view = (HexView *) source_object;
	HexHighlight *next_highlight;
	guint next_highlight_idx;

	g_assert (GHEX_IS_SEARCH_BAR (self));
	g_assert (HEX_IS_VIEW (view));

	next_highlight = hex_view_find_next_highlight_finish (view, res, &next_highlight_idx);

	if (next_highlight)
	{
		guint next_highlight_num = next_highlight_idx + 1;

		ghex_search_bar_set_selected_highlight (self, next_highlight_num);
	}
}

static void
next_match_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GHexSearchBar *self = GHEX_SEARCH_BAR(user_data);
	HexView *view = ghex_pane_get_hex (GHEX_PANE(self));
	HexHighlightList *hl_list;
	guint n_highlights;

	if (!self->auto_highlight || !view)
		return;

	hl_list = hex_auto_highlight_get_highlights (self->auto_highlight);
	n_highlights = g_list_model_get_n_items (G_LIST_MODEL(hl_list));

	if (n_highlights != 0)
	{

		if (self->selected_highlight)
		{
			guint next_highlight_num = 0;

			if (self->wraparound)
			{
				next_highlight_num = self->selected_highlight + 1;

				if (next_highlight_num > n_highlights)
					next_highlight_num = 1;
			}
			else
			{
				next_highlight_num = CLAMP (self->selected_highlight + 1, 1, n_highlights);
			}

			ghex_search_bar_set_selected_highlight (self, next_highlight_num);
		}
		else
		{
			hex_view_find_next_highlight_async (view, hl_list, NULL, next_highlight_found_cb, self);
		}
	}
}

static void
prev_highlight_found_cb (GObject *source_object, GAsyncResult *res, gpointer data)
{
	GHexSearchBar *self = data;
	HexView *view = (HexView *) source_object;
	HexHighlight *prev_highlight;
	guint prev_highlight_idx;

	g_assert (GHEX_IS_SEARCH_BAR (self));
	g_assert (HEX_IS_VIEW (view));

	prev_highlight = hex_view_find_prev_highlight_finish (view, res, &prev_highlight_idx);

	if (prev_highlight)
	{
		guint prev_highlight_num = prev_highlight_idx + 1;

		ghex_search_bar_set_selected_highlight (self, prev_highlight_num);
	}
}

static void
prev_match_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GHexSearchBar *self = GHEX_SEARCH_BAR(user_data);
	HexView *view = ghex_pane_get_hex (GHEX_PANE(self));
	HexHighlightList *hl_list;
	guint n_highlights;

	if (!self->auto_highlight || !view)
		return;

	hl_list = hex_auto_highlight_get_highlights (self->auto_highlight);
	n_highlights = g_list_model_get_n_items (G_LIST_MODEL(hl_list));

	if (n_highlights != 0)
	{
		if (self->selected_highlight)
		{
			guint prev_highlight_num = 0;

			if (self->wraparound)
			{
				if (self->selected_highlight == 1)
					prev_highlight_num = n_highlights;
				else
					prev_highlight_num = self->selected_highlight - 1;
			}
			else
			{
				prev_highlight_num = CLAMP (self->selected_highlight - 1, 1, n_highlights);
			}

			ghex_search_bar_set_selected_highlight (self, prev_highlight_num);
		}
		else
		{
			hex_view_find_prev_highlight_async (view, hl_list, NULL, prev_highlight_found_cb, self);
		}
	}
}

static void
clear_matches_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GHexSearchBar *self = GHEX_SEARCH_BAR(user_data);
	HexView *view = ghex_pane_get_hex (GHEX_PANE(self));

	if (!view)
		return;

	hex_view_clear_auto_highlights (view);
}

static void
ghex_search_bar_close (GHexPane *pane)
{
	GHexSearchBar *self = GHEX_SEARCH_BAR(pane);

	_ghex_search_bar_cancel_query (self);

	GHEX_PANE_CLASS(ghex_search_bar_parent_class)->close (pane);
}

static void
ghex_search_bar_init (GHexSearchBar *self)
{
	gtk_widget_init_template (GTK_WIDGET(self));

	g_signal_connect_object (self, "notify::search-flags", G_CALLBACK(ghex_search_bar_refresh_query), self, G_CONNECT_SWAPPED);

	/* Setup actions which use bindings to determine when they should be enabled/disabled.
	 * XREF: class_init, for other actions.
	 */
	{
		GActionEntry find_entries[] = {
			{"next-match", next_match_action},
			{"prev-match", prev_match_action},
			{"clear-matches", clear_matches_action},
		};
		g_autoptr(GSimpleActionGroup) find_actions = g_simple_action_group_new ();
		GAction *action;

		g_action_map_add_action_entries (G_ACTION_MAP(find_actions), find_entries, G_N_ELEMENTS (find_entries), self);

		gtk_widget_insert_action_group (GTK_WIDGET(self), "find", G_ACTION_GROUP(find_actions));

		action = g_action_map_lookup_action (G_ACTION_MAP(find_actions), "next-match");
		g_object_bind_property_full (self, "auto-highlight", action, "enabled", G_BINDING_SYNC_CREATE, util_have_object_transform_to, NULL, NULL, NULL);

		action = g_action_map_lookup_action (G_ACTION_MAP(find_actions), "prev-match");
		g_object_bind_property_full (self, "auto-highlight", action, "enabled", G_BINDING_SYNC_CREATE, util_have_object_transform_to, NULL, NULL, NULL);
	}
}

static void
ghex_search_bar_dispose (GObject *object)
{
	GHexSearchBar *self = GHEX_SEARCH_BAR(object);

	g_clear_handle_id (&self->search_entry_refresh_timeout_id, g_source_remove);

	{
		HexDocument *search_entry_doc = hex_view_get_document (self->search_entry);

		g_clear_signal_handler (&self->typeahead_handler, search_entry_doc);
	}

	g_clear_object (&self->auto_highlight);

	gtk_widget_dispose_template (GTK_WIDGET(self), GHEX_TYPE_SEARCH_BAR);

	G_OBJECT_CLASS(ghex_search_bar_parent_class)->dispose (object);
}

static void
ghex_search_bar_finalize (GObject *object)
{
	GHexSearchBar *self = GHEX_SEARCH_BAR(object);

	G_OBJECT_CLASS(ghex_search_bar_parent_class)->finalize (object);
}

static void
ghex_search_bar_class_init (GHexSearchBarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose =  ghex_search_bar_dispose;
	object_class->finalize = ghex_search_bar_finalize;
	object_class->set_property = ghex_search_bar_set_property;
	object_class->get_property = ghex_search_bar_get_property;

	GHEX_PANE_CLASS(klass)->close = ghex_search_bar_close;

	properties[PROP_AUTO_HIGHLIGHT] = g_param_spec_object ("auto-highlight", NULL, NULL,
			HEX_TYPE_AUTO_HIGHLIGHT,
			default_flags | G_PARAM_READABLE);

	properties[PROP_SELECTED_HIGHLIGHT] = g_param_spec_uint ("selected-highlight", NULL, NULL,
			0, UINT_MAX, 0,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_WRAPAROUND] = g_param_spec_boolean ("wraparound", NULL, NULL,
			TRUE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_TYPEAHEAD] = g_param_spec_boolean ("typeahead", NULL, NULL,
			TRUE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_REPLACE_MODE] = g_param_spec_boolean ("replace-mode", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_REGEX_ENABLED] = g_param_spec_boolean ("regex-enabled", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_IGNORE_CASE] = g_param_spec_boolean ("ignore-case", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_SEARCH_FLAGS] = g_param_spec_flags ("search-flags", NULL, NULL,
			HEX_TYPE_SEARCH_FLAGS,
			HEX_SEARCH_NONE,
			default_flags | G_PARAM_READABLE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	gtk_widget_class_install_property_action (widget_class, "search-options.regex", "regex-enabled");
	gtk_widget_class_install_property_action (widget_class, "search-options.ignore-case", "ignore-case");

	gtk_widget_class_install_action (widget_class, "search.submit", NULL, (GtkWidgetActionActivateFunc) ghex_search_bar_refresh_query);
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Return, 0, "search.submit", NULL);
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Enter, 0, "search.submit", NULL);

	gtk_widget_class_set_css_name (widget_class, "searchbar");

	gtk_widget_class_set_template_from_resource (widget_class, RESOURCE_BASE_PATH "/ghex-search-bar.ui");

	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, breakpoint_bin);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, replace_all_button);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, replace_button);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, replace_entry);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, search_entry);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, search_progress_revealer);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, search_progress_bar);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, search_progress_cancel_button);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, num_matches_label);

	gtk_widget_class_bind_template_callback (widget_class, _ghex_search_bar_cancel_query);
}

GtkWidget *
ghex_search_bar_new (void)
{
	return g_object_new (GHEX_TYPE_SEARCH_BAR, NULL);
}
