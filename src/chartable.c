/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* chartable.c - a window with a character table

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "chartable.h"
#include "ghex-window.h"
#include "ui.h"

static char *ascii_non_printable_label[] = {
	"NUL",
	"SOH",
	"STX",
	"ETX",
	"EOT",
	"ENQ",
	"ACK",
	"BEL",
	"BS",
	"TAB",
	"LF",
	"VT",
	"FF",
	"CR",
	"SO",
	"SI",
	"DLE",
	"DC1",
	"DC2",
	"DC3",
	"DC4",
	"NAK",
	"SYN",
	"ETB",
	"CAN",
	"EM",
	"SUB",
	"ESC",
	"FS",
	"GS",
	"RS",
	"US"
};

struct _GHexCharacters
{
	GHexDialog parent;
};

G_DEFINE_TYPE (GHexCharacters, g_hex_characters, G_HEX_TYPE_DIALOG)

static void
g_hex_characters_class_init (GHexCharactersClass *klass)
{
}

static void
g_hex_characters_row_activated (GtkTreeView       *tree,
                                GtkTreePath       *path,
                                GtkTreeViewColumn *col,
                                GHexCharacters    *self)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GHexWindow *win = g_hex_dialog_get_window (G_HEX_DIALOG (self));
	GValue value = { 0 };

	model = gtk_tree_view_get_model (tree);

	selection = gtk_tree_view_get_selection(tree);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	gtk_tree_model_get_value(model, &iter, 2, &value);
	if(win->gh) {
		hex_document_set_byte (gtk_hex_get_document (win->gh), (guchar) atoi (g_value_get_string (&value)),
							   gtk_hex_get_cursor_pos (win->gh),
							   gtk_hex_get_insert (win->gh), TRUE);
		gtk_hex_set_cursor(win->gh, gtk_hex_get_cursor_pos (win->gh) + 1);
	}
	g_value_unset(&value);
}

static void
g_hex_characters_init (GHexCharacters *self)
{
	static gchar *fmt[] = { NULL, "%02X", "%03d", "%03o" };
	static gchar *titles[] = {  N_("ASCII"), N_("Hex"), N_("Decimal"),
								N_("Octal"), N_("Binary") };
	gchar *real_titles[5];
	GtkWidget *sw, *ctv;
	GtkListStore *store;
	GtkCellRenderer *cell_renderer;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	int i, col;
	gchar *label, ascii_printable_label[2], bin_label[9], *row[5];

	sw = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
					   "hscrollbar-policy", GTK_POLICY_NEVER,
					   "expand", TRUE,
					   "visible", TRUE,
					   NULL);

	for(i = 0; i < 5; i++)
		real_titles[i] = _(titles[i]);
	store = gtk_list_store_new (5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	cell_renderer = gtk_cell_renderer_text_new ();
	ctv = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
	for (i = 0; i < 5; i++) {
		column = gtk_tree_view_column_new_with_attributes (real_titles[i], cell_renderer, "text", i, NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW(ctv), column);
	}

	bin_label[8] = 0;
	ascii_printable_label[1] = 0;
	for (i = 0; i < 256; i++) {
		if (i < 0x20) {
			row[0] = ascii_non_printable_label[i];
		} else if (i < 0x7f) {
			ascii_printable_label[0] = i;
			row[0] = ascii_printable_label;
		} else {
			row[0] = "";
		}
		for (col = 1; col < 4; col++) {
#if defined(__GNUC__) && (__GNUC__ > 4)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
			label = g_strdup_printf(fmt[col], i);
#if defined(__GNUC__) && (__GNUC__ > 4)
#pragma GCC diagnostic pop
#endif
			row[col] = label;
		}
		for(col = 0; col < 8; col++) {
			bin_label[7-col] = (i & (1L << col))?'1':'0';
			row[4] = bin_label;
		}

		gtk_list_store_append (GTK_LIST_STORE (store), &iter);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				   0, row[0],
				   1, row[1],
				   2, row[2],
				   3, row[3],
				   4, row[4],
				   -1);

		for (col = 1; col < 4; col++)
			g_free (row[col]);
	}

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ctv));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect(G_OBJECT(ctv), "row-activated",
					 G_CALLBACK(g_hex_characters_row_activated), self);
	gtk_widget_grab_focus(ctv);

	gtk_container_add(GTK_CONTAINER(sw), ctv);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (self))), sw);
	gtk_widget_show(ctv);
}

GtkWidget *
g_hex_characters_new (GHexWindow *parent)
{
	return g_object_new (G_HEX_TYPE_CHARACTERS,
						 "use-header-bar", TRUE,
						 "modal", FALSE,
						 "transient-for", parent,
						 "title", _("Character table"),
						 "width-request", 320,
						 "height-request", 256,
						 NULL);
}

