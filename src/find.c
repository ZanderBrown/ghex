/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* find.c - finding data

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gtkhex.h"
#include "find.h"
#include "ui.h"

struct _GHexFind
{
	GtkDialog parent;

	HexDocument *f_doc;

	GtkHex_AutoHighlight *auto_highlight;
};

G_DEFINE_TYPE (GHexFind, g_hex_find, GTK_TYPE_DIALOG)

#define FIND_PREV 1

static void
g_hex_find_class_init (GHexFindClass *klass)
{
}

static void
g_hex_find_response (GHexFind *self, gint res, gpointer data)
{
	guint offset, str_len;
	gchar *str;
	GHexWindow *win = GHEX_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (self)));
	GtkHex *gh = win->gh;

	if (res != GTK_RESPONSE_APPLY && res != FIND_PREV) {
		if (self->auto_highlight) gtk_hex_delete_autohighlight(gh, self->auto_highlight);
		self->auto_highlight = NULL;

		gtk_widget_destroy (GTK_WIDGET (self));
		return;
	}

	if(win->gh == NULL) {
		display_error_dialog (win, _("There is no active document to search!"));
		return;
	}

	if((str_len = get_search_string (self->f_doc, &str)) == 0) {
		display_error_dialog (win, _("There is no string to search for!"));
		return;
	}

	if (self->auto_highlight) gtk_hex_delete_autohighlight (gh, self->auto_highlight);
	self->auto_highlight = NULL;
	self->auto_highlight = gtk_hex_insert_autohighlight (gh, str, str_len, "red");

	if (res == FIND_PREV) {
		if (hex_document_find_backward (gh->document,
										gh->cursor_pos, (guchar *) str, str_len, &offset)) {
			gtk_hex_set_cursor (gh, offset);
		} else {
			ghex_window_flash (win, _("Beginning Of File reached"));
			display_info_dialog (win, _("String was not found!\n"));
		}
	} else {
		if(hex_document_find_forward(gh->document,
									 gh->cursor_pos+1, (guchar *) str, str_len, &offset)) {
			gtk_hex_set_cursor(gh, offset);
		} else {
			ghex_window_flash(win, _("End Of File reached"));
			display_info_dialog(win, _("String was not found!\n"));
		}
	}

	if(str)
		g_free(str);
}

static void
g_hex_find_init (GHexFind *self)
{
	GtkWidget *f_next, *f_prev, *f_close;
	GtkWidget *f_gh;

	g_signal_connect (G_OBJECT (self), "response", G_CALLBACK (g_hex_find_response), NULL);

	self->f_doc = hex_document_new ();
	f_gh = create_hex_view (self->f_doc);
	gtk_widget_set_vexpand (GTK_WIDGET (f_gh), TRUE);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (self))),
					   f_gh);
	gtk_widget_show (f_gh);

	f_next = gtk_dialog_add_button (GTK_DIALOG (self), _("Next"), GTK_RESPONSE_APPLY);
	f_prev = gtk_dialog_add_button (GTK_DIALOG (self), _("Previous"), FIND_PREV);
	f_close = gtk_dialog_add_button (GTK_DIALOG (self), _("Close"), GTK_RESPONSE_CANCEL);

	gtk_window_set_position (GTK_WINDOW (self), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_default (GTK_WINDOW (self), f_next);

	if (GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (f_gh))) {
		add_atk_namedesc (f_gh, _("Find Data"), _("Enter the hex data or ASCII data to search for"));
		add_atk_namedesc (f_next, _("Find Next"), _("Finds the next occurrence of the search string"));
		add_atk_namedesc (f_prev, _("Find previous"), _("Finds the previous occurrence of the search string "));
		add_atk_namedesc (f_close, _("Cancel"), _("Closes find data window"));
	}
}

GtkWidget *
g_hex_find_new (GtkWindow *parent)
{
  return g_object_new (G_HEX_TYPE_FIND,
					   "use-header-bar", TRUE,
					   "modal", FALSE,
					   "transient-for", parent,
					   "title", _("Find"),
					   NULL);
}

