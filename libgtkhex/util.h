// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

#include <gtk/gtk.h>
#include "hex-document.h"
#include "hex-highlight.h"

/* Generic utility Tuple that up to 5 data can be chucked into */

typedef struct UtilTuple UtilTuple;
struct UtilTuple
{
	void *data[5];
	GDestroyNotify destroy[5];
};

UtilTuple *util_tuple_new (void);
void util_tuple_destroy (UtilTuple *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (UtilTuple, util_tuple_destroy);

/* Other utility functions */

void util_warn_invalid_enum (const char *desc, GType enum_type, int enum_value);
gboolean util_delete_selection_in_doc (gpointer source_object);
gboolean util_zero_selection_in_doc (gpointer source_object);
gboolean util_have_object_transform_to (GBinding *binding, const GValue *from_value, GValue *to_value, gpointer data);
char * util_gdk_rgba_to_hex_color (const GdkRGBA *color);
