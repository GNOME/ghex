#include <gnome.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgnome/libgnome.h>

#include <libgnomeui/libgnomeui.h>

#include <gnome-mdi-pouch.h>
#include <gnome-mdi-roo.h>

#include <gnome-mdi.h>
#include <gnome-document.h>

#include <gtkhex.h>

#define DEBUG

static void             gnome_mdi_class_init     (GnomeMDIClass  *);
static void             gnome_mdi_init           (GnomeMDI *);
static void             gnome_mdi_destroy        (GtkObject *);
static GnomeDocument   *gnome_mdi_find_document  (GnomeMDI *, gchar *);

static GtkWidget       *find_item_by_label       (GtkMenuShell *, gchar *);

static GtkWidget       *doc_list_menu_create     (GnomeMDI *);
static void             doc_list_menu_insert     (GtkMenuBar *, GtkWidget *);
static void             doc_list_menu_remove_item(GnomeMDI *, GnomeDocument *);
static void             doc_list_menu_add_item   (GnomeMDI *, GnomeDocument *);
static void             doc_list_activated_cb    (GtkWidget *, GnomeMDI *);

static void             doc_menus_insert         (GtkMenuBar *, GList *);
static void             doc_menus_remove         (GtkMenuBar *);

static void             app_create               (GnomeMDI *);
static void             app_destroy              (GnomeApp *);
static void             app_set_title            (GnomeMDI *, GnomeApp *);
static void             app_set_active_view      (GnomeMDI *, GnomeApp *, GtkWidget *);

static gint             app_close_top            (GnomeApp *, GdkEventAny *, GnomeMDI *);
static gint             app_close_book           (GnomeApp *, GdkEventAny *, GnomeMDI *);
static gint             app_close_ms             (GnomeApp *, GdkEventAny *, GnomeMDI *);

static void             set_page_by_widget       (GtkNotebook *, GtkWidget *);

static GtkWidget       *book_create              (GnomeMDI *);
static void             book_switch_page         (GtkNotebook *, GtkNotebookPage *,
						  gint, GnomeMDI *);
static void             book_add_view            (GtkNotebook *, GtkWidget *);
static void             book_remove_view         (GtkWidget *, GnomeMDI *);

static void             toplevel_focus           (GnomeApp *, GdkEventFocus *, GnomeMDI *);

static void             top_add_view             (GnomeMDI *, GnomeDocument *, GtkWidget *);

static void             book_drag                (GtkNotebook *, GdkEvent *, GnomeMDI *);
static void             book_drop                (GtkNotebook *, GdkEvent *, GnomeMDI *);
static void             rootwin_drop             (GtkWidget *, GdkEvent *, GnomeMDI *);

static GnomeUIInfo     *copy_ui_info_tree        (GnomeUIInfo *);
static void             free_ui_info_tree        (GnomeUIInfo *);

#define DND_DATA_TYPE "mdi/bookpage"

/* this is just for simple debugging (as little gdb or ddd as possible...) */
#define EMBRACE(s) { printf("<\n"); s; printf(">\n"); }

/* accepted DND types for the Notebook */
char *possible_drag_types[] = { DND_DATA_TYPE };
char *accepted_drop_types[] = { DND_DATA_TYPE };

enum {
  CREATE_MENUS,
  CREATE_TOOLBAR,
  ADD_DOCUMENT,
  REMOVE_DOCUMENT,
  ADD_VIEW,
  REMOVE_VIEW,
  DOCUMENT_CHANGED,
  APP_CREATED,       /* so new GnomeApps can be further customized by applications */
  LAST_SIGNAL
};

typedef GtkWidget *(*GnomeMDISignal1) (GtkObject *, gpointer);
typedef gboolean   (*GnomeMDISignal2) (GtkObject *, gpointer, gpointer);
typedef void       (*GnomeMDISignal3) (GtkObject *, gpointer, gpointer, gpointer);
typedef void       (*GnomeMDISignal4) (GtkObject *, gpointer, gpointer);

static gint mdi_signals[LAST_SIGNAL];

static GtkObjectClass *parent_class = NULL;

static void gnome_mdi_marshal_1 (GtkObject	    *object,
				 GtkSignalFunc       func,
				 gpointer	     func_data,
				 GtkArg	            *args) {
  GnomeMDISignal1 rfunc;
  gpointer *return_val;

  rfunc = (GnomeMDISignal1) func;
  return_val = GTK_RETLOC_POINTER (args[0]);
  
  *return_val = (* rfunc)(object, func_data);
}

static void gnome_mdi_marshal_2 (GtkObject	    *object,
				 GtkSignalFunc       func,
				 gpointer	     func_data,
				 GtkArg	            *args) {
  GnomeMDISignal2 rfunc;
  gint *return_val;

  rfunc = (GnomeMDISignal2) func;
  return_val = GTK_RETLOC_BOOL (args[1]);
  
  *return_val = (* rfunc)(object, GTK_VALUE_POINTER(args[0]), func_data);
}

static void gnome_mdi_marshal_3 (GtkObject	    *object,
				 GtkSignalFunc       func,
				 gpointer	     func_data,
				 GtkArg	            *args) {
  GnomeMDISignal3 rfunc;

  rfunc = (GnomeMDISignal3) func;
  
  (* rfunc)(object, GTK_VALUE_POINTER(args[0]), GTK_VALUE_POINTER(args[1]), func_data);
}

static void gnome_mdi_marshal_4 (GtkObject	    *object,
				 GtkSignalFunc       func,
				 gpointer	     func_data,
				 GtkArg	            *args) {
  GnomeMDISignal4 rfunc;

  rfunc = (GnomeMDISignal4) func;
  
  (* rfunc)(object, GTK_VALUE_POINTER(args[0]), func_data);
}


guint gnome_mdi_get_type () {
  static guint mdi_type = 0;
  
  if (!mdi_type) {
    GtkTypeInfo mdi_info = {
      "GnomeMDI",
      sizeof (GnomeMDI),
      sizeof (GnomeMDIClass),
      (GtkClassInitFunc) gnome_mdi_class_init,
      (GtkObjectInitFunc) gnome_mdi_init,
      (GtkArgSetFunc) NULL,
      (GtkArgGetFunc) NULL,
    };
    
    mdi_type = gtk_type_unique (gtk_object_get_type (), &mdi_info);
  }

  return mdi_type;
}

static void gnome_mdi_class_init (GnomeMDIClass *class) {
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  object_class->destroy = gnome_mdi_destroy;

  mdi_signals[CREATE_MENUS] = gtk_signal_new ("create_menus",
					      GTK_RUN_LAST,
					      object_class->type,
					      GTK_SIGNAL_OFFSET (GnomeMDIClass, create_menus),
					      gnome_mdi_marshal_1,
					      GTK_TYPE_POINTER, 0);
  mdi_signals[CREATE_TOOLBAR] = gtk_signal_new ("create_toolbar",
						GTK_RUN_LAST,
						object_class->type,
						GTK_SIGNAL_OFFSET (GnomeMDIClass, create_menus),
						gnome_mdi_marshal_1,
						GTK_TYPE_POINTER, 0);
  mdi_signals[ADD_DOCUMENT] = gtk_signal_new ("add_document",
					      GTK_RUN_LAST,
					      object_class->type,
					      GTK_SIGNAL_OFFSET (GnomeMDIClass, add_document),
					      gnome_mdi_marshal_2,
					      GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  mdi_signals[REMOVE_DOCUMENT] = gtk_signal_new ("remove_document",
						 GTK_RUN_LAST,
						 object_class->type,
						 GTK_SIGNAL_OFFSET (GnomeMDIClass, remove_document),
						 gnome_mdi_marshal_2,
						 GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  mdi_signals[ADD_VIEW] = gtk_signal_new ("add_view",
					  GTK_RUN_LAST,
					  object_class->type,
					  GTK_SIGNAL_OFFSET (GnomeMDIClass, add_view),
					  gnome_mdi_marshal_2,
					  GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  mdi_signals[REMOVE_VIEW] = gtk_signal_new ("remove_view",
					     GTK_RUN_LAST,
					     object_class->type,
					     GTK_SIGNAL_OFFSET (GnomeMDIClass, remove_view),
					     gnome_mdi_marshal_2,
					     GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  mdi_signals[DOCUMENT_CHANGED] = gtk_signal_new ("document_changed",
					     GTK_RUN_LAST,
					     object_class->type,
					     GTK_SIGNAL_OFFSET (GnomeMDIClass, document_changed),
					     gnome_mdi_marshal_3,
					     GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_POINTER);
  mdi_signals[APP_CREATED] = gtk_signal_new ("app_created",
					     GTK_RUN_LAST,
					     object_class->type,
					     GTK_SIGNAL_OFFSET (GnomeMDIClass, app_created),
					     gnome_mdi_marshal_4,
					     GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

  gtk_object_class_add_signals (object_class, mdi_signals, LAST_SIGNAL);

  class->create_menus = NULL;
  class->create_toolbar = NULL;
  class->add_document = NULL;
  class->remove_document = NULL;
  class->add_view = NULL;
  class->remove_view = NULL;
  class->document_changed = NULL;
  class->app_created = NULL;

  parent_class = gtk_type_class (gtk_object_get_type ());
}

static void gnome_mdi_destroy(GtkObject *object) {
  GnomeMDI *mdi;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GNOME_IS_MDI (object));

#ifdef DEBUG
  printf("GnomeMDI: destroying!\n");
#endif

  mdi = GNOME_MDI (object);

#ifdef DEBUG
  if(mdi->documents)
    g_warning("GnomeMDI destroying: non-removed documents exist!\n");
#endif

  g_free (mdi->appname);
  g_free (mdi->title);

  if(GTK_OBJECT_CLASS(parent_class)->destroy)
    (* GTK_OBJECT_CLASS(parent_class)->destroy)(GTK_OBJECT(mdi));
}

static void gnome_mdi_init (GnomeMDI *mdi) {
  GTK_WIDGET_SET_FLAGS (mdi, GTK_BASIC);

  mdi->flags = 0;

  mdi->documents = NULL;
  mdi->windows = NULL;

  mdi->active_doc = NULL;
  mdi->active_window = NULL;
  mdi->active_view = NULL;

  mdi->root_window = NULL;
}

GtkObject *gnome_mdi_new(gchar *appname, gchar *title) {
  GnomeMDI *mdi;

  mdi = gtk_type_new (gnome_mdi_get_type ());

  mdi->appname = g_strdup(appname);
  mdi->title = g_strdup(title);

  return GTK_OBJECT (mdi);
}

static gint book_has_child(GtkNotebook *book, GtkWidget *child) {
  GList *page_node;

  page_node = book->children;
  while(page_node) {
    if( ((GtkNotebookPage *)page_node->data)->child == child)
      return TRUE;

    page_node = g_list_next(page_node);
  }

  return FALSE;
}

static void set_page_by_widget(GtkNotebook *book, GtkWidget *child) {
  GList *page_node;
  gint i = 0;

  page_node = book->children;
  while(page_node) {
    if( ((GtkNotebookPage *)page_node->data)->child == child) {
      gtk_notebook_set_page(book, i);
      return;
    }

    i++;
    page_node = g_list_next(page_node);
  }

  return;
}
static GtkWidget *find_item_by_label(GtkMenuShell *shell, gchar *label) {
  GList *child;
  GtkWidget *grandchild;

  child = shell->children;
  while(child) {
    grandchild = GTK_BIN(child->data)->child;
    if( (GTK_IS_LABEL(grandchild)) &&
	(strcmp(GTK_LABEL(grandchild)->label, label) == 0) )
      return GTK_WIDGET(child->data);

    child = g_list_next(child);
  }

  return NULL;
}

static void doc_list_activated_cb(GtkWidget *w, GnomeMDI *mdi) {
  GnomeDocument *doc;
  GtkWindow *window;


#if 0
  doc = gnome_mdi_find_document(mdi, GTK_LABEL(GTK_BIN(w)->child)->label);
#endif
  doc = gtk_object_get_data(GTK_OBJECT(w), "GnomeDocument");

  if( doc && (doc != mdi->active_doc) ) {
    mdi->active_doc = doc;

    if(doc->views)
      mdi->active_view = GTK_WIDGET(doc->views->data);
    else
      gnome_mdi_add_view(mdi, doc);

    window = GTK_WINDOW(VIEW_GET_WINDOW(mdi->active_view));

    if(mdi->flags & GNOME_MDI_NOTEBOOK)
      set_page_by_widget(GTK_NOTEBOOK(GNOME_APP(window)->contents), mdi->active_view);
    
    /* TODO: hmmm... I dont know how to give focus to the window, so that it would
       receive keyboard events */
    gdk_window_raise(GTK_WIDGET(window)->window);
    gtk_window_set_focus(window, mdi->active_view);
    gtk_window_activate_focus(window);
  }
}

static void doc_menus_insert(GtkMenuBar *menubar, GList *menu_list) {
  GList *children;
  GtkWidget *menu;
  gint i;

  i = 0;
  children = GTK_MENU_SHELL(menubar)->children;
  while( children && (gtk_object_get_data(GTK_OBJECT(children->data), "MDIDocumentMenu") == NULL) ) {
    i++;
    children = g_list_next(children);
  }

  if(menu_list)
    gtk_object_set_data(GTK_OBJECT(menu_list->data), "MDIDocMenuFirst", (gpointer)TRUE);

  while(menu_list) {
    menu = GTK_WIDGET(menu_list->data);
    gtk_menu_bar_insert(menubar, menu, i);
    i++;
    menu_list = g_list_remove(menu_list, menu_list->data);
  }

  if(menu)
    gtk_object_set_data(GTK_OBJECT(menu), "MDIDocMenuLast", (gpointer)TRUE);
}

static void doc_menus_remove(GtkMenuBar *menubar) {
  GList *children;
  gboolean more;

  children = GTK_MENU_SHELL(menubar)->children;
  while( children &&
	 (gtk_object_get_data(GTK_OBJECT(children->data), "MDIDocMenuFirst") == NULL) )
    children = g_list_next(children);

  more = (children != NULL);

  while ( children && more ) {
    if(gtk_object_get_data(GTK_OBJECT(children->data), "MDIDocMenuLast"))
      more = FALSE;
    gtk_container_remove(GTK_CONTAINER(menubar), GTK_WIDGET(children->data));
    children = g_list_next(children);
  }
}

static void doc_list_menu_insert(GtkMenuBar *menubar, GtkWidget *doc_menu) {
  GList *children;
  gint i;

  i = 0;
  children = GTK_MENU_SHELL(menubar)->children;
  while( children && (gtk_object_get_data(GTK_OBJECT(children->data), "MDIDocumentList") == NULL) ) {
    i++;
    children = g_list_next(children);
  }

  gtk_menu_bar_insert(menubar, doc_menu, i);

  gtk_object_set_data(GTK_OBJECT(menubar), "MDIDocListMenu", doc_menu);
}

static GtkWidget *doc_list_menu_create(GnomeMDI *mdi) {
  GtkWidget *menu, *item, *title_item;
  GList *doc;

  title_item = gtk_menu_item_new_with_label(_("Documents"));
  gtk_widget_show(title_item);

  if(mdi->documents) {
    menu = gtk_menu_new();

    doc = mdi->documents;
    while(doc) {
      item = gtk_menu_item_new_with_label(GNOME_DOCUMENT(doc->data)->title);
      gtk_signal_connect(GTK_OBJECT(item), "activate",
			 GTK_SIGNAL_FUNC(doc_list_activated_cb), mdi);

      gtk_widget_show(item);

      gtk_menu_append(GTK_MENU(menu), item);

      doc = g_list_next(doc);
    }

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(title_item), menu);
  }

  return title_item;
}

static void doc_list_menu_remove_item(GnomeMDI *mdi, GnomeDocument *doc) {
  GtkWidget *item;
  GtkMenuItem *menu;
  GtkMenuShell *shell;
  GList *list;

  GnomeApp *app;
  GList *app_node;

  app_node = mdi->windows;
  while(app_node) {
    app = GNOME_APP(app_node->data);
    menu = GTK_MENU_ITEM(gtk_object_get_data(GTK_OBJECT(app->menubar), "MDIDocListMenu"));

    shell = GTK_MENU_SHELL(menu->submenu);

    item = find_item_by_label(shell, doc->title);
    
    gtk_container_remove(GTK_CONTAINER(shell), item);
    
    gtk_widget_queue_resize (GTK_WIDGET (shell));

    app_node = g_list_next(app_node);
  }
}

static void doc_list_menu_add_item(GnomeMDI *mdi, GnomeDocument *doc) {
  GtkWidget *item, *submenu;
  GtkMenuItem *menu;
  GnomeApp *app;
  GList *app_node;

  app_node = mdi->windows;
  while(app_node) {
    app = GNOME_APP(app_node->data);

    item = gtk_menu_item_new_with_label(doc->title);
    gtk_signal_connect(GTK_OBJECT(item), "activate",
		       GTK_SIGNAL_FUNC(doc_list_activated_cb), mdi);
    gtk_object_set_data(GTK_OBJECT(item), "GnomeDocument", doc);
    gtk_widget_show(item);

    menu = GTK_MENU_ITEM(gtk_object_get_data(GTK_OBJECT(app->menubar), "MDIDocListMenu"));

    if((submenu = menu->submenu) == NULL) {
      submenu = gtk_menu_new();

      gtk_menu_item_set_submenu(menu, submenu);
    }

    gtk_menu_append(GTK_MENU(submenu), item);

    gtk_widget_queue_resize(GTK_WIDGET(submenu));

    app_node = g_list_next(app_node);
  }
}

static GtkWidget *book_create(GnomeMDI *mdi) {
  GtkWidget *us, *rw;

  us = gtk_notebook_new();

  gnome_app_set_contents(mdi->active_window, us);

  gtk_widget_realize(us);

  gtk_signal_connect(GTK_OBJECT(us), "switch_page",
		     GTK_SIGNAL_FUNC(book_switch_page), mdi);
  gtk_signal_connect(GTK_OBJECT(us), "drag_request_event",
		     GTK_SIGNAL_FUNC(book_drag), mdi);
  gtk_signal_connect (GTK_OBJECT(us), "drop_data_available_event",
		      GTK_SIGNAL_FUNC(book_drop), mdi);
  
  if(mdi->root_window == NULL) {
    rw = gnome_rootwin_new ();
    
    gtk_signal_connect (GTK_OBJECT(rw), "drop_data_available_event",
			GTK_SIGNAL_FUNC(rootwin_drop), mdi);
    
    gtk_widget_realize (rw);
    gtk_widget_dnd_drop_set (rw, TRUE, accepted_drop_types, 1, FALSE);
    gtk_widget_show (rw);
    mdi->root_window = GNOME_ROOTWIN (rw);
  }

  gtk_widget_dnd_drop_set (us, TRUE, accepted_drop_types, 1, FALSE);
  gtk_widget_dnd_drag_set (us, TRUE, possible_drag_types, 1);

  gtk_widget_show(us);

  return us;
}

static void book_add_view(GtkNotebook *book, GtkWidget *view) {
  GnomeDocument *doc;
  GtkWidget *title_label;

  doc = VIEW_GET_DOCUMENT(view);

  title_label = gtk_label_new(doc->title);

  gtk_notebook_append_page(book, view, title_label);

  set_page_by_widget(book, view);  
}

static void book_remove_view(GtkWidget *view, GnomeMDI *mdi) {
  GnomeDocument *doc;

  doc = VIEW_GET_DOCUMENT(view);

  gnome_mdi_remove_view(mdi, view, FALSE);
}

static void book_switch_page(GtkNotebook *book, GtkNotebookPage *page, gint page_num, GnomeMDI *mdi) {
  GnomeApp *app;

#ifdef DEBUG
  printf("book switch page\n");
#endif

  if(page_num == -1)
    return;

  mdi->active_view = GTK_WIDGET(page->child);
  mdi->active_doc = VIEW_GET_DOCUMENT(mdi->active_view);

  app = VIEW_GET_WINDOW(mdi->active_view);

  app_set_active_view(mdi, app, mdi->active_view);

#ifdef DEBUG
  if(mdi->active_doc)
    printf("active doc <%s>\n", mdi->active_doc->title);
#endif
}

static void toplevel_focus(GnomeApp *app, GdkEventFocus *event, GnomeMDI *mdi) {
  /* updates active_view and active_doc when a new toplevel receives focus */
  g_return_if_fail(GNOME_IS_APP(app));

  mdi->active_window = app;

  if(app->contents == NULL)
    return;

  if((mdi->flags & GNOME_MDI_TOPLEVEL) || (mdi->flags & GNOME_MDI_MODAL)) {
    mdi->active_view = app->contents;
    mdi->active_doc = VIEW_GET_DOCUMENT(mdi->active_view);
  }
  else if((mdi->flags & GNOME_MDI_NOTEBOOK) && (GTK_NOTEBOOK(app->contents)->cur_page != NULL)) {
    mdi->active_view = GTK_NOTEBOOK(app->contents)->cur_page->child;
    mdi->active_doc = VIEW_GET_DOCUMENT(mdi->active_view);
  }

#ifdef DEBUG
  if(mdi->active_doc)
    printf("GnomeMDI: active doc <%s>\n", mdi->active_doc->title);
#endif
}

static void book_drag(GtkNotebook *book, GdkEvent *event, GnomeMDI *mdi) {
#ifdef DEBUG
  printf("GnomeMDI: starting page drag!\n");
#endif
  gtk_widget_dnd_data_set(GTK_WIDGET(book), event, book, sizeof(book));
}

static void book_drop(GtkNotebook *book, GdkEvent *event, GnomeMDI *mdi) {
  GtkWidget *view;
  GtkNotebook *old_book;
  GnomeApp *app;

#ifdef DEBUG
  printf("GnomeMDI: page dropped on book!\n");
#endif

  if(strcmp(event->dropdataavailable.data_type, DND_DATA_TYPE) != 0) {
#ifdef DEBUG
    printf("GnomeMDI: unrecognized data dropped!\n");
#endif
    return;
  }

#ifdef 0
  /* can get this to work */
  old_book = GTK_NOTEBOOK(event->dropdataavailable.data);
#endif

  /* so this is a workaround */
  old_book = GTK_NOTEBOOK(mdi->active_window->contents);

  if(book == old_book)
    return;

  if(old_book->cur_page) {
    view = old_book->cur_page->child;
    gtk_container_remove(GTK_CONTAINER(old_book), view);

    book_add_view(book, view);

    if(old_book->cur_page == NULL) {
      app = GNOME_APP(GTK_WIDGET(old_book)->parent->parent);
      mdi->windows = g_list_remove(mdi->windows, app);
      gtk_widget_destroy(GTK_WIDGET(app));
    }
  }
}

static void rootwin_drop(GtkWidget *rw, GdkEvent *event, GnomeMDI *mdi) {
  GtkWidget *view, *new_book;
  GtkNotebook *old_book;
  GnomeApp *app;

#ifdef DEBUG
  printf("GnomeMDI: page dropped on root window!\n");
#endif

  if(strcmp(event->dropdataavailable.data_type, DND_DATA_TYPE) != 0) {
#ifdef DEBUG
    printf("GnomeMDI: unrecognized data dropped!\n");
#endif
    return;
  }

#if 0
  /* dont know why this doesnt work... */
  old_book = GTK_NOTEBOOK(event->dropdataavailable.data);
#endif

  /* so this is a workaround */
  old_book = GTK_NOTEBOOK(mdi->active_window->contents);

  if(g_list_length(old_book->children) == 1)
    return;

  if(old_book->cur_page) {
    view = old_book->cur_page->child;
    gtk_container_remove(GTK_CONTAINER(old_book), view);

    app_create(mdi);

    new_book = book_create(mdi);

    book_add_view(GTK_NOTEBOOK(new_book), view);

    gtk_widget_show(GTK_WIDGET(mdi->active_window));
  }
}

static gint app_close_top(GnomeApp *app, GdkEventAny *event, GnomeMDI *mdi) {
  GnomeDocument *doc = NULL;
  GList *doc_node;
  gint handler_ret = TRUE;

  if(g_list_length(mdi->windows) == 1) {
    /* since this is the last window, we only close it and destroy the MDI if
       ALL the remaining documents can be closed, which we check in advance */
    doc_node = mdi->documents;
    while(doc_node) {
      gtk_signal_emit(GTK_OBJECT(mdi), mdi_signals[REMOVE_DOCUMENT], doc_node->data, &handler_ret);

      if(handler_ret == FALSE)
	return TRUE;

      doc_node = g_list_next(doc_node);
    }

    gnome_mdi_remove_all_documents(mdi, TRUE);
    mdi->windows = g_list_remove(mdi->windows, app);
    gtk_widget_destroy(GTK_WIDGET(app));
    gtk_object_destroy(GTK_OBJECT(mdi));
  }
  else if(app->contents) {
    doc = VIEW_GET_DOCUMENT(app->contents);
    if(DOC_LAST_VIEW(doc)) {

      /* if this is the last view, we should remove the document! */
      if(!gnome_mdi_remove_document(mdi, doc, FALSE))
	return TRUE;
    }
    else
      gnome_mdi_remove_view(mdi, app->contents, FALSE);
  }

  return FALSE;
}

static gint app_close_book(GnomeApp *app, GdkEventAny *event, GnomeMDI *mdi) {
  GnomeDocument *doc;
  GtkWidget *view;
  GList *page_node, *node;
  gint handler_ret = TRUE, all_views;
 
  if(g_list_length(mdi->windows) == 1) {
    node = mdi->documents;
    while(node) {
      gtk_signal_emit(GTK_OBJECT(mdi), mdi_signals[REMOVE_DOCUMENT], node->data, &handler_ret);

      if(handler_ret == FALSE)
	return TRUE;

      node = g_list_next(node);
    }

    gnome_mdi_remove_all_documents(mdi, TRUE);
    mdi->windows = g_list_remove(mdi->windows, app);
    gtk_widget_destroy(GTK_WIDGET(app));
    gtk_object_destroy(GTK_OBJECT(mdi));
  }
  else {
    /* first check if all the documents in this notebook can be removed */
    page_node = GTK_NOTEBOOK(app->contents)->children;
    while(page_node) {
      view = ((GtkNotebookPage *)page_node->data)->child;
      doc = VIEW_GET_DOCUMENT(view);

      page_node = g_list_next(page_node);

      node = doc->views;
      while(node) {
	if(VIEW_GET_WINDOW(node->data) != app)
	  break;

	node = g_list_next(node);
      }

      if(node == NULL) {   /* all the views reside in this GnomeApp */
	gtk_signal_emit(GTK_OBJECT(mdi), mdi_signals[REMOVE_DOCUMENT], doc, &handler_ret);
	if(handler_ret == FALSE)
	  return TRUE;
      }
    }

    /* now actually remove all documents/views! */
    page_node = GTK_NOTEBOOK(app->contents)->children;
    while(page_node) {
      view = ((GtkNotebookPage *)page_node->data)->child;
      doc = VIEW_GET_DOCUMENT(view);
      
      page_node = g_list_next(page_node);

      if(DOC_LAST_VIEW(doc))
	gnome_mdi_remove_document(mdi, doc, TRUE);
      else
	gnome_mdi_remove_view(mdi, view, TRUE);
    }
  }

  return FALSE;
}

static gint app_close_ms(GnomeApp *app, GdkEventAny *event, GnomeMDI *mdi) {
  gint ret = FALSE;

  while(mdi->documents)
    if(!gnome_mdi_remove_document(mdi, GNOME_DOCUMENT(mdi->documents->data), FALSE))
      ret = TRUE;

  gtk_widget_destroy(GTK_WIDGET(app));
  gtk_object_destroy(GTK_OBJECT(mdi));

  return FALSE;
}

static void app_set_title(GnomeMDI *mdi, GnomeApp *app) {
  gchar *fullname;
  GnomeDocument *doc;

  if( (mdi->flags & (GNOME_MDI_MODAL | GNOME_MDI_TOPLEVEL)) &&
      app->contents && (doc = VIEW_GET_DOCUMENT(app->contents)) ) {
    fullname = g_malloc(strlen(mdi->title)+strlen(doc->title)+3);
    strcpy(fullname, mdi->title);
    strcat(fullname, ": ");
    strcat(fullname, doc->title);
    gtk_window_set_title(GTK_WINDOW(app), fullname);
    g_free(fullname);
  }
  else
    gtk_window_set_title(GTK_WINDOW(app), mdi->title);
}

static void app_set_active_view(GnomeMDI *mdi, GnomeApp *app, GtkWidget *view) {
  GList *menu_list = NULL;
  GnomeDocument *doc;

  app_set_title(mdi, app);

  doc_menus_remove(GTK_MENU_BAR(app->menubar));
  if(view) {
    doc = VIEW_GET_DOCUMENT(view);
    gtk_signal_emit_by_name(GTK_OBJECT(doc), "create_menus", view, &menu_list);
    if(menu_list)
      doc_menus_insert(GTK_MENU_BAR(app->menubar), menu_list);
  }
}

static void app_destroy(GnomeApp *app) {
  GnomeUIInfo *ui_info;

  ui_info = gtk_object_get_data(GTK_OBJECT(app), "MDIMenuUIInfo");
  if(ui_info)
    free_ui_info_tree(ui_info);
  ui_info = gtk_object_get_data(GTK_OBJECT(app), "MDIToolbarUIInfo");
  if(ui_info)
    free_ui_info_tree(ui_info);
}

static void app_create(GnomeMDI *mdi) {
  GtkWidget *window;
  GtkWidget *doc_menu;
  GtkMenuBar *menubar = NULL;
  GtkToolbar *toolbar = NULL;
  GtkSignalFunc func;
  GnomeUIInfo *ui_info;

  window = gnome_app_new(mdi->appname, mdi->title);

#if 0
  /* is this really necessary? */
  gtk_window_set_wmclass (GTK_WINDOW (window), mdi->appname, mdi->appname);
#endif

  gtk_widget_realize(window);

  mdi->windows = g_list_append(mdi->windows, window);

  if(mdi->flags & GNOME_MDI_MS)
    func = GTK_SIGNAL_FUNC(app_close_ms);
  if((mdi->flags & GNOME_MDI_TOPLEVEL) || (mdi->flags & GNOME_MDI_MODAL))
    func = GTK_SIGNAL_FUNC(app_close_top);
  if(mdi->flags & GNOME_MDI_NOTEBOOK)
    func = GTK_SIGNAL_FUNC(app_close_book);

  gtk_signal_connect(GTK_OBJECT(window), "delete_event",
		     func, mdi);
  gtk_signal_connect(GTK_OBJECT(window), "focus_in_event",
		     GTK_SIGNAL_FUNC(toplevel_focus), mdi);
  gtk_signal_connect(GTK_OBJECT(window), "destroy",
		     GTK_SIGNAL_FUNC(app_destroy), NULL);

  gtk_window_position (GTK_WINDOW(window), GTK_WIN_POS_MOUSE);

  /* set up menus */
  if(mdi->menu_template) {
    ui_info = copy_ui_info_tree(mdi->menu_template);
    gnome_app_create_menus(GNOME_APP(window), ui_info);
    gtk_object_set_data(GTK_OBJECT(window), "MDIMenuUIInfo", ui_info);

    /* this is a dirty hack for GHex since libgnomeui can't tag items with data */
    gtk_object_set_data(GTK_OBJECT(ui_info[1].widget), "MDIDocumentMenu", (gpointer)TRUE);
    gtk_object_set_data(GTK_OBJECT(ui_info[3].widget), "MDIDocumentList", (gpointer)TRUE);

    menubar = GTK_MENU_BAR(GNOME_APP(window)->menubar);
  }
  else {
    /* we use the (hopefully) supplied create_menus signal handler */
    gtk_signal_emit (GTK_OBJECT (mdi), mdi_signals[CREATE_MENUS], &menubar);

    if(menubar) {
      gtk_widget_show(GTK_WIDGET(menubar));
      gnome_app_set_menus(GNOME_APP(window), GTK_MENU_BAR(menubar));
    }
  }

  /* we add the document list menu to the toolbar */
  if(menubar) {
    doc_menu = doc_list_menu_create(mdi);
    doc_list_menu_insert(GTK_MENU_BAR(menubar), doc_menu);
  }

  /* create toolbar */
  if(mdi->toolbar_template) {
    ui_info = copy_ui_info_tree(mdi->menu_template);
    gnome_app_create_toolbar(GNOME_APP(window), ui_info);
    gtk_object_set_data(GTK_OBJECT(window), "MDIToolbarUIInfo", ui_info);
  }
  else {
    gtk_signal_emit(GTK_OBJECT (mdi), mdi_signals[CREATE_TOOLBAR], &toolbar);

    if(toolbar) {
      gtk_widget_show(GTK_WIDGET(toolbar));
      gnome_app_set_toolbar(GNOME_APP(window), toolbar);
    }
  }

  mdi->active_window = GNOME_APP(window);
  mdi->active_doc = NULL;
  mdi->active_view = NULL;

  gtk_signal_emit(GTK_OBJECT(mdi), mdi_signals[APP_CREATED], window);
}

static void top_add_view(GnomeMDI *mdi, GnomeDocument *doc, GtkWidget *view) {
  GnomeApp *window;
  gboolean app_full;

  if(app_full = (mdi->active_window->contents != NULL) )
    app_create(mdi);

  window = mdi->active_window;

  if(doc && view)
    gnome_app_set_contents(window, view);

  app_set_active_view(mdi, window, view);

  if(!GTK_WIDGET_VISIBLE(window))
    gtk_widget_show(GTK_WIDGET(window));

  if(!app_full) {
    mdi->active_view = view;
    mdi->active_doc = doc;
  }
}

gint gnome_mdi_add_view(GnomeMDI *mdi, GnomeDocument *doc) {
  GtkWidget *title_label, *view;
  GtkWidget *app;
  gint ret = TRUE;

  view = gnome_document_add_view(doc);

  gtk_signal_emit(GTK_OBJECT(mdi), mdi_signals[ADD_VIEW], view, &ret);

  if(ret == FALSE)
    gnome_document_remove_view(doc, view);
  else if(mdi->flags & GNOME_MDI_MS)
    gnome_mdi_pouch_add(GNOME_MDI_POUCH(mdi->active_window->contents), view);
  else if(mdi->flags & GNOME_MDI_NOTEBOOK)
    book_add_view(GTK_NOTEBOOK(mdi->active_window->contents), view);
  else if(mdi->flags & GNOME_MDI_TOPLEVEL)
    /* add a new toplevel unless the remaining one is empty */
    top_add_view(mdi, doc, view);
  else if(mdi->flags & GNOME_MDI_MODAL) {
    /* replace the existing view if there is one */
    if(mdi->active_window->contents) {
      gnome_mdi_remove_view(mdi, mdi->active_window->contents, TRUE);
      mdi->active_window->contents = NULL;
    }

    mdi->active_view = view;
    mdi->active_doc = doc;

    gnome_app_set_contents(mdi->active_window, view);
    app_set_active_view(mdi, mdi->active_window, view);
  }

  return ret;
}

gint gnome_mdi_remove_view(GnomeMDI *mdi, GtkWidget *view, gint force) {
  GtkWidget *parent;
  GnomeApp *window;
  GnomeDocument *doc;
  gint ret = TRUE;

#ifdef DEBUG
  printf("GnomeMDI: removing view!\n");
#endif

  if(!force)
    gtk_signal_emit(GTK_OBJECT(mdi), mdi_signals[REMOVE_VIEW], view, &ret);

  if(ret == FALSE)
    return FALSE;

  if(view == mdi->active_view) {
    mdi->active_view = NULL;
    mdi->active_doc = NULL;
  }

  doc = VIEW_GET_DOCUMENT(view);

  parent = view->parent;

  window = VIEW_GET_WINDOW(view);

  if(mdi->flags & GNOME_MDI_MS)
    gnome_mdi_pouch_remove(GNOME_MDI_POUCH(GTK_WIDGET(parent)->parent), view);
  else
    gtk_container_remove(GTK_CONTAINER(parent), view);

  if(mdi->flags & (GNOME_MDI_TOPLEVEL | GNOME_MDI_MODAL)) {
    window->contents = NULL;

    /* if this is NOT the last toplevel, we destroy it */
    if(g_list_length(mdi->windows) > 1) {
      mdi->windows = g_list_remove(mdi->windows, window);
      gtk_widget_destroy(GTK_WIDGET(window));
    }
    else
      app_set_active_view(mdi, window, NULL);
  }
  else if( (mdi->flags & GNOME_MDI_NOTEBOOK) &&
	   (GTK_NOTEBOOK(parent)->cur_page == NULL) &&
	   (g_list_length(mdi->windows) > 1) ) {
    /* if this is NOT the last toplevel, destroy it! */
    mdi->windows = g_list_remove(mdi->windows, window);
    gtk_widget_destroy(GTK_WIDGET(window));
  }
  else if(mdi->active_doc == NULL)
    app_set_active_view(mdi, window, NULL);

  /* remove this view from the document's view list */
  gnome_document_remove_view(doc, view);

  return TRUE;
}

gint gnome_mdi_add_document(GnomeMDI *mdi, GnomeDocument *doc) {
  GtkWidget *view;
  gint ret = TRUE;

  g_return_if_fail(mdi != NULL);
  g_return_if_fail(GNOME_IS_MDI(mdi));
  g_return_if_fail(doc != NULL);
  g_return_if_fail(GNOME_IS_DOCUMENT(doc));

  gtk_signal_emit(GTK_OBJECT(mdi), mdi_signals[ADD_DOCUMENT], doc, &ret);

  if(ret == FALSE)
    return FALSE;

  doc->owner = mdi;

#ifdef 0
  gnome_mdi_add_view(mdi, doc);
#endif

  mdi->documents = g_list_append(mdi->documents, doc);

  doc_list_menu_add_item(mdi, doc);

  return TRUE;
}

gint gnome_mdi_remove_document(GnomeMDI *mdi, GnomeDocument *doc, gint force) {
  GtkWidget *view, *parent;
  gint ret = TRUE;

  g_return_if_fail(mdi != NULL);
  g_return_if_fail(GNOME_IS_MDI(mdi));
  g_return_if_fail(doc != NULL);
  g_return_if_fail(GNOME_IS_DOCUMENT(doc));

  g_return_if_fail(doc->owner == mdi);

  if(!force)
    gtk_signal_emit(GTK_OBJECT(mdi), mdi_signals[REMOVE_DOCUMENT], doc, &ret);

  if(ret == FALSE)
    return FALSE;

  while(doc->views)
    gnome_mdi_remove_view(mdi, GTK_WIDGET(doc->views->data), TRUE);

#ifdef DEBUG
  printf("GnomeMDI: removing document!\n");
#endif

  mdi->documents = g_list_remove(mdi->documents, doc);

  if(mdi->active_doc == doc) {
    mdi->active_doc = NULL;
    mdi->active_view = NULL;
  }

  doc_list_menu_remove_item(mdi, doc);

  gtk_object_destroy(GTK_OBJECT(doc));

  return TRUE;
}

gint gnome_mdi_remove_all_documents(GnomeMDI *mdi, gint force) {
  GList *doc_node;
  GnomeDocument *doc;
  gint handler_ret = TRUE;

  if(!force) {
    doc_node = mdi->documents;
    while(doc_node) {
      gtk_signal_emit(GTK_OBJECT(mdi), mdi_signals[REMOVE_DOCUMENT], doc_node->data, &handler_ret);

      if(handler_ret == FALSE)
	return FALSE;

      doc_node = g_list_next(doc_node);
    }
  }

  doc_node = mdi->documents;
  while(doc_node) {
    doc = GNOME_DOCUMENT(doc_node->data);
    doc_node = g_list_next(doc_node);
    gnome_mdi_remove_document(mdi, doc, TRUE);
  }

  return TRUE;
}

static GnomeDocument *gnome_mdi_find_document(GnomeMDI *mdi, gchar *title) {
  GList *doc;

  doc = mdi->documents;
  while(doc) {
    if(strcmp(GNOME_DOCUMENT(doc->data)->title, title) == 0)
      return GNOME_DOCUMENT(doc->data);

    doc = g_list_next(doc);
  }

  return NULL;
}

void gnome_mdi_set_mode(GnomeMDI *mdi, gint mode) {
  GtkWidget *us, *view;
  GnomeDocument *doc;
  GList *doc_node, *view_node;

  if(mode & mdi->flags)
    return;

  /* remove all views from their parents */
  doc_node = mdi->documents;
  while(doc_node) {
    doc = GNOME_DOCUMENT(doc_node->data);
    view_node = doc->views;
    while(view_node) {
      view = GTK_WIDGET(view_node->data);

      if(mdi->flags & (GNOME_MDI_TOPLEVEL | GNOME_MDI_MODAL))
	VIEW_GET_WINDOW(view)->contents = NULL;

      gtk_container_remove(GTK_CONTAINER(view->parent), view);

      view_node = g_list_next(view_node);

      /* if we are to change mode to MODAL, destroy all views except the active one */
      if( (mdi->flags & GNOME_MDI_MODAL)  && (view != mdi->active_view) )
	gnome_document_remove_view(doc, view);
    }
    doc_node = g_list_next(doc_node);
  }

  /* remove all GnomeApps */
  while(mdi->windows) {
    gtk_widget_destroy(GTK_WIDGET(mdi->windows->data));
    mdi->windows = g_list_remove(mdi->windows, mdi->windows->data);
  }

  mdi->active_window = NULL;

  mdi->flags &= ~GNOME_MDI_MODE_FLAGS;
  mdi->flags |= mode;

  app_create(mdi);

  if(mdi->flags & GNOME_MDI_MS) {
    us = gnome_mdi_pouch_new();
    gtk_widget_show(us);
    gnome_app_set_contents(mdi->active_window, us);
  }
  else if(mdi->flags & GNOME_MDI_NOTEBOOK)
    book_create(mdi);

  /* re-implant views in proper containers */
  doc_node = mdi->documents;
  while(doc_node) {
    doc = GNOME_DOCUMENT(doc_node->data);
    view_node = doc->views;
    while(view_node) {
      view = GTK_WIDGET(view_node->data);

      if(mdi->flags & GNOME_MDI_MS)
	gnome_mdi_pouch_add(GNOME_MDI_POUCH(mdi->active_window->contents), view);
      else if(mdi->flags & GNOME_MDI_NOTEBOOK)
	book_add_view(GTK_NOTEBOOK(mdi->active_window->contents), view);
      else if(mdi->flags & GNOME_MDI_TOPLEVEL)
	/* add a new toplevel unless the remaining one is empty */
	top_add_view(mdi, doc, view);
      else if(mdi->flags & GNOME_MDI_MODAL) {
	/* replace the existing view if there is one */
	if(mdi->active_window->contents) {
	  gnome_mdi_remove_view(mdi, mdi->active_window->contents, TRUE);
	  mdi->active_window->contents = NULL;
	}
	mdi->active_view = view;
	mdi->active_doc = doc;
	gnome_app_set_contents(mdi->active_window, view);
	app_set_active_view(mdi, mdi->active_window, view);
      }

      view_node = g_list_next(view_node);
    }
    doc_node = g_list_next(doc_node);
  }  

  if(!GTK_WIDGET_VISIBLE(mdi->active_window))
    gtk_widget_show(GTK_WIDGET(mdi->active_window));
}

GnomeDocument *gnome_mdi_active_document(GnomeMDI *mdi) {
  if(mdi->active_view)
    return(VIEW_GET_DOCUMENT(mdi->active_view));

  return NULL;
}

static GtkMenuBar *gnome_mdi_create_menus(GnomeMDI *mdi) {
  GtkMenuBar *menubar;

  gtk_signal_emit(GTK_OBJECT(mdi), mdi_signals[CREATE_MENUS], &menubar);

  return menubar;
}

void gnome_mdi_set_menu_template(GnomeMDI *mdi, GnomeUIInfo *menu_tmpl) {
  mdi->menu_template = menu_tmpl;
}

void gnome_mdi_set_toolbar_template(GnomeMDI *mdi, GnomeUIInfo *tbar_tmpl) {
  mdi->toolbar_template = tbar_tmpl;
}

/* the app-helper support routines */
static GnomeUIInfo *copy_ui_info_tree(GnomeUIInfo source[]) {
  GnomeUIInfo *copy;
  int i, count;

#ifdef DEBUG
  printf("copying UI info tree!\n");
#endif

  for(count = 0; source[count].type != GNOME_APP_UI_ENDOFINFO; count++)
    ;

  count++;

  copy = g_malloc(count*sizeof(GnomeUIInfo));

  if(copy == NULL) {
    g_warning("Could not allocate new GnomeUIInfo");
    return NULL;
  }

  memcpy(copy, source, count*sizeof(GnomeUIInfo));

  for(i = 0; i < count; i++) {
    if( (source[i].type == GNOME_APP_UI_SUBTREE) ||
	(source[i].type == GNOME_APP_UI_RADIOITEMS) )
      copy[i].moreinfo = copy_ui_info_tree(source[i].moreinfo);
  }

  return copy;
}

static void free_ui_info_tree(GnomeUIInfo *root) {
  int count;

#ifdef DEBUG
  printf("freeing UI Info tree!\n");
#endif

  for(count = 0; root[count].type != GNOME_APP_UI_ENDOFINFO; count++)
    if( (root[count].type == GNOME_APP_UI_SUBTREE) ||
	(root[count].type == GNOME_APP_UI_RADIOITEMS) )
      free_ui_info_tree(root[count].moreinfo);

  g_free(root);
}
