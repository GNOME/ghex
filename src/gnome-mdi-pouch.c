/*
 * gnome-mdi-pouch.c
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */

#include <gnome-mdi-pouch.h>
#include <gnome-mdi-roo.h>

typedef void (*ContainerFunc)(GtkContainer *, GtkWidget *);

static void gnome_mdi_pouch_class_init    (GnomeMDIPouchClass    *);
static void gnome_mdi_pouch_init          (GnomeMDIPouch         *);
static void gnome_mdi_pouch_map           (GtkWidget        *);
static void gnome_mdi_pouch_unmap         (GtkWidget        *);
static void gnome_mdi_pouch_realize       (GtkWidget        *);
static void gnome_mdi_pouch_size_request  (GtkWidget        *,
					   GtkRequisition   *);
static void gnome_mdi_pouch_size_allocate (GtkWidget        *,
					   GtkAllocation    *);
static void gnome_mdi_pouch_paint         (GtkWidget        *,
					   GdkRectangle     *);
static void gnome_mdi_pouch_draw          (GtkWidget        *,
					   GdkRectangle     *);
static gint gnome_mdi_pouch_expose        (GtkWidget        *,
					   GdkEventExpose   *);
static void gnome_mdi_pouch_foreach       (GtkContainer     *,
					   GtkCallback      ,
					   gpointer         );
static void gnome_mdi_pouch_draw_outline(GnomeMDIPouch *, gint, gint, gint, gint);
static void gnome_mdi_pouch_move_start(GnomeMDIPouch *, GnomeMDIRoo *);
static void gnome_mdi_pouch_move_end(GnomeMDIPouch *, GnomeMDIRoo *, gboolean);
static void gnome_mdi_pouch_move_motion(GnomeMDIPouch *, GnomeMDIRoo *, gint, gint);
static void gnome_mdi_pouch_resize_start(GnomeMDIPouch *, GnomeMDIRoo *);
static void gnome_mdi_pouch_resize_end(GnomeMDIPouch *, GnomeMDIRoo *, gboolean);
static void gnome_mdi_pouch_resize_motion(GnomeMDIPouch *, GnomeMDIRoo *, gint, gint);

enum {
  MOVE_START,
  MOVE_END,
  MOVE_MOTION,
  RESIZE_START,
  RESIZE_END,
  RESIZE_MOTION,
  LAST_SIGNAL
};

static gint pouch_signals[LAST_SIGNAL] = { 0 };

typedef void (*GnomeMDIPouchSignal1) (GtkObject *, gpointer, gpointer);
typedef void (*GnomeMDIPouchSignal2) (GtkObject *, gpointer, gboolean, gpointer);
typedef void (*GnomeMDIPouchSignal3) (GtkObject *, gpointer, gint, gint, gpointer);

static GtkContainerClass *parent_class = NULL;

guint gnome_mdi_pouch_get_type ()
{
  static guint pouch_type = 0;

  if (!pouch_type)
    {
      GtkTypeInfo pouch_info =
      {
	"GnomeMDIPouch",
	sizeof (GnomeMDIPouch),
	sizeof (GnomeMDIPouchClass),
	(GtkClassInitFunc) gnome_mdi_pouch_class_init,
	(GtkObjectInitFunc) gnome_mdi_pouch_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      pouch_type = gtk_type_unique (gtk_container_get_type (), &pouch_info);
    }

  return pouch_type;
}


static void gnome_mdi_pouch_marshal_1 (GtkObject            *object,
				       GtkSignalFunc        func,
				       gpointer	            func_data,
				       GtkArg	            *args) {
  GnomeMDIPouchSignal1 rfunc;

  rfunc = (GnomeMDIPouchSignal1) func;
  
  (* rfunc)(object, GTK_VALUE_POINTER(args[0]), func_data);
}

static void gnome_mdi_pouch_marshal_2 (GtkObject            *object,
				       GtkSignalFunc        func,
				       gpointer	            func_data,
				       GtkArg	            *args) {
  GnomeMDIPouchSignal2 rfunc;

  rfunc = (GnomeMDIPouchSignal2) func;
  
  (* rfunc)(object, GTK_VALUE_POINTER(args[0]), GTK_VALUE_BOOL(args[1]), func_data);
}

static void gnome_mdi_pouch_marshal_3 (GtkObject            *object,
				       GtkSignalFunc        func,
				       gpointer	            func_data,
				       GtkArg	            *args) {
  GnomeMDIPouchSignal3 rfunc;

  rfunc = (GnomeMDIPouchSignal3) func;
  
  (* rfunc)(object, GTK_VALUE_POINTER(args[0]), GTK_VALUE_INT(args[1]), GTK_VALUE_INT(args[2]), func_data);
}

static void gnome_mdi_pouch_class_init (GnomeMDIPouchClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (gtk_container_get_type ());

  pouch_signals[MOVE_START] =
    gtk_signal_new ("move_start",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GnomeMDIPouchClass, move_start),
                    gnome_mdi_pouch_marshal_1,
		    GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
  pouch_signals[MOVE_END] =
    gtk_signal_new ("move_end",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GnomeMDIPouchClass, move_end),
                    gnome_mdi_pouch_marshal_2,
		    GTK_TYPE_NONE, 1, GTK_TYPE_POINTER, GTK_TYPE_BOOL);
  pouch_signals[MOVE_MOTION] =
    gtk_signal_new ("move_motion",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GnomeMDIPouchClass, move_motion),
                    gnome_mdi_pouch_marshal_3,
		    GTK_TYPE_NONE, 3, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);
  pouch_signals[RESIZE_START] =
    gtk_signal_new ("resize_start",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GnomeMDIPouchClass, resize_start),
                    gnome_mdi_pouch_marshal_1,
		    GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
  pouch_signals[RESIZE_END] =
    gtk_signal_new ("resize_end",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GnomeMDIPouchClass, resize_end),
                    gnome_mdi_pouch_marshal_2,
		    GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_BOOL);
  pouch_signals[RESIZE_MOTION] =
    gtk_signal_new ("resize_motion",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GnomeMDIPouchClass, resize_motion),
                    gnome_mdi_pouch_marshal_3,
		    GTK_TYPE_NONE, 3, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);


  widget_class->map = gnome_mdi_pouch_map;
  widget_class->unmap = gnome_mdi_pouch_unmap;
  widget_class->realize = gnome_mdi_pouch_realize;
  widget_class->size_request = gnome_mdi_pouch_size_request;
  widget_class->size_allocate = gnome_mdi_pouch_size_allocate;
  widget_class->draw = gnome_mdi_pouch_draw;
  widget_class->expose_event = gnome_mdi_pouch_expose;

  container_class->add = (ContainerFunc)gnome_mdi_pouch_add;
  container_class->remove = (ContainerFunc)gnome_mdi_pouch_remove;
  container_class->foreach = gnome_mdi_pouch_foreach;

  class->move_start = gnome_mdi_pouch_move_start;
  class->move_end = gnome_mdi_pouch_move_end;
  class->move_motion = gnome_mdi_pouch_move_motion;
  class->resize_start = gnome_mdi_pouch_resize_start;
  class->resize_end = gnome_mdi_pouch_resize_end;
  class->resize_motion = gnome_mdi_pouch_resize_motion;
}

static void gnome_mdi_pouch_init (GnomeMDIPouch *pouch)
{
  GTK_WIDGET_UNSET_FLAGS(pouch, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS(pouch, GTK_BASIC);
 
  pouch->roos = NULL;
  pouch->outline_gc = NULL;
  pouch->flags = GNOME_MDI_POUCH_SHADOW_DRAG;
}

GtkWidget *gnome_mdi_pouch_new ()
{
  GnomeMDIPouch *pouch;

  pouch = gtk_type_new (gnome_mdi_pouch_get_type ());
  return GTK_WIDGET (pouch);
}  

static void gnome_mdi_pouch_map (GtkWidget *widget) {
  GnomeMDIPouch *pouch;
  GnomeMDIRoo *roo;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_POUCH (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  pouch = GNOME_MDI_POUCH (widget);

  gdk_window_show (widget->window);

  children = pouch->roos;
  while (children) {
    roo = children->data;
    children = children->next;

    if (GTK_WIDGET_VISIBLE (GTK_WIDGET(roo)) &&
	!GTK_WIDGET_MAPPED (GTK_WIDGET(roo)) )
      gtk_widget_map (GTK_WIDGET(roo));
  }
}

static void gnome_mdi_pouch_unmap (GtkWidget *widget) {
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_POUCH (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
}

static void gnome_mdi_pouch_realize (GtkWidget *widget) {
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_POUCH (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, 
				   attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void gnome_mdi_pouch_size_request (GtkWidget      *widget,
					  GtkRequisition *requisition) {
  GnomeMDIPouch *pouch;  
  GnomeMDIRoo *roo;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_POUCH (widget));
  g_return_if_fail (requisition != NULL);

  pouch = GNOME_MDI_POUCH (widget);
  requisition->width = 0;
  requisition->height = 0;

  children = pouch->roos;
  while (children) {
    roo = children->data;
    children = children->next;

    if (GTK_WIDGET_VISIBLE (GTK_WIDGET(roo))) {
      gtk_widget_size_request (GTK_WIDGET(roo), &GTK_WIDGET(roo)->requisition);

      requisition->height = MAX (requisition->height,
				 GTK_WIDGET(roo)->allocation.y +
				 GTK_WIDGET(roo)->requisition.height);
      requisition->width = MAX (requisition->width,
				GTK_WIDGET(roo)->allocation.x +
				GTK_WIDGET(roo)->requisition.width);
    }
  }

  requisition->height += (GTK_CONTAINER (pouch)->border_width + widget->style->klass->ythickness) * 2;
  requisition->width += (GTK_CONTAINER (pouch)->border_width + widget->style->klass->xthickness) * 2;
}

static void gnome_mdi_pouch_size_allocate (GtkWidget     *widget,
					   GtkAllocation *allocation) {
  GnomeMDIPouch *pouch;
  GnomeMDIRoo *roo;
  GtkAllocation roo_allocation;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_POUCH(widget));
  g_return_if_fail (allocation != NULL);

  pouch = GNOME_MDI_POUCH (widget);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, 
			    allocation->y,
			    allocation->width, 
			    allocation->height);

  children = pouch->roos;
  while (children) {
    roo = children->data;
    children = children->next;
      
    if (GTK_WIDGET_VISIBLE (roo)) {
      roo_allocation.x = roo->x + GTK_CONTAINER(pouch)->border_width + widget->style->klass->xthickness;
      roo_allocation.y = roo->y + GTK_CONTAINER(pouch)->border_width + widget->style->klass->ythickness;
      roo_allocation.width = GTK_WIDGET(roo)->requisition.width;
      roo_allocation.height = GTK_WIDGET(roo)->requisition.height;
      gtk_widget_size_allocate (GTK_WIDGET(roo), &roo_allocation);
    }
  }
}

static void gnome_mdi_pouch_paint (GtkWidget    *widget,
				   GdkRectangle *area) {
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_POUCH (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget)) {
    gdk_window_clear_area (widget->window,
			   area->x, area->y,
			   area->width, area->height);

    /* if we have erased any part of the border, redraw all shadows */
    if((area->x <= widget->style->klass->xthickness) ||
       (area->y <= widget->style->klass->ythickness) ||
       (area->x + area->width >= widget->allocation.width - widget->style->klass->xthickness) ||
       (area->y + area->height >= widget->allocation.height - widget->style->klass->ythickness))
      gtk_draw_shadow(widget->style, widget->window,
		      GTK_STATE_NORMAL, GTK_SHADOW_IN,
		      0, 0, -1, -1);
  }
}

static void gnome_mdi_pouch_draw_outline(GnomeMDIPouch *pouch, gint x, gint y, gint width, gint height) {
  if(pouch->outline_gc == NULL) {
    pouch->outline_gc = gdk_gc_new(GTK_WIDGET(pouch)->window);
    gdk_gc_set_function(pouch->outline_gc, GDK_INVERT);
  }  

  x = MAX(x, 0);
  y = MAX(y, 0);

  gdk_draw_rectangle(GTK_WIDGET(pouch)->window, pouch->outline_gc,
		     pouch->flags & GNOME_MDI_POUCH_SHADOW_DRAG,
		     x, y, width, height);
}  

static void gnome_mdi_pouch_draw (GtkWidget    *widget,
				  GdkRectangle *area) {
  GnomeMDIPouch *pouch;
  GnomeMDIRoo *roo;
  GdkRectangle roo_area;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GNOME_IS_MDI_POUCH (widget));

  if (GTK_WIDGET_DRAWABLE (widget)) {
    pouch = GNOME_MDI_POUCH (widget);
    gnome_mdi_pouch_paint (widget, area);

    children = pouch->roos;
    while (children) {
      roo = children->data;
      children = children->next;

      if (gtk_widget_intersect (GTK_WIDGET(roo), area, &roo_area))
	gtk_widget_draw (GTK_WIDGET(roo), &roo_area);
    }
  }
}

static gint gnome_mdi_pouch_expose (GtkWidget      *widget,
				    GdkEventExpose *event) {
  GnomeMDIPouch *pouch;
  GnomeMDIRoo *roo;
  GdkEventExpose child_event;
  GList *children;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GNOME_IS_MDI_POUCH (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget)) {
    pouch = GNOME_MDI_POUCH (widget);

    if (event->window == widget->window)
      gnome_mdi_pouch_paint (widget, &event->area);

    child_event = *event;

    children = pouch->roos;
    while (children) {
      roo = children->data;
      children = children->next;

      if (GTK_WIDGET_NO_WINDOW (GTK_WIDGET(roo)) &&
	  gtk_widget_intersect (GTK_WIDGET(roo), &event->area, 
				&child_event.area))
	gtk_widget_event (GTK_WIDGET(roo), (GdkEvent*) &child_event);
    }
  }
  
  return FALSE;
}

void gnome_mdi_pouch_add (GnomeMDIPouch *pouch,
			  GtkWidget    *widget) {
  GnomeMDIRoo *roo;

  g_return_if_fail (pouch != NULL);
  g_return_if_fail (GNOME_IS_MDI_POUCH (pouch));
  g_return_if_fail (widget != NULL);

  roo = GNOME_MDI_ROO(gnome_mdi_roo_new());

  roo->x = 0;
  roo->y = 0;
  roo->parent = pouch;
  gtk_widget_set_parent(GTK_WIDGET(roo), GTK_WIDGET(pouch));
  gtk_container_add(GTK_CONTAINER (roo), widget);
  gtk_widget_show(GTK_WIDGET(roo));

  pouch->roos = g_list_append (pouch->roos, roo); 

  if(GTK_WIDGET_REALIZED (pouch) && !GTK_WIDGET_REALIZED (roo))
    gtk_widget_realize(GTK_WIDGET(roo));

  if (GTK_WIDGET_MAPPED (pouch) && !GTK_WIDGET_MAPPED (roo))
    gtk_widget_map (GTK_WIDGET(roo));

  if (GTK_WIDGET_VISIBLE (pouch) && GTK_WIDGET_VISIBLE (roo))
    gtk_widget_queue_resize (GTK_WIDGET (pouch));
}

void gnome_mdi_pouch_remove (GnomeMDIPouch *pouch,
			     GtkWidget    *widget) {
  GtkWidget *roo;
  GList *children;

  g_return_if_fail (pouch != NULL);
  g_return_if_fail (GNOME_IS_MDI_POUCH (pouch));
  g_return_if_fail (widget != NULL);

  children = pouch->roos;
  while (children) {
    roo = children->data;

    if (GNOME_MDI_ROO(roo)->child == widget) {
      gtk_container_remove(GTK_CONTAINER(roo), widget);

      pouch->roos = g_list_remove_link (pouch->roos, children);
      g_list_free (children);

      if (GTK_WIDGET_VISIBLE (roo) && GTK_WIDGET_VISIBLE (pouch))
	gtk_widget_queue_resize (GTK_WIDGET (pouch));

      gtk_widget_destroy(roo);

      break;
    }

    children = children->next;
  }
}

static void gnome_mdi_pouch_foreach (GtkContainer *container,
				     GtkCallback   callback,
				     gpointer      callback_data) {
  GnomeMDIPouch *pouch;
  GtkWidget *roo;
  GList *children;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GNOME_IS_MDI_POUCH (container));
  g_return_if_fail (callback != NULL);

  pouch = GNOME_MDI_POUCH (container);

  children = pouch->roos;
  while (children) {
    roo = children->data;
    children = children->next;

    (* callback) (roo, callback_data);
  }
}

static void gnome_mdi_pouch_move_start(GnomeMDIPouch *pouch, GnomeMDIRoo *roo) {
  pouch->move_x = roo->x;
  pouch->move_y = roo->y;
  gnome_mdi_pouch_draw_outline(pouch, GTK_WIDGET(roo)->allocation.x - 1, GTK_WIDGET(roo)->allocation.y - 1,
			       GTK_WIDGET(roo)->allocation.width + 2, GTK_WIDGET(roo)->allocation.height + 2);
}

static void gnome_mdi_pouch_move_end(GnomeMDIPouch *pouch, GnomeMDIRoo *roo, gboolean keep) {
  gnome_mdi_pouch_draw_outline(pouch, GTK_WIDGET(roo)->allocation.x + pouch->move_x - roo->x - 1,
			       GTK_WIDGET(roo)->allocation.y + pouch->move_y - roo->y - 1,
			       GTK_WIDGET(roo)->allocation.width + 2, GTK_WIDGET(roo)->allocation.height + 2);
  if(keep) {
    roo->x = pouch->move_x;
    roo->y = pouch->move_y;
    gtk_widget_set_uposition(GTK_WIDGET(roo),
			     roo->x + GTK_CONTAINER(pouch)->border_width +
			     GTK_WIDGET(roo)->style->klass->xthickness,
			     roo->y + GTK_CONTAINER(pouch)->border_width +
			     GTK_WIDGET(roo)->style->klass->ythickness);
  }
}

static void gnome_mdi_pouch_move_motion(GnomeMDIPouch *pouch, GnomeMDIRoo *roo, gint dx, gint dy) {
  gnome_mdi_pouch_draw_outline(pouch, GTK_WIDGET(roo)->allocation.x + pouch->move_x - roo->x - 1,
			       GTK_WIDGET(roo)->allocation.y + pouch->move_y - roo->y - 1,
			       GTK_WIDGET(roo)->allocation.width + 2, GTK_WIDGET(roo)->allocation.height + 2);
  pouch->move_x += dx;
  pouch->move_y += dy;
  pouch->move_x = MAX(pouch->move_x, 0);
  pouch->move_y = MAX(pouch->move_y, 0);

  gnome_mdi_pouch_draw_outline(pouch, GTK_WIDGET(roo)->allocation.x + pouch->move_x - roo->x - 1,
			       GTK_WIDGET(roo)->allocation.y + pouch->move_y - roo->y - 1,
			       GTK_WIDGET(roo)->allocation.width + 2, GTK_WIDGET(roo)->allocation.height + 2);
}

static void gnome_mdi_pouch_resize_start(GnomeMDIPouch *pouch, GnomeMDIRoo *roo) {
  pouch->move_x = roo->x;
  pouch->move_y = roo->y;
  gnome_mdi_pouch_draw_outline(pouch, GTK_WIDGET(roo)->allocation.x - 1, GTK_WIDGET(roo)->allocation.y - 1,
			       GTK_WIDGET(roo)->allocation.width + 2, GTK_WIDGET(roo)->allocation.height + 2);
}

static void gnome_mdi_pouch_resize_end(GnomeMDIPouch *pouch, GnomeMDIRoo *roo, gboolean keep) {
  gnome_mdi_pouch_draw_outline(pouch, GTK_WIDGET(roo)->allocation.x + pouch->move_x - roo->x - 1,
			       GTK_WIDGET(roo)->allocation.y + pouch->move_y - roo->y - 1,
			       GTK_WIDGET(roo)->allocation.width + 2, GTK_WIDGET(roo)->allocation.height + 2);
}

static void gnome_mdi_pouch_resize_motion(GnomeMDIPouch *pouch, GnomeMDIRoo *roo, gint dx, gint dy) {
  gnome_mdi_pouch_draw_outline(pouch, GTK_WIDGET(roo)->allocation.x + pouch->move_x - roo->x - 1,
			       GTK_WIDGET(roo)->allocation.y + pouch->move_y - roo->y - 1,
			       GTK_WIDGET(roo)->allocation.width + 2, GTK_WIDGET(roo)->allocation.height + 2);
  pouch->move_x += dx;
  pouch->move_y += dy;
  gnome_mdi_pouch_draw_outline(pouch, GTK_WIDGET(roo)->allocation.x + pouch->move_x - roo->x - 1,
			       GTK_WIDGET(roo)->allocation.y + pouch->move_y - roo->y - 1,
			       GTK_WIDGET(roo)->allocation.width + 2, GTK_WIDGET(roo)->allocation.height + 2);
}
