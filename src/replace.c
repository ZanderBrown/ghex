/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* replace.c - replaceing data

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
#include "replace.h"
#include "ui.h"

struct _GHexReplace
{
	GHexDialog parent;

	GtkWidget *f_gh, *r_gh;
	HexDocument *f_doc, *r_doc;
};

G_DEFINE_TYPE (GHexReplace, g_hex_replace, G_HEX_TYPE_DIALOG)

#define REPLACE_FIND 1
#define REPLACE_REPLACE_ALL 2

static void
g_hex_replace_class_init (GHexReplaceClass *klass)
{
}

static void
g_hex_replace_response (GHexReplace *self, gint res, gpointer data)
{
	GHexWindow *win = GHEX_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (self)));
	GtkHex     *gh = win->gh;
	gchar      *str = NULL, *rep_str = NULL;
	guint       str_len, rep_len;

	if (res != REPLACE_FIND &&
		  res != REPLACE_REPLACE_ALL &&
		  res != GTK_RESPONSE_APPLY) {
		gtk_widget_destroy (GTK_WIDGET (self));
		return;
	}

	if (win == NULL || win->gh == NULL) {
		display_error_dialog (GTK_WIDGET (win), _("There is no active document to search!"));
		return;
	}

	if ((str_len = get_search_string (self->f_doc, &str)) == 0) {
		display_error_dialog (GTK_WIDGET (win), _("There is no string to search for!"));
		return;
	}

	if (res == REPLACE_FIND) {
		guint offset;

		if (hex_document_find_forward (gtk_hex_get_document (gh),
									   gtk_hex_get_cursor_pos (gh)+1, (guchar *) str, str_len, &offset))
			gtk_hex_set_cursor (gh, offset);
		else {
			display_info_dialog (GTK_WINDOW (win), _("String was not found!\n"));
			ghex_window_flash (win, _("End Of File reached"));
		}
	} else if (res == GTK_RESPONSE_APPLY) {
		guint offset;
		HexDocument *doc = gtk_hex_get_document (win->gh);

		rep_len = get_search_string (self->r_doc, &rep_str);

		if (str_len > hex_document_get_file_size (doc) - gtk_hex_get_cursor_pos (gh))
			goto clean_up;

		if (hex_document_compare_data (doc, (guchar *) str, gtk_hex_get_cursor_pos (gh), str_len) == 0)
			hex_document_set_data (doc, gtk_hex_get_cursor_pos (gh),
								   rep_len, str_len, (guchar *) rep_str, TRUE);

		if (hex_document_find_forward(doc, gtk_hex_get_cursor_pos (gh) + rep_len, (guchar *) str, str_len,
									  &offset)) {
			gtk_hex_set_cursor (gh, offset);
		} else {
			display_info_dialog (GTK_WINDOW (win), _("End Of File reached!"));
			ghex_window_flash (win, _("End Of File reached!"));
		}
	} else {
		gchar *flash;
		guint offset, count, cursor_pos;
		HexDocument *doc = gtk_hex_get_document (gh);

		rep_len = get_search_string (self->r_doc, &rep_str);

		if(str_len > hex_document_get_file_size (doc) - gtk_hex_get_cursor_pos (gh))
			goto clean_up;

		count = 0;
		cursor_pos = 0;

		while (hex_document_find_forward (doc, cursor_pos, (guchar *) str, str_len,
										  &offset)) {
			hex_document_set_data (doc, offset, rep_len, str_len, (guchar *) rep_str, TRUE);
			cursor_pos = offset + rep_len;
			count++;
		}

		gtk_hex_set_cursor (gh, MIN(offset, hex_document_get_file_size (doc)));

		if(count == 0) {
			display_info_dialog (GTK_WINDOW (win), _("No occurrences were found."));
		}

		flash = g_strdup_printf (ngettext ("Replaced %d occurrence.",
										   "Replaced %d occurrences.",
										   count), count);
		ghex_window_flash (win, flash);
		g_free (flash);
	}


  clean_up:
	g_free (rep_str);
	g_free (str);
}

static void
g_hex_replace_init (GHexReplace *self)
{
	GtkWidget *replace, *replace_all, *next, *close;
	GtkWidget *label;
	GtkWidget *wrap;

	g_signal_connect (G_OBJECT (self), "response", G_CALLBACK (g_hex_replace_response), NULL);

	wrap = g_object_new (GTK_TYPE_BOX,
						 "orientation", GTK_ORIENTATION_VERTICAL,
						 "spacing", 5,
						 "margin", 16,
						 NULL);

	label = gtk_label_new (_("Find"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_END);
	gtk_box_pack_start (GTK_BOX (wrap), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	self->f_doc = hex_document_new ();
	self->f_gh = create_hex_view (self->f_doc);
	gtk_box_pack_start (GTK_BOX (wrap), self->f_gh, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (self->f_gh), 0);
	gtk_widget_show (self->f_gh);

	label = gtk_label_new (_("Replace With"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_END);
	gtk_box_pack_start (GTK_BOX (wrap), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	self->r_doc = hex_document_new ();
	self->r_gh = create_hex_view (self->r_doc);
	gtk_box_pack_start (GTK_BOX (wrap), self->r_gh, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (self->r_gh), 0);
	gtk_widget_show (self->r_gh);

	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))),
						wrap, TRUE, TRUE, 0);
	gtk_widget_show (wrap);

	replace = gtk_dialog_add_button (GTK_DIALOG (self), _("Replace"), GTK_RESPONSE_APPLY);
	next = gtk_dialog_add_button (GTK_DIALOG (self), _("Find"), REPLACE_FIND);
	replace_all= gtk_dialog_add_button (GTK_DIALOG (self), _("Replace _All"), REPLACE_REPLACE_ALL);
	close = gtk_dialog_add_button (GTK_DIALOG (self), _("Cancel"), GTK_RESPONSE_CANCEL);

	if (GTK_IS_ACCESSIBLE(gtk_widget_get_accessible(self->f_gh))) {
		add_atk_namedesc (self->f_gh, _("Find Data"), _("Enter the hex data or ASCII data to search for"));
		add_atk_namedesc (self->r_gh, _("Replace Data"), _("Enter the hex data or ASCII data to replace with"));
		add_atk_namedesc (next, _("Find next"), _("Finds the next occurrence of the search string"));
		add_atk_namedesc (replace, _("Replace"), _("Replaces the search string with the replace string"));
		add_atk_namedesc (replace_all, _("Replace All"), _("Replaces all occurrences of the search string with the replace string"));
		add_atk_namedesc (close, _("Cancel"), _("Closes find and replace data window"));
	}
}

GtkWidget *
g_hex_replace_new (GHexWindow *parent)
{
  return g_object_new (G_HEX_TYPE_REPLACE,
					   "use-header-bar", TRUE,
					   "modal", FALSE,
					   "transient-for", parent,
					   "title", _("Find & Replace"),
					   NULL);
}

