#pragma once

#include "hex-widget.h"

G_BEGIN_DECLS

/* Type declaration */

#define GHEX_TYPE_VIEW_CONTAINER (ghex_view_container_get_type())
G_DECLARE_FINAL_TYPE (GHexViewContainer, ghex_view_container, GHEX, VIEW_CONTAINER, GtkWidget)

/* Method declarations */

GtkWidget *	ghex_view_container_new (void);
HexWidget * ghex_view_container_get_hex (GHexViewContainer *self);
void ghex_view_container_set_document (GHexViewContainer *self, HexDocument *doc);
HexDocument * ghex_view_container_get_document (GHexViewContainer *self);

G_END_DECLS
