/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* replace.h - types related to replace dialogs

   Copyright (C) 2018 Zander Brown

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

   Author: Zander Brown <zbrown@gnome.org>
*/

#ifndef __REPLACE_H__
#define __REPLACE_H__

#include <gtk/gtk.h>

#include "ghex-window.h"

G_BEGIN_DECLS

#define G_HEX_TYPE_REPLACE (g_hex_replace_get_type ())
G_DECLARE_FINAL_TYPE (GHexReplace, g_hex_replace, G_HEX, REPLACE, GtkDialog)

GtkWidget *g_hex_replace_new (GtkWindow *parent);

G_END_DECLS

#endif
