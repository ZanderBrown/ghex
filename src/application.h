/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* application.h - application class

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

#ifndef __APPLICATION_H__
#define __APPLICATION_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define G_HEX_TYPE_APPLICATION (g_hex_application_get_type ())
G_DECLARE_FINAL_TYPE (GHexApplication, g_hex_application, G_HEX, APPLICATION, GtkApplication)

GHexApplication *
g_hex_application_new              ();

void
g_hex_application_set_group_by      (GHexApplication        *self,
                                     const gchar            *group_by);
const gchar *
g_hex_application_get_group_by      (GHexApplication        *self);
void
g_hex_application_set_shaded_box    (GHexApplication        *self,
                                     guint                   shaded_box);
guint
g_hex_application_get_shaded_box    (GHexApplication        *self);
void
g_hex_application_set_data_font     (GHexApplication       *self,
                                    const gchar            *data_font);
const gchar *
g_hex_application_get_data_font     (GHexApplication       *self);
void
g_hex_application_set_header_font   (GHexApplication       *self,
                                     const gchar           *header_font);
const gchar *
g_hex_application_get_header_font   (GHexApplication       *self);
void
g_hex_application_set_font_name     (GHexApplication       *self,
                                     const gchar           *font);
const gchar *
g_hex_application_get_font_name     (GHexApplication       *self);
void
g_hex_application_get_font          (GHexApplication       *self,
                                     PangoFontMetrics     **metrics,
                                     PangoFontDescription **desc);
void
g_hex_application_set_show_offset   (GHexApplication *self,
                                     gboolean         show_offet);
gboolean
g_hex_application_get_show_offset   (GHexApplication *self);
void
g_hex_application_set_undo_depth    (GHexApplication        *self,
                                     guint                   undo_depth);
guint
g_hex_application_get_undo_depth    (GHexApplication        *self);
void
g_hex_application_set_offset_format (GHexApplication       *self,
                                     const gchar           *offset_format);
const gchar *
g_hex_application_get_offset_format (GHexApplication       *self);

G_END_DECLS

#endif
