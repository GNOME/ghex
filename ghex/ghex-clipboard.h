// vim: ts=4 sw=4 breakindent breakindentopt=shift\:4

/* ghex-clipboard.h - Declarations for GHex clipboard dialogs

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

   Original GHex Author: Jaka Mocnik <jaka@gnu.org>
*/

#pragma once

#include "gtkhex.h"

G_BEGIN_DECLS

#define GHEX_TYPE_CLIPBOARD_DIALOG (ghex_clipboard_dialog_get_type ())
G_DECLARE_DERIVABLE_TYPE (GHexClipboardDialog, ghex_clipboard_dialog, GHEX, CLIPBOARD_DIALOG, GtkWindow)

struct _GHexClipboardDialogClass
{
	GtkWindowClass window;
};

HexWidget * ghex_clipboard_dialog_get_hex (GHexClipboardDialog *self);
void ghex_clipboard_dialog_set_hex (GHexClipboardDialog *self, HexWidget *hex);

#define GHEX_TYPE_PASTE_SPECIAL_DIALOG (ghex_paste_special_dialog_get_type ())
G_DECLARE_FINAL_TYPE (GHexPasteSpecialDialog, ghex_paste_special_dialog, GHEX, PASTE_SPECIAL_DIALOG, GHexClipboardDialog)

GtkWidget * ghex_paste_special_dialog_new (GtkWindow *parent);

#define GHEX_TYPE_COPY_SPECIAL_DIALOG (ghex_copy_special_dialog_get_type ())
G_DECLARE_FINAL_TYPE (GHexCopySpecialDialog, ghex_copy_special_dialog, GHEX, COPY_SPECIAL_DIALOG, GHexClipboardDialog)

GtkWidget * ghex_copy_special_dialog_new (GtkWindow *parent);

G_END_DECLS
