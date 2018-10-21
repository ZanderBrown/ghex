/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* print.h - printing related stuff for ghex

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

#include "dialog.h"

#ifndef __GHEX_PREFERENCES_H__
#define __GHEX_PREFERENCES_H__

G_BEGIN_DECLS


#define G_HEX_TYPE_PREFERENCES (g_hex_prefrences_get_type ())
G_DECLARE_FINAL_TYPE (GHexPreferences, g_hex_prefrences, G_HEX, PREFERENCES, GHexDialog)

GtkWidget *g_hex_prefrences_new (GHexWindow *parent);

extern guint group_type[3];
extern gchar *group_type_label[3];

G_END_DECLS

#endif /* !__GHEX_PREFERENCES_H__ */
