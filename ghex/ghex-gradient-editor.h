// vim: linebreak breakindent breakindentopt=shift\:4

#include "ghex-gradient-bar.h"

G_BEGIN_DECLS

#define GHEX_TYPE_GRADIENT_EDITOR (ghex_gradient_editor_get_type ())
G_DECLARE_FINAL_TYPE (GHexGradientEditor, ghex_gradient_editor, GHEX, GRADIENT_EDITOR, GtkWidget)

GtkWidget * ghex_gradient_editor_new (void);
GtkWidget * ghex_gradient_editor_get_bar (GHexGradientEditor *self);

G_END_DECLS
