#include <gnome.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgnome/libgnome.h>

#include <libgnomeui/libgnomeui.h>

#include <hex-document.h>

#include "ghex.h"
#include "gtkhex.h"
#include "callbacks.h"

#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

static void hex_document_class_init        (HexDocumentClass *klass);
static void hex_document_init              (HexDocument *);
static GtkWidget *hex_document_create_view (GnomeDocument *);
static void hex_document_destroy           (GtkObject *);

#ifndef USE_APP_HELPER
static GList *hex_document_create_menus    (GnomeDocument *, GtkWidget *);
#endif

GnomeUIInfo edit_menu[] = {
  { GNOME_APP_UI_ITEM, "Find...", NULL, find_cb, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SEARCH, 'F',
    GDK_CONTROL_MASK, NULL },
  { GNOME_APP_UI_ITEM, "Replace...", NULL, find_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL, 'R',
    GDK_CONTROL_MASK, NULL },
  { GNOME_APP_UI_SEPARATOR, NULL, NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
  { GNOME_APP_UI_ITEM, "Goto Byte...", NULL, find_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL, 'G',
    GDK_CONTROL_MASK, NULL },
  { GNOME_APP_UI_ENDOFINFO }
};

GnomeUIInfo doc_menu[] = {
  { GNOME_APP_UI_SUBTREE, "Edit", NULL, edit_menu, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
  { GNOME_APP_UI_ENDOFINFO }
};

guint group_type[3] = {
  GROUP_BYTE,
  GROUP_WORD,
  GROUP_LONG,
};

gchar *group_type_label[3] = {
  _("Single Bytes"),
  _("Words"),
  _("Longwords"),
};

static GnomeDocumentClass *parent_class = NULL;

guint hex_document_get_type () {
  static guint document_type = 0;

  if (!document_type) {
    GtkTypeInfo document_info = {
      "HexDocument",
      sizeof (HexDocument),
      sizeof (HexDocumentClass),
      (GtkClassInitFunc) hex_document_class_init,
      (GtkObjectInitFunc) hex_document_init,
      (GtkArgSetFunc) NULL,
      (GtkArgGetFunc) NULL,
    };
    
    document_type = gtk_type_unique (gnome_document_get_type (), &document_info);
  }
  
  return document_type;
}

static GtkWidget *hex_document_create_view(GnomeDocument *doc) {
  GtkWidget *new_view;

  new_view = gtk_hex_new(HEX_DOCUMENT(doc));

  /* TODO: perhaps it would be nicer to put such stuff in the MDI add_view signal handler */
  gtk_hex_set_group_type(GTK_HEX(new_view), def_group_type);
  gtk_hex_set_font(GTK_HEX(new_view), def_font);

  gtk_widget_show(new_view);

  return new_view;
}

static GtkWidget *create_group_type_menu(GtkHex *gh) {
  GtkWidget *menu;
  GtkWidget *menuitem;
  GSList *group;
  int i;

  menu = gtk_menu_new ();
  group = NULL;
  
  for(i = 0; i < 3; i++) {
    menuitem = gtk_radio_menu_item_new_with_label (group, group_type_label[i]);
    if((1L << i) == gh->group_type)
      gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
    gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(set_group_type_cb), &group_type[i]);
    group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM(menuitem));
    gtk_menu_append (GTK_MENU (menu), menuitem);
    gtk_widget_show (menuitem);
  }

  return menu;
}

#ifndef USE_APP_HELPER
static GList *hex_document_create_menus(GnomeDocument *doc, GtkWidget *view) {
  GList *menu_list;
  GtkWidget *menu, *w;
  GtkAcceleratorTable *accel = NULL;

  menu_list = NULL;

  /* the Edit menu */
  menu = gtk_menu_new();

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_SEARCH, _("Find..."));
  gtk_widget_show(w);
  gtk_widget_install_accelerator(w, accel, "activate",
				 'F', GDK_CONTROL_MASK);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(find_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_BLANK,_("Replace..."));
  gtk_widget_show(w);
  gtk_widget_install_accelerator(w, accel, "activate",
				 'R', GDK_CONTROL_MASK);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(replace_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gtk_menu_item_new();
  gtk_widget_show(w);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_BLANK,_("Goto byte"));
  gtk_widget_show(w);
  gtk_widget_install_accelerator(w, accel, "activate",
				 'G', GDK_CONTROL_MASK);
  gtk_signal_connect(GTK_OBJECT(w), "activate",
		     GTK_SIGNAL_FUNC(jump_cb), NULL);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gtk_menu_item_new();
  gtk_widget_show(w);
  gtk_menu_append(GTK_MENU(menu), w);

  w = gnome_stock_menu_item(GNOME_STOCK_MENU_BLANK, _("Group Data As"));
  gtk_widget_show(w);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), create_group_type_menu(GTK_HEX(view)));
  gtk_menu_append(GTK_MENU(menu), w);

  w = gtk_menu_item_new_with_label(_("Edit"));
  gtk_widget_show(w);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), menu);
  menu_list = g_list_append(menu_list, w);

  return menu_list;
}  
#endif

static void hex_document_destroy(GtkObject *obj) {
  HexDocument *hex;

  hex = HEX_DOCUMENT(obj);

  if(hex->buffer)
    free(hex->buffer);

  if(hex->file_name)
    free(hex->file_name);

  if(GTK_OBJECT_CLASS(parent_class)->destroy)
    (* GTK_OBJECT_CLASS(parent_class)->destroy)(GTK_OBJECT(hex));
}

static void hex_document_class_init (HexDocumentClass *class) {
  GtkObjectClass *object_class;
  GnomeDocumentClass *doc_class;

  object_class = (GtkObjectClass*)class;
  doc_class = GNOME_DOCUMENT_CLASS(class);

  object_class->destroy = hex_document_destroy;

  doc_class->create_view = hex_document_create_view;
#ifndef USE_APP_HELPER
  doc_class->create_menus = hex_document_create_menus;
#endif

  parent_class = gtk_type_class (gnome_document_get_type ());
}

void hex_document_set_byte(HexDocument *document, guchar val, guint offset) {
  GNOME_DOCUMENT(document)->changed = TRUE;

  if((offset >= 0) && (offset < document->buffer_size))
    document->buffer[offset] = val;

  document->change_data.start = offset;
  document->change_data.end = offset;

  gnome_document_changed(GNOME_DOCUMENT(document), &document->change_data);
}

void hex_document_set_data(HexDocument *document, guint offset, guint len, guchar *data) {
  guint i;

  GNOME_DOCUMENT(document)->changed = TRUE;

  for(i = 0; (offset < document->buffer_size) && (i < len); offset++, i++)
    document->buffer[offset] = data[i];

  document->change_data.start = offset;
  document->change_data.end = offset + i - 1;

  gnome_document_changed(GNOME_DOCUMENT(document), &document->change_data);
}

static void hex_document_init (HexDocument *document) {
  document->buffer = NULL;
  document->buffer_size = 0;
#ifdef USE_APP_HELPER
  GNOME_DOCUMENT(document)->menu_template = doc_menu;
#else
  GNOME_DOCUMENT(document)->menu_template = NULL;
#endif
}

HexDocument *hex_document_new(gchar *name) {
  HexDocument *document;

  struct stat stats;
  int i;

  /* hopefully using stat() works for all flavours of UNIX...
     don't know for sure, though */
  if(!stat(name, &stats)) {
    if(document = gtk_type_new (hex_document_get_type ())) {
      document->buffer_size = stats.st_size;

      if((document->buffer = (guchar *)malloc(document->buffer_size)) != NULL) {
	if((document->file_name = (gchar *)strdup(name)) != NULL) {
	  for(i = strlen(document->file_name); (i >= 0) && (document->file_name[i] != '/'); i--)
	    ;

	  if(document->file_name[i] == '/')
	    document->path_end = &document->file_name[i+1];
	  else
	    document->path_end = document->file_name;

	  if((document->file = fopen(name, "r")) != NULL) {
	    document->buffer_size = fread(document->buffer, 1, document->buffer_size, document->file);
	    gnome_document_set_title(GNOME_DOCUMENT(document), document->path_end);
	    fclose(document->file);
	    document->file = 0;
	    return document;
	  }
	  free(document->file_name);
	}
	free(document->buffer);
      }
      gtk_object_destroy(GTK_OBJECT(document));
    }
  }

  return document;
}
  
gint hex_document_read(HexDocument *doc) {
  if((doc->file = fopen(doc->file_name, "r")) != NULL) {
    doc->buffer_size = fread(doc->buffer, 1, doc->buffer_size, doc->file);
    fclose(doc->file);
    doc->file = 0;
    return 0;
  }

  doc->change_data.start = 0;
  doc->change_data.end = doc->buffer_size - 1;

  gnome_document_changed(GNOME_DOCUMENT(doc), &doc->change_data);

  return 1;
}

gint hex_document_write(HexDocument *doc) {
  if((doc->file = fopen(doc->file_name, "w")) != NULL) {
    fwrite(doc->buffer, 1, doc->buffer_size, doc->file);
    fclose(doc->file);
    doc->file = 0;
    return 0;
  }
  return 1;
}

gint compare_data(guchar *s1, guchar *s2, gint len) {
  while(len > 0) {
    if((*s1) != (*s2))
      return ((*s1) - (*s2));
    s1++;
    s2++;
    len--;
  }

  return 0;
}

/*
 * search functions return 0 if the string was successfully found and != 0 otherwise;
 * the offset of the first occurence of the searched-for string is returned in *found
 */
gint find_string_forward(HexDocument *doc, guint start, guchar *what, gint len, guint *found) {
  guint pos;

  pos = start;
  while(pos < doc->buffer_size) {
    if(compare_data(&doc->buffer[pos], what, len) == 0) {
      *found = pos;
      return 0;
    }
    pos++;
  }

  return 1;
}

gint find_string_backward(HexDocument *doc, guint start, guchar *what, gint len, guint *found) {
  guint pos;

  pos = start;
  while(pos >= 0) {
    if(compare_data(&doc->buffer[pos], what, len) == 0) {
      *found = pos;
      return 0;
    }
    pos--;
  }

  return 1;
}
