/*
 * hex-document.h
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */

#ifndef __HEX_DOCUMENT_H__
#define __HEX_DOCUMENT_H__

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgnome/libgnome.h>

#include <libgnomeui/libgnomeui.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define HEX_DOCUMENT(obj)          GTK_CHECK_CAST (obj, hex_document_get_type (), HexDocument)
#define HEX_DOCUMENT_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, hex_document_get_type (), HexDocumentClass)
#define IS_HEX_DOCUMENT(obj)       GTK_CHECK_TYPE (obj, hex_document_get_type ())

typedef struct _HexDocument       HexDocument;
typedef struct _HexDocumentClass  HexDocumentClass;
typedef struct _HexChangeData     HexChangeData;

struct _HexChangeData {
  gint start, end;
};

struct _HexDocument
{
  GnomeMDIChild mdi_child;

  gchar *file_name;
  gchar *path_end;
  FILE *file;

  guchar *buffer;
  guint buffer_size;

  gboolean changed;

  HexChangeData change_data;
};

struct _HexDocumentClass
{
  GnomeMDIChildClass parent_class;

  void (*document_changed)(HexDocument *, gpointer);
};

HexDocument *hex_document_new(gchar *);
void hex_document_set_data(HexDocument *, guint, guint, guchar *);
void hex_document_set_byte(HexDocument *, guchar, guint);
gint hex_document_read(HexDocument *doc);
gint hex_document_write(HexDocument *doc);
void hex_document_changed(HexDocument *doc, gpointer change_data);
gint find_string_forward(HexDocument *doc, guint start, guchar *what, gint len, guint *found);
gint find_string_backward(HexDocument *doc, guint start, guchar *what, gint len, guint *found);

#endif /* __HEX_DOCUMENT_H__ */

