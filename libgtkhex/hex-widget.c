// vim: linebreak breakindent breakindentopt=shift\:4

#define G_LOG_DOMAIN "hex-widget"

#include "hex-widget.h"

#include "hex-private-common.h"
#include "gtkhex-paste-data.h"
#include "hex-highlight-private.h"
#include "hex-auto-highlight-private.h"
#include "hex-text-offsets.h"
#include "util.h"

#include "libgtkhex-enums.h"

enum
{
	PROP_0,
	PROP_SHOW_OFFSETS,
	PROP_SHOW_HEX,
	PROP_SHOW_ASCII,
	PROP_AUTO_GEOMETRY_CPL,
	PROP_GROUP_TYPE,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _HexWidget
{
	HexView parent_instance;

	GBinding *auto_geometry_binding;
	int auto_geo_cpl;
	HexGroupType group_type;
	
	/* From template */
	GtkWidget *offsets;
	GtkWidget *xdisp;
	GtkWidget *adisp;
};

G_DEFINE_FINAL_TYPE (HexWidget, hex_widget, HEX_TYPE_VIEW)

static void
auto_geometry_set_cb (HexWidget *self, GParamSpec *pspec, gpointer user_data)
{
	gboolean auto_geometry = hex_view_get_auto_geometry (HEX_VIEW(self));

	g_clear_object (&self->auto_geometry_binding);

	if (auto_geometry)
	{
		self->auto_geometry_binding = g_object_bind_property (self, "auto-geometry-cpl", self, "cpl", G_BINDING_SYNC_CREATE);
	}
}

void
hex_widget_set_show_offsets (HexWidget *self, gboolean show)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	if (show != hex_widget_get_show_offsets (self))
		gtk_widget_set_visible (self->offsets, show);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_SHOW_OFFSETS]);
}

gboolean
hex_widget_get_show_offsets (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), TRUE);

	return gtk_widget_get_visible (self->offsets);
}

void
hex_widget_set_show_hex (HexWidget *self, gboolean show)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	if (show != hex_widget_get_show_hex (self))
		gtk_widget_set_visible (self->xdisp, show);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_SHOW_HEX]);
}

gboolean
hex_widget_get_show_hex (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), TRUE);

	return gtk_widget_get_visible (self->xdisp);
}

void
hex_widget_set_show_ascii (HexWidget *self, gboolean show)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	if (show != hex_widget_get_show_ascii (self))
		gtk_widget_set_visible (self->adisp, show);

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_SHOW_ASCII]);
}

gboolean
hex_widget_get_show_ascii (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), TRUE);

	return gtk_widget_get_visible (self->adisp);
}

static void
_hex_widget_set_auto_geometry_cpl (HexWidget *self, int auto_geo_cpl)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	if (auto_geo_cpl == self->auto_geo_cpl)
		return;

	self->auto_geo_cpl = auto_geo_cpl;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_AUTO_GEOMETRY_CPL]);
}

int
hex_widget_get_auto_geometry_cpl (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), 20);

	return self->auto_geo_cpl;
}

void
hex_widget_set_group_type (HexWidget *self, HexGroupType group_type)
{
	g_return_if_fail (HEX_IS_WIDGET (self));

	self->group_type = group_type;

	g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_GROUP_TYPE]);
}

HexGroupType
hex_widget_get_group_type (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), HEX_GROUP_BYTE);

	return self->group_type;
}

static void
hex_widget_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexWidget *self = HEX_WIDGET(object);

	switch (property_id)
	{
		case PROP_SHOW_OFFSETS:
			hex_widget_set_show_offsets (self, g_value_get_boolean (value));
			break;

		case PROP_SHOW_HEX:
			hex_widget_set_show_hex (self, g_value_get_boolean (value));
			break;

		case PROP_SHOW_ASCII:
			hex_widget_set_show_ascii (self, g_value_get_boolean (value));
			break;

		case PROP_GROUP_TYPE:
			hex_widget_set_group_type (self, g_value_get_enum (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_widget_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexWidget *self = HEX_WIDGET(object);

	switch (property_id)
	{
		case PROP_SHOW_OFFSETS:
			g_value_set_boolean (value, hex_widget_get_show_offsets (self));
			break;

		case PROP_SHOW_HEX:
			g_value_set_boolean (value, hex_widget_get_show_hex (self));
			break;

		case PROP_SHOW_ASCII:
			g_value_set_boolean (value, hex_widget_get_show_ascii (self));
			break;

		case PROP_AUTO_GEOMETRY_CPL:
			g_value_set_int (value, hex_widget_get_auto_geometry_cpl (self));
			break;

		case PROP_GROUP_TYPE:
			g_value_set_enum (value, hex_widget_get_group_type (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static gboolean
hex_widget_focus (GtkWidget *widget, GtkDirectionType dir)
{
	HexWidget *self = HEX_WIDGET(widget);

	switch (dir)
	{
		case GTK_DIR_TAB_BACKWARD:
			if (gtk_widget_is_focus (self->xdisp))
				return FALSE;
			else if (gtk_widget_is_focus (self->adisp))
				return gtk_widget_grab_focus (self->xdisp);
			break;

		case GTK_DIR_TAB_FORWARD:
			if (gtk_widget_is_focus (self->adisp))
				return FALSE;
			else if (gtk_widget_is_focus (self->xdisp))
				return gtk_widget_grab_focus (self->adisp);
			break;

		default:
			break;
	}

	return GTK_WIDGET_CLASS (hex_widget_parent_class)->focus (widget, dir);
}

static gboolean
hex_widget_grab_focus (GtkWidget *widget)
{
	HexWidget *self = HEX_WIDGET(widget);

	if (hex_widget_get_show_hex (self) && gtk_widget_grab_focus (self->xdisp))
		return TRUE;
	else if (hex_widget_get_show_ascii (self) && gtk_widget_grab_focus (self->adisp))
		return TRUE;

	return GTK_WIDGET_CLASS (hex_widget_parent_class)->grab_focus (widget);
}

static void
hex_widget_measure (GtkWidget *widget, GtkOrientation orientation, int for_size, int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
	GtkWidget *child;
	int minimum_size = 0;
	int natural_size = 0;

	for (child = gtk_widget_get_first_child (widget);
			child != NULL;
			child = gtk_widget_get_next_sibling (child))
	{
		int child_min = 0, child_nat = 0;

		if (!gtk_widget_should_layout (child))
			continue;

		gtk_widget_measure (child, orientation,
				/* for-size: */ -1,		/* == unknown. */
				&child_min, &child_nat,
				NULL, NULL);
		minimum_size = MAX (minimum_size, child_min);
		natural_size = MAX (natural_size, child_nat);
	}

	if (minimum != NULL)
		*minimum = minimum_size;
	if (natural != NULL)
		*natural = natural_size;
}

#define BASE_ALLOC (GtkAllocation){.x = 0, .y = 0, .width = 0, .height = full_height}
static void
hex_widget_size_allocate (GtkWidget *widget, int full_width, int full_height, int baseline)
{
	HexWidget *self = HEX_WIDGET(widget);
	GtkWidget *child;
	GtkAllocation off_alloc = BASE_ALLOC;
	GtkAllocation hex_alloc = BASE_ALLOC;
	GtkAllocation asc_alloc = BASE_ALLOC;
	GtkWidget *hex = NULL, *ascii = NULL, *offsets = NULL;
	int tmp_auto_cpl = 0;
	const int char_width = hex_view_get_char_width (HEX_VIEW(self));

	g_return_if_fail (char_width != 0);

	for (child = gtk_widget_get_first_child (widget);
			child != NULL;
			child = gtk_widget_get_next_sibling (child))
	{
		if (! gtk_widget_should_layout (child))
			continue;

		/* Setup allocation depending on what column we're in.
		 * This loop is run through again once we obtain some initial values. */

		if (HEX_IS_TEXT_OFFSETS (child))
			offsets = child;
		else if (HEX_IS_TEXT_HEX (child))
			hex = child;
		else if (HEX_IS_TEXT_ASCII (child))
			ascii = child;
		else
		{
			GtkRequisition child_req = {0};
			GtkAllocation alloc = BASE_ALLOC;

			g_debug ("%s: unexpected child: %s - %p", __func__, G_OBJECT_TYPE_NAME (child), (void *) child);

			/* just position the widget in the centre at its preferred
			 * size. TODO: check v/halign and v/hexand
			 */
			gtk_widget_get_preferred_size (child, &child_req, NULL);
			alloc.width = child_req.width;
			alloc.height = child_req.height;
			alloc.x = (full_width / 2) - (alloc.width / 2);
			alloc.y = (full_height / 2) - (alloc.height / 2);
			gtk_widget_size_allocate (child, &alloc, -1);
		}
	}

	/* Order doesn't really matter for the offsets column since
	 * it's essentially fixed, so let's do that first.
	 */
	if (offsets)
	{
		int min, nat;

		gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1, &min, &nat, NULL, NULL);

		g_debug ("%s: min: %d - nat: %d", __func__, min, nat);

		/* offsets always goes at x coordinate 0 so just leave it as it's
		 * zeroed out anyway. */

		off_alloc.width = nat;
	}

	if (ascii)
	{
		int ascii_padding;

		asc_alloc.x = off_alloc.width;
		asc_alloc.width = full_width - off_alloc.width;

		/* Hack to estimate how much space GTK needs for CSS borders, padding, etc. */
		gtk_widget_size_allocate (ascii, &(GtkAllocation){.x=asc_alloc.x, .y=0, .width=asc_alloc.width, .height=full_width}, -1);
		ascii_padding = asc_alloc.width - gtk_widget_get_width (ascii);

		tmp_auto_cpl = gtk_widget_get_width (ascii) / char_width; // - 1;

		if (hex)
		{
			int tot_cpl, ascii_cpl;
			int hex_padding;
			const int max_width_left = asc_alloc.width;

			/* Same hack as above */
			gtk_widget_size_allocate (hex, &(GtkAllocation){.x=asc_alloc.x, .y=0, .width=max_width_left, .height=full_height}, -1);
			hex_padding = max_width_left - gtk_widget_get_width (hex);

			tot_cpl = gtk_widget_get_width (hex) / char_width;

			/* Calculate how many hex vs. ascii characters can be stuffed
			 * on one line.
			 */
			ascii_cpl = 0;
			do {
				int tmp_asc_w = ascii_cpl * char_width + ascii_padding;
				int tmp_hex_w = util_hex_cpl_from_ascii_cpl (ascii_cpl, self->group_type) * char_width + hex_padding;

				if (tmp_asc_w + tmp_hex_w > max_width_left)
					break;

				if (ascii_cpl % self->group_type == 0 &&
						tot_cpl < self->group_type * 3)
					break;

				asc_alloc.width = tmp_asc_w;
				hex_alloc.width = tmp_hex_w;

				++ascii_cpl;
				tot_cpl -= 3;   /* 2 for hex disp, 1 for ascii disp */
		
				if (ascii_cpl % self->group_type == 0) /* just ended a group */
					tot_cpl--;
			}
			while (tot_cpl > 0);

			g_assert (hex_alloc.width + asc_alloc.width <= max_width_left);

			if (max_width_left > hex_alloc.width + asc_alloc.width)
			{
				const int extra_space = max_width_left - (hex_alloc.width + asc_alloc.width);

				hex_alloc.width += 0.5 * extra_space;
				asc_alloc.width += 0.5 * extra_space;
			}

			hex_alloc.x = off_alloc.width;
			asc_alloc.x = hex_alloc.x + hex_alloc.width;

			tmp_auto_cpl = ascii_cpl;
		}
	}

	// FIXME
	if (hex && !ascii)
	{
		int tmp_cpl;

		hex_alloc.x = off_alloc.width;
		hex_alloc.width = full_width - off_alloc.width;

		/* FIXME: This is kind of lazy and will be optimized for the 'byte'
		 * grouptype; rework if possible to adapt to group type. */
		tmp_auto_cpl = (hex_alloc.width / char_width) / 3;
	}

	if (offsets)
		gtk_widget_size_allocate (offsets, &off_alloc, -1);
	if (hex)
		gtk_widget_size_allocate (hex, &hex_alloc, -1);
	if (ascii)
		gtk_widget_size_allocate (ascii, &asc_alloc, -1);

	_hex_widget_set_auto_geometry_cpl (self, tmp_auto_cpl);

	GTK_WIDGET_CLASS(hex_widget_parent_class)->size_allocate (widget, full_width, full_height, baseline);
}
#undef BASE_ALLOC

static void
hex_widget_dispose (GObject *object)
{
	HexWidget *self = HEX_WIDGET(object);

	gtk_widget_dispose_template (GTK_WIDGET(object), HEX_TYPE_WIDGET);

	/* Chain up */
	G_OBJECT_CLASS(hex_widget_parent_class)->dispose (object);
}

static void
hex_widget_finalize (GObject *object)
{
	HexWidget *self = HEX_WIDGET(object);

	/* Chain up */
	G_OBJECT_CLASS(hex_widget_parent_class)->finalize (object);
}

static void
hex_widget_constructed (GObject *object)
{
	HexWidget *self = HEX_WIDGET(object);

	/* We need the object fully constructed before we bind the font properties so
	 * it can't be done in the ui file.
	 */
	g_object_bind_property (self, "font", self->adisp, "font", G_BINDING_SYNC_CREATE);
	g_object_bind_property (self, "font", self->xdisp, "font", G_BINDING_SYNC_CREATE);
	g_object_bind_property (self, "font", self->offsets, "font", G_BINDING_SYNC_CREATE);

	// FIXME - not sure if we still need this here or if it can be in the .ui file
	g_object_bind_property (self, "cpl", self->adisp, "cpl", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
	g_object_bind_property (self, "cpl", self->xdisp, "cpl", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
	g_object_bind_property (self, "cpl", self->offsets, "cpl", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	/* Chain up */
	G_OBJECT_CLASS(hex_widget_parent_class)->constructed (object);
}

static void
hex_widget_class_init (HexWidgetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamFlags default_flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

	object_class->constructed = hex_widget_constructed;
	object_class->dispose = hex_widget_dispose;
	object_class->finalize = hex_widget_finalize;
	object_class->set_property = hex_widget_set_property;
	object_class->get_property = hex_widget_get_property;

	widget_class->focus = hex_widget_focus;
	widget_class->grab_focus = hex_widget_grab_focus;
	widget_class->size_allocate = hex_widget_size_allocate;
	widget_class->measure = hex_widget_measure;

	/* TEMPLATE */

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libgtkhex/ui/hex-widget.ui");

	gtk_widget_class_bind_template_child (widget_class, HexWidget, offsets);
	gtk_widget_class_bind_template_child (widget_class, HexWidget, xdisp);
	gtk_widget_class_bind_template_child (widget_class, HexWidget, adisp);

	/* PROPERTIES */

	properties[PROP_SHOW_OFFSETS] = g_param_spec_boolean ("show-offsets", NULL, NULL,
			TRUE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_SHOW_HEX] = g_param_spec_boolean ("show-hex", NULL, NULL,
			TRUE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_SHOW_ASCII] = g_param_spec_boolean ("show-ascii", NULL, NULL,
			TRUE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_AUTO_GEOMETRY_CPL] = g_param_spec_int ("auto-geometry-cpl", NULL, NULL,
			0, 10000, 20,
			default_flags | G_PARAM_READABLE);

	properties[PROP_GROUP_TYPE] = g_param_spec_enum ("group-type", NULL, NULL,
			HEX_TYPE_GROUP_TYPE,
			HEX_GROUP_BYTE,
			default_flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	/* Keybindings */

	/* INS - toggle insert mode */
	gtk_widget_class_install_property_action (widget_class, "document.insert-mode", "insert-mode");
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Insert, 0, "document.insert-mode", NULL);
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Insert, 0, "document.insert-mode", NULL);

	/* Load global CSS data for widget */
	{
		GdkDisplay *display = gdk_display_get_default ();

		if (display != NULL)
		{
			g_autoptr(GtkCssProvider) css_provider = gtk_css_provider_new ();

			gtk_css_provider_load_from_resource (css_provider, "/org/gnome/libgtkhex/css/libgtkhex.css");

			gtk_style_context_add_provider_for_display (display, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_THEME-1);
		}
	}
}

static void
hex_widget_init (HexWidget *self)
{
	gtk_widget_init_template (GTK_WIDGET(self));

	gtk_widget_set_focusable (GTK_WIDGET(self), TRUE);

	g_signal_connect (self, "notify::auto-geometry", G_CALLBACK(auto_geometry_set_cb), NULL);
}

GtkWidget *
hex_widget_new (void)
{
	return g_object_new (HEX_TYPE_WIDGET, NULL);
}

HexTextHex *
hex_widget_get_hex_display (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), NULL);

	return HEX_TEXT_HEX(self->xdisp);
}

HexTextAscii *
hex_widget_get_ascii_display (HexWidget *self)
{
	g_return_val_if_fail (HEX_IS_WIDGET (self), NULL);

	return HEX_TEXT_ASCII(self->adisp);
}
