#include <string.h>
#include <gnome.h>
#include <gtk/gtk.h>

#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>

#include <gnome-mdi-pouch.h>
#include <gnome-mdi-roo.h>

#define MIN_TITLE_WIDTH     32
#define TITLE_BUTTON_WIDTH  24
#define TITLE_PADDING        2
#define BORDER_WIDTH         6
#define BORDER_HEIGHT        6
#define SIZE_CORNER_PIXELS  16

#define ROO_DRAG         (1L << 0)
#define ROO_SIZE_RB      (1L << 1)
#define ROO_SIZE_B       (1L << 2)
#define ROO_SIZE_LB      (1L << 3)
#define ROO_EXPAND_CHILD (1L << 4)
#define ROO_IN           (1L << 7)

#define ROO_SIZE_FLAGS   (ROO_SIZE_RB | ROO_SIZE_B | ROO_SIZE_LB)

enum {
  ROO_CLOSE_SIGNAL,
  ROO_MINIMIZE_SIGNAL,
  ROO_MAXIMIZE_SIGNAL,
  LAST_SIGNAL
};

static gint roo_signals[LAST_SIGNAL] = { 0, 0, 0 };

static void gnome_mdi_roo_class_init     (GnomeMDIRooClass   *klass);
static void gnome_mdi_roo_init           (GnomeMDIRoo        *roo);
static void gnome_mdi_roo_map            (GtkWidget        *widget);
static void gnome_mdi_roo_unmap          (GtkWidget        *widget);
static void gnome_mdi_roo_realize        (GtkWidget        *widget);
static void gnome_mdi_roo_size_request   (GtkWidget        *widget,
					  GtkRequisition   *requisition);
static void gnome_mdi_roo_size_allocate  (GtkWidget        *widget,
					  GtkAllocation    *allocation);
static void gnome_mdi_roo_paint          (GtkWidget        *widget,
					  GdkRectangle     *area);
static void gnome_mdi_roo_draw           (GtkWidget        *widget,
					  GdkRectangle     *area);
static gint gnome_mdi_roo_expose         (GtkWidget        *widget,
					  GdkEventExpose   *event);
static gint gnome_mdi_roo_button_press   (GtkWidget        *widget,
					  GdkEventButton   *event);
static gint gnome_mdi_roo_button_release (GtkWidget        *widget,
					  GdkEventButton   *event);
static gint gnome_mdi_roo_motion_notify  (GtkWidget         *widget,
					  GdkEventMotion    *event);
static gint gnome_mdi_roo_enter_notify   (GtkWidget        *widget,
					  GdkEventCrossing *event);
static gint gnome_mdi_roo_leave_notify   (GtkWidget        *widget,
					  GdkEventCrossing *event);
static gint gnome_mdi_roo_focus_in       (GtkWidget        *widget,
					  GdkEventFocus    *event);
static gint gnome_mdi_roo_focus_out      (GtkWidget        *widget,
					  GdkEventFocus    *event);
static void gnome_mdi_roo_add            (GtkContainer     *container,
					  GtkWidget        *widget);
static void gnome_mdi_roo_remove         (GtkContainer     *container,
					  GtkWidget        *widget);
static void gnome_mdi_roo_foreach        (GtkContainer     *container,
					  GtkCallback       callback,
					  gpointer          callback_data);

static GtkContainerClass *parent_class;

guint
gnome_mdi_roo_get_type ()
{
  static guint roo_type = 0;

  if (!roo_type)
    {
      GtkTypeInfo roo_info =
      {
	"GnomeMDIRoo",
	sizeof (GnomeMDIRoo),
	sizeof (GnomeMDIRooClass),
	(GtkClassInitFunc) gnome_mdi_roo_class_init,
	(GtkObjectInitFunc) gnome_mdi_roo_init,
	(GtkArgSetFunc) NULL,
	(GtkArgGetFunc) NULL,
      };

      roo_type = gtk_type_unique (gtk_container_get_type (), &roo_info);
    }

  return roo_type;
}

static void
gnome_mdi_roo_class_init (GnomeMDIRooClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;

  parent_class = gtk_type_class (gtk_container_get_type ());

  roo_signals[ROO_CLOSE_SIGNAL] =
    gtk_signal_new ("close",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GnomeMDIRooClass, close),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  roo_signals[ROO_MINIMIZE_SIGNAL] =
    gtk_signal_new ("minimize",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GnomeMDIRooClass, minimize),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  roo_signals[ROO_MAXIMIZE_SIGNAL] =
    gtk_signal_new ("maximize",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GnomeMDIRooClass, maximize),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  widget_class->map = gnome_mdi_roo_map;
  widget_class->unmap = gnome_mdi_roo_unmap;
  widget_class->realize = gnome_mdi_roo_realize;
  widget_class->draw = gnome_mdi_roo_draw;
  widget_class->size_request = gnome_mdi_roo_size_request;
  widget_class->size_allocate = gnome_mdi_roo_size_allocate;
  widget_class->expose_event = gnome_mdi_roo_expose;
  widget_class->motion_notify_event = gnome_mdi_roo_motion_notify;
  widget_class->button_press_event = gnome_mdi_roo_button_press;
  widget_class->button_release_event = gnome_mdi_roo_button_release;
  widget_class->enter_notify_event = gnome_mdi_roo_enter_notify;
  widget_class->leave_notify_event = gnome_mdi_roo_leave_notify;
  widget_class->focus_in_event = gnome_mdi_roo_focus_in;
  widget_class->focus_out_event = gnome_mdi_roo_focus_out;

  container_class->add = gnome_mdi_roo_add;
  container_class->remove = gnome_mdi_roo_remove;
  container_class->foreach = gnome_mdi_roo_foreach;

  klass->close = NULL;
  klass->minimize = NULL;
  klass->maximize = NULL;
}

static void
gnome_mdi_roo_init (GnomeMDIRoo *roo)
{
  GTK_WIDGET_SET_FLAGS (roo, GTK_CAN_FOCUS);

  roo->child = NULL;
  roo->flags = ROO_EXPAND_CHILD;
  roo->x = roo->y = 0;
  roo->x_base = roo->y_base = 0;
  roo->min_width = roo->min_height = 0;
  roo->parent = NULL;
}

GtkWidget*
gnome_mdi_roo_new ()
{
  return GTK_WIDGET (gtk_type_new (gnome_mdi_roo_get_type ()));
}

static void gnome_mdi_roo_map (GtkWidget *widget) {
  GnomeMDIRoo *roo;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_ROO (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  roo = GNOME_MDI_ROO (widget);

  if (!GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_show (widget->window);
  else
    gtk_widget_queue_draw (widget);

  if (roo->child &&
      GTK_WIDGET_VISIBLE (roo->child) &&
      !GTK_WIDGET_MAPPED (roo->child))
    gtk_widget_map (roo->child);
}

static void gnome_mdi_roo_unmap (GtkWidget *widget) {
  GnomeMDIRoo *roo;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_ROO (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  roo = GNOME_MDI_ROO (widget);

  if (GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_clear_area (widget->window,
			   widget->allocation.x,
			   widget->allocation.y,
			   widget->allocation.width,
			   widget->allocation.height);
  else
    gdk_window_hide (widget->window);

  if (roo->child &&
      GTK_WIDGET_VISIBLE (roo->child) &&
       GTK_WIDGET_MAPPED (roo->child))
    gtk_widget_unmap (roo->child);
}

static void
gnome_mdi_roo_realize (GtkWidget *widget)
{
  GnomeMDIRoo *roo;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_ROO (widget));

  roo = GNOME_MDI_ROO (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  border_width = GTK_CONTAINER (widget)->border_width;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_BUTTON1_MOTION_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, roo);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void gnome_mdi_roo_size_request (GtkWidget      *widget,
					GtkRequisition *requisition) {
  GnomeMDIRoo *roo;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_ROO (widget));
  g_return_if_fail (requisition != NULL);

  roo = GNOME_MDI_ROO (widget);
  roo->title_height = widget->style->font->ascent + widget->style->font->descent
                      + 2*TITLE_PADDING + 2*widget->style->klass->ythickness;
  roo->min_width = 2*TITLE_BUTTON_WIDTH + MIN_TITLE_WIDTH;
  roo->min_height = roo->title_height + GTK_CONTAINER (roo)->border_width * 2 + BORDER_HEIGHT;

  requisition->width = roo->min_width;
  requisition->height = roo->min_height;

  if ((roo->child) && (GTK_WIDGET_VISIBLE(roo->child))) {
    gtk_widget_size_request (roo->child, &roo->child->requisition);
    requisition->width = MAX(requisition->width, roo->child->requisition.width +
			                         (GTK_CONTAINER (roo)->border_width + BORDER_WIDTH) * 2);
    requisition->height += roo->child->requisition.height;
  }
}

static void gnome_mdi_roo_size_allocate (GtkWidget     *widget,
					 GtkAllocation *allocation) {
  GnomeMDIRoo *roo;
  GtkAllocation child_allocation;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_ROO(widget));
  g_return_if_fail (allocation != NULL);

  roo = GNOME_MDI_ROO (widget);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget)) {
    gdk_window_move_resize (widget->window,
			    allocation->x, 
			    allocation->y,
			    allocation->width, 
			    allocation->height);
  }

  if ((roo->child) && (GTK_WIDGET_VISIBLE (roo->child))) {
    child_allocation.x = BORDER_WIDTH + GTK_CONTAINER(roo)->border_width;
    child_allocation.y = roo->title_height;
    child_allocation.y += GTK_CONTAINER(roo)->border_width;
    
    if(roo->flags & ROO_EXPAND_CHILD) {
      child_allocation.width = allocation->width - 2*BORDER_WIDTH;
      child_allocation.height = allocation->height - roo->title_height - BORDER_HEIGHT;
    }
    else {
      child_allocation.width = roo->child->requisition.width;
      child_allocation.height = roo->child->requisition.height;
    }
    gtk_widget_size_allocate (roo->child, &child_allocation);
  }
}

static void gnome_mdi_roo_paint(GtkWidget *widget,
				GdkRectangle *area) {
  GnomeMDIRoo *roo;
  gchar *title;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_ROO (widget));

  roo = GNOME_MDI_ROO(widget);

  if(area->x < BORDER_WIDTH)
    gtk_draw_shadow(widget->style, widget->window,
		    GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		    0, roo->title_height,
		    BORDER_WIDTH, widget->allocation.height - roo->title_height);

  if(area->x + area->width > widget->allocation.width - BORDER_WIDTH)
    gtk_draw_shadow(widget->style, widget->window,
		    GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		    widget->allocation.width - BORDER_WIDTH, roo->title_height,
		    BORDER_WIDTH, widget->allocation.height - roo->title_height);

  if(area->y + area->height > widget->allocation.height - BORDER_HEIGHT)
    gtk_draw_shadow(widget->style, widget->window,
		    GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		    BORDER_WIDTH, widget->allocation.height - BORDER_HEIGHT,
		    widget->allocation.width - 2*BORDER_WIDTH, BORDER_HEIGHT);

  if(area->y < roo->title_height) {
    gtk_draw_shadow(widget->style, widget->window,
		    GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		    0, 0, TITLE_BUTTON_WIDTH, roo->title_height);
    gtk_draw_shadow(widget->style, widget->window,
		    GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		    widget->allocation.width - TITLE_BUTTON_WIDTH, 0, TITLE_BUTTON_WIDTH, roo->title_height);
    gtk_draw_shadow(widget->style, widget->window,
		    GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		    TITLE_BUTTON_WIDTH, 0, widget->allocation.width - 2*TITLE_BUTTON_WIDTH, roo->title_height);
#if 0
    if(title = (gchar *)gtk_object_get_data(GTK_OBJECT(roo->child), "title_data"))
      gtk_draw_string(widget->style, widget->window, GTK_STATE_NORMAL, TITLE_BUTTON_WIDTH + 4,
		      widget->style->font->ascent + TITLE_PADDING + widget->style->klass->ythickness, title);
#endif
  }
}

static void gnome_mdi_roo_draw (GtkWidget    *widget,
				GdkRectangle *area) {
  GnomeMDIRoo *roo;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_ROO (widget));

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget)) {
    roo = GNOME_MDI_ROO (widget);

    if (roo->child &&
	gtk_widget_intersect (roo->child, area, &child_area))
      gtk_widget_draw (roo->child, &child_area);

    gnome_mdi_roo_paint(widget, area);
  }
}

static gint gnome_mdi_roo_expose (GtkWidget      *widget,
				  GdkEventExpose *event) {
  GnomeMDIRoo *roo;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GNOME_IS_MDI_ROO (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget)) {
      roo = GNOME_MDI_ROO (widget);

      child_event = *event;

      if (roo->child &&
	  GTK_WIDGET_NO_WINDOW (roo->child) &&
	  gtk_widget_intersect (roo->child, &event->area, &child_event.area))
	gtk_widget_event (roo->child, (GdkEvent*) &child_event);

      gnome_mdi_roo_paint(widget, &event->area);
  }

  return FALSE;
}

static void gnome_mdi_roo_add (GtkContainer *container,
			       GtkWidget    *widget) {
  GnomeMDIRoo *roo;
  gchar *title;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GNOME_IS_MDI_ROO (container));
  g_return_if_fail (widget != NULL);

  roo = GNOME_MDI_ROO (container);

  if (!roo->child) {
    gtk_widget_set_parent (widget, GTK_WIDGET (container));

    if (GTK_WIDGET_VISIBLE (widget->parent)) {
      if (GTK_WIDGET_REALIZED (widget->parent) &&
	  !GTK_WIDGET_REALIZED (widget))
	gtk_widget_realize (widget);
	  
      if (GTK_WIDGET_MAPPED (widget->parent) &&
	  !GTK_WIDGET_MAPPED (widget))
	gtk_widget_map (widget);
    }

    roo->child = widget;

    if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (container))
      gtk_widget_queue_resize (widget);
  }
}

static void gnome_mdi_roo_remove (GtkContainer *container,
				  GtkWidget    *widget) {
  GnomeMDIRoo *roo;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GNOME_IS_MDI_ROO (container));
  g_return_if_fail (widget != NULL);

  roo = GNOME_MDI_ROO (container);

  if (roo->child == widget) {
    gtk_widget_unparent (widget);
    roo->child = NULL;
    
    if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (container))
      gtk_widget_queue_resize (GTK_WIDGET (container));
  }
}

static void gnome_mdi_roo_foreach (GtkContainer *container,
				   GtkCallback   callback,
				   gpointer      callback_data) {
  GnomeMDIRoo *roo;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GNOME_IS_MDI_ROO (container));
  g_return_if_fail (callback != NULL);

  roo = GNOME_MDI_ROO (container);

  if (roo->child)
    (* callback) (roo->child, callback_data);
}

static gint
gnome_mdi_roo_button_press (GtkWidget      *widget,
			    GdkEventButton *event)
{
  GnomeMDIRoo *roo;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GNOME_IS_MDI_ROO (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type == GDK_BUTTON_PRESS) {
    roo = GNOME_MDI_ROO (widget);

    if (GTK_WIDGET_CAN_DEFAULT (widget) && (event->button == 1))
      gtk_widget_grab_default (widget);
    if (!GTK_WIDGET_HAS_FOCUS (widget))
      gtk_widget_grab_focus (widget);
    
    if (event->button == 1) {
      roo->x_base = event->x_root;
      roo->y_base = event->y_root;
      if( (event->y < roo->title_height) && (event->x > TITLE_BUTTON_WIDTH) &&
	  (event->x < widget->allocation.width - TITLE_BUTTON_WIDTH) ) {
	gtk_grab_add(GTK_WIDGET(roo));
	gtk_signal_emit_by_name(GTK_OBJECT(roo->parent), "move_start", roo);
	roo->flags |= ROO_DRAG;
      }
      else if( ((event->y > widget->allocation.height - SIZE_CORNER_PIXELS) &&
		(event->x > widget->allocation.width - BORDER_WIDTH)) ||
	       ((event->x > widget->allocation.width - SIZE_CORNER_PIXELS) &&
		(event->y > widget->allocation.height - BORDER_HEIGHT)) ) {
	gtk_grab_add(GTK_WIDGET(roo));
	roo->flags |= ROO_SIZE_RB;
      }
      else if( (event->y > widget->allocation.height - BORDER_HEIGHT) &&
	       (event->x > SIZE_CORNER_PIXELS) &&
	       (event->x < widget->allocation.width - SIZE_CORNER_PIXELS) ) {
	gtk_grab_add(GTK_WIDGET(roo));
	roo->flags |= ROO_SIZE_B;
      }
      else if( ((event->y > widget->allocation.height - SIZE_CORNER_PIXELS) &&
		(event->x < BORDER_WIDTH)) ||
	       ((event->x < SIZE_CORNER_PIXELS) &&
		(event->y > widget->allocation.height - BORDER_HEIGHT)) ) {
	gtk_grab_add(GTK_WIDGET(roo));
	roo->flags |= ROO_SIZE_LB;
      }
    }
  }
  
  return TRUE;
}

static gint
gnome_mdi_roo_button_release (GtkWidget      *widget,
			      GdkEventButton *event)
{
  GnomeMDIRoo *roo;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GNOME_IS_MDI_ROO (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  roo = GNOME_MDI_ROO (widget);

  if (event->button == 1) {
    if(roo->flags & (ROO_DRAG | ROO_SIZE_FLAGS)) {
      gtk_signal_emit_by_name(GTK_OBJECT(roo->parent), "move_end", roo, TRUE);
      gtk_grab_remove (GTK_WIDGET (roo));
    }

    roo->flags &= ~(ROO_DRAG | ROO_SIZE_FLAGS);
  }

  return TRUE;
}

static gint gnome_mdi_roo_motion_notify(GtkWidget *widget,
					GdkEventMotion *event) {
  GnomeMDIRoo *roo;
  gint x, y;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GNOME_IS_MDI_ROO(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  roo = GNOME_MDI_ROO(widget);

  if(roo->flags & ROO_DRAG) {
    gtk_signal_emit_by_name(GTK_OBJECT(roo->parent), "move_motion", roo,
			    (gint)(event->x_root - roo->x_base), (gint)(event->y_root - roo->y_base));

    roo->x_base = event->x_root;
    roo->y_base = event->y_root;
  }
  else if(roo->flags & ROO_SIZE_RB) {
    x = widget->allocation.width + event->x_root - roo->x_base;
    x = MAX(roo->min_width, x);
    y = widget->allocation.height + event->y_root - roo->y_base;
    y = MAX(roo->min_height, y);
    roo->x_base = event->x_root;
    roo->y_base = event->y_root;

    gtk_widget_set_usize(widget, x, y);
  }
  else if(roo->flags & ROO_SIZE_B) {
    y = widget->allocation.height + event->y_root - roo->y_base;
    y = MAX(roo->min_height, y);
    roo->y_base = event->y_root;

    gtk_widget_set_usize(widget, widget->allocation.width, y);
  }
  else if(roo->flags & ROO_SIZE_LB) {
    x = widget->allocation.width - event->x_root + roo->x_base;
    x = MAX(roo->min_width, x);
    y = widget->allocation.height + event->y_root - roo->y_base;
    y = MAX(roo->min_height, y);
    roo->x += event->x_root - roo->x_base;
    roo->x = MAX(0, roo->x);
    roo->x_base = event->x_root;
    roo->y_base = event->y_root;

    gtk_widget_set_uposition(widget,
			     roo->x + GTK_CONTAINER(roo->parent)->border_width + widget->style->klass->xthickness,
			     widget->allocation.y);
    gtk_widget_set_usize(widget, x, y);
  } 

  return FALSE;
}

static gint
gnome_mdi_roo_enter_notify (GtkWidget        *widget,
			 GdkEventCrossing *event)
{
  GnomeMDIRoo *roo;
  GtkWidget *event_widget;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GNOME_IS_MDI_ROO (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  roo = GNOME_MDI_ROO (widget);
  event_widget = gtk_get_event_widget ((GdkEvent*) event);

  if ((event_widget == widget) &&
      (event->detail != GDK_NOTIFY_INFERIOR))
    {
      roo->flags |= ROO_IN;
    }

  return FALSE;
}

static gint
gnome_mdi_roo_leave_notify (GtkWidget        *widget,
			 GdkEventCrossing *event)
{
  GnomeMDIRoo *roo;
  GtkWidget *event_widget;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GNOME_IS_MDI_ROO (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  roo = GNOME_MDI_ROO (widget);
  event_widget = gtk_get_event_widget ((GdkEvent*) event);

  if ((event_widget == widget) &&
      (event->detail != GDK_NOTIFY_INFERIOR))
    {
      roo->flags &= ~ROO_IN;
    }

  return FALSE;
}

static gint
gnome_mdi_roo_focus_in (GtkWidget     *widget,
		     GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GNOME_IS_MDI_ROO (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

  return FALSE;
}

static gint
gnome_mdi_roo_focus_out (GtkWidget     *widget,
		      GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GNOME_IS_MDI_ROO (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

  return FALSE;
}
