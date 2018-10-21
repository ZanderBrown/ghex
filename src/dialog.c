/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* dialog.c - base dialog

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

   Authors:
   Zander Brown <zbrown@gnome.org>
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib/gi18n.h>

#include "dialog.h"

G_DEFINE_TYPE (GHexDialog, g_hex_dialog, GTK_TYPE_DIALOG)

GHexWindow *
g_hex_dialog_get_window (GHexDialog *self)
{
	return GHEX_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (self)));
}

static void
g_hex_dialog_class_init (GHexDialogClass *klass)
{
}

static void
g_hex_dialog_init (GHexDialog *self)
{
	GtkWidget *content;

	content = gtk_dialog_get_content_area (GTK_DIALOG (self));

	gtk_container_set_border_width (GTK_CONTAINER (content), 0);
}

