/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* hex-document-ui.c - menu definitions and callbacks for hex-document MDI child

   Copyright (C) 1998 - 2004 Free Software Foundation

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

#include <config.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "hex-document.h"
#include "ghex-window.h"
#include "gtkhex.h"
#include "findreplace.h"
#include "find.h"
#include "goto.h"
#include "replace.h"
#include "ui.h"

void
set_doc_menu_sensitivity(HexDocument *doc)
{
	GList *view_node;
	GtkWidget *view;
	gboolean sensitive;
	GHexWindow *win;
	GAction *act;

	view_node = doc->views;

	while (view_node) {
		view = GTK_WIDGET(view_node->data);

		win = GHEX_WINDOW(gtk_widget_get_toplevel(view));

		g_return_if_fail (win != NULL);
 
		sensitive = doc->undo_top != NULL;
		act = g_action_map_lookup_action (G_ACTION_MAP (win), "undo");
		g_simple_action_set_enabled (G_SIMPLE_ACTION (act), sensitive);
	
		sensitive = doc->undo_stack && doc->undo_top != doc->undo_stack;
		act = g_action_map_lookup_action (G_ACTION_MAP (win), "redo");
		g_simple_action_set_enabled (G_SIMPLE_ACTION (act), sensitive);

		view_node = view_node->next;
	}

}

void
find_cb (GSimpleAction *action,
         GVariant *value,
         GHexWindow *window)
{
	GtkWidget *dialog;

	dialog = g_hex_find_new (GTK_WINDOW (window));

	gtk_window_present (GTK_WINDOW (dialog));
}

void
advanced_find_cb (GSimpleAction *action,
                  GVariant *value,
                  GHexWindow *window)
{
	if (G_LIKELY (!window->find_advanced || !GTK_IS_WIDGET (window->find_advanced))) {
		window->find_advanced = g_hex_find_advanced_new (window);
	}

	gtk_window_present (GTK_WINDOW (window->find_advanced));
}


void
replace_cb (GSimpleAction *action,
            GVariant *value,
            GHexWindow *window)
{
	GtkWidget *dialog;

	dialog = g_hex_replace_new (window);

	gtk_window_present (GTK_WINDOW (dialog));
}

void
jump_cb (GSimpleAction *action,
         GVariant *value,
         GHexWindow *window)
{
	GtkWidget *dialog;

	dialog = g_hex_goto_new (window);

	gtk_window_present (GTK_WINDOW (dialog));
}

void
undo_cb (GSimpleAction *action,
         GVariant *value,
         GHexWindow *window)
{
	GHexWindow *win = GHEX_WINDOW (window);
	HexDocument *doc;
	HexChangeData *cd;

	if(win->gh == NULL)
		return;

	doc = gtk_hex_get_document (win->gh);

	if(doc->undo_top) {
		cd = (HexChangeData *)doc->undo_top->data;

		hex_document_undo(doc);

		gtk_hex_set_cursor(win->gh, cd->start);
		gtk_hex_set_nibble(win->gh, cd->lower_nibble);
	}
}

void
redo_cb (GSimpleAction *action,
         GVariant *value,
         GHexWindow *window)
{
	GHexWindow *win = GHEX_WINDOW(window);
	HexDocument *doc;
	HexChangeData *cd;

	if(win->gh == NULL)
		return;

	doc = gtk_hex_get_document (win->gh);

	if(doc->undo_stack && doc->undo_top != doc->undo_stack) {
		hex_document_redo(doc);

		cd = (HexChangeData *)doc->undo_top->data;

		gtk_hex_set_cursor(win->gh, cd->start);
		gtk_hex_set_nibble(win->gh, cd->lower_nibble);
	}
}
