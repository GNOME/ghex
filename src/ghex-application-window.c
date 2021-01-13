/* vim: ts=4 sw=4 colorcolumn=80
 */

#include <gtkhex.h>
#include "ghex-application-window.h"
#include "hex-dialog.h"

struct _GHexApplicationWindow
{
	GtkApplicationWindow parent_instance;

	GtkHex *gh;
	HexDialog *dialog;
	GtkWidget *dialog_widget;
	guint statusbar_id;
	GtkAdjustment *adj;
/*
 * for i in `cat ghex-application-window.ui |grep -i 'id=' |sed -e 's,^\s*,,g' |sed -e 's,.*id=",,' |sed -e 's,">,,'`; do echo $i >> tmp.txt; done
 */

/* for i in `cat tmp.txt`; do echo GtkWidget *${i}; done
 */
	GtkWidget *child_box;
	GtkWidget *conversions_box;
	GtkWidget *pane_toggle_button;
	GtkWidget *insert_mode_button;
	GtkWidget *statusbar;
	GtkWidget *scrollbar;
};

G_DEFINE_TYPE (GHexApplicationWindow, ghex_application_window,
		GTK_TYPE_APPLICATION_WINDOW)

/* CALLBACKS */

static void
cursor_moved_cb(GtkHex *gtkhex, gpointer user_data)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(user_data);
    int current_pos;
    HexDialogVal64 val;

    current_pos = gtk_hex_get_cursor(gtkhex);
//    ghex_window_update_status_message(self);

    for (int i = 0; i < 8; i++)
    {
        /* returns 0 on buffer overflow, which is what we want */
        val.v[i] = gtk_hex_get_byte(gtkhex, current_pos+i);
    }
    hex_dialog_updateview (self->dialog, &val);
}


/* ACTIONS */

static void
toggle_conversions (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);

	(void)parameter, (void)action_name;		/* unused */

	if (gtk_widget_is_visible (self->conversions_box)) {
		gtk_widget_set_visible (self->conversions_box, FALSE);
		gtk_button_set_icon_name (GTK_BUTTON(self->pane_toggle_button),
				"pan-up-symbolic");
	} else {
		gtk_widget_set_visible (self->conversions_box, TRUE);
		gtk_button_set_icon_name (GTK_BUTTON(self->pane_toggle_button),
				"pan-down-symbolic");
	}
}

static void
toggle_insert_mode (GtkWidget *widget,
		const char *action_name,
		GVariant *parameter)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(widget);

	(void)parameter, (void)action_name;		/* unused */

	/* this tests whether the button is pressed AFTER its state has changed. */
	if (gtk_toggle_button_get_active (self->insert_mode_button)) {
		g_debug("%s: TOGGLING INSERT MODE", __func__);
		gtk_hex_set_insert_mode(self->gh, TRUE);
	} else {
		g_debug("%s: UNTOGGLING INSERT MODE", __func__);
		gtk_hex_set_insert_mode(self->gh, FALSE);
	}
}

/* --- */

// DUMB TEST
static void
set_statusbar(GHexApplicationWindow *self, const char *str)
{
	guint id = 
		gtk_statusbar_get_context_id (GTK_STATUSBAR(self->statusbar),
				"status");

	gtk_statusbar_push (GTK_STATUSBAR(self->statusbar), id, str);
}

static void
ghex_application_window_init(GHexApplicationWindow *self)
{
	GtkWidget *widget = GTK_WIDGET(self);
	GtkStyleContext *context;
	GtkCssProvider *provider;

	gtk_widget_init_template (widget);

	/* Setup conversions box and pane */
	self->dialog = hex_dialog_new ();
	self->dialog_widget = hex_dialog_getview (self->dialog);

	gtk_box_append (GTK_BOX(self->conversions_box), self->dialog_widget);
	gtk_widget_set_visible (self->conversions_box, FALSE);

	/* CSS - conversions_box */
	context = gtk_widget_get_style_context (self->conversions_box);
	provider = gtk_css_provider_new ();

	gtk_css_provider_load_from_data (provider,
									 "box {\n"
									 "   padding: 20px;\n"
									 "}\n", -1);

	/* add the provider to our widget's style context. */
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);


	// TEST
	set_statusbar(self, "Offset: 0x0");
}

static void
ghex_application_window_dispose(GObject *object)
{
	GHexApplicationWindow *self = GHEX_APPLICATION_WINDOW(object);

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(ghex_application_window_parent_class)->dispose(object);
}

static void
ghex_application_window_finalize(GObject *gobject)
{
	/* here, you would free stuff. I've got nuthin' for ya. */

	/* --- */

	/* Boilerplate: chain up
	 */
	G_OBJECT_CLASS(ghex_application_window_parent_class)->finalize(gobject);
}

static void
ghex_application_window_class_init(GHexApplicationWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	/* <boilerplate> */
	object_class->dispose = ghex_application_window_dispose;
	object_class->finalize = ghex_application_window_finalize;
	/* </boilerplate> */

	gtk_widget_class_set_template_from_resource (widget_class,
					"/org/gnome/ghex/ghex-application-window.ui");

	/* ACTIONS */

	gtk_widget_class_install_action (widget_class, "ghex.show-conversions",
			NULL,	// GVariant string param_type
			toggle_conversions);

	gtk_widget_class_install_action (widget_class, "ghex.insert-mode",
			NULL,	// GVariant string param_type
			toggle_insert_mode);

	/* 
	 * for i in `cat tmp.txt`; do echo "gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow, ${i});"; done
	 */
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			child_box);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			conversions_box);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			pane_toggle_button);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			insert_mode_button);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			statusbar);
	gtk_widget_class_bind_template_child (widget_class, GHexApplicationWindow,
			scrollbar);
}

GtkWidget *
ghex_application_window_new(void)
{
	return g_object_new(GHEX_TYPE_APPLICATION_WINDOW, NULL);
}

void
ghex_application_window_add_hex(GHexApplicationWindow *self, GtkHex *gh)
{
	g_return_if_fail (GTK_IS_HEX(gh));

	self->gh = gh;

	gtk_box_prepend (GTK_BOX(self->child_box), GTK_WIDGET(gh));

	/* Setup signals */
    g_signal_connect(G_OBJECT(gh), "cursor-moved",
                     G_CALLBACK(cursor_moved_cb), self);

	/* Setup scrollbar */
	self->adj = gtk_hex_get_adjustment(gh);
	gtk_scrollbar_set_adjustment (GTK_SCROLLBAR(self->scrollbar), self->adj);
}
