// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-text"

#include "hex-text.h"

#include "hex-highlight-private.h"
#include "hex-private-common.h"

#define OFFSCREEN_LINE_MARGIN	1
#define TOP_LINE_MARGIN			6
#define FALLBACK_BLINK_RATE		1200	/* in ms - this happens to be the GTK default */

typedef struct
{
	/* Could use an array, but using a basic hashtable gives us poor-man's
	 * negative-indexing.
	 */
	GHashTable *layouts;	/* key: int; val: PangoLayout */
	HexDocument *document;
	HexTextRenderData *render_data_;	/* use getter */
	gboolean cursor_visible;
	guint cursor_blink_id;
	guint scroll_to_cursor_id;
} HexTextPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (HexText, hex_text, HEX_TYPE_VIEW)

/* --- */

static void
released_cb (HexText *self, int n_press, double x, double y, GtkGestureClick *UNUSED)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);
	HexTextRenderData *render_data = hex_text_get_render_data (self);
	HexTextClass *klass = HEX_TEXT_GET_CLASS (self);

	if (! klass->released)
	{
		g_debug ("%s: No 'released' vfunc set for HexText subclass %s",
				__func__, G_OBJECT_TYPE_NAME (self));
		return;
	}

	if (render_data->top_disp_line == 0)
		y -= TOP_LINE_MARGIN;

	{
		GtkAdjustment *vadj = hex_view_get_vadjustment (HEX_VIEW(self));
		int num_lines = hex_view_get_n_vis_lines (HEX_VIEW(self));
		int click_line = (int)y / render_data->line_height;
		int rel_y = (int)y % render_data->line_height;
		PangoLayout *layout = g_hash_table_lookup (priv->layouts, GINT_TO_POINTER (click_line));

		klass->released (self, layout, click_line, x, rel_y);
	}
}

static void
single_left_click_cb (HexText *self, int n_press, double x, double y, GtkGestureClick *gesture)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);
	HexTextRenderData *render_data = hex_text_get_render_data (self);
	HexTextClass *klass = HEX_TEXT_GET_CLASS (self);
	GdkEventSequence *sequence;
	GdkEvent *event;
	GdkModifierType state;

	if (! klass->pressed)
	{
		g_debug ("%s: No 'pressed' vfunc set for HexText subclass %s",
				__func__, G_OBJECT_TYPE_NAME (self));
		return;
	}

	sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE(gesture));
	event = gtk_gesture_get_last_event (GTK_GESTURE(gesture), sequence);
	state = gdk_event_get_modifier_state (event);

	if (render_data->top_disp_line == 0)
		y -= TOP_LINE_MARGIN;

	{
		GtkAdjustment *vadj = hex_view_get_vadjustment (HEX_VIEW(self));
		int num_lines = hex_view_get_n_vis_lines (HEX_VIEW(self));
		int click_line = (int)(y - render_data->fine_translate_value) / render_data->line_height;
		int rel_y = (int)(y - render_data->fine_translate_value) % render_data->line_height;
		PangoLayout *layout = g_hash_table_lookup (priv->layouts, GINT_TO_POINTER (click_line));

		klass->pressed (self, state, layout, click_line, x, rel_y);
	}
}

static void
drag_cb (HexText *self, double offset_x, double offset_y, GtkGestureDrag *gesture)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);
	HexTextRenderData *render_data = hex_text_get_render_data (self);
	HexTextClass *klass = HEX_TEXT_GET_CLASS (self);
	double x = 0, y = 0;

	if (! klass->dragged)
	{
		g_debug ("%s: No 'dragged' vfunc set for HexText subclass %s",
				__func__, G_OBJECT_TYPE_NAME (self));
		return;
	}

	gtk_gesture_drag_get_start_point (gesture, &x, &y);
	x += offset_x;
	y += offset_y;

	if (render_data->top_disp_line == 0)
		y -= TOP_LINE_MARGIN;

	{
		GtkScrollType scroll = GTK_SCROLL_NONE;

		if (y < 0)
			scroll = GTK_SCROLL_STEP_UP;
		else if (y > gtk_widget_get_height (GTK_WIDGET(self)))
			scroll = GTK_SCROLL_STEP_DOWN;

		{
			GtkAdjustment *vadj = hex_view_get_vadjustment (HEX_VIEW(self));
			int num_lines = hex_view_get_n_vis_lines (HEX_VIEW(self));
			int drag_line = (int)(y - render_data->fine_translate_value) / render_data->line_height;
			int rel_y = (int)(y - render_data->fine_translate_value) % render_data->line_height;
			PangoLayout *layout = g_hash_table_lookup (priv->layouts, GINT_TO_POINTER (drag_line));

			klass->dragged (self, layout, drag_line, x, rel_y, scroll);
		}
	}
}

static void
right_click_cb (HexText *self, int n_press, double x, double y, GtkGestureClick *gesture)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);
	HexTextRenderData *render_data = hex_text_get_render_data (self);
	HexTextClass *klass = HEX_TEXT_GET_CLASS (self);

	if (! klass->right_click)
	{
		g_debug ("%s: No 'right_click' vfunc set for HexText subclass %s",
				__func__, G_OBJECT_TYPE_NAME (self));
		return;
	}

	if (render_data->top_disp_line == 0)
		y -= TOP_LINE_MARGIN;

	{
		GtkAdjustment *vadj = hex_view_get_vadjustment (HEX_VIEW(self));
		int num_lines = hex_view_get_n_vis_lines (HEX_VIEW(self));
		int click_line = (int)y / render_data->line_height;
		int rel_y = (int)y % render_data->line_height;
		PangoLayout *layout = g_hash_table_lookup (priv->layouts, GINT_TO_POINTER (click_line));

		klass->right_click (self, layout, click_line, x, rel_y, x, y);
	}
}

static void
update_render_data (HexText *self)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);
	int line_width, line_height;
	PangoLayout *layout = g_hash_table_lookup (priv->layouts, GINT_TO_POINTER (0));

	if (! layout)
		return;

	pango_layout_get_pixel_size (layout, &line_width, &line_height);

	priv->render_data_->line_height = line_height;
}

/* --- */

static void
adj_value_changed_cb (GtkAdjustment *adj, HexText *self)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);
	double value = gtk_adjustment_get_value (adj);

	/* Convert adjustment value into line space */
	double line_pos = value / HEX_ADJ_PIXEL_MULTIPLIER;

	/* Stable integer + fractional split */
	int top_line = line_pos;
	double frac  = line_pos - top_line;

	/* Kill boundary jitter from floating point noise */
	const double EPS = 1e-6;

	if (frac < EPS)
		frac = 0.0;
	else if (frac > 1.0 - EPS)
	{
		top_line++;
		frac = 0.0;
	}

	// FIXME - move

	priv->render_data_->top_disp_line = top_line;
	priv->render_data_->fine_translate_value = -(frac * priv->render_data_->line_height);

	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
scroll_to_cursor__timeout_func (HexText *self)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);
	HexTextRenderData *render_data = hex_text_get_render_data (self);
	GtkAdjustment *vadj = hex_view_get_vadjustment (HEX_VIEW(self));
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));
	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);
	int cpl = hex_view_get_cpl (HEX_VIEW(self));
	int cursor_line;
	int DEBUG_line_num_where_cursor_visible;
	int adj_distance, adj_modifier, new_adj_value;
	const int min_scroll_step = HEX_ADJ_PIXEL_MULTIPLIER / 2;

	if (cpl == 0)
	{
		//g_debug ("%s: cpl is 0 - removing scroll_to_cursor_id", __func__);
		priv->scroll_to_cursor_id = 0;
		return G_SOURCE_REMOVE;
	}

	cursor_line = cursor_pos / cpl;

	if (hex_text_offset_is_visible (self, cursor_pos, &DEBUG_line_num_where_cursor_visible))
	{
		/* Snap adjustment so cursor is not obstructed. */
		gtk_adjustment_set_value (vadj, render_data->top_disp_line * HEX_ADJ_PIXEL_MULTIPLIER);

		//g_debug ("removing scroll_to_cursor_id - cursor already visible on line num: %d", DEBUG_line_num_where_cursor_visible);
		priv->scroll_to_cursor_id = 0;

		return G_SOURCE_REMOVE;
	}

#if 0
	if (hex_text_offset_is_visible (self, cursor_pos, &DEBUG_line_num_where_cursor_visible))
	{
		/* Snap adjustment right to the top if we're on the top line. Otherwise if
		 * going up we'll stop scrolling even if the cursor is partially visible
		 * which looks weird.
		 */
		if (cursor_line == 0)
			gtk_adjustment_set_value (vadj, 0);

		g_debug ("removing scroll_to_cursor_id - cursor already visible on line num: %d", DEBUG_line_num_where_cursor_visible);
		priv->scroll_to_cursor_id = 0;
		return G_SOURCE_REMOVE;
	}
#endif

	adj_distance = ABS (cursor_line - render_data->top_disp_line) * HEX_ADJ_PIXEL_MULTIPLIER;

	//g_debug ("adj_distance: %d", adj_distance);
	//g_debug ("page size: %d", (int) gtk_adjustment_get_page_size (vadj));

	if (adj_distance <= gtk_adjustment_get_page_size (vadj))
	{
		adj_modifier = min_scroll_step;

		//g_debug ("adj_distance <= a page size - choosing %d", min_scroll_step);
	}
	else
	{
		adj_modifier = MAX (adj_distance / 2, min_scroll_step);
#if 0
		g_debug ("adj_modifier poss1: %d", adj_distance / 2);
		g_debug ("adj_modifier poss2: %d", min_scroll_step);
		g_debug ("adj_modifier CHOOSING: %d", adj_modifier);
#endif
	}

	if (cursor_line > render_data->top_disp_line)
		new_adj_value = gtk_adjustment_get_value (vadj) + adj_modifier;
	else
		new_adj_value = gtk_adjustment_get_value (vadj) - adj_modifier;

	new_adj_value = CLAMP (new_adj_value, gtk_adjustment_get_lower (vadj), gtk_adjustment_get_upper (vadj));

	gtk_adjustment_set_value (vadj, new_adj_value);

	return G_SOURCE_CONTINUE;
}

#if 0
static gboolean
scroll_to_cursor__timeout_func (HexText *self)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);
	GtkAdjustment *vadj = hex_view_get_vadjustment (HEX_VIEW(self));
	gint64 cursor_pos = hex_view_get_cursor_pos (HEX_VIEW(self));
	int cpl = hex_view_get_cpl (HEX_VIEW(self));
	int cursor_line;
	int DEBUG_line_num_where_cursor_visible;
	int adj_modifier;

	if (cpl == 0)
	{
		g_debug ("%s: cpl is 0 - removing scroll_to_cursor_id", __func__);
		priv->scroll_to_cursor_id = 0;
		return G_SOURCE_REMOVE;
	}

	cursor_line = cursor_pos / cpl;

	if (hex_text_offset_is_visible (self, cursor_pos, &DEBUG_line_num_where_cursor_visible))
	{
		g_debug ("removing scroll_to_cursor_id - cursor already visible on line num: %d", DEBUG_line_num_where_cursor_visible);
		priv->scroll_to_cursor_id = 0;
		return G_SOURCE_REMOVE;
	}

	adj_modifier = MAX ((int) (ABS (cursor_line - priv->render_data->top_disp_line) / 2) * HEX_ADJ_PIXEL_MULTIPLIER, (int) HEX_ADJ_PIXEL_MULTIPLIER / 2);

	g_debug ("adj_modifier poss1: %d", (int) ((ABS (cursor_line - priv->render_data->top_disp_line) / 2) * HEX_ADJ_PIXEL_MULTIPLIER));
	g_debug ("adj_modifier poss2: %d", (int) (HEX_ADJ_PIXEL_MULTIPLIER / 2));
	g_debug ("adj_modifier: CHOOSING: %d", (int) adj_modifier);

	if (cursor_line > priv->render_data->top_disp_line)
		gtk_adjustment_set_value (vadj, gtk_adjustment_get_value (vadj) + adj_modifier);
	else
		gtk_adjustment_set_value (vadj, gtk_adjustment_get_value (vadj) - adj_modifier);

	return G_SOURCE_CONTINUE;
}
#endif

static void
scroll_to_cursor (HexText *self)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);

	if (priv->scroll_to_cursor_id)
		return;

	priv->scroll_to_cursor_id = gtk_widget_add_tick_callback (GTK_WIDGET(self), (GtkTickCallback) scroll_to_cursor__timeout_func, NULL, NULL);
}

#if 0
static void
scroll_to_cursor (HexText *self)
{
	int cpl = hex_view_get_cpl (HEX_VIEW(self));
	gint64 cursor_pos = hex_view_get_cursor_pos (HEX_VIEW(self));
	GtkAdjustment *vadj = hex_view_get_vadjustment (HEX_VIEW(self));
	int old_adj_value, new_adj_value;

	if (cpl == 0)
		return;

	old_adj_value = gtk_adjustment_get_value (vadj);

	new_adj_value = (cursor_pos / cpl) * HEX_ADJ_PIXEL_MULTIPLIER;

	gtk_adjustment_set_value (vadj, new_adj_value);
}
#endif

static gboolean
blink_cb (HexText *self)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);

	priv->cursor_visible = !priv->cursor_visible;
	gtk_widget_queue_draw (GTK_WIDGET(self));

	return G_SOURCE_CONTINUE;
}

static void
reset_cursor_blink_timer (HexText *self)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);
	gboolean blink_cursor = hex_view_get_blink_cursor (HEX_VIEW(self));
	static GtkSettings *gtk_settings;
	int blink_rate = FALLBACK_BLINK_RATE;

	g_object_get (gtk_widget_get_settings (GTK_WIDGET(self)),
			"gtk-cursor-blink-time", &blink_rate,
			NULL);

	/* :gtk-cursor-blink-time is the time for the whole blink cycle (on and
	 * off), so div by 2
	 */
	blink_rate /= 2;

	priv->cursor_visible = TRUE;

	g_clear_handle_id (&priv->cursor_blink_id, g_source_remove);

	if (blink_cursor)
		priv->cursor_blink_id = g_timeout_add (blink_rate, G_SOURCE_FUNC(blink_cb), self);

	gtk_widget_queue_draw (GTK_WIDGET(self));
}

static void
cursor_pos_cb (HexText *self, GParamSpec *pspec, HexSelection *selection)
{
	g_assert (HEX_IS_TEXT (self));
	g_assert (HEX_IS_SELECTION (selection));

	const gint64 cursor_pos = hex_selection_get_cursor_pos (selection);

	reset_cursor_blink_timer (self);

	if (! hex_text_offset_is_visible (self, cursor_pos, NULL))
		scroll_to_cursor (self);
}

static void
selection_notify_cb (HexView *view, GParamSpec *pspec, gpointer user_data)
{
	HexText *self = HEX_TEXT(view);
	HexSelection *selection = hex_view_get_selection (HEX_VIEW(self));

	if (selection)
	{
		g_signal_connect_object (selection, "notify::cursor-pos", G_CALLBACK(cursor_pos_cb), self, G_CONNECT_SWAPPED);
	}
}

/* Not a formal property as getting formal notify:: signals is not necessary */

void
hex_text_set_cursor_visible (HexText *self, gboolean visible)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);

	priv->cursor_visible = visible;
}

gboolean
hex_text_get_cursor_visible (HexText *self)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);

	return priv->cursor_visible;
}

static void
hex_text_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
	HexText *self = HEX_TEXT(widget);

	update_render_data (self);
}

static void
hex_text_render_line (HexText *self, GtkSnapshot *snapshot, int line_num, PangoLayout *layout)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);
	GdkRGBA font_color;

	g_assert (PANGO_IS_LAYOUT (layout));

	gtk_widget_get_color (GTK_WIDGET(self), &font_color);

	gtk_snapshot_append_layout (snapshot, layout, &font_color);
}

static void
hex_text_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	HexText *self = HEX_TEXT(widget);
	HexTextPrivate *priv = hex_text_get_instance_private (self);
	HexTextRenderData *render_data = hex_text_get_render_data (self);
	HexTextClass *klass = HEX_TEXT_GET_CLASS (self);
	HexDocument *document = hex_view_get_document (HEX_VIEW(self));
	HexBuffer *buf = hex_document_get_buffer (document);
	gint64 payload = hex_buffer_get_payload_size (buf);
	int cpl = hex_view_get_cpl (HEX_VIEW(self));

	if (! klass->format_line)
	{
		g_debug ("%s: The HexText subclass %s has no format_line method defined",
				__func__, G_OBJECT_TYPE_NAME (self));
		return;
	}

	gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, gtk_widget_get_width (widget), gtk_widget_get_height (widget)));

	for (int i = 0 - OFFSCREEN_LINE_MARGIN; i < hex_view_get_n_vis_lines (HEX_VIEW(self)) + OFFSCREEN_LINE_MARGIN; ++i)
	{
		graphene_point_t point = {
			.x = 0,
			.y = i * render_data->line_height + render_data->fine_translate_value,
		};
		gint64 line_offset;
		int line_len;
		g_autofree char *formatted_line = NULL;
		g_autofree guchar *line_data = NULL;
		PangoLayout *layout = NULL;

		line_offset = (render_data->top_disp_line + i) * cpl;

		if (line_offset < 0)
			continue;
		if (line_offset > payload)
			break;

		line_len = CLAMP (payload - line_offset, 0, cpl);

		if (line_len == 0)
		{
			/* 0 line_len is fine if it's the first line - ie, an empty file. */
			if (line_offset == 0)
				;
			else
				continue;
		}

		line_data = hex_buffer_get_data (buf, line_offset, line_len);

		if (! line_data)
		{
			if (line_len == 0 && line_offset == 0)
				;
			else
				continue;
		}

		formatted_line = klass->format_line (self, i, line_data, line_len);

		if ((layout = g_hash_table_lookup (priv->layouts, GINT_TO_POINTER (i))) == NULL)
		{
			layout = gtk_widget_create_pango_layout (GTK_WIDGET(self), NULL);
			g_hash_table_insert (priv->layouts, GINT_TO_POINTER (i), layout);
		}

		g_assert (PANGO_IS_LAYOUT (layout));

		pango_layout_set_markup (layout, formatted_line, -1);

		gtk_snapshot_save (snapshot);
		gtk_snapshot_translate (snapshot, &point);

		if (render_data->top_disp_line == 0)
		{
			graphene_point_t top_margin = {
				.x = 0,
				.y = TOP_LINE_MARGIN
			};

			gtk_snapshot_translate (snapshot, &top_margin);
		}

		klass->render_line (self, snapshot, i, layout);
		gtk_snapshot_restore (snapshot);
	}

	gtk_snapshot_pop (snapshot);
}

static void
adj_setup_signals_cb (HexView *view, GParamSpec *pspec, gpointer user_data)
{
	HexText *self = HEX_TEXT(view);
	GtkAdjustment *vadj = hex_view_get_vadjustment (view);

	if (!vadj)
		return;

	g_signal_connect_object (vadj, "value-changed", G_CALLBACK (adj_value_changed_cb), self, G_CONNECT_DEFAULT);
}

static void
hex_text_dispose (GObject *object)
{
	HexText *self = HEX_TEXT(object);
	HexTextPrivate *priv = hex_text_get_instance_private (self);

	g_clear_pointer (&priv->layouts, g_hash_table_unref);

	/* Chain up */
	G_OBJECT_CLASS(hex_text_parent_class)->dispose (object);
}

static void
hex_text_finalize (GObject *object)
{
	HexText *self = HEX_TEXT(object);
	HexTextPrivate *priv = hex_text_get_instance_private (self);

	g_free (priv->render_data_);
	g_clear_handle_id (&priv->cursor_blink_id, g_source_remove);

	/* Chain up */
	G_OBJECT_CLASS(hex_text_parent_class)->finalize (object);
}

static void
hex_text_class_init (HexTextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->dispose = hex_text_dispose;
	object_class->finalize = hex_text_finalize;

	widget_class->size_allocate = hex_text_size_allocate;
	widget_class->snapshot = hex_text_snapshot;

	klass->format_line = NULL;	/* pure virtual function */
	klass->pressed = NULL;		/* pure virtual function */
	klass->released = NULL;		/* pure virtual function */
	klass->dragged = NULL;		/* pure virtual function */
	klass->right_click = NULL;	/* pure virtual function */
	klass->render_line = hex_text_render_line;
}

static void
hex_text_init (HexText *self)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);

	priv->render_data_ = g_new0 (HexTextRenderData, 1);

	priv->layouts = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
	{
		PangoLayout *initial_pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET(self), NULL);

		g_hash_table_insert (priv->layouts, GINT_TO_POINTER (0), initial_pango_layout);
	}

	gtk_widget_add_css_class (GTK_WIDGET(self), "hex-text");

	g_signal_connect (self, "notify::vadjustment", G_CALLBACK(adj_setup_signals_cb), NULL);
	g_signal_connect (self, "notify::selection", G_CALLBACK(selection_notify_cb), NULL);
	g_signal_connect (self, "notify::blink-cursor", G_CALLBACK(reset_cursor_blink_timer), NULL);

	{
		GtkGesture *gesture;

		/* Left click pressed */
		gesture = gtk_gesture_click_new ();
		g_signal_connect_swapped (gesture, "pressed", G_CALLBACK(single_left_click_cb), self);
		g_signal_connect_swapped (gesture, "released", G_CALLBACK(released_cb), self);
		gtk_widget_add_controller (GTK_WIDGET(self), GTK_EVENT_CONTROLLER(gesture));

		/* Left mouse dragged */
		gesture = gtk_gesture_drag_new ();
		g_signal_connect_swapped (gesture, "drag-update", G_CALLBACK(drag_cb), self);
		gtk_widget_add_controller (GTK_WIDGET(self), GTK_EVENT_CONTROLLER(gesture));

		/* Right click pressed */
		gesture = gtk_gesture_click_new ();
		gtk_gesture_single_set_button (GTK_GESTURE_SINGLE(gesture), 3); 
		g_signal_connect_swapped (gesture, "pressed", G_CALLBACK(right_click_cb), self);
		gtk_widget_add_controller (GTK_WIDGET(self), GTK_EVENT_CONTROLLER(gesture));
	}

	{
		GtkEventController *focus = gtk_event_controller_focus_new ();
		gtk_widget_add_controller (GTK_WIDGET(self), focus);
		g_signal_connect_swapped (focus, "enter", G_CALLBACK(gtk_widget_queue_draw), self);
		g_signal_connect_swapped (focus, "leave", G_CALLBACK(gtk_widget_queue_draw), self);
	}
}

/* transfer none */
HexTextRenderData *
hex_text_get_render_data (HexText *self)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);

	update_render_data (self);

	return priv->render_data_;
}

gboolean
hex_text_offset_is_visible (HexText *self, gint64 offset, int *line_num)
{
	HexTextPrivate *priv = hex_text_get_instance_private (self);
	HexTextRenderData *render_data = hex_text_get_render_data (self);
	int cpl = hex_view_get_cpl (HEX_VIEW(self));
	int top_line = render_data->top_disp_line;
	int num_lines = hex_view_get_n_vis_lines (HEX_VIEW(self));

	/* Special case: blank file with nothing in it yet */

	if (offset == 0 && num_lines == 0)
	{
		if (line_num)
			*line_num = 0;

		return TRUE;
	}

	for (int line = 0; line < num_lines; ++line)
	{
		gint64 line_start_offset = (top_line + line) * cpl;
		gint64 line_end_offset = line_start_offset + cpl - 1;

		if (offset >= line_start_offset && offset <= line_end_offset)
		{
			if (line_num)
				*line_num = line;

			return TRUE;
		}
	}

	return FALSE;
}

gboolean
hex_text_highlight_is_visible (HexText *self, HexHighlight *highlight, int disp_line_num, int *disp_line_offset_start, int *disp_line_offset_end)
{
	g_return_val_if_fail (HEX_IS_TEXT (self), FALSE);
	g_return_val_if_fail (HEX_IS_HIGHLIGHT (highlight), FALSE);

	HexTextPrivate *priv = hex_text_get_instance_private (self);
	HexTextRenderData *render_data = hex_text_get_render_data (self);
	int cpl = hex_view_get_cpl (HEX_VIEW(self));
	int top_line = render_data->top_disp_line;
	int num_lines = hex_view_get_n_vis_lines (HEX_VIEW(self));

	gint64 line_start_offset = (top_line + disp_line_num) * cpl;
	gint64 line_end_offset = line_start_offset + cpl - 1;

	gint64 real_start_offset = MIN (highlight->start_offset, highlight->end_offset);
	gint64 real_end_offset = MAX (highlight->start_offset, highlight->end_offset);

	/* No overlap */

    if (real_end_offset <= line_start_offset || real_start_offset >= line_end_offset)
	{
        return FALSE;
    }

	/* Set retvals */

	{
		int disp_line_offset_start__retval = -1;
		int disp_line_offset_end__retval = -1;

		if (disp_line_offset_start)
		{
			gint64 effective_start = MAX (real_start_offset, line_start_offset);
			disp_line_offset_start__retval = effective_start - line_start_offset;
		}

		if (disp_line_offset_end)
		{
			gint64 effective_end = MIN (real_end_offset, line_end_offset);
			disp_line_offset_end__retval = effective_end - line_start_offset;
		}

		// FIXME - should this be the behaviour?
		//
		/* Only cursor should be rendered (not a highlight) if the start and end offsets are the same.
		*/
		if (disp_line_offset_start__retval == disp_line_offset_end__retval)
			return FALSE;

		*disp_line_offset_start = disp_line_offset_start__retval;
		*disp_line_offset_end = disp_line_offset_end__retval;
	}
	return TRUE;
}
