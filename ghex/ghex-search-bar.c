#include "ghex-search-bar.h"
#include "hex-auto-highlight-private.h"
#include "libgtkhex-enums.h"

#include "config.h"

enum
{
	PROP_AUTO_HIGHLIGHT = 1,
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
	GCancellable *cancellable;
	gboolean replace_mode;

	gboolean regex_enabled;
	gboolean ignore_case;
	HexSearchFlags search_flags;

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
};

G_DEFINE_FINAL_TYPE (GHexSearchBar, ghex_search_bar, GHEX_TYPE_PANE)

static void
auto_highlight_search_progress_update_cb (GHexSearchBar *self, double progress, HexAutoHighlight *auto_highlight)
{
	g_assert (GHEX_IS_SEARCH_BAR (self));
	g_assert (HEX_IS_AUTO_HIGHLIGHT (auto_highlight));

	gtk_widget_set_sensitive (GTK_WIDGET(self->search_progress_cancel_button), TRUE);
	gtk_revealer_set_reveal_child (self->search_progress_revealer, TRUE);
	gtk_progress_bar_set_fraction (self->search_progress_bar, progress);
}

static void
auto_highlight_refresh_complete_cb (GHexSearchBar *self, HexAutoHighlight *auto_highlight)
{
	g_assert (GHEX_IS_SEARCH_BAR (self));
	g_assert (HEX_IS_AUTO_HIGHLIGHT (auto_highlight));

	gtk_widget_set_sensitive (GTK_WIDGET(self->search_progress_cancel_button), FALSE);
	gtk_revealer_set_reveal_child (self->search_progress_revealer, FALSE);
	gtk_progress_bar_set_fraction (self->search_progress_bar, 0.0);
}

/* transfer none */
static void
_ghex_search_bar_set_auto_highlight (GHexSearchBar *self, HexAutoHighlight *auto_highlight)
{
	HexView *substantive_view;

	g_return_if_fail (GHEX_IS_SEARCH_BAR (self));
	g_return_if_fail (auto_highlight == NULL || HEX_IS_AUTO_HIGHLIGHT (auto_highlight));

	substantive_view = ghex_pane_get_hex (GHEX_PANE(self));
	g_return_if_fail (substantive_view != NULL);

	if (self->auto_highlight)
		hex_view_remove_auto_highlight (substantive_view, self->auto_highlight);

	if (g_set_object (&self->auto_highlight, auto_highlight))
	{
		if (auto_highlight)
		{
			g_signal_connect_object (auto_highlight, "search-progress-update", G_CALLBACK(auto_highlight_search_progress_update_cb), self, G_CONNECT_SWAPPED);

			g_signal_connect_object (auto_highlight, "refresh-complete", G_CALLBACK(auto_highlight_refresh_complete_cb), self, G_CONNECT_SWAPPED);

			hex_view_insert_auto_highlight (substantive_view, self->auto_highlight);
		}

		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_AUTO_HIGHLIGHT]);
	}
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

static void
_ghex_search_bar_refresh_query (GHexSearchBar *self)
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

static void
ghex_search_bar_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GHexSearchBar *self = GHEX_SEARCH_BAR(object);

	switch (property_id)
	{
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
_ghex_search_bar_cancel_query (GHexSearchBar *self)
{
	g_assert (GHEX_IS_SEARCH_BAR (self));

	_ghex_search_bar_set_auto_highlight (self, NULL);
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
	HexDocument *search_entry_doc = NULL;

	gtk_widget_init_template (GTK_WIDGET(self));

	self->cancellable = g_cancellable_new ();

	search_entry_doc = hex_view_get_document (self->search_entry);

	g_signal_connect_object (search_entry_doc, "document-changed", G_CALLBACK(_ghex_search_bar_refresh_query), self, G_CONNECT_SWAPPED);
	g_signal_connect_object (self, "map", G_CALLBACK(_ghex_search_bar_refresh_query), self, G_CONNECT_SWAPPED);
	g_signal_connect_object (self, "notify::search-flags", G_CALLBACK(_ghex_search_bar_refresh_query), self, G_CONNECT_SWAPPED);
}

static void
ghex_search_bar_dispose (GObject *object)
{
	GHexSearchBar *self = GHEX_SEARCH_BAR(object);

	gtk_widget_dispose_template (GTK_WIDGET(self), GHEX_TYPE_SEARCH_BAR);

	g_cancellable_cancel (self->cancellable);
	g_clear_object (&self->cancellable);

	g_clear_object (&self->auto_highlight);

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

	gtk_widget_class_bind_template_callback (widget_class, _ghex_search_bar_cancel_query);
}

GtkWidget *
ghex_search_bar_new (void)
{
	return g_object_new (GHEX_TYPE_SEARCH_BAR, NULL);
}
