/* vim: ts=4 sw=4 colorcolumn=80
 */
/* hex-file-monitor.h - Declaration of file monitor object
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

#ifndef HEX_FILE_MONITOR_H
#define HEX_FILE_MONITOR_H

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define HEX_TYPE_FILE_MONITOR hex_file_monitor_get_type ()
G_DECLARE_FINAL_TYPE (HexFileMonitor, hex_file_monitor, HEX, FILE_MONITOR, GObject)

HexFileMonitor * hex_file_monitor_new (GFile *file);
gboolean hex_file_monitor_set_file (HexFileMonitor *monitor, GFile *file);
gboolean hex_file_monitor_get_changed (HexFileMonitor *monitor);
void hex_file_monitor_reset (HexFileMonitor *monitor);

G_END_DECLS

#endif /* HEX_FILE_MONITOR_H */
