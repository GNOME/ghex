/*
 * Copyright 2002 Sun Microsystems Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkwidget.h>
#include <sys/types.h>
#include <bonobo.h>
#include <libgail-util/gailmisc.h>
#include <atk/atkobject.h>

#include <accessiblegtkhex.h>
#include <gtkhex.h>

static void accessible_gtk_hex_class_init       (AccessibleGtkHexClass *klass);

static gint accessible_gtk_hex_get_n_children   (AtkObject *obj);

static void accessible_gtk_hex_real_initialize  (AtkObject *obj, gpointer data);

static void accessible_gtk_hex_finalize         (GObject *object);


/* AtkText Interfaces */

static void atk_text_interface_init                        (AtkTextIface *iface);

static gchar* accessible_gtk_hex_get_text                  (AtkText *text,
							    gint start_pos,
							    gint end_pos);

static gunichar accessible_gtk_hex_get_character_at_offset (AtkText *text,
							    gint    offset);

static gchar* accessible_gtk_hex_get_text_before_offset    (AtkText *text,
							    gint    offset,
							    AtkTextBoundary bound_type,
							    gint *start_offset,
							    gint *end_offset);

static gchar* accessible_gtk_hex_get_text_after_offset     (AtkText *text,
							    gint offset,
							    AtkTextBoundary bound_type,
							    gint *start_offset,
							    gint *end_offset);

static gchar* accessible_gtk_hex_get_text_at_offset        (AtkText *text,
							    gint offset,
							    AtkTextBoundary  bound_type,
							    gint *start_offset,
							    gint *end_offset);


static gint accessible_gtk_hex_get_caret_offset            (AtkText *text);


static gint accessible_gtk_hex_get_character_count         (AtkText *text);


/* AtkEditable Text Interfaces */

static void atk_editable_text_interface_init      (AtkEditableTextIface *iface);

static void accessible_gtk_hex_set_text_contents  (AtkEditableText *text,
						   const gchar *string);

static void accessible_gtk_hex_insert_text        (AtkEditableText *text,
						   const gchar *string,
						   gint length,
						   gint *position);


static void accessible_gtk_hex_delete_text        (AtkEditableText *text,
						   gint start_pos,
						   gint end_pos);


/* Callbacks */

static void _accessible_gtk_hex_changed_cb      (GtkHex *gtkhex);

static void _accessible_gtk_hex_cursor_moved_cb (GtkHex *gtkhex);


static gpointer parent_class = NULL;

GType
accessible_gtk_hex_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static GTypeInfo tinfo =
		{
			0, /* class size */
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) accessible_gtk_hex_class_init, /*
class init */
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			0, /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc)NULL, /* instance init */
			NULL /* value table */
		};

		static const GInterfaceInfo atk_text_info =
		{
			(GInterfaceInitFunc) atk_text_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		static const GInterfaceInfo atk_editable_text_info =
		{
			(GInterfaceInitFunc) atk_editable_text_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		}; 

		/*
		 * Figure out the size of the class and instance
		 * we are deriving from
		 */
		AtkObjectFactory *factory;
		GType derived_type;
		GTypeQuery query;
		GType derived_atk_type;

		derived_type = g_type_parent (gtk_hex_get_type());
		factory = atk_registry_get_factory (atk_get_default_registry(),
							derived_type);
		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);
		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		type = g_type_register_static (derived_atk_type,
						"AccessibleGtkHex", &tinfo, 0);

		g_type_add_interface_static (type, ATK_TYPE_TEXT, &atk_text_info);

		g_type_add_interface_static (type, ATK_TYPE_EDITABLE_TEXT,
						&atk_editable_text_info); 
       
	}

	return type;
}

static void
accessible_gtk_hex_class_init (AccessibleGtkHexClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	g_return_if_fail (class != NULL);
	parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize = accessible_gtk_hex_finalize;

	class->get_n_children = accessible_gtk_hex_get_n_children;
	class->initialize = accessible_gtk_hex_real_initialize;
}

static void
accessible_gtk_hex_real_initialize (AtkObject *obj,
				    gpointer  data)
{
	AccessibleGtkHex *accessible_gtk_hex;
	GtkHex *gtk_hex;
	GtkAccessible *accessible;
	GtkWidget *widget;

	g_return_if_fail (obj != NULL);

	ATK_OBJECT_CLASS(parent_class)->initialize(obj, data);

	accessible_gtk_hex = ACCESSIBLE_GTK_HEX(obj);

	gtk_hex = GTK_HEX (data);
	g_return_if_fail (gtk_hex != NULL);

	accessible = GTK_ACCESSIBLE (obj);
	g_return_if_fail (accessible != NULL);
	accessible->widget = GTK_WIDGET (gtk_hex);

	accessible_gtk_hex->textutil = gail_text_util_new();

	widget = GTK_WIDGET (data);

	g_signal_connect (G_OBJECT (gtk_hex), "data_changed",
			  G_CALLBACK (_accessible_gtk_hex_changed_cb), NULL);

	g_signal_connect (G_OBJECT (gtk_hex), "cursor_moved",
			  G_CALLBACK (_accessible_gtk_hex_cursor_moved_cb), NULL);

}

AtkObject *
accessible_gtk_hex_new (GtkWidget *widget)
{
	GObject *object;
	AtkObject *accessible;

	object = g_object_new (ACCESSIBLE_TYPE_GTK_HEX, NULL);
	g_return_val_if_fail (object != NULL, NULL);

	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, widget);

	accessible->role = ATK_ROLE_TEXT;

	return accessible;
}

static gint
accessible_gtk_hex_get_n_children (AtkObject* obj)
{
	return 0;
}

static void
accessible_gtk_hex_finalize (GObject *object)
{
	AccessibleGtkHex *gtkhex = ACCESSIBLE_GTK_HEX(object);

	g_object_unref (gtkhex->textutil);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
atk_text_interface_init (AtkTextIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->get_text = accessible_gtk_hex_get_text;
	iface->get_text_before_offset = accessible_gtk_hex_get_text_before_offset;
	iface->get_text_after_offset = accessible_gtk_hex_get_text_after_offset;
	iface->get_text_at_offset = accessible_gtk_hex_get_text_at_offset;
	iface->get_character_count = accessible_gtk_hex_get_character_count;
	iface->get_character_at_offset = accessible_gtk_hex_get_character_at_offset;
	iface->get_caret_offset = accessible_gtk_hex_get_caret_offset;
}

static gchar*
accessible_gtk_hex_get_text (AtkText *text,
			     gint start_pos,
			     gint end_pos)
{

	AccessibleGtkHex *access_gtk_hex;
	GtkWidget *widget;
	GtkHex *gtk_hex ;
	gchar *str, *utf8;
        
	widget = GTK_ACCESSIBLE (text)->widget;

	access_gtk_hex = ACCESSIBLE_GTK_HEX (text);
	g_return_val_if_fail (access_gtk_hex->textutil, NULL);

	gtk_hex = GTK_HEX (widget);

	if (gtk_hex->active_view == VIEW_ASCII)
	{
		str = g_malloc (gtk_hex->document->file_size);
		format_ablock (gtk_hex, str, 0, gtk_hex->document->file_size);
	}

	if (gtk_hex->active_view == VIEW_HEX)
	{
		str = g_malloc (gtk_hex->document->file_size*3);
		format_xblock (gtk_hex, str, 0,gtk_hex->document->file_size);
	}

	utf8 = g_locale_to_utf8 (str, -1, NULL, NULL, NULL);
	gail_text_util_text_setup (access_gtk_hex->textutil, utf8);

	g_free (str);
	g_free (utf8);

	return gail_text_util_get_substring (access_gtk_hex->textutil,
					     start_pos, end_pos);
}


static gchar*
accessible_gtk_hex_get_text_before_offset (AtkText *text,
					   gint offset,
					   AtkTextBoundary boundary_type,
					   gint *start_offset,
					   gint *end_offset)
{
	return gail_text_util_get_text (ACCESSIBLE_GTK_HEX (text)->textutil,
					NULL, GAIL_BEFORE_OFFSET,
					boundary_type, offset,
					start_offset, end_offset);
}


static gchar*
accessible_gtk_hex_get_text_after_offset (AtkText *text,
					  gint offset,
					  AtkTextBoundary boundary_type,
					  gint *start_offset,
					  gint *end_offset)
{
	return gail_text_util_get_text (ACCESSIBLE_GTK_HEX (text)->textutil,
					NULL, GAIL_AFTER_OFFSET,
					boundary_type, offset,
					start_offset, end_offset);
}


static gchar*
accessible_gtk_hex_get_text_at_offset (AtkText *text,
				       gint offset,
				       AtkTextBoundary boundary_type,
				       gint *start_offset,
				       gint *end_offset)
{
	return gail_text_util_get_text (ACCESSIBLE_GTK_HEX (text)->textutil,
					NULL, GAIL_AT_OFFSET,
					boundary_type, offset,
					start_offset, end_offset);
}

static gint
accessible_gtk_hex_get_character_count (AtkText *text)
{

	GtkWidget *widget;
	GtkHex *gtk_hex ;

	widget = GTK_ACCESSIBLE (text)->widget;
	
	gtk_hex = GTK_HEX (widget);

	return gtk_hex->document->file_size;
}

static gunichar
accessible_gtk_hex_get_character_at_offset (AtkText *text,
					    gint offset)
{

	GtkWidget *widget;
	GtkHex *gtk_hex ;
	gchar str[2];
	gunichar c;
	
	widget = GTK_ACCESSIBLE (text)->widget;
	gtk_hex = GTK_HEX (widget);

	if (gtk_hex->active_view == VIEW_ASCII) {
		format_ablock (gtk_hex, str, offset, offset+1);
		c = g_utf8_get_char_validated (str, 1);
	}
	if (gtk_hex->active_view == VIEW_HEX) {
		format_xbyte (gtk_hex, offset, str);
		c = g_utf8_get_char_validated (str, 2);
	}

	return c;
}

static gint
accessible_gtk_hex_get_caret_offset (AtkText *text)
{
	GtkHex *gtk_hex;
	GtkWidget *widget;

	widget = GTK_ACCESSIBLE (text)->widget;
	g_return_val_if_fail (widget != NULL, 0);

	gtk_hex = GTK_HEX (widget);

	return gtk_hex_get_cursor (gtk_hex);
}

static void
atk_editable_text_interface_init (AtkEditableTextIface *iface)
{
	g_return_if_fail (iface != NULL);
	iface->set_text_contents = accessible_gtk_hex_set_text_contents;
	iface->insert_text = accessible_gtk_hex_insert_text;
	iface->delete_text = accessible_gtk_hex_delete_text;
}

static void
accessible_gtk_hex_set_text_contents (AtkEditableText *text,
				      const gchar *string)
{
	GtkHex *gtkhex;
	GtkWidget *widget;
	gint len;

	widget = GTK_ACCESSIBLE (text)->widget;
	g_return_if_fail (widget != NULL);
	gtkhex = GTK_HEX (widget);

	len = g_utf8_strlen (string, -1);

	hex_document_delete_data (gtkhex->document, 0, gtkhex->document->file_size, FALSE);
	hex_document_set_data (gtkhex->document, 0, len, 0, (guchar *)string, TRUE);
}

static void
accessible_gtk_hex_insert_text (AtkEditableText *text,
				const gchar *string,
				gint length,
				gint *position)
{
	GtkHex *gtkhex;
	GtkWidget *widget;

	widget = GTK_ACCESSIBLE (text)->widget;
	g_return_if_fail (widget != NULL);

	gtkhex = GTK_HEX (widget);

	hex_document_set_data (gtkhex->document, *position, length, 0, (guchar *)string, TRUE);

}


static void
accessible_gtk_hex_delete_text (AtkEditableText *text,
				gint start_pos,
				gint end_pos)
{
	GtkHex *gtkhex;
	GtkWidget *widget;

	widget = GTK_ACCESSIBLE (text)->widget;

	g_return_if_fail (widget != NULL);

	gtkhex = GTK_HEX (widget);

	hex_document_delete_data (gtkhex->document, start_pos, end_pos-start_pos, FALSE);

}

static void
_accessible_gtk_hex_changed_cb (GtkHex *gtkhex)
{
	AtkObject *accessible;
	AccessibleGtkHex *accessible_gtk_hex;
	gchar *str, *utf8;

	accessible = gtk_widget_get_accessible (GTK_WIDGET (gtkhex));

	accessible_gtk_hex = ACCESSIBLE_GTK_HEX (accessible);

	g_signal_emit_by_name (accessible, "text_changed::delete");
	g_signal_emit_by_name (accessible, "text_changed::insert");

	if (gtkhex->active_view == VIEW_ASCII)
	{
		str = g_malloc (gtkhex->document->file_size);
		format_ablock (gtkhex, str, 0, gtkhex->document->file_size);
	}

	if (gtkhex->active_view == VIEW_HEX)
	{
		str = g_malloc (gtkhex->document->file_size*3);
		format_xblock (gtkhex, str, 0, gtkhex->document->file_size);
	}

	utf8 = g_locale_to_utf8 (str, -1, NULL, NULL, NULL);
	gail_text_util_text_setup (accessible_gtk_hex->textutil, str);

	g_free (str);
	g_free (utf8); 
}



static void
_accessible_gtk_hex_cursor_moved_cb (GtkHex *gtkhex)
{	

	AtkObject *accessible;
	
	accessible = gtk_widget_get_accessible (GTK_WIDGET (gtkhex));

	g_signal_emit_by_name (G_OBJECT(accessible), "text_caret_moved");

}
