/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ghex-notebook-tab.c - GHex notebook tab

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

#include "ghex-notebook-tab.h"

enum signal_types {
	CLOSED,
	LAST_SIGNAL
};

struct _GHexNotebookTab
{
	GtkWidget parent_instance;
	
	GtkWidget *label;
	GtkWidget *close_btn;
	GtkHex *gh;				/* GtkHex widget activated when tab is clicked */
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (GHexNotebookTab, ghex_notebook_tab, GTK_TYPE_WIDGET)


/* Callbacks */

/* _document_changed_cb helper fcn. */
static void
tab_bold_label (GHexNotebookTab *self, gboolean bold)
{
	GtkLabel *label = GTK_LABEL(self->label);
	const char *text;
	char *new = NULL;

	text = gtk_label_get_text (label);

	if (bold) {
		new = g_strdup_printf("<b>%s</b>", text);
	}
	else {
		new = g_strdup (text);
	}
	gtk_label_set_markup (label, new);
	g_free (new);
}

static void
ghex_notebook_tab_document_changed_cb (HexDocument *doc,
		gpointer change_data,
		gboolean push_undo,
		gpointer user_data)
{
	GHexNotebookTab *self = GHEX_NOTEBOOK_TAB(user_data);

	(void)change_data, (void)push_undo; 	/* unused */

	tab_bold_label (self, hex_document_has_changed (doc));
}

static void
ghex_notebook_tab_close_click_cb (GtkButton *button,
               gpointer   user_data)
{
	GHexNotebookTab *self = GHEX_NOTEBOOK_TAB(user_data);

	g_signal_emit(self,
			signals[CLOSED],
			0);		/* GQuark detail (just set to 0 if unknown) */
}


/* CONSTRUCTORS AND DESTRUCTORS */

static void
ghex_notebook_tab_init (GHexNotebookTab *self)
{
	GtkWidget *widget = GTK_WIDGET (self);
	GtkLayoutManager *layout_manager;

	/* Set spacing between label and close button. */

	layout_manager = gtk_widget_get_layout_manager (widget);
	gtk_box_layout_set_spacing (GTK_BOX_LAYOUT(layout_manager), 12);
	
	/* Set up our label to hold the document name and the close button. */

	self->label = gtk_label_new (_("Untitled document"));
	self->close_btn = gtk_button_new ();

	gtk_widget_set_halign (self->close_btn, GTK_ALIGN_END);
	gtk_button_set_icon_name (GTK_BUTTON(self->close_btn),
			"window-close-symbolic");
	gtk_button_set_has_frame (GTK_BUTTON(self->close_btn), FALSE);

	gtk_widget_set_parent (self->label, widget);
	gtk_widget_set_parent (self->close_btn, widget);

	/* SIGNALS */

    g_signal_connect (self->close_btn, "clicked",
                     G_CALLBACK(ghex_notebook_tab_close_click_cb), self);
}

static void
ghex_notebook_tab_dispose (GObject *object)
{
	GHexNotebookTab *self = GHEX_NOTEBOOK_TAB(object);
	GtkWidget *widget = GTK_WIDGET(self);
	GtkWidget *child;

	/* Unparent children */
	g_clear_pointer (&self->label, gtk_widget_unparent);
	g_clear_pointer (&self->close_btn, gtk_widget_unparent);
	g_clear_object (&self->gh);

	/* Boilerplate: chain up */
	G_OBJECT_CLASS(ghex_notebook_tab_parent_class)->dispose(object);
}

static void
ghex_notebook_tab_finalize (GObject *object)
{
	GHexNotebookTab *self = GHEX_NOTEBOOK_TAB(object);

	/* Boilerplate: chain up */
	G_OBJECT_CLASS(ghex_notebook_tab_parent_class)->finalize(object);
}

static void
ghex_notebook_tab_class_init (GHexNotebookTabClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = ghex_notebook_tab_dispose;
	object_class->finalize = ghex_notebook_tab_finalize;

	/* Layout manager: box-style layout. */
	gtk_widget_class_set_layout_manager_type (widget_class,
			GTK_TYPE_BOX_LAYOUT);

	/* SIGNALS */

	signals[CLOSED] = g_signal_new_class_handler("closed",
			G_OBJECT_CLASS_TYPE(object_class),
			G_SIGNAL_RUN_FIRST,
		/* GCallback class_handler: */
			NULL,
		/* no accumulator or accu_data */
			NULL, NULL,
		/* GSignalCMarshaller c_marshaller: */
			NULL,		/* use generic marshaller */
		/* GType return_type: */
			G_TYPE_NONE,
		/* guint n_params: */
			0);
}

/* Internal Methods */ 

static void
refresh_file_name (GHexNotebookTab *self)
{
	HexDocument *doc;

   	doc = gtk_hex_get_document (self->gh);

	gtk_label_set_markup (GTK_LABEL(self->label),
			hex_document_get_basename (doc));
	tab_bold_label (self, hex_document_has_changed (doc));
}

/* Public Methods */
 
GtkWidget *
ghex_notebook_tab_new (void)
{
	return g_object_new (GHEX_TYPE_NOTEBOOK_TAB,
			/* no properties to set */
			NULL);
}

void
ghex_notebook_tab_add_hex (GHexNotebookTab *self, GtkHex *gh)
{
	HexDocument *doc;

	/* Do some sanity checks, as this method requires that some ducks be in
	 * a row -- we need a valid GtkHex that is pre-loaded with a valid
	 * HexDocument.
	 */
	g_return_if_fail (GHEX_IS_NOTEBOOK_TAB (self));
	g_return_if_fail (GTK_IS_HEX (gh));

	doc = gtk_hex_get_document (gh);
	g_return_if_fail (HEX_IS_DOCUMENT (doc));

	/* Associate this notebook tab with a GtkHex widget. */
	g_object_ref (gh);
	self->gh = gh;

	/* Set name of tab. */
	refresh_file_name (self);

	/* HexDocument - Setup signals */
	g_signal_connect (doc, "document-changed",
			G_CALLBACK(ghex_notebook_tab_document_changed_cb), self);

	g_signal_connect_swapped (doc, "file-name-changed",
			G_CALLBACK(refresh_file_name), self);

	g_signal_connect_swapped (doc, "file-saved",
			G_CALLBACK(refresh_file_name), self);
}

const char *
ghex_notebook_tab_get_filename (GHexNotebookTab *self)
{
	g_return_val_if_fail (GTK_IS_LABEL (GTK_LABEL(self->label)),
			NULL);

	return gtk_label_get_text (GTK_LABEL(self->label));
}

GtkHex *
ghex_notebook_tab_get_hex (GHexNotebookTab *self)
{
	g_return_val_if_fail (GTK_IS_HEX (self->gh), NULL);

	return self->gh;
}
