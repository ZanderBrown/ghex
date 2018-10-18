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
   If not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Author: Jaka Mocnik <jaka@gnu.org>
*/

#ifndef __FINDREPLACE_H__
#define __FINDREPLACE_H__ 

#include <gtk/gtk.h>

#include "gtkhex.h"
#include "findreplace.h"
#include "ghex-window.h"

G_BEGIN_DECLS

#define G_HEX_TYPE_FIND_ADD (g_hex_find_add_get_type ())
G_DECLARE_FINAL_TYPE (GHexFindAdd, g_hex_find_add, G_HEX, FIND_ADD, GtkDialog)

GtkWidget *g_hex_find_add_new (GtkWindow *parent);


typedef struct _AdvancedFindDialog AdvancedFindDialog;

struct _AdvancedFindDialog {
	GHexWindow *parent;

	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkListStore *list;
	GtkWidget *tree;
	GtkWidget *f_next, *f_prev;
	GtkWidget *f_new, *f_remove;
	GtkWidget *f_close;
};

/* creation of dialogs */
AdvancedFindDialog *create_advanced_find_dialog(GHexWindow *parent);
void               delete_advanced_find_dialog (AdvancedFindDialog *dialog);

G_END_DECLS

#endif /* !__FINDREPLACE_H__ */
