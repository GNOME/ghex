// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-text-editable"

#include "hex-text-editable.h"
#include "hex-text-common.h"
#include "util.h"

#define SCROLL_TIMEOUT 100

enum
{
	PROP_0,
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
} HexTextEditablePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (HexTextEditable, hex_text_editable, HEX_TYPE_TEXT)

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
		util_delete_selection_in_doc (self);
	else
		util_zero_selection_in_doc (self);
}

static void
backspace_action (HexTextEditable *self)
{
	HexTextEditablePrivate *priv = hex_text_editable_get_instance_private (self);
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	HexHighlight *highlight = hex_selection_get_highlight (selection);
	gint64 cursor_pos = hex_selection_get_cursor_pos (selection);

	/* If we have only one character selected, what we want to do
	 * is delete the prior character. Otherwise, this should
	 * essentially work the same way as delete if we have a
	 * highlighted selection.
	 */

	if (hex_highlight_get_n_selected (highlight) == 1)
	{
		if (cursor_pos <= 0)
			return;

		--cursor_pos;
		hex_selection_collapse (selection, cursor_pos);
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
hex_text_editable_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexTextEditable *self = HEX_TEXT_EDITABLE(object);

	switch (property_id)
	{
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
hex_text_editable_init (HexTextEditable *self)
{
	HexTextEditablePrivate *priv = hex_text_editable_get_instance_private (self);

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

	klass->move_cursor = hex_text_editable_real_move_cursor;

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
}
