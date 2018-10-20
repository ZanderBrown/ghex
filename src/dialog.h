/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* dialog.h - base dialog

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

#ifndef __DIALOG_H__
#define __DIALOG_H__

#include <gtk/gtk.h>

#include "ghex-window.h"

G_BEGIN_DECLS

#define G_HEX_TYPE_DIALOG (g_hex_dialog_get_type ())
G_DECLARE_DERIVABLE_TYPE (GHexDialog, g_hex_dialog, G_HEX, DIALOG, GtkDialog)

struct _GHexDialogClass {
	GtkDialogClass parent_class;
};

GHexWindow *
g_hex_dialog_get_window (GHexDialog *self);

G_END_DECLS

#endif
