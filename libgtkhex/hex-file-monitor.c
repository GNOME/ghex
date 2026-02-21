/* vim: ts=4 sw=4 colorcolumn=80
 */
/* hex-file-monitor.c - Implementation of file monitor object
 *
 * Copyright © 2025 Jordan Christiansen
 *
 * GHex is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GHex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GHex; see the file COPYING.
 * If not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Original GHex Author: Jaka Mocnik
 */

#include <stdio.h>

#include "hex-file-monitor.h"

#include <glib-object.h>
#include <glib.h>

struct _HexFileMonitor
{
	GObject parent;
	GFile *file;
	GFileMonitor *monitor;
	gboolean changed;
};
G_DEFINE_TYPE (HexFileMonitor, hex_file_monitor, G_TYPE_OBJECT)

typedef enum {
	PROP_CHANGED = 1,
	PROP_LAST,
} HexFileMonitorProperty;

static GParamSpec *properties[PROP_LAST];

static void
hex_file_monitor_changed_cb (HexFileMonitor *self,
                             GFile               *file,
                             GFile               *other_file,
                             GFileMonitorEvent    event,
                             GFileMonitor        *monitor)
{
	if (event == G_FILE_MONITOR_EVENT_CHANGED) {
		g_debug ("%s: File %s changed", __func__, g_file_peek_path(file));
		self->changed = TRUE;
		g_object_notify_by_pspec(G_OBJECT (self), properties[PROP_CHANGED]);
	}
}

HexFileMonitor *
hex_file_monitor_new (GFile *file)
{
	HexFileMonitor *self;

	g_return_val_if_fail (G_IS_FILE (file), NULL);

	self = g_object_new (HEX_TYPE_FILE_MONITOR, NULL);
	g_return_val_if_fail (self, NULL);

	self->file = g_object_ref (file);

	self->monitor = g_file_monitor_file (self->file, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);

	if (self->monitor == NULL)
	{
		g_warning ("%s: Failed to set up file monitor.", __func__);
		g_clear_object (&self);
		return self;
	}

	g_file_monitor_set_rate_limit (self->monitor, 500);
	g_signal_connect_object (
		self->monitor,
		"changed",
		G_CALLBACK (hex_file_monitor_changed_cb),
		self,
		G_CONNECT_SWAPPED
	);

	g_debug ("%s: Set up watch on %s", __func__, g_file_peek_path(file));

	return self;
}

/**
 * hex_file_monitor_get_changed:
 * @self: a [class@Hex.FileMonitor] object
 *
 * Check whether the monitored file has changed.
 *
 * Returns: %TRUE if the file has changed; %FALSE otherwise
 */
gboolean
hex_file_monitor_get_changed (HexFileMonitor *self)
{
	g_return_val_if_fail (HEX_IS_FILE_MONITOR (self), FALSE);

	return self->changed;
}

/**
 * hex_file_monitor_reset:
 * @self: a [class@Hex.FileMonitor] object
 *
 * Clear any previously seen file change events and begin watching for changes again.
 */
void
hex_file_monitor_reset (HexFileMonitor *self)
{
	g_return_if_fail (HEX_IS_FILE_MONITOR (self));

	self->changed = FALSE;
}

static void
hex_file_monitor_init (HexFileMonitor *self)
{
}

static void
hex_file_monitor_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)
{
	HexFileMonitor *monitor = HEX_FILE_MONITOR (object);

	switch (property_id)
	{
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_file_monitor_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	HexFileMonitor *monitor = HEX_FILE_MONITOR (object);

	switch (property_id)
	{
		case PROP_CHANGED:
			g_value_set_boolean(value, hex_file_monitor_get_changed(monitor));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_file_monitor_dispose (GObject *object)
{
	HexFileMonitor *monitor = HEX_FILE_MONITOR (object);

	g_clear_object (&monitor->file);
	g_clear_object (&monitor->monitor);

	G_OBJECT_CLASS (hex_file_monitor_parent_class)->dispose (object);
}

static void
hex_file_monitor_finalize (GObject *object)
{
	G_OBJECT_CLASS (hex_file_monitor_parent_class)->finalize (object);
}

static void
hex_file_monitor_class_init (HexFileMonitorClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->set_property = hex_file_monitor_set_property;
	gobject_class->get_property = hex_file_monitor_get_property;
	gobject_class->dispose = hex_file_monitor_dispose;
	gobject_class->finalize = hex_file_monitor_finalize;

	properties [PROP_CHANGED] =
		g_param_spec_boolean ("changed",
		                      "Changed",
		                      "If the open file changed on disk since it was last read",
		                      FALSE,
		                      (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (gobject_class, PROP_LAST, properties);
}

