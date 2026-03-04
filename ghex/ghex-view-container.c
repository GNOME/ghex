#include "ghex-view-container.h"

#include "ghex-conversion-pane.h"
#include "ghex-mark-pane.h"
#include "ghex-info-bar.h"
#include "ghex-statusbar.h"
#include "configuration.h"
#include "gtkhex-layout-manager.h"
#include "common-ui.h"

#include "config.h"

enum
{
	PROP_0,
	PROP_DOCUMENT,
	PROP_HEX,
	PROP_LOADING,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _GHexViewContainer
{
	GtkWidget parent_instance;

	/* From template: */

	/* Direct child: */
	AdwViewStack *stack;

	HexWidget *hex;
	GHexMarkPane *mark_pane;
	GHexConversionPane *conversion_pane;
	GtkToggleButton *conversions_toggle_button;
	GHexInfoBar *info_bar;
	GHexStatusbar *statusbar;
	GtkRevealer *mark_pane_revealer;
	GtkScrolledWindow *scrolled_window;
	GtkRevealer *conversions_revealer;
};

G_DEFINE_TYPE (GHexViewContainer, ghex_view_container, GTK_TYPE_WIDGET)

HexWidget *
ghex_view_container_get_hex (GHexViewContainer *self)
{
	g_return_val_if_fail (GHEX_IS_VIEW_CONTAINER (self), NULL);

	return self->hex;
}

static void
doc_changed_info_bar_cb (HexDocument *doc, HexChangeData *change_data, gboolean undoable, GHexInfoBar *info_bar)
{
	g_assert (HEX_IS_DOCUMENT (doc));
	g_assert (GHEX_IS_INFO_BAR (info_bar));

	if (hex_document_get_changed (doc) && hex_change_data_get_external_file_change (change_data))
	{
		ghex_info_bar_set_shown (info_bar, TRUE);
	}
	else
	{
		ghex_info_bar_set_shown (info_bar, FALSE);
	}
}

static void
doc_file_loaded_info_bar_cb (GHexInfoBar *info_bar)
{
	g_assert (GHEX_IS_INFO_BAR (info_bar));

	ghex_info_bar_set_shown (info_bar, FALSE);
}

static void
file_read_started_cb (GHexViewContainer *self)
{
	g_assert (GHEX_IS_VIEW_CONTAINER (self));

	ghex_view_container_set_loading (self, TRUE);
}

static void
file_loaded_cb (GHexViewContainer *self)
{
	g_assert (GHEX_IS_VIEW_CONTAINER (self));

	ghex_view_container_set_loading (self, FALSE);
}

static void
doc_read_cb (GObject *source_object, GAsyncResult *res, gpointer data)
{
	GHexViewContainer *self = data;
	HexDocument *doc = (HexDocument *) source_object;
	g_autoptr(GError) error = NULL;
	gboolean retval = FALSE;

	g_assert (GHEX_IS_VIEW_CONTAINER (self));
	g_assert (HEX_IS_DOCUMENT (doc));

	retval = hex_document_read_finish (doc, res, &error);

	if (retval)
	{
		g_debug ("%s: document %p read successfully", __func__, doc);
	}
	else
	{
		GtkWindow *parent = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET(self)));

		g_debug ("%s: document read failed", __func__);

		// FIXME/TODO - error should never be null here.
		ghex_display_dialog (parent, error ? error->message : "Unknown error");

		g_signal_emit_by_name (self, "destroy");
	}
}

void
ghex_view_container_set_document (GHexViewContainer *self, HexDocument *doc)
{
	g_return_if_fail (GHEX_IS_VIEW_CONTAINER (self));

	hex_view_set_document (HEX_VIEW(self->hex), doc);

	if (hex_document_get_file (doc) != NULL)
	{
		// FIXME - this shouldn't be necessary - need to improve our plumbing here.
		ghex_view_container_set_loading (self, TRUE);

		hex_document_read_async (doc, NULL, doc_read_cb, self);
	}

	g_signal_connect_object (doc, "file-read-started", G_CALLBACK(file_read_started_cb), self, G_CONNECT_SWAPPED);
	g_signal_connect_object (doc, "file-loaded", G_CALLBACK(file_loaded_cb), self, G_CONNECT_SWAPPED);

	g_signal_connect_object (doc, "document-changed", G_CALLBACK(doc_changed_info_bar_cb), self->info_bar, G_CONNECT_DEFAULT);
	g_signal_connect_object (doc, "file-loaded", G_CALLBACK(doc_file_loaded_info_bar_cb), self->info_bar, G_CONNECT_SWAPPED);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_DOCUMENT]);
}

HexDocument *
ghex_view_container_get_document (GHexViewContainer *self)
{
	g_return_val_if_fail (GHEX_IS_VIEW_CONTAINER (self), NULL);

	return hex_view_get_document (HEX_VIEW(self->hex));
}

gboolean
ghex_view_container_get_loading (GHexViewContainer *self)
{
	if (g_strcmp0 (adw_view_stack_get_visible_child_name (self->stack), "loading-page") == 0)
	{
		return TRUE;
	}

	return FALSE;
}

void
ghex_view_container_set_loading (GHexViewContainer *self, gboolean loading)
{
	if (ghex_view_container_get_loading (self) != loading)
	{
		if (loading)
		{
			adw_view_stack_set_visible_child_name (self->stack, "loading-page");
		}
		else
		{
			adw_view_stack_set_visible_child_name (self->stack, "container-page");
		}

		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_LOADING]);
	}
}

static void
ghex_view_container_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GHexViewContainer *self = GHEX_VIEW_CONTAINER(object);

	switch (property_id)
	{
		case PROP_DOCUMENT:
			ghex_view_container_set_document (self, g_value_get_object (value));
			break;

		case PROP_LOADING:
			ghex_view_container_set_loading (self, g_value_get_boolean (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_view_container_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	GHexViewContainer *self = GHEX_VIEW_CONTAINER(object);

	switch (property_id)
	{
		case PROP_HEX:
			g_value_set_object (value, ghex_view_container_get_hex (self));
			break;

		case PROP_DOCUMENT:
			g_value_set_object (value, ghex_view_container_get_document (self));
			break;

		case PROP_LOADING:
			g_value_set_boolean (value, ghex_view_container_get_loading (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
activate_mark_action (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexViewContainer *self = (GHexViewContainer *) widget;

	g_assert (GHEX_IS_VIEW_CONTAINER (self));

	if (! gtk_revealer_get_child_revealed (self->mark_pane_revealer))
		return;

	const int mark_num = g_variant_get_int32 (parameter);

	ghex_mark_pane_set_mark_num (self->mark_pane, mark_num);
}

static void
sel_cursor_pos_notify_conversion_pane_cb (GHexViewContainer *self, GParamSpec *pspec, HexSelection *selection)
{
	GHexConversionVal64 val;
	HexDocument *doc;
	HexBuffer *buf;

	g_assert (GHEX_IS_VIEW_CONTAINER (self));
	g_assert (HEX_IS_SELECTION (selection));
	
	doc = hex_view_get_document (HEX_VIEW(self->hex));
	buf = hex_document_get_buffer (doc);

	// FIXME - make this less manual
	{
		const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);

		for (int i = 0; i < 8; i++)
		{
			/* returns 0 on buffer overflow, which is what we want */
			val.v[i] = hex_buffer_get_byte (buf, cursor_pos + i);
		}
	}

	ghex_conversion_pane_update (self->conversion_pane, &val);
}

static void
scrolled_window_notify_hex (GHexViewContainer *self)
{
	GtkWidget *child = gtk_scrolled_window_get_child (self->scrolled_window);

	if (child && HEX_IS_VIEW (child))
	{
		g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_HEX]);
	}
}

static void
mark_pane_close_cb (GtkRevealer *mark_pane_revealer)
{
	gtk_revealer_set_reveal_child (mark_pane_revealer, FALSE);
}

static void
bind_settings (GHexViewContainer *self)
{
	GSettings *settings = ghex_get_global_settings ();

	g_settings_bind (settings, "font", self->hex, "font", G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (settings, "show-offsets", self->hex, "show-offsets", G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (settings, "display-control-characters", hex_widget_get_ascii_display (self->hex), "display-control-characters", G_SETTINGS_BIND_DEFAULT);

	{
		HexWidgetLayout *layout_manager = HEX_WIDGET_LAYOUT(gtk_widget_get_layout_manager (GTK_WIDGET(self->hex)));

		g_settings_bind (settings, "group-data-by", layout_manager, "group-type", G_SETTINGS_BIND_DEFAULT);
	}
}

static void
setup_actions (GHexViewContainer *self)
{
	g_autoptr(GSimpleActionGroup) actions = g_simple_action_group_new ();

	gtk_widget_insert_action_group (GTK_WIDGET(self), "container", G_ACTION_GROUP(actions));

	/* container.mark-pane */
	{
		g_autoptr(GPropertyAction) action = g_property_action_new ("mark-pane", self->mark_pane_revealer, "reveal-child");

		g_action_map_add_action (G_ACTION_MAP(actions), G_ACTION(action));
	}
}

static void
ghex_view_container_init (GHexViewContainer *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

	g_object_bind_property (self->conversions_toggle_button, "active", self->conversions_revealer, "reveal-child", G_BINDING_DEFAULT);

	g_signal_connect_object (self->scrolled_window, "notify::child", G_CALLBACK(scrolled_window_notify_hex), self, G_CONNECT_SWAPPED);

	bind_settings (self);
	setup_actions (self);
}

static void
ghex_view_container_dispose (GObject *object)
{
	GHexViewContainer *self = GHEX_VIEW_CONTAINER(object);

	gtk_widget_dispose_template (GTK_WIDGET(self), GHEX_TYPE_VIEW_CONTAINER);

	/* Chain up */
	G_OBJECT_CLASS(ghex_view_container_parent_class)->dispose (object);
}

static void
ghex_view_container_finalize (GObject *object)
{
	GHexViewContainer *self = GHEX_VIEW_CONTAINER(object);

	/* Chain up */
	G_OBJECT_CLASS(ghex_view_container_parent_class)->finalize (object);
}

static void
ghex_view_container_constructed (GObject *object)
{
	GHexViewContainer *self = GHEX_VIEW_CONTAINER(object);
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self->hex));

	g_signal_connect_object (selection, "notify::cursor-pos", G_CALLBACK(sel_cursor_pos_notify_conversion_pane_cb), self, G_CONNECT_SWAPPED);
}

static void
ghex_view_container_class_init (GHexViewContainerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose =  ghex_view_container_dispose;
	object_class->finalize = ghex_view_container_finalize;
	object_class->constructed = ghex_view_container_constructed;
	object_class->set_property = ghex_view_container_set_property;
	object_class->get_property = ghex_view_container_get_property;

	properties[PROP_HEX] = g_param_spec_object ("hex", NULL, NULL,
			HEX_TYPE_VIEW,
			default_flags | G_PARAM_READABLE);

	properties[PROP_DOCUMENT] = g_param_spec_object ("document", NULL, NULL,
			HEX_TYPE_DOCUMENT,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_LOADING] = g_param_spec_boolean ("loading", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	/* Actions (where bindings not required) */

	gtk_widget_class_install_action (widget_class, "container.activate-mark", "i", activate_mark_action);

	/* < auto-generated activate-mark bindings > */

	/* r! ghex/generate-mark-bindings.sh
	 */

	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_0,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			0);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_0,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			0);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_1,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			1);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_1,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			1);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_2,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			2);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_2,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			2);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_3,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			3);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_3,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			3);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_4,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			4);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_4,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			4);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_5,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			5);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_5,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			5);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_6,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			6);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_6,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			6);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_7,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			7);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_7,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			7);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_8,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			8);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_8,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			8);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_9,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			9);
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_9,
			GDK_CONTROL_MASK,
			"container.activate-mark",
			"i",
			9);

	/* </ auto-generated activate-mark bindings > */

	/* Template */

	gtk_widget_class_set_template_from_resource (widget_class, RESOURCE_BASE_PATH "/ghex-view-container.ui");

	gtk_widget_class_bind_template_child (widget_class, GHexViewContainer, stack);
	gtk_widget_class_bind_template_child (widget_class, GHexViewContainer, hex);
	gtk_widget_class_bind_template_child (widget_class, GHexViewContainer, conversion_pane);
	gtk_widget_class_bind_template_child (widget_class, GHexViewContainer, conversions_toggle_button);
	gtk_widget_class_bind_template_child (widget_class, GHexViewContainer, info_bar);
	gtk_widget_class_bind_template_child (widget_class, GHexViewContainer, scrolled_window);
	gtk_widget_class_bind_template_child (widget_class, GHexViewContainer, conversions_revealer);
	gtk_widget_class_bind_template_child (widget_class, GHexViewContainer, statusbar);
	gtk_widget_class_bind_template_child (widget_class, GHexViewContainer, mark_pane_revealer);
	gtk_widget_class_bind_template_child (widget_class, GHexViewContainer, mark_pane);

	gtk_widget_class_bind_template_callback (widget_class, mark_pane_close_cb);
}

GtkWidget *
ghex_view_container_new (void)
{
	return g_object_new (GHEX_TYPE_VIEW_CONTAINER, NULL);
}
