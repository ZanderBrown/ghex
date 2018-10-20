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
#include "dialog.h"

G_BEGIN_DECLS

#define G_HEX_TYPE_FIND_CONDITION (g_hex_find_condition_get_type ())
G_DECLARE_FINAL_TYPE (GHexFindCondition, g_hex_find_condition, G_HEX, FIND_CONDITION, GObject)

GHexFindCondition *
g_hex_find_condition_new (GString              *cond,
						  GtkHex_AutoHighlight *hlight);

#define G_HEX_TYPE_FIND_ROW (g_hex_find_row_get_type ())
G_DECLARE_FINAL_TYPE (GHexFindRow, g_hex_find_row, G_HEX, FIND_ROW, GtkListBoxRow)

GtkWidget *g_hex_find_row_new (GHexFindCondition *cond);

#define G_HEX_TYPE_FIND_ADD (g_hex_find_add_get_type ())
G_DECLARE_FINAL_TYPE (GHexFindAdd, g_hex_find_add, G_HEX, FIND_ADD, GtkDialog)

GtkWidget *g_hex_find_add_new (GtkWindow *parent);

#define G_HEX_TYPE_FIND_ADVANCED (g_hex_find_advanced_get_type ())
G_DECLARE_FINAL_TYPE (GHexFindAdvanced, g_hex_find_advanced, G_HEX, FIND_ADVANCED, GHexDialog)

GtkWidget *g_hex_find_advanced_new (GHexWindow *parent);

G_END_DECLS

#endif /* !__FINDREPLACE_H__ */
