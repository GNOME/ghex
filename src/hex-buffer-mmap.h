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
 */

// WIP - NOT WORKING CODE!

#ifndef HEX_DOCUMENT_MMAP_H
#define HEX_DOCUMENT_MMAP_H

#include "hex-buffer-iface.h"

#define _GNU_SOURCE

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

G_BEGIN_DECLS

#define HEX_TYPE_BUFFER_MMAP hex_buffer_mmap_get_type ()
G_DECLARE_FINAL_TYPE (HexBufferMmap, hex_buffer_mmap, HEX, BUFFER_MMAP, GObject)

HexBufferMmap *hex_buffer_mmap_new (GFile *file);

G_END_DECLS
#endif
