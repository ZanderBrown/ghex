/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex.h - definition of a GtkHex widget, modified for use with GnomeMDI

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

#ifndef __GHEX_CONVERTER_H__
#define __GHEX_CONVERTER_H__

#include <gtk/gtk.h>

#include "dialog.h"

G_BEGIN_DECLS

#define G_HEX_TYPE_CONVERTER (g_hex_converter_get_type ())
G_DECLARE_FINAL_TYPE (GHexConverter, g_hex_converter, G_HEX, CONVERTER, GHexDialog)

GtkWidget *
g_hex_converter_new          (GHexWindow *parent);
void
g_hex_converter_set_can_grap (GHexConverter *self,
                              gboolean       can_grab);
gboolean
g_hex_converter_get_can_grap (GHexConverter *self);


G_END_DECLS

#endif /* !__GHEX_CONVERTER_H__ */
