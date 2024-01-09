/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex-layout-manager.c - definition of a HexWidget layout manager

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

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#define DEFAULT_OFFSET_CPL	8

struct _HexWidgetLayout {
	GtkLayoutManager parent_instance;

	int char_width;
	HexWidgetGroupType group_type;

	int cpl;
	int hex_cpl;
	int offset_cpl;

	int cursor_x, cursor_y;
};

G_DEFINE_TYPE (HexWidgetLayout, hex_widget_layout, GTK_TYPE_LAYOUT_MANAGER)

struct _HexWidgetLayoutChild {
	GtkLayoutChild parent_instance;

	HexWidgetLayoutColumn column;
};

enum {
  PROP_CHILD_COLUMN = 1,
  N_CHILD_PROPERTIES
};

static GParamSpec *child_props[N_CHILD_PROPERTIES];

/* Some code required to use g_param_spec_enum below. */

#define HEX_WIDGET_LAYOUT_COLUMN (hex_widget_layout_column_get_type ())
static GType
hex_widget_layout_column_get_type (void)
{
	static GType hex_layout_column_type = 0;
	static const GEnumValue format_types[] = {
		{NO_COLUMN, "No column", "no-column"},
		{OFFSETS_COLUMN, "Offsets", "offsets"},
		{HEX_COLUMN, "Hex", "hex"},
		{ASCII_COLUMN, "ASCII", "ascii"},
		{0, NULL, NULL}
	};
	if (! hex_layout_column_type) {
		hex_layout_column_type =
			g_enum_register_static ("HexWidgetLayoutColumn", format_types);
	}
	return hex_layout_column_type;
}


G_DEFINE_TYPE (HexWidgetLayoutChild, hex_widget_layout_child, GTK_TYPE_LAYOUT_CHILD)


/* LAYOUT CHILD METHODS */

static void
hex_widget_layout_child_set_property (GObject *gobject,
		guint         prop_id,
		const GValue *value,
		GParamSpec   *pspec)
{
	HexWidgetLayoutChild *self = HEX_WIDGET_LAYOUT_CHILD(gobject);

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
hex_widget_layout_child_get_property (GObject    *gobject,
		guint       prop_id,
		GValue     *value,
		GParamSpec *pspec)
{
	HexWidgetLayoutChild *self = HEX_WIDGET_LAYOUT_CHILD(gobject);

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
hex_widget_layout_child_finalize (GObject *gobject)
{
	HexWidgetLayoutChild *self = HEX_WIDGET_LAYOUT_CHILD (gobject);

	G_OBJECT_CLASS (hex_widget_layout_child_parent_class)->finalize (gobject);
}

static void
hex_widget_layout_child_dispose (GObject *gobject)
{
	HexWidgetLayoutChild *self = HEX_WIDGET_LAYOUT_CHILD (gobject);

	G_OBJECT_CLASS (hex_widget_layout_child_parent_class)->finalize (gobject);
}

static void
hex_widget_layout_child_class_init (HexWidgetLayoutChildClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->set_property = hex_widget_layout_child_set_property;
	gobject_class->get_property = hex_widget_layout_child_get_property;
	gobject_class->finalize = hex_widget_layout_child_finalize;
	gobject_class->dispose = hex_widget_layout_child_dispose;

	child_props[PROP_CHILD_COLUMN] = g_param_spec_enum ("column",
			"Column type",
			"The column type of a child of a hex layout",
			HEX_WIDGET_LAYOUT_COLUMN,
			NO_COLUMN,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
				G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (gobject_class,
			N_CHILD_PROPERTIES, child_props);
}

static void
hex_widget_layout_child_init (HexWidgetLayoutChild *self)
{
}


/* LAYOUT MANAGER METHODS */

static void
hex_widget_layout_measure (GtkLayoutManager *layout_manager,
		GtkWidget		*widget,
		GtkOrientation	orientation,
		int				for_size,
		int				*minimum,
		int				*natural,
		int				*minimum_baseline,
		int				*natural_baseline)
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

#define BASE_ALLOC {.x = 0, .y = 0, .width = 0, .height = full_height}
static void
hex_widget_layout_allocate (GtkLayoutManager *layout_manager,
		GtkWidget        *widget,
		int               full_width,
		int               full_height,
		int               baseline)		/* N/A */
{
	HexWidgetLayout *self = HEX_WIDGET_LAYOUT (layout_manager);
	GtkWidget *child;
	gboolean have_hex = FALSE;
	gboolean have_ascii = FALSE;
	GtkAllocation off_alloc = BASE_ALLOC;
	GtkAllocation hex_alloc = BASE_ALLOC;
	GtkAllocation asc_alloc = BASE_ALLOC;
	GtkAllocation sbar_alloc = BASE_ALLOC;

	int avail_width = full_width;
	GtkWidget *hex = NULL, *ascii = NULL, *offsets = NULL, *scrollbar = NULL;

	for (child = gtk_widget_get_first_child (widget);
			child != NULL;
			child = gtk_widget_get_next_sibling (child))
	{
		HexWidgetLayoutChild *child_info;

		if (GTK_IS_POPOVER (child))
		{
			gtk_popover_set_pointing_to (GTK_POPOVER(child),
					&(const GdkRectangle){ self->cursor_x, self->cursor_y, 1, 1 });
		}

		/* If it's invisible, etc., this should fail. */
		if (! gtk_widget_should_layout (child))
			continue;

		/* Setup allocation depending on what column we're in.
		 * This loop is run through again once we obtain some initial values. */

		child_info = HEX_WIDGET_LAYOUT_CHILD(
				gtk_layout_manager_get_layout_child (layout_manager, child));

		switch (child_info->column)
		{
			case OFFSETS_COLUMN:	offsets = child;
				break;
			case HEX_COLUMN:		hex = child;
				break;
			case ASCII_COLUMN:		ascii = child;
				break;
			case SCROLLBAR_COLUMN:	scrollbar = child;
				break;

			case NO_COLUMN:
			{
				GtkRequisition child_req = {0};
				GtkAllocation alloc = BASE_ALLOC;

				/* just position the widget in the centre at its preferred
				 * size. TODO: check v/halign and v/hexand
				 */
				gtk_widget_get_preferred_size (child, &child_req, NULL);
				alloc.width = child_req.width;
				alloc.height = child_req.height;
				alloc.x = (full_width / 2) - (alloc.width / 2);
				alloc.y = (full_height / 2) - (alloc.height / 2);
				gtk_widget_size_allocate (child, &alloc, -1);
				return;
			}
				break;

				/* We won't test for this each loop. */
			default:
				g_error ("%s: Programmer error. Requested column invalid.",
						__func__);
				break;
		}
	}

	/* Order doesn't really matter for the offsets & scrollbar columns since
	 * they're essentially fixed, so let's do those first.
	 */
	if (offsets)
	{
		GtkBorder margins, padding, borders;
		GtkStyleContext *context = gtk_widget_get_style_context (offsets);

		gtk_style_context_get_margin (context, &margins);
		gtk_style_context_get_padding (context, &padding);
		gtk_style_context_get_border (context, &borders);

		/* nb: offsets always goes at x coordinate 0 so just leave it as it's
		 * zeroed out anyway. */

		off_alloc.width = self->offset_cpl * self->char_width +
			margins.left + margins.right +
			padding.left + padding.right +
			borders.left + borders.right;

		avail_width -= off_alloc.width;
	}

	if (scrollbar)
	{
		GtkRequisition req = { .width = 0, .height = 0 };
		gtk_widget_get_preferred_size (scrollbar, &req, NULL);

		/* It's always going to be its full width and will always be to the far
		 * right, so just plop it there.
		 */
		sbar_alloc.x = full_width - req.width;
		sbar_alloc.width = req.width;
		sbar_alloc.height = full_height;

		avail_width -= sbar_alloc.width;
	}

	/* Let's measure ascii next, as hex's width is essentially locked to it, if
	 * it's visible. Since hex and ascii are both drawing areas, they both
	 * default to preferring a width of 0, so we have to measure completely.
	 */
	if (ascii)
	{
		int ascii_max_width;

		/* Use width of offsets (if any) as baseline for x posn of ascii and
		 * go from there.
		 */
		asc_alloc.x = off_alloc.width;

		ascii_max_width = asc_alloc.width =
			full_width - off_alloc.width - sbar_alloc.width;

		self->cpl = asc_alloc.width / self->char_width - 1;
		self->hex_cpl = 0;

		if (hex)
		{
			GtkStyleContext *context;
			int tot_cpl, hex_cpl, ascii_cpl;
			int max_width_left = ascii_max_width;

			GtkBorder hex_margins, hex_padding;
			GtkBorder asc_margins, asc_padding;

			context = gtk_widget_get_style_context (hex);
			gtk_style_context_get_margin (context, &hex_margins);
			gtk_style_context_get_padding (context, &hex_padding);

			max_width_left = max_width_left - 
				hex_margins.left - hex_margins.right -
				hex_padding.left - hex_padding.right;

			context = gtk_widget_get_style_context (ascii);
			gtk_style_context_get_margin (context, &asc_margins);
			gtk_style_context_get_padding (context, &asc_padding);

			max_width_left = max_width_left - 
				asc_margins.left - asc_margins.right -
				asc_padding.left - asc_padding.right;

			tot_cpl = max_width_left / self->char_width;

			/* Calculate how many hex vs. ascii characters can be stuffed
			 * on one line.
			 */
			ascii_cpl = 0;
			do {
				if (ascii_cpl % self->group_type == 0 &&
						tot_cpl < self->group_type * 3)
					break;
		
				++ascii_cpl;
				tot_cpl -= 3;   /* 2 for hex disp, 1 for ascii disp */
		
				if (ascii_cpl % self->group_type == 0) /* just ended a group */
					tot_cpl--;
			}
			while (tot_cpl > 0);

			hex_cpl = hex_widget_layout_util_hex_cpl_from_ascii_cpl (
					ascii_cpl, self->group_type);

			asc_alloc.width = ascii_cpl * self->char_width;
			hex_alloc.width = max_width_left - asc_alloc.width;

			/* add back the margins and padding prev. deducted from values.
			 */
			asc_alloc.width += asc_margins.left + asc_margins.right +
				asc_padding.left + asc_padding.right;

			hex_alloc.width += hex_margins.left + hex_margins.right +
				hex_padding.left + hex_padding.right;

			hex_alloc.x = off_alloc.width;
			asc_alloc.x = hex_alloc.x + hex_alloc.width;

			self->hex_cpl = hex_cpl;
			self->cpl = ascii_cpl;
		}
	}

	/* Already determined what to do if have ascii and hex together */
	if (hex && !ascii)
	{
		hex_alloc.x = off_alloc.width;
		hex_alloc.width = full_width - off_alloc.width - sbar_alloc.width;

		self->hex_cpl = hex_alloc.width / self->char_width;
		/* FIXME: This is kind of lazy and will be optimized for the 'byte'
		 * grouptype; rework if possible to adapt to group type. */
		self->cpl = (hex_alloc.width / self->char_width) / 3;
		self->hex_cpl = hex_widget_layout_util_hex_cpl_from_ascii_cpl (
				self->cpl, self->group_type);

		/* TODO - get_cpl_from_hex_width ()
		self->hex_cpl = ???
		self->cpl = ???
		*/
	}

	if (offsets)
		gtk_widget_size_allocate (offsets, &off_alloc, -1);
	if (scrollbar)
		gtk_widget_size_allocate (scrollbar, &sbar_alloc, -1);
	if (hex)
		gtk_widget_size_allocate (hex, &hex_alloc, -1);
	if (ascii)
		gtk_widget_size_allocate (ascii, &asc_alloc, -1);
}
#undef BASE_ALLOC

static GtkSizeRequestMode
hex_widget_layout_get_request_mode (GtkLayoutManager *layout_manager,
		GtkWidget        *widget)
{
	/* I understand this is the default return type anyway; but I guess it
	 * makes sense to explicit.
	 */
	return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static GtkLayoutChild *
hex_widget_layout_create_layout_child (GtkLayoutManager *manager,
		GtkWidget        *widget,
		GtkWidget        *for_child)
{
	return g_object_new (HEX_TYPE_WIDGET_LAYOUT_CHILD,
			"layout-manager", manager,
			"child-widget", for_child,
			NULL);
}

static void
hex_widget_layout_class_init (HexWidgetLayoutClass *klass)
{
	GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

	layout_class->get_request_mode = hex_widget_layout_get_request_mode;
	layout_class->measure = hex_widget_layout_measure;
	layout_class->allocate = hex_widget_layout_allocate;
	layout_class->create_layout_child = hex_widget_layout_create_layout_child;
}

static void
hex_widget_layout_init (HexWidgetLayout *self)
{
	self->offset_cpl = DEFAULT_OFFSET_CPL;
	/* Just pick an arbitrary initial default */
	self->char_width = 20;
	self->group_type = HEX_WIDGET_GROUP_BYTE;
}

/* HexWidgetLayout - Public Methods */

GtkLayoutManager *
hex_widget_layout_new (void)
{
  return g_object_new (HEX_TYPE_WIDGET_LAYOUT, NULL);
}

void
hex_widget_layout_set_char_width (HexWidgetLayout *layout, int width)
{
	layout->char_width = width;

	gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER(layout));
}

void
hex_widget_layout_set_group_type (HexWidgetLayout *layout,
		HexWidgetGroupType group_type)
{
	layout->group_type = group_type;

	gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER(layout));
}

int
hex_widget_layout_get_cpl (HexWidgetLayout *layout)
{
	return layout->cpl;
}

int
hex_widget_layout_get_hex_cpl (HexWidgetLayout *layout)
{
	return layout->hex_cpl;
}

void
hex_widget_layout_set_cursor_pos (HexWidgetLayout *layout, int x, int y)
{
	layout->cursor_x = x;
	layout->cursor_y = y;
}

void
hex_widget_layout_set_offset_cpl (HexWidgetLayout *layout, int offset_cpl)
{
	layout->offset_cpl = offset_cpl;
}

int
hex_widget_layout_get_offset_cpl (HexWidgetLayout *layout)
{
	return layout->offset_cpl;
}

/* HexWidgetLayoutChild - Public Methods */

void
hex_widget_layout_child_set_column (HexWidgetLayoutChild *child,
		HexWidgetLayoutColumn column)
{
	g_return_if_fail (HEX_IS_WIDGET_LAYOUT_CHILD (child));

	if (child->column == column)
		return;

	child->column = column;

	gtk_layout_manager_layout_changed
		(gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD(child)));

	g_object_notify_by_pspec (G_OBJECT(child),
			child_props[PROP_CHILD_COLUMN]);
}

/* Utility functions */

int
hex_widget_layout_util_hex_cpl_from_ascii_cpl (int ascii_cpl,
		HexWidgetGroupType group_type)
{
	return ascii_cpl * 2 + ascii_cpl / group_type;
}

G_GNUC_END_IGNORE_DEPRECATIONS
