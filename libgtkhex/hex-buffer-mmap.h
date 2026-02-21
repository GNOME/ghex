/* vim: ts=4 sw=4 colorcolumn=80                                                
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- *
 */
/* hex-document-mmap.h - `mmap` implementation of the HexBuffer iface.
 *
 * Based on code from aoeui, Copyright © 2007, 2008 Peter Klausler,
 * licensed by the author/copyright-holder under GPLv2 only.
 *
 * Source as adapted herein licensed for GHex to GPLv2+ with written
 * permission from Peter Klausler dated December 13, 2021 (see
 * associated git log).
 *
 * Copyright © 2021 Logan Rathbone
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

#ifndef HEX_DOCUMENT_MMAP_H
#define HEX_DOCUMENT_MMAP_H

#include <hex-buffer-iface.h>

#define _GNU_SOURCE

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

G_BEGIN_DECLS

#define HEX_TYPE_BUFFER_MMAP hex_buffer_mmap_get_type ()
G_DECLARE_FINAL_TYPE (HexBufferMmap, hex_buffer_mmap, HEX, BUFFER_MMAP, GObject)

/* HexBufferNewFunc declaration */
HexBuffer *hex_buffer_mmap_new (GFile *file);

G_END_DECLS
#endif
