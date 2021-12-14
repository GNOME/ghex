/* vim: ts=4 sw=4 colorcolumn=80                                                
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- *
 */
/* hex-document-malloc.h - `malloc` implementation of the HexBuffer iface.
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

#ifndef HEX_DOCUMENT_MALLOC_H
#define HEX_DOCUMENT_MALLOC_H

#include "hex-buffer-iface.h"

#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

G_BEGIN_DECLS

#define HEX_TYPE_BUFFER_MALLOC hex_buffer_malloc_get_type ()
G_DECLARE_FINAL_TYPE (HexBufferMalloc, hex_buffer_malloc, HEX, BUFFER_MALLOC, GObject)

HexBufferMalloc *hex_buffer_malloc_new (GFile *file);

G_END_DECLS
#endif
