/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex-layout-manager.h - declaration of a GtkHex layout manager

   Copyright © 2021 Logan Rathbone <poprocks@gmail.com>

   GHex is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   GHex is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GHex; see the file COPYING.
   If not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Original GHex Author: Jaka Mocnik <jaka@gnu.org>
*/

#include "gtkhex-layout-manager.h"

#define OFFSETS_CPL		10
#define HEX_RATIO		0.75
#define ASCII_RATIO		0.25

struct _GtkHexLayout {
	GtkLayoutManager parent_instance;

	GtkWidget *offets;
	GtkWidget *hex;
	GtkWidget *ascii;

	guint char_width;
	guint group_type;

	int cpl;
	int hex_cpl;
};

G_DEFINE_TYPE (GtkHexLayout, gtk_hex_layout, GTK_TYPE_LAYOUT_MANAGER)

struct _GtkHexLayoutChild {
	GtkLayoutChild parent_instance;

	GtkHexLayoutColumn column;
};

enum {
  PROP_CHILD_COLUMN = 1,
  N_CHILD_PROPERTIES
};

static GParamSpec *child_props[N_CHILD_PROPERTIES];

/* Some code required to use g_param_spec_enum below. */

#define GTK_HEX_LAYOUT_COLUMN (gtk_hex_layout_column_get_type ())
static GType
gtk_hex_layout_column_get_type (void)
{
	static GType hex_layout_column_type = 0;
	static const GEnumValue format_types[] = {
		{OFFSETS_COLUMN, "Offsets", "offsets"},
		{HEX_COLUMN, "Hex", "hex"},
		{ASCII_COLUMN, "ASCII", "ascii"},
		{0, NULL, NULL}
	};
	if (! hex_layout_column_type) {
		hex_layout_column_type =
			g_enum_register_static ("GstAudioParseFormat", format_types);
	}
	return hex_layout_column_type;
}


G_DEFINE_TYPE (GtkHexLayoutChild, gtk_hex_layout_child, GTK_TYPE_LAYOUT_CHILD)


/* LAYOUT CHILD METHODS */

static void
gtk_hex_layout_child_set_property (GObject *gobject,
		guint         prop_id,
		const GValue *value,
		GParamSpec   *pspec)
{
	GtkHexLayoutChild *self = GTK_HEX_LAYOUT_CHILD(gobject);

	switch (prop_id)
	{
		case PROP_CHILD_COLUMN:
			self->column = g_value_get_enum (value);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
			break;
	}
}

static void
gtk_hex_layout_child_get_property (GObject    *gobject,
		guint       prop_id,
		GValue     *value,
		GParamSpec *pspec)
{
	GtkHexLayoutChild *self = GTK_HEX_LAYOUT_CHILD(gobject);

	switch (prop_id)
	{
		case PROP_CHILD_COLUMN:
			g_value_set_enum (value, self->column);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
			break;
	}
}

static void
gtk_hex_layout_child_finalize (GObject *gobject)
{
	GtkHexLayoutChild *self = GTK_HEX_LAYOUT_CHILD (gobject);

	G_OBJECT_CLASS (gtk_hex_layout_child_parent_class)->finalize (gobject);
}

static void
gtk_hex_layout_child_dispose (GObject *gobject)
{
	GtkHexLayoutChild *self = GTK_HEX_LAYOUT_CHILD (gobject);

	G_OBJECT_CLASS (gtk_hex_layout_child_parent_class)->finalize (gobject);
}

static void
gtk_hex_layout_child_class_init (GtkHexLayoutChildClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->set_property = gtk_hex_layout_child_set_property;
	gobject_class->get_property = gtk_hex_layout_child_get_property;
	gobject_class->finalize = gtk_hex_layout_child_finalize;
	gobject_class->dispose = gtk_hex_layout_child_dispose;

	child_props[PROP_CHILD_COLUMN] = g_param_spec_enum ("column",
			"Column type",
			"The column type of a child of a hex layout",
			GTK_HEX_LAYOUT_COLUMN,
			HEX_COLUMN,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
				G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (gobject_class,
			N_CHILD_PROPERTIES, child_props);
}

static void
gtk_hex_layout_child_init (GtkHexLayoutChild *self)
{
}


/* LAYOUT MANAGER METHODS */

static void
gtk_hex_layout_measure (GtkLayoutManager *layout_manager,
		GtkWidget		*widget,
		GtkOrientation	orientation,
		int				for_size,
		int				*minimum,
		int				*natural,
		int				*minimum_baseline,
		int				*natural_baseline)
{
	GtkHexLayoutChild *child_info;
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

		child_info =
			GTK_HEX_LAYOUT_CHILD(gtk_layout_manager_get_layout_child
					(layout_manager, child));

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

/* Helper */
static void
get_cpl_from_ascii_width (GtkHexLayout *self, int width)
{
	int hex_cpl, ascii_cpl;

	/* Hex characters per line is a simple calculation: */

	ascii_cpl = width / self->char_width;
	while (ascii_cpl % self->group_type != 0)
		--ascii_cpl;

	hex_cpl = ascii_cpl * 2;
   	hex_cpl += ascii_cpl / self->group_type;

	self->hex_cpl = hex_cpl;
	self->cpl = ascii_cpl;
}

static void
gtk_hex_layout_allocate (GtkLayoutManager *layout_manager,
		GtkWidget        *widget,
		int               width,
		int               height,
		int               baseline)
{
	GtkHexLayout *self = GTK_HEX_LAYOUT (layout_manager);
	GtkHexLayoutChild *child_info;
	GtkWidget *child;
	gboolean have_offsets = FALSE;
	gboolean have_hex = FALSE;
	gboolean have_ascii = FALSE;
	GtkAllocation base_alloc = {.x = 0, .y = 0, .width = 0, .height = height};
	GtkAllocation off_alloc = base_alloc;	/* GdkRectangle */
	GtkAllocation hex_alloc = base_alloc;
	GtkAllocation asc_alloc = base_alloc;
	GtkAllocation scr_alloc = base_alloc;
	GtkAllocation tmp_alloc = {0};

	for (child = gtk_widget_get_first_child (widget);
			child != NULL;
			child = gtk_widget_get_next_sibling (child))
	{
		GtkRequisition child_req = { .width = 0, .height = 0 };

		if (! gtk_widget_should_layout (child))
			continue;

		/* Get preferred size of the child widget */

		gtk_widget_get_preferred_size (child, &child_req, NULL);

		/* Setup allocation depending on what column we're in. */

		child_info =
			GTK_HEX_LAYOUT_CHILD(gtk_layout_manager_get_layout_child (layout_manager,
						child));

		switch (child_info->column)
		{
			case OFFSETS_COLUMN:
				have_offsets = TRUE;
				off_alloc.width = OFFSETS_CPL * self->char_width;
				break;
			case HEX_COLUMN:
				have_hex = TRUE;
				break;
			case ASCII_COLUMN:
				have_ascii = TRUE;
				break;
			case SCROLLBAR_COLUMN:
				scr_alloc.x = width;
				scr_alloc.width = child_req.width;
				scr_alloc.height = height;
				break;
			default:
				g_error ("%s: Programming error. "
						"The requested column is invalid.",
						__func__);
				break;
		}
	}
	
	for (child = gtk_widget_get_first_child (widget);
			child != NULL;
			child = gtk_widget_get_next_sibling (child))
	{
		GtkRequisition child_req = { .width = 0, .height = 0 };

		if (! gtk_widget_should_layout (child))
			continue;

		/* Get preferred size of the child widget */

		gtk_widget_get_preferred_size (child, &child_req, NULL);

		/* Setup allocation depending on what column we're in. */

		child_info =
			GTK_HEX_LAYOUT_CHILD(gtk_layout_manager_get_layout_child
					(layout_manager, child));

		switch (child_info->column)
		{
			case OFFSETS_COLUMN:
				tmp_alloc = off_alloc;
				break;

			case HEX_COLUMN:
				hex_alloc.width = width - off_alloc.width;
				hex_alloc.width -= scr_alloc.width;
				if (have_ascii) {
					hex_alloc.width *= HEX_RATIO;
					hex_alloc.x += off_alloc.width;
				}
				tmp_alloc = hex_alloc;
				break;

			case ASCII_COLUMN:
				asc_alloc.x += off_alloc.width;
				asc_alloc.width = width;
				asc_alloc.width -= off_alloc.width;
				asc_alloc.width -= scr_alloc.width;
				if (have_hex) {
					asc_alloc.width *= ASCII_RATIO;
					asc_alloc.x += (width - off_alloc.width - scr_alloc.width)
						* HEX_RATIO;
				}
				tmp_alloc = asc_alloc;

				get_cpl_from_ascii_width (self, asc_alloc.width);

				break;

			case SCROLLBAR_COLUMN:
				tmp_alloc = scr_alloc;
				break;

			default:
				g_error ("%s: Programming error. The requested column is invalid.",
						__func__);
				break;
		}

		gtk_widget_size_allocate (child,
			&tmp_alloc,
			-1);	/* baseline, or -1 */
	}
}

static GtkSizeRequestMode
gtk_hex_layout_get_request_mode (GtkLayoutManager *layout_manager,
		GtkWidget        *widget)
{
	/* I understand this is the default return type anyway; but I guess it
	 * makes sense to explicit.
	 */
	return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static GtkLayoutChild *
gtk_hex_layout_create_layout_child (GtkLayoutManager *manager,
		GtkWidget        *widget,
		GtkWidget        *for_child)
{
	return g_object_new (GTK_TYPE_HEX_LAYOUT_CHILD,
			"layout-manager", manager,
			"child-widget", for_child,
			NULL);
}

static void
gtk_hex_layout_class_init (GtkHexLayoutClass *klass)
{
	GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

	layout_class->get_request_mode = gtk_hex_layout_get_request_mode;
	layout_class->measure = gtk_hex_layout_measure;
	layout_class->allocate = gtk_hex_layout_allocate;
	layout_class->create_layout_child = gtk_hex_layout_create_layout_child;
}

static void
gtk_hex_layout_init (GtkHexLayout *self)
{
	/* FIXME - dumb test initial default */
	self->char_width = 20;
	self->group_type = GROUP_BYTE;
}

/* GtkHexLayout - Public Methods */

GtkLayoutManager *
gtk_hex_layout_new (void)
{
  return g_object_new (GTK_TYPE_HEX_LAYOUT, NULL);
}

void
gtk_hex_layout_set_char_width (GtkHexLayout *layout, guint width)
{
	layout->char_width = width;

	gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER(layout));
}

void
gtk_hex_layout_set_group_type (GtkHexLayout *layout, guint group_type)
{
	layout->group_type = group_type;

	gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER(layout));
}

int
gtk_hex_layout_get_cpl (GtkHexLayout *layout)
{
	return layout->cpl;
}

int
gtk_hex_layout_get_hex_cpl (GtkHexLayout *layout)
{
	return layout->hex_cpl;
}

/* GtkHexLayoutChild - Public Methods */

void
gtk_hex_layout_child_set_column (GtkHexLayoutChild *child,
		GtkHexLayoutColumn column)
{
	g_return_if_fail (GTK_IS_HEX_LAYOUT_CHILD (child));

	if (child->column == column)
		return;

	child->column = column;

	gtk_layout_manager_layout_changed
		(gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD(child)));

	g_object_notify_by_pspec (G_OBJECT(child),
			child_props[PROP_CHILD_COLUMN]);
}
