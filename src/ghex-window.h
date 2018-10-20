/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * ghex-window.c: everything describing a single ghex window
 *
 * Copyright (C) 2002 - 2004 the Free Software Foundation
 *
 * GHex is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GHex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GHex; see the file COPYING.
 * If not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Author: Jaka Mocnik  <jaka@gnu.org>
 */

#ifndef __GHEX_WINDOW_H__
#define __GHEX_WINDOW_H__

#include <math.h>
#include <ctype.h>

#include "gtkhex.h"
#include "application.h"

G_BEGIN_DECLS 

#define GHEX_TYPE_WINDOW            (ghex_window_get_type ())
#define GHEX_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GHEX_TYPE_WINDOW, GHexWindow))
#define GHEX_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GHEX_TYPE_WINDOW, GHexWindowClass))
#define GHEX_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GHEX_TYPE_WINDOW))
#define GHEX_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GHEX_TYPE_WINDOW))
#define GHEX_WINDOW_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GHEX_TYPE_WINDOW, GHexWindowClass))

typedef struct _GHexWindow      GHexWindow;
typedef struct _GHexWindowClass GHexWindowClass;

struct _GHexWindow 
{
    GtkApplicationWindow win;

    GtkHex    *gh;
    GtkWidget *vbox;
    GtkWidget *contents;
    GtkWidget *statusbar;
    GtkWidget *statusbar_insert_mode;
    GtkWidget *statusbar_display_mode;
    GtkWidget *statusbar_display_mode_btn;
    guint      statusbar_tooltip_id;

    gboolean changed, undo_sens, redo_sens;

    struct _HexDialog *dialog;
    GtkWidget *dialog_widget;

    GtkWidget *advanced_find_dialog;

    GtkWidget *characters;
    GtkWidget *converter;
};

struct _GHexWindowClass
{
    GtkApplicationWindowClass klass;
};

GType             ghex_window_get_type           (void) G_GNUC_CONST;
GtkWidget         *ghex_window_new               (GHexApplication    *application);
GtkWidget         *ghex_window_new_from_doc      (GHexApplication    *application,
                                                  HexDocument        *doc);
GtkWidget         *ghex_window_new_from_file     (GHexApplication    *application,
                                                  const gchar        *filename);
void              ghex_window_set_contents       (GHexWindow *win, GtkWidget  *child);
void              ghex_window_destroy_contents   (GHexWindow *win);
gboolean          ghex_window_load(GHexWindow *win, const gchar *filename);
gboolean          ghex_window_close              (GHexWindow *win);
const GList       *ghex_window_get_list          (void);
GHexWindow        *ghex_window_get_active        (void);
void              ghex_window_set_doc_name       (GHexWindow *win,
                                                  const gchar *name);
void              ghex_window_set_sensitivity    (GHexWindow *win);
void              ghex_window_show_status        (GHexWindow *win,
                                                  const gchar *msg);
void              ghex_window_flash              (GHexWindow *win,
                                                  const gchar * flash);
GHexWindow        *ghex_window_find_for_doc       (HexDocument *doc);

void ghex_window_sync_converter_item(GHexWindow *win, gboolean state);

gboolean ghex_window_ok_to_close(GHexWindow *win);
gboolean ghex_window_save_as(GHexWindow *win);

void ghex_window_update_status_message(GHexWindow *win);

G_END_DECLS

#endif /* __GHEX_WINDOW_H__ */
