#include "ghex-search-bar.h"
#include "hex-auto-highlight-private.h"

#include "config.h"

enum
{
	PROP_AUTO_HIGHLIGHT = 1,
	PROP_REPLACE_MODE,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

enum signal_types {
	SIGNAL_ONE,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

struct _GHexSearchBar
{
	GHexPane parent_instance;

	HexAutoHighlight *auto_highlight;
	GCancellable *cancellable;
	gboolean replace_mode;

	/* From template: */

	gpointer breakpoint_bin;
	gpointer close_button;
	gpointer grid;
	gpointer move_previous;
	gpointer move_next;
	gpointer options_button;
	gpointer replace_all_button;
	gpointer replace_button;
	gpointer replace_entry;
	gpointer replace_mode_button;
	gpointer search_entry;
	gpointer search_progress_revealer;
	gpointer search_progress_bar;
};

G_DEFINE_FINAL_TYPE (GHexSearchBar, ghex_search_bar, GHEX_TYPE_PANE)

static void
auto_highlight_search_progress_update_cb (GHexSearchBar *self, double progress, HexAutoHighlight *auto_highlight)
{
	g_assert (GHEX_IS_SEARCH_BAR (self));
	g_assert (HEX_IS_AUTO_HIGHLIGHT (auto_highlight));

	gtk_revealer_set_reveal_child (self->search_progress_revealer, TRUE);
	gtk_progress_bar_set_fraction (self->search_progress_bar, progress);
}

static void
auto_highlight_refresh_complete_cb (GHexSearchBar *self, HexAutoHighlight *auto_highlight)
{
	g_assert (GHEX_IS_SEARCH_BAR (self));
	g_assert (HEX_IS_AUTO_HIGHLIGHT (auto_highlight));

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
search_entry_refresh_cb (GHexSearchBar *self)
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
			"found-msg", "Found",	// TEST
			"not-found-msg", "Not found",	// TEST
			NULL);

	substantive_view = ghex_pane_get_hex (GHEX_PANE(self));
	substantive_doc = hex_view_get_document (substantive_view);

	auto_highlight = hex_auto_highlight_new (substantive_doc, search_info);

	_ghex_search_bar_set_auto_highlight (self, auto_highlight);
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

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_search_bar_close (GHexPane *pane)
{
	GHexSearchBar *self = GHEX_SEARCH_BAR(pane);

	_ghex_search_bar_set_auto_highlight (self, NULL);

	GHEX_PANE_CLASS(ghex_search_bar_parent_class)->close (pane);
}

static void
ghex_search_bar_init (GHexSearchBar *self)
{
	gtk_widget_init_template (GTK_WIDGET(self));

	self->cancellable = g_cancellable_new ();

	g_signal_connect_object (self->close_button, "clicked", G_CALLBACK(ghex_pane_close), self, G_CONNECT_SWAPPED);

	{
		HexDocument *search_entry_doc = hex_view_get_document (self->search_entry);

		g_signal_connect_object (search_entry_doc, "document-changed", G_CALLBACK(search_entry_refresh_cb), self, G_CONNECT_SWAPPED);

		g_signal_connect_object (self, "map", G_CALLBACK(search_entry_refresh_cb), self, G_CONNECT_SWAPPED);
	}
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

	gtk_widget_class_set_css_name (widget_class, "searchbar");

	gtk_widget_class_set_template_from_resource (widget_class, RESOURCE_BASE_PATH "/ghex-search-bar.ui");

	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, breakpoint_bin);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, close_button);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, grid);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, move_previous);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, move_next);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, options_button);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, replace_all_button);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, replace_button);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, replace_entry);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, replace_mode_button);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, search_entry);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, search_progress_revealer);
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, search_progress_bar);
}

GtkWidget *
ghex_search_bar_new (void)
{
	return g_object_new (GHEX_TYPE_SEARCH_BAR, NULL);
}
