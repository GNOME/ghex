/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* findreplace.h - types related to find and replace dialogs

   Copyright (C) 2004 Free Software Foundation

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
   If not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifndef __FINDREPLACE_H__
#define __FINDREPLACE_H__ 

#include <gtk/gtk.h>

#include "gtkhex.h"
#include "findreplace.h"
#include "ghex-window.h"

G_BEGIN_DECLS

typedef struct _JumpDialog JumpDialog;

struct _JumpDialog {
	GtkWidget *window;
	GtkWidget *int_entry;
	GtkWidget *ok, *cancel;
};

typedef struct _FindDialog FindDialog;
typedef struct _ReplaceDialog ReplaceDialog;

struct _ReplaceDialog {
	GtkWidget *window;
	GtkWidget *f_gh, *r_gh;
	HexDocument *f_doc, *r_doc;
	GtkWidget *replace, *replace_all, *next, *close;
	
	GtkHex_AutoHighlight *auto_highlight;
}; 

struct _FindDialog {
	GtkWidget *window;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	HexDocument *f_doc;
	GtkWidget *f_gh;
	GtkWidget *f_next, *f_prev, *f_close;
	
	GtkHex_AutoHighlight *auto_highlight;
};

typedef struct _AdvancedFindDialog AdvancedFindDialog;
typedef struct _AdvancedFind_AddDialog AdvancedFind_AddDialog;

struct _AdvancedFindDialog {
	GHexWindow *parent;
	AdvancedFind_AddDialog *addDialog;

	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkListStore *list;
	GtkWidget *tree;
	GtkWidget *f_next, *f_prev;
	GtkWidget *f_new, *f_remove;
	GtkWidget *f_close;
};

struct _AdvancedFind_AddDialog {
	AdvancedFindDialog *parent;
  
	GtkWidget *window;
	GtkWidget *f_gh;
	HexDocument *f_doc;
	GtkWidget *colour;
};

extern FindDialog     *find_dialog;
extern ReplaceDialog  *replace_dialog;
extern JumpDialog     *jump_dialog;

/* creation of dialogs */
FindDialog         *create_find_dialog         (void);
ReplaceDialog      *create_replace_dialog      (void);
JumpDialog         *create_jump_dialog         (void);
AdvancedFindDialog *create_advanced_find_dialog(GHexWindow *parent);
void               delete_advanced_find_dialog (AdvancedFindDialog *dialog);

G_END_DECLS

#endif /* !__FINDREPLACE_H__ */
