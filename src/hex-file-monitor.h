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
