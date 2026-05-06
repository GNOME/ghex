#include "ghex-color-grid.h"

enum
{
	PROP_0,
	PROP_ONE,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

enum
{
	SIG_CHANGED,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

struct _GHexColorGrid
{
	GtkWidget parent_instance;
	
	gpointer grid;
	GdkRGBA *colors;
};

G_DEFINE_FINAL_TYPE (GHexColorGrid, ghex_color_grid, GTK_TYPE_WIDGET)

static void
emit_changed (GHexColorGrid *self)
{
	g_signal_emit (self, signals[SIG_CHANGED], 0);
}

static void
ghex_color_grid_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	GHexColorGrid *self = GHEX_COLOR_GRID(object);

	switch (property_id)
	{
		case PROP_ONE:
			/* --- */
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
ghex_color_grid_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	GHexColorGrid *self = GHEX_COLOR_GRID(object);

	switch (property_id)
	{
		case PROP_ONE:
			/* --- */
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
sync_color_buttons_from_colors (GHexColorGrid *self)
{
	g_assert (GHEX_IS_COLOR_GRID (self));

	for (int i = 0; i < 16; ++i) {
		for (int j = 0; j < 16; ++j)
		{
			const int row = i + 1;
			const int col = j + 1;
			const int off = j + 16 * i;

			gpointer subgrid = gtk_grid_get_child_at (self->grid, col, row);
			GtkColorDialogButton *color_btn = GTK_COLOR_DIALOG_BUTTON(gtk_grid_get_child_at (subgrid, 0, 0));

			gtk_color_dialog_button_set_rgba (color_btn, &self->colors[off]);
		}
	}
}

static void
color_btn_color_changed_cb (GHexColorGrid *self, GParamSpec *pspec, gpointer color_btn)
{
	guint8 offset;
	const GdkRGBA *color;

	g_assert (GHEX_IS_COLOR_GRID (self));
	g_assert (GTK_IS_COLOR_DIALOG_BUTTON (color_btn));

	offset = GPOINTER_TO_INT (g_object_get_data (color_btn, "offset"));

	color = gtk_color_dialog_button_get_rgba (color_btn);
	g_assert (color != NULL);

	self->colors[offset] = *color;

	emit_changed (self);
}

static void
ghex_color_grid_init (GHexColorGrid *self)
{
	self->colors = g_new0 (GdkRGBA, 256);

	self->grid = gtk_grid_new ();

	gtk_grid_set_row_homogeneous (self->grid, TRUE);
	gtk_grid_set_column_homogeneous (self->grid, TRUE);

	gtk_widget_set_parent (self->grid, GTK_WIDGET(self));

	/* Setup row/col labels */

	for (int i = 0; i < 16; ++i)
	{
		const int idx = i + 1;
		gpointer row_label = gtk_label_new (NULL);
		gpointer col_label = gtk_label_new (NULL);
		g_autofree char *text = g_strdup_printf ("<b><tt>%02X</tt></b>", i);

		gtk_label_set_markup (row_label, text);
		gtk_widget_set_valign (row_label, GTK_ALIGN_START);

		gtk_label_set_markup (col_label, text);

		gtk_grid_attach (self->grid, row_label, 0, idx, 1, 1);
		gtk_grid_attach (self->grid, col_label, idx, 0, 1, 1);
	}
	
	/* Setup color buttons in grid */

	for (int i = 0; i < 16; ++i) {
		for (int j = 0; j < 16; ++j)
		{
			const int row = i + 1;
			const int col = j + 1;
			const int off = j + 16 * i;

			gpointer subgrid = gtk_grid_new ();
			gpointer color_btn = gtk_color_dialog_button_new (gtk_color_dialog_new ());
			gpointer label = gtk_label_new (NULL);
			g_autofree char *label_str = g_strdup_printf ("<tt><small>%02X</small></tt>", off);

			g_object_set_data (color_btn, "offset", GINT_TO_POINTER(off));

			g_signal_connect_object (color_btn, "notify::rgba", G_CALLBACK(color_btn_color_changed_cb), self, G_CONNECT_SWAPPED);

			gtk_label_set_markup (label, label_str);

			gtk_grid_attach (subgrid, color_btn, 0, 0, 1, 1);
			gtk_grid_attach (subgrid, label, 0, 1, 1, 1);

			gtk_grid_attach (self->grid, subgrid, col, row, 1, 1);
		}
	}
}

static void
ghex_color_grid_dispose (GObject *object)
{
	GHexColorGrid *self = GHEX_COLOR_GRID(object);

	g_clear_pointer (&self->grid, gtk_widget_unparent);

	G_OBJECT_CLASS(ghex_color_grid_parent_class)->dispose (object);
}

static void
ghex_color_grid_finalize (GObject *object)
{
	GHexColorGrid *self = GHEX_COLOR_GRID(object);

	g_free (self->colors);

	G_OBJECT_CLASS(ghex_color_grid_parent_class)->finalize (object);
}

static void
ghex_color_grid_class_init (GHexColorGridClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	object_class->dispose =  ghex_color_grid_dispose;
	object_class->finalize = ghex_color_grid_finalize;
	object_class->set_property = ghex_color_grid_set_property;
	object_class->get_property = ghex_color_grid_get_property;

	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

	properties[PROP_ONE] = g_param_spec_string ("property-one", NULL, NULL,
			NULL,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	signals[SIG_CHANGED] = g_signal_new_class_handler ("changed",
			G_OBJECT_CLASS_TYPE (object_class),
			G_SIGNAL_RUN_LAST,
			NULL,
			NULL, NULL, NULL,	
			G_TYPE_NONE,
			0);
}

GHexColorGrid *
ghex_color_grid_new (void)
{
	return g_object_new (GHEX_TYPE_COLOR_GRID, NULL);
}

void
ghex_color_grid_populate (GHexColorGrid *self, const GdkRGBA *colors)
{
	g_return_if_fail (GHEX_IS_COLOR_GRID (self));
	g_return_if_fail (colors != NULL);

	memcpy (self->colors, colors, 256 * sizeof(GdkRGBA));

	g_idle_add_once ((GSourceOnceFunc) sync_color_buttons_from_colors, self);
}

/* Returns: array length=256 -- free with `g_free()` */

GdkRGBA *
ghex_color_grid_to_rgba_array (GHexColorGrid *self)
{
	GdkRGBA *out = NULL;

	g_return_val_if_fail (GHEX_IS_COLOR_GRID (self), NULL);

	out = g_new0 (GdkRGBA, 256);

	memcpy (out, self->colors, 256 * sizeof(GdkRGBA));

	return out;
}
