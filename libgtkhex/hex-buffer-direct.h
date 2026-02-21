/* vim: ts=4 sw=4 colorcolumn=80                                                
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- *
 */
/* hex-document-direct.h - `direct` implementation of the HexBuffer iface.
 *
 * Copyright © 2022 Logan Rathbone
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
 */

#ifndef HEX_DOCUMENT_DIRECT_H
#define HEX_DOCUMENT_DIRECT_H

#include <hex-buffer-iface.h>

#define _GNU_SOURCE

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

G_BEGIN_DECLS

#define HEX_TYPE_BUFFER_DIRECT hex_buffer_direct_get_type ()
G_DECLARE_FINAL_TYPE (HexBufferDirect, hex_buffer_direct, HEX, BUFFER_DIRECT, GObject)

/* HexBufferNewFunc declaration */
HexBuffer *hex_buffer_direct_new (GFile *file);

G_END_DECLS
#endif
