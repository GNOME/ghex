// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-text-editable"

#include "gtkhex-paste-data.h"
#include "hex-text-editable.h"
#include "hex-text-common.h"
#include "util.h"

#define SCROLL_TIMEOUT 100

enum
{
	PROP_0,
	PROP_CAN_UNDO,
	PROP_CAN_REDO,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

enum signal_types {
	SIGNAL_ONE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct
{
	gint64 selection_anchor;
	guint scroll_timeout;
	GtkScrollType auto_scroll;
	GtkWidget *context_menu;
	GtkWidget *geometry_popover;
	gboolean can_undo;
	gboolean can_redo;
} HexTextEditablePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (HexTextEditable, hex_text_editable, HEX_TYPE_TEXT)

static gboolean
delete_selection_in_doc (HexTextEditable *self)
{
	g_assert (HEX_IS_TEXT_EDITABLE (self));

	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	HexHighlight *highlight = hex_selection_get_highlight (selection);
	const gint64 start_offset = hex_selection_get_start_offset (selection);
	const gsize len = hex_highlight_get_n_selected (highlight);

	g_assert (start_offset >= 0);
	g_assert (len != 0);

	hex_document_delete_data (document, start_offset, len, TRUE);
	hex_selection_collapse (selection, start_offset);

	return TRUE;
}

// FIXME - I don't think I care for this approach.
#define ZEROS_BUFSIZ 512
static const char zeros[ZEROS_BUFSIZ] = {0};

static gboolean
zero_selection_in_doc (HexTextEditable *self)
{
	g_assert (HEX_IS_TEXT_EDITABLE (self));

	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	HexHighlight *highlight = hex_selection_get_highlight (selection);
	const gint64 start_offset = hex_selection_get_start_offset (selection);
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	const gsize len = hex_highlight_get_n_selected (highlight);
	gsize written = 0;
	gint64 i = -1;
	g_autoptr(GListModel) change_list = NULL;

	g_assert (len != 0);

	i = hex_selection_get_start_offset (selection);
	g_assert (i >= 0);

	change_list = hex_document_set_data_multi_start (document);

	while (written < len)
	{
		size_t batch_size = len-written < ZEROS_BUFSIZ ? len-written : ZEROS_BUFSIZ;

		hex_document_set_data_multi_add (document, change_list, i, batch_size, batch_size, zeros);

		i += batch_size;
		written += batch_size;
	}

	hex_document_set_data_multi_end (document, g_steal_pointer (&change_list), TRUE);

	hex_selection_collapse (selection, cursor_pos);

	return TRUE;
}

static void
paste_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexTextEditable *self = user_data;
	hex_text_editable_paste_from_clipboard (self);
}

static void
copy_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexTextEditable *self = user_data;
	hex_text_editable_copy_to_clipboard (self);
}

static void
cut_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexTextEditable *self = user_data;
	hex_text_editable_cut_to_clipboard (self);
}

static void
undo_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexTextEditable *self = user_data;
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	GListModel *change_list;
	g_autoptr(HexChangeData) first_change = NULL;

	g_assert (HEX_IS_DOCUMENT (document));
	g_assert (hex_document_get_can_undo (document));

	change_list = hex_document_get_undo_data (document);
	if (g_list_model_get_n_items (change_list) > 0)
		first_change = g_list_model_get_item (change_list, 0);

	hex_document_undo (document);

	hex_selection_collapse (selection, hex_change_data_get_start_offset (first_change));
}

static void
redo_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	HexTextEditable *self = user_data;
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	GListModel *change_list;
	guint n_items;
	g_autoptr(HexChangeData) last_change = NULL;

	g_assert (HEX_IS_DOCUMENT (document));
	g_assert (hex_document_get_can_redo (document));

	hex_document_redo (document);

	change_list = hex_document_get_undo_data (document);
	if ((n_items = g_list_model_get_n_items (change_list)) > 0)
		last_change = g_list_model_get_item (change_list, n_items - 1);

	hex_selection_collapse (selection, hex_change_data_get_start_offset (last_change));
}

static void
update_geometry_cb (HexTextEditable *self)
{
	HexTextEditablePrivate *priv = hex_text_editable_get_instance_private (self);
	GtkSpinButton *cpl_spinbtn = g_object_get_data (G_OBJECT(priv->geometry_popover), "cpl_spinbtn");
	GtkCheckButton *auto_geometry_checkbtn = g_object_get_data (G_OBJECT(priv->geometry_popover), "auto_geometry_checkbtn");

	if (! hex_view_get_auto_geometry (HEX_VIEW(self)))
		hex_view_set_cpl (HEX_VIEW(self), gtk_spin_button_get_value (cpl_spinbtn));
}

static void
geometry_popover_action (HexTextEditable *self)
{
	HexTextEditablePrivate *priv = hex_text_editable_get_instance_private (self);
	GdkRectangle context_menu_pos = {0};

	gtk_popover_get_pointing_to (GTK_POPOVER(priv->context_menu), &context_menu_pos);
	gtk_popover_set_pointing_to (GTK_POPOVER(priv->geometry_popover), &context_menu_pos);
	gtk_popover_popup (GTK_POPOVER(priv->geometry_popover));
}

static void
delete_action (HexTextEditable *self)
{
	HexTextEditablePrivate *priv = hex_text_editable_get_instance_private (self);

	if (hex_view_get_insert_mode (HEX_VIEW(self)))
		delete_selection_in_doc (self);
	else
		zero_selection_in_doc (self);
}

static void
backspace_action (HexTextEditable *self)
{
	HexTextEditablePrivate *priv = hex_text_editable_get_instance_private (self);
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	HexHighlight *highlight = hex_selection_get_highlight (selection);
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	const gsize n_selected = hex_highlight_get_n_selected (highlight);

	/* If we have only one character selected, what we want to do
	 * is delete the prior character. Otherwise, this should
	 * essentially work the same way as delete if we have a
	 * highlighted selection.
	 */

	if (n_selected == 1)
	{
		if (cursor_pos < 1)
			return;

		hex_selection_collapse (selection, MAX (cursor_pos - 1, 0));
	}

	delete_action (self);

}

static void
move_cursor_action (GtkWidget *widget, const char *action_name, GVariant *parameter)
{
	HexTextEditable *self = HEX_TEXT_EDITABLE(widget);
	GtkMovementStep step;
	int count;
	gboolean extend_selection;

	g_variant_get (parameter, "(iib)", &step, &count, &extend_selection);

	hex_text_editable_move_cursor (self, step, count, extend_selection);
}

static void
hex_text_editable_real_move_cursor (HexTextEditable *self, GtkMovementStep step, int count, gboolean extend_selection)
{
	HexDocument *doc = hex_view_get_document (HEX_VIEW(self));
	HexBuffer *buf = hex_document_get_buffer (doc);
	const int cpl = hex_view_get_cpl (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	const gboolean insert_mode = hex_view_get_insert_mode (HEX_VIEW(self));
	const gint64 file_end_pos = hex_text_common_get_file_end_cursor_pos (self);
	gint64 new_cursor_pos = 0;

	switch (step)
	{
		case GTK_MOVEMENT_DISPLAY_LINES:
			new_cursor_pos = cursor_pos + count * cpl;
			break;

		case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
			new_cursor_pos = hex_text_common_get_cursor_display_line_ends (HEX_TEXT(self), count);
			break;

		case GTK_MOVEMENT_BUFFER_ENDS:
			if (count < 0)
				new_cursor_pos = 0;
			else
			{
				new_cursor_pos = file_end_pos;
			}
			break;

		case GTK_MOVEMENT_PAGES:
		{
			int num_lines = hex_view_get_n_vis_lines (HEX_VIEW(self));

			new_cursor_pos = cursor_pos + num_lines * cpl * count;
		}
			break;

		default:
			return;
	}

	hex_text_common_finish_move_cursor (self, new_cursor_pos, extend_selection);
}

void
hex_text_editable_move_cursor (HexTextEditable *self, GtkMovementStep step, int count, gboolean extend_selection)
{
	HexTextEditableClass *klass = HEX_TEXT_EDITABLE_GET_CLASS (self);

	g_return_if_fail (klass->move_cursor != NULL);

	klass->move_cursor (self, step, count, extend_selection);
}

void
hex_text_editable_render_highlights_for_line (HexTextEditable *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	HexTextEditableClass *klass = HEX_TEXT_EDITABLE_GET_CLASS (self);

	g_return_if_fail (klass->render_highlights_for_line != NULL);

	klass->render_highlights_for_line (self, snapshot, line_num, layout);
}

void
hex_text_editable_render_cursor (HexTextEditable *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	HexTextEditableClass *klass = HEX_TEXT_EDITABLE_GET_CLASS (self);

	g_return_if_fail (klass->render_cursor != NULL);

	klass->render_cursor (self, snapshot, line_num, layout);
}

static gboolean
scroll_timeout_handler (HexTextEditable *self)
{
	HexTextEditablePrivate *priv = hex_text_editable_get_instance_private (self);

	switch (priv->auto_scroll)
	{
		case GTK_SCROLL_STEP_UP:
			hex_text_editable_move_cursor (self, GTK_MOVEMENT_DISPLAY_LINES, -1, TRUE);
			break;

		case GTK_SCROLL_STEP_DOWN:
			hex_text_editable_move_cursor (self, GTK_MOVEMENT_DISPLAY_LINES, 1, TRUE);
			break;

		default:
			g_clear_handle_id (&priv->scroll_timeout, g_source_remove);
			return G_SOURCE_REMOVE;
	}

	return G_SOURCE_CONTINUE;
}

static void
hex_text_editable_pressed (HexText *ht, GdkModifierType state, PangoLayout *layout, int click_line, int rel_x, int rel_y)
{
	HexTextEditable *self = HEX_TEXT_EDITABLE(ht);

	gboolean grab_focus = gtk_widget_grab_focus (GTK_WIDGET(self));

	/* Don't chain up - parent implementation is pure virtual */
}

static void
hex_text_editable_released (HexText *ht, PangoLayout *layout, int click_line, int rel_x, int rel_y)
{
	HexTextEditable *self = HEX_TEXT_EDITABLE(ht);
	HexTextEditablePrivate *priv = hex_text_editable_get_instance_private (self);

	priv->auto_scroll = GTK_SCROLL_NONE;
	g_clear_handle_id (&priv->scroll_timeout, g_source_remove);

	/* Don't chain up - parent implementation is pure virtual */
}

static void
hex_text_editable_dragged (HexText *ht, PangoLayout *layout, int drag_line, int rel_x, int rel_y, GtkScrollType scroll)
{
	HexTextEditable *self = HEX_TEXT_EDITABLE(ht);
	HexTextEditablePrivate *priv = hex_text_editable_get_instance_private (self);

	if (priv->auto_scroll == GTK_SCROLL_NONE || priv->auto_scroll != scroll)
	{
		g_clear_handle_id (&priv->scroll_timeout, g_source_remove);

		switch (scroll)
		{
			case GTK_SCROLL_STEP_UP:
			case GTK_SCROLL_STEP_DOWN:
				priv->auto_scroll = scroll;

				priv->scroll_timeout = g_timeout_add (SCROLL_TIMEOUT, G_SOURCE_FUNC(scroll_timeout_handler), self);
				break;

			default:
				priv->auto_scroll = GTK_SCROLL_NONE;
				break;
		}
	}
}

static void
hex_text_editable_right_click (HexText *ht, PangoLayout *layout, int click_line, int rel_x, int rel_y, int abs_x, int abs_y)
{
	HexTextEditable *self = HEX_TEXT_EDITABLE(ht);
	HexTextEditablePrivate *priv = hex_text_editable_get_instance_private (self);

	gboolean grab_focus = gtk_widget_grab_focus (GTK_WIDGET(self));

	gtk_popover_set_pointing_to (GTK_POPOVER(priv->context_menu), &(GdkRectangle){abs_x, abs_y, 1, 1});
	gtk_popover_popup (GTK_POPOVER(priv->context_menu));

	/* Don't chain up - parent implementation is pure virtual */
}

static void
hex_text_editable_render_line (HexText *ht, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	HexTextEditable *self = HEX_TEXT_EDITABLE(ht);

	hex_text_editable_render_highlights_for_line (self, snapshot, line_num, layout);

	HEX_TEXT_CLASS(hex_text_editable_parent_class)->render_line (ht, snapshot, line_num, layout);

	hex_text_editable_render_cursor (self, snapshot, line_num, layout);
}

static void
hex_text_editable_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexTextEditable *self = HEX_TEXT_EDITABLE(object);

	switch (property_id)
	{
		case PROP_CAN_UNDO:
			hex_text_editable_set_can_undo (self, g_value_get_boolean (value));
			break;

		case PROP_CAN_REDO:
			hex_text_editable_set_can_redo (self, g_value_get_boolean (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_text_editable_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexTextEditable *self = HEX_TEXT_EDITABLE(object);

	switch (property_id)
	{
		case PROP_CAN_UNDO:
			g_value_set_boolean (value, hex_text_editable_get_can_undo (self));
			break;

		case PROP_CAN_REDO:
			g_value_set_boolean (value, hex_text_editable_get_can_redo (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_text_editable_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
	HexTextEditable *self = HEX_TEXT_EDITABLE(widget);
	HexTextEditablePrivate *priv = hex_text_editable_get_instance_private (self);

	/* According to TFM, you have to do this if you're not using a layout manager.
	 */
	gtk_popover_present (GTK_POPOVER(priv->context_menu));
	gtk_popover_present (GTK_POPOVER(priv->geometry_popover));

	GTK_WIDGET_CLASS(hex_text_editable_parent_class)->size_allocate (widget, width, height, baseline);
}

static void
document_set_cb (HexTextEditable *self, GParamSpec *pspec, gpointer user_data)
{
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));

	g_object_bind_property (document, "can-undo", self, "can-undo", G_BINDING_SYNC_CREATE);
	g_object_bind_property (document, "can-redo", self, "can-redo", G_BINDING_SYNC_CREATE);
}

static void
hex_text_editable_init (HexTextEditable *self)
{
	HexTextEditablePrivate *priv = hex_text_editable_get_instance_private (self);

	g_signal_connect (self, "notify::document", G_CALLBACK(document_set_cb), NULL);

	/* Context Menu and Geometry Popover */
	{
		g_autoptr(GtkBuilder) builder = gtk_builder_new_from_resource ("/org/gnome/libgtkhex/ui/context-menu.ui");
		GMenuModel *menu = G_MENU_MODEL(gtk_builder_get_object (builder, "context-menu"));

		priv->context_menu = gtk_popover_menu_new_from_model (menu);
		gtk_widget_set_parent (priv->context_menu, GTK_WIDGET(self));

		priv->geometry_popover = GTK_WIDGET(gtk_builder_get_object (builder, "geometry_popover"));
		gtk_widget_set_parent (priv->geometry_popover, GTK_WIDGET(self));

		{
			GtkWidget *auto_geometry_checkbtn = GTK_WIDGET(gtk_builder_get_object (builder, "auto_geometry_checkbtn"));
			GtkWidget *cpl_spinbtn = GTK_WIDGET(gtk_builder_get_object (builder, "cpl_spinbtn"));

			g_object_bind_property (self, "auto-geometry", auto_geometry_checkbtn, "active", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
			g_object_bind_property (self, "auto-geometry", cpl_spinbtn, "sensitive", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
			g_object_bind_property (self, "cpl", cpl_spinbtn, "value", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
		}
	}

	/* Setup actions and use bindings to determine when they should be enabled/disabled.*/
	{
		GActionEntry document_entries[] = {
			{"undo", undo_action},
			{"redo", redo_action},
		};
		g_autoptr(GSimpleActionGroup) document_actions = g_simple_action_group_new ();
		GAction *action;

		g_action_map_add_action_entries (G_ACTION_MAP(document_actions), document_entries, G_N_ELEMENTS (document_entries), self);

		gtk_widget_insert_action_group (GTK_WIDGET(self), "document", G_ACTION_GROUP(document_actions));

		action = g_action_map_lookup_action (G_ACTION_MAP(document_actions), "undo");
		g_object_bind_property (self, "can-undo", action, "enabled", G_BINDING_SYNC_CREATE);

		action = g_action_map_lookup_action (G_ACTION_MAP(document_actions), "redo");
		g_object_bind_property (self, "can-redo", action, "enabled", G_BINDING_SYNC_CREATE);
	}
	{
		GActionEntry clipboard_entries[] = {
			{"copy", copy_action},
			{"cut", cut_action},
			{"paste", paste_action},
		};
		g_autoptr(GSimpleActionGroup) clipboard_actions = g_simple_action_group_new ();
		GAction *action;

		g_action_map_add_action_entries (G_ACTION_MAP(clipboard_actions), clipboard_entries, G_N_ELEMENTS (clipboard_entries), self);

		gtk_widget_insert_action_group (GTK_WIDGET(self), "clipboard", G_ACTION_GROUP(clipboard_actions));

		action = g_action_map_lookup_action (G_ACTION_MAP(clipboard_actions), "copy");
		g_object_bind_property_full (self, "document", action, "enabled", G_BINDING_SYNC_CREATE, util_have_object_transform_to, NULL, NULL, NULL);

		action = g_action_map_lookup_action (G_ACTION_MAP(clipboard_actions), "cut");
		g_object_bind_property_full (self, "document", action, "enabled", G_BINDING_SYNC_CREATE, util_have_object_transform_to, NULL, NULL, NULL);

		action = g_action_map_lookup_action (G_ACTION_MAP(clipboard_actions), "paste");
		g_object_bind_property_full (self, "document", action, "enabled", G_BINDING_SYNC_CREATE, util_have_object_transform_to, NULL, NULL, NULL);
	}
}

static void
hex_text_editable_dispose (GObject *object)
{
	HexTextEditable *self = HEX_TEXT_EDITABLE(object);
	HexTextEditablePrivate *priv = hex_text_editable_get_instance_private (self);

	g_clear_handle_id (&priv->scroll_timeout, g_source_remove);
	g_clear_pointer (&priv->context_menu, gtk_widget_unparent);
	g_clear_pointer (&priv->geometry_popover, gtk_widget_unparent);

	/* Chain up */
	G_OBJECT_CLASS(hex_text_editable_parent_class)->dispose (object);
}

static void
hex_text_editable_finalize (GObject *object)
{
	HexTextEditable *self = HEX_TEXT_EDITABLE(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_text_editable_parent_class)->finalize (object);
}

static void
hex_text_editable_class_init (HexTextEditableClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose =  hex_text_editable_dispose;
	object_class->finalize = hex_text_editable_finalize;
	object_class->set_property = hex_text_editable_set_property;
	object_class->get_property = hex_text_editable_get_property;

	widget_class->size_allocate = hex_text_editable_size_allocate;

	HEX_TEXT_CLASS(klass)->dragged = hex_text_editable_dragged;
	HEX_TEXT_CLASS(klass)->pressed = hex_text_editable_pressed;
	HEX_TEXT_CLASS(klass)->released = hex_text_editable_released;
	HEX_TEXT_CLASS(klass)->right_click = hex_text_editable_right_click;
	HEX_TEXT_CLASS(klass)->render_line = hex_text_editable_render_line;

	klass->move_cursor = hex_text_editable_real_move_cursor;

	properties[PROP_CAN_UNDO] = g_param_spec_boolean ("can-undo", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE);

	properties[PROP_CAN_REDO] = g_param_spec_boolean ("can-redo", NULL, NULL,
			FALSE,
			default_flags | G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	/* Actions */

	// FIXME - action signals instead??

	gtk_widget_class_install_action (widget_class, "editable.move-cursor", "(iib)", move_cursor_action);

	gtk_widget_class_install_action (widget_class, "editable.backspace", NULL, (GtkWidgetActionActivateFunc)backspace_action);

	gtk_widget_class_install_action (widget_class, "editable.delete", NULL, (GtkWidgetActionActivateFunc)delete_action);

	gtk_widget_class_install_action (widget_class, "layout.geometry", NULL, (GtkWidgetActionActivateFunc)geometry_popover_action);

	/* Keybindings */

	hex_text_common_add_move_binding (widget_class, GDK_KEY_Up, 0,
			GTK_MOVEMENT_DISPLAY_LINES, -1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_KP_Up, 0,
			GTK_MOVEMENT_DISPLAY_LINES, -1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_Down, 0,
			GTK_MOVEMENT_DISPLAY_LINES, 1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_KP_Down, 0,
			GTK_MOVEMENT_DISPLAY_LINES, 1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_Home, 0,
			GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_KP_Home, 0,
			GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_End, 0,
			GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_KP_End, 0,
			GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_Home, GDK_CONTROL_MASK,
			GTK_MOVEMENT_BUFFER_ENDS, -1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_KP_Home, GDK_CONTROL_MASK,
			GTK_MOVEMENT_BUFFER_ENDS, -1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_End, GDK_CONTROL_MASK,
			GTK_MOVEMENT_BUFFER_ENDS, 1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_KP_End, GDK_CONTROL_MASK,
			GTK_MOVEMENT_BUFFER_ENDS, 1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_Page_Up, 0,
			GTK_MOVEMENT_PAGES, -1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_KP_Page_Up, 0,
			GTK_MOVEMENT_PAGES, -1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_Page_Down, 0,
			GTK_MOVEMENT_PAGES, 1);

	hex_text_common_add_move_binding (widget_class, GDK_KEY_KP_Page_Down, 0,
			GTK_MOVEMENT_PAGES, 1);

	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_BackSpace, 0,
			"editable.backspace", NULL);

	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Delete, 0,
			"editable.delete", NULL);

	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Delete, 0,
			"editable.delete", NULL);

	/* Ctrl+c - copy */
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_c, GDK_CONTROL_MASK,
			"clipboard.copy", NULL);

	/* Ctrl+x - cut */
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_x, GDK_CONTROL_MASK,
			"clipboard.cut", NULL);

	/* Ctrl+v - paste */
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_v, GDK_CONTROL_MASK,
			"clipboard.paste", NULL);

	/* Ctrl+z - undo */
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_z, GDK_CONTROL_MASK,
			"document.undo", NULL);

	/* Ctrl+y - redo */
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_y, GDK_CONTROL_MASK,
			"document.redo", NULL);
}

/* Clipboard operations */

static size_t
clamp_rep_len (HexTextEditable *self, size_t start_pos, size_t rep_len)
{
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	HexBuffer *buf = hex_document_get_buffer (document);
	size_t payload_size = hex_buffer_get_payload_size (buf);

	if (rep_len > payload_size - start_pos)
		rep_len = payload_size - start_pos;

	return rep_len;
}

static void
paste_set_data (HexTextEditable *self, char *data, gsize data_len)
{
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 start_offset = hex_selection_get_start_offset (selection);
	const gint64 end_offset = hex_selection_get_end_offset (selection);
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	gsize start_pos = hex_selection_get_cursor_pos (selection);
	gboolean insert_mode = hex_view_get_insert_mode (HEX_VIEW(self));
	gsize len = data_len;
	gsize rep_len = 0;

	if (start_offset != end_offset)
	{
		gsize selection_len, end_pos;

		start_pos = MIN (start_offset, end_offset);
		end_pos = MAX (start_offset, end_offset);
		selection_len = end_pos - start_pos + 1;

		if (insert_mode)
			rep_len = selection_len;
		else
			len = rep_len = MIN (selection_len, len);
	}

	if (!insert_mode)
		len = rep_len = clamp_rep_len (self, start_pos, len);

	hex_document_set_data (document,
			start_pos,
			len,
			/* rep_len is either:
			 * 0 (insert w/o replacing),
			 * len (delete and paste the same number of bytes),
			 * or a different number (delete and paste an arbitrary number
			 * of bytes)
			 */
			rep_len,
			data,
			TRUE);

	hex_selection_set_cursor_pos (selection, start_pos + len);
}

static void
plaintext_paste_received_cb (GObject *source_object,
		GAsyncResult *result,
		gpointer user_data)
{
	g_autoptr(HexTextEditable) self = user_data;	/* async func took a ref */
	GdkClipboard *clipboard = (GdkClipboard *)source_object;
	g_autofree char *text = NULL;
	g_autoptr(GError) error = NULL;

	g_assert (HEX_IS_TEXT_EDITABLE (self));
	g_assert (GDK_IS_CLIPBOARD (clipboard));

	g_debug ("%s: We DON'T have HexPasteData. Falling back to plaintext paste",
			__func__);

	text = gdk_clipboard_read_text_finish (clipboard, result, &error);

	if (text)
		paste_set_data (self, text, strlen(text));
	else
		g_critical ("Error pasting text: %s", error->message);
}

static void
paste_helper (HexTextEditable *self, GdkClipboard *clipboard)
{
	GdkContentProvider *content;
	GValue value = G_VALUE_INIT;
	HexPasteData *paste;
	gboolean have_hex_paste_data = FALSE;

	content = gdk_clipboard_get_content (clipboard);
	g_value_init (&value, HEX_TYPE_PASTE_DATA);

	/* If the clipboard contains our special HexPasteData, we'll use it.
	 * If not, just fall back to plaintext.
	 */
	have_hex_paste_data = content ?
		gdk_content_provider_get_value (content, &value, NULL) : FALSE;

	if (have_hex_paste_data)
	{
		char *doc_data;
		size_t start_pos, len, rep_len;

		g_debug("%s: We HAVE HexPasteData.", __func__);

		paste = HEX_PASTE_DATA(g_value_get_object (&value));
		doc_data = hex_paste_data_get_doc_data (paste);
		len = hex_paste_data_get_elems (paste);

		paste_set_data (self, doc_data, len);
	}
	else
	{
		gdk_clipboard_read_text_async (clipboard,
				NULL,	/* cancellable */
				plaintext_paste_received_cb,
				g_object_ref (self));
	}
}

void
hex_text_editable_paste_from_clipboard (HexTextEditable *self)
{
	GdkClipboard *clipboard;

	g_return_if_fail (HEX_IS_TEXT_EDITABLE (self));

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET(self));
	g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

	paste_helper (self, clipboard);
}

void
hex_text_editable_copy_to_clipboard (HexTextEditable *self)
{
	HexDocument *document;
	HexSelection *selection;
	HexHighlight *highlight;
	GdkClipboard *clipboard;
	HexPasteData *paste;
	GdkContentProvider *provider_union;
	GdkContentProvider *provider_array[2];
	gint64 start_pos;
	gsize len;
	char *doc_data;
	char *string;

	g_return_if_fail (HEX_IS_TEXT_EDITABLE (self));

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET(self));

	selection = hex_view_get_selection (HEX_VIEW(self));
	highlight = hex_selection_get_highlight (selection);

	start_pos = MIN (hex_selection_get_start_offset (selection), hex_selection_get_end_offset (selection));
	len = hex_highlight_get_n_selected (highlight);

	g_return_if_fail (len);

	/* Grab the raw data from the HexDocument. */
	document = hex_view_get_document (HEX_VIEW(self));
	doc_data = hex_buffer_get_data (hex_document_get_buffer (document),
			start_pos, len);

	/* Setup a union of HexPasteData and a plain C string */
	paste = hex_paste_data_new (doc_data, len);
	g_return_if_fail (HEX_IS_PASTE_DATA(paste));
	string = hex_paste_data_get_string (paste);

	provider_array[0] =
		gdk_content_provider_new_typed (HEX_TYPE_PASTE_DATA, paste);
	provider_array[1] =
		gdk_content_provider_new_typed (G_TYPE_STRING, string);

	provider_union = gdk_content_provider_new_union (provider_array, 2);

	/* Finally, set our content to our newly created union. */
	gdk_clipboard_set_content (clipboard, provider_union);
}

void
hex_text_editable_cut_to_clipboard (HexTextEditable *self)
{
	g_return_if_fail (HEX_IS_TEXT_EDITABLE (self));

	hex_text_editable_copy_to_clipboard (self);

	if (hex_view_get_insert_mode (HEX_VIEW(self)))
		delete_selection_in_doc (self);
	else
		zero_selection_in_doc (self);
}

void
hex_text_editable_set_can_undo (HexTextEditable *self, gboolean can_undo)
{
	HexTextEditablePrivate *priv;

	g_return_if_fail (HEX_IS_TEXT_EDITABLE (self));

	priv = hex_text_editable_get_instance_private (self);

	priv->can_undo = can_undo;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_CAN_UNDO]);
}

gboolean
hex_text_editable_get_can_undo (HexTextEditable *self)
{
	HexTextEditablePrivate *priv;

	g_return_val_if_fail (HEX_IS_TEXT_EDITABLE (self), FALSE);

	priv = hex_text_editable_get_instance_private (self);

	return priv->can_undo;
}

void
hex_text_editable_set_can_redo (HexTextEditable *self, gboolean can_redo)
{
	HexTextEditablePrivate *priv;

	g_return_if_fail (HEX_IS_TEXT_EDITABLE (self));

	priv = hex_text_editable_get_instance_private (self);

	priv->can_redo = can_redo;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_CAN_REDO]);
}

gboolean
hex_text_editable_get_can_redo (HexTextEditable *self)
{
	HexTextEditablePrivate *priv;

	g_return_val_if_fail (HEX_IS_TEXT_EDITABLE (self), FALSE);

	priv = hex_text_editable_get_instance_private (self);

	return priv->can_redo;
}
