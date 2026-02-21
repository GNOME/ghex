/* vim: ts=4 sw=4 colorcolumn=80
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex.h - declaration of a HexWidget widget

   Copyright © 1997 - 2004 Free Software Foundation

   Copyright © 2005-2020 Various individual contributors, including
   but not limited to: Jonathon Jongsma, Kalev Lember, who continued
   to maintain the source code under the licensing terms described
   herein and below.

   Copyright © 2021-2026 Logan Rathbone <poprocks@gmail.com>

   GHex is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   GHex is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GHex; see the file COPYING.
   If not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Original Author: Jaka Mocnik <jaka@gnu.org>
*/

#pragma once

#include <gtk/gtk.h>

#include "gtkhex-layout-manager.h"
#include "gtkhex-paste-data.h"
#include "hex-auto-highlight.h"
#include "hex-buffer-iface.h"
#include "hex-buffer-malloc.h"
#include "hex-document.h"
#include "hex-file-monitor.h"
#include "hex-highlight.h"
#include "hex-mark.h"
#include "hex-search-info.h"
#include "hex-selection.h"
#include "hex-text-ascii.h"
#include "hex-text-common.h"
#include "hex-text-editable.h"
#include "hex-text.h"
#include "hex-text-hex.h"
#include "hex-text-offsets.h"
#include "hex-view.h"
#include "hex-widget.h"
