/* vim: ts=4 sw=4 colorcolumn=80
 */

#include "conversions-pane.h"

struct _ConversionsPane
{
	GtkWidget parent_instance;

/*
 * for i in `cat conversions-pane.ui |grep -i 'id=' |sed -e 's,^\s*,,g' |sed -e 's,.*id=",,' |sed -e 's,">,,'`; do echo $i >> tmp.txt; done
 */

/* for i in `cat tmp.txt`; do echo GtkWidget *${i}; done
 */
	GtkWidget *conversions_pane;
	GtkWidget *signed_8bit_label;
	GtkWidget *signed_32bit_label;
	GtkWidget *signed_hexadecimal_label;
	GtkWidget *unsigned_8bit_label;
	GtkWidget *unsigned_32bit_label;
	GtkWidget *octal_label;
	GtkWidget *signed_16bit_label;
	GtkWidget *signed_64bit_label;
	GtkWidget *binary_label;
	GtkWidget *unsigned_16bit_label;
	GtkWidget *unsigned_64bit_label;
	GtkWidget *float_32bit_label;
	GtkWidget *float_64bit_label;
	GtkWidget *stream_length_spin_button;
	GtkWidget *little_endian_check_button;
	GtkWidget *unsigned_and_float_as_hex_check_button;
};

G_DEFINE_TYPE (ConversionsPane, conversions_pane,
		GTK_TYPE_WIDGET)

static void
conversions_pane_init(ConversionsPane *self)
{
	GtkWidget *widget = GTK_WIDGET(self);
	GtkCssProvider *provider;
	GtkStyleContext *context;

	gtk_widget_init_template (widget);

	/* css */

	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_path (provider, "style.css");
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_provider (context,
			GTK_STYLE_PROVIDER(provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
conversions_pane_dispose(GObject *object)
{
	ConversionsPane *self = CONVERSIONS_PANE(object);

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(conversions_pane_parent_class)->dispose(object);
}

static void
conversions_pane_finalize(GObject *gobject)
{
	/* here, you would free stuff. I've got nuthin' for ya. */

	/* --- */

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(conversions_pane_parent_class)->finalize(gobject);
}

static void
conversions_pane_class_init(ConversionsPaneClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	/* <boilerplate> */
	object_class->dispose = conversions_pane_dispose;
	object_class->finalize = conversions_pane_finalize;
	/* </boilerplate> */

	gtk_widget_class_set_layout_manager_type (widget_class,
					GTK_TYPE_BOX_LAYOUT);

	gtk_widget_class_set_template_from_resource (widget_class,
					"/org/gnome/ghex/conversions-pane.ui");

	/* 
	 * for i in `cat tmp.txt`; do echo "gtk_widget_class_bind_template_child (widget_class, ConversionsPane, ${i});"; done
	 */
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, conversions_pane);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, signed_8bit_label);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, signed_32bit_label);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, signed_hexadecimal_label);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, unsigned_8bit_label);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, unsigned_32bit_label);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, octal_label);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, signed_16bit_label);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, signed_64bit_label);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, binary_label);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, unsigned_16bit_label);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, unsigned_64bit_label);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, float_32bit_label);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, float_64bit_label);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, stream_length_spin_button);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, little_endian_check_button);
	gtk_widget_class_bind_template_child (widget_class, ConversionsPane, unsigned_and_float_as_hex_check_button);
}

GtkWidget *
conversions_pane_new(void)
{
	return g_object_new(CONVERSIONS_TYPE_PANE, NULL);
}
