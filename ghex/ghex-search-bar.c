#include "ghex-search-bar.h"
#include "hex-auto-highlight-private.h"

#include "config.h"

enum
{
	PROP_AUTO_HIGHLIGHT = 1,
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
	HexSearchInfo *search_info;
};

G_DEFINE_FINAL_TYPE (GHexSearchBar, ghex_search_bar, GHEX_TYPE_PANE)

static void
hex_changed_cb (GHexPane *pane, GParamSpec *pspec G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
	GHexSearchBar *self = (GHexSearchBar *) pane;
	HexView *hex = NULL;

	g_assert (GHEX_IS_SEARCH_BAR (self));

	hex = ghex_pane_get_hex (pane);
	if (!hex) return;

	hex_view_insert_auto_highlight (hex, self->auto_highlight);
}

static void
search_entry_changed_cb (GHexSearchBar *self, HexChangeData *change_data, gboolean undoable, HexDocument *doc)
{
	HexBuffer *buf;

	g_assert (GHEX_IS_SEARCH_BAR (self));
	g_assert (HEX_IS_WIDGET (self->search_entry));
	g_assert (HEX_IS_DOCUMENT (doc));

	buf = hex_document_get_buffer (doc);

	g_assert (HEX_IS_BUFFER (buf));

	{
		const gint64 payload_size = hex_buffer_get_payload_size (buf);
		g_autofree char *contents = hex_buffer_get_data (buf, 0, payload_size);

		g_object_set (self->search_info,
				"what", g_steal_pointer (&contents),
				"len", payload_size,
				"found-msg", "Found",	// TEST
				"not-found-msg", "Not found",	// TEST
				NULL);
	}
}

static void
TEST_search_bar_refresh_ready_cb (GObject *source_object, GAsyncResult *res, gpointer data)
{
	HexAutoHighlight *auto_highlight = (HexAutoHighlight *) source_object;
	gboolean retval;

	g_assert (HEX_IS_AUTO_HIGHLIGHT (auto_highlight));

	retval = hex_auto_highlight_refresh_finish (auto_highlight, res);

	g_debug ("%s: refresh complete - status: %d", __func__, retval);
}

static void
search_info_changed_cb (GHexSearchBar *self, GParamSpec *pspec, HexSearchInfo *search_info)
{
	g_assert (GHEX_IS_SEARCH_BAR (self));
	g_assert (HEX_IS_SEARCH_INFO (search_info));

	g_cancellable_cancel (self->cancellable);
	g_set_object (&self->cancellable, g_cancellable_new ());

	hex_auto_highlight_refresh_async (self->auto_highlight, self->cancellable, TEST_search_bar_refresh_ready_cb, NULL);
}

/* transfer none */
static void
_ghex_search_bar_set_auto_highlight (GHexSearchBar *self, HexAutoHighlight *auto_highlight)
{
	g_return_if_fail (GHEX_IS_SEARCH_BAR (self));
	g_return_if_fail (HEX_IS_AUTO_HIGHLIGHT (auto_highlight));

	if (g_set_object (&self->auto_highlight, auto_highlight))
		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_AUTO_HIGHLIGHT]);
}

/* transfer none */
static HexAutoHighlight *
_ghex_search_bar_get_auto_highlight (GHexSearchBar *self)
{
	g_return_val_if_fail (GHEX_IS_SEARCH_BAR (self), NULL);

	return self->auto_highlight;
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
		case PROP_AUTO_HIGHLIGHT:
			_ghex_search_bar_set_auto_highlight (self, g_value_get_object (value));
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
			g_value_set_object (value, _ghex_search_bar_get_auto_highlight (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_search_bar_init (GHexSearchBar *self)
{
	gtk_widget_init_template (GTK_WIDGET(self));

	self->cancellable = g_cancellable_new ();

	g_signal_connect_object (self->close_button, "clicked", G_CALLBACK(ghex_pane_close), self, G_CONNECT_SWAPPED);

	{
		HexDocument *search_entry_doc = hex_view_get_document (self->search_entry);

		g_signal_connect_object (search_entry_doc, "document-changed", G_CALLBACK(search_entry_changed_cb), self, G_CONNECT_SWAPPED);
	}

	g_signal_connect_object (self->search_info, "notify::what", G_CALLBACK(search_info_changed_cb), self, G_CONNECT_SWAPPED);

	g_signal_connect (self, "notify::hex", G_CALLBACK(hex_changed_cb), NULL);
}

static void
ghex_search_bar_constructed (GObject *object)
{
	GHexSearchBar *self = GHEX_SEARCH_BAR(object);

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

	object_class->constructed = ghex_search_bar_constructed;
	object_class->dispose =  ghex_search_bar_dispose;
	object_class->finalize = ghex_search_bar_finalize;
	object_class->set_property = ghex_search_bar_set_property;
	object_class->get_property = ghex_search_bar_get_property;

	properties[PROP_AUTO_HIGHLIGHT] = g_param_spec_object ("auto-highlight", NULL, NULL,
			HEX_TYPE_AUTO_HIGHLIGHT,
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
	gtk_widget_class_bind_template_child (widget_class, GHexSearchBar, search_info);
}

GtkWidget *
ghex_search_bar_new (void)
{
	return g_object_new (GHEX_TYPE_SEARCH_BAR, NULL);
}
