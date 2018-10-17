/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ui.h - main application user interface & ui utility functions

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

#ifndef __GHEX_UI_H__
#define __GHEX_UI_H__

#include <gtk/gtk.h>

#include "ghex-window.h"
#include "preferences.h"

G_BEGIN_DECLS

/* various ui convenience functions */
void create_dialog_title   (GtkWidget *, gchar *);
gint ask_user              (GtkMessageDialog *);
void display_error_dialog (GHexWindow *win, const gchar *msg);
void display_info_dialog (GHexWindow *win, const gchar *msg, ...);
void raise_and_focus_widget(GtkWidget *);
void set_doc_menu_sensitivity(HexDocument *doc);
void update_dialog_titles (void);

/* hiding widgets on cancel or delete_event */
gint delete_event_cb(GtkWidget *, GdkEventAny *, GtkWindow *);
void cancel_cb      (GtkWidget *, GtkWidget *);

/* File menu */
void open_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void save_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void save_as_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void export_html_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void revert_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void print_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void print_preview_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void close_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void quit_app_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);

/* Edit menu */
void undo_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void redo_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void copy_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void cut_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void paste_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void find_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void advanced_find_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void replace_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void jump_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void insert_mode_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void prefs_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);

/* View menu */
void add_view_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void group_data_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);

/* Windows menu */
void character_table_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void converter_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void type_dialog_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);

/* Help menu */
void help_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);
void about_cb (GSimpleAction *action, GVariant *value, GHexWindow *window);

void file_list_activated_cb (GtkAction *action, gpointer user_data);

/* tmp utils */
GtkWidget *create_hex_view(HexDocument *doc);
gint get_search_string(HexDocument *doc, gchar **str);

G_END_DECLS

#endif /* !__GHEX_UI_H__ */
