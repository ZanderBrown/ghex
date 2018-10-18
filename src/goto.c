/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* goto.c - jump to byte

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
#include "goto.h"
#include "ui.h"

struct _GHexGoto
{
	GtkDialog parent;

	GtkWidget *entry;
};

G_DEFINE_TYPE (GHexGoto, g_hex_goto, GTK_TYPE_DIALOG)

static void
g_hex_goto_class_init (GHexGotoClass *klass)
{
}

static void
g_hex_goto_show (GHexGoto *self, gpointer data)
{
	gtk_widget_grab_focus (self->entry);
}

static void
g_hex_goto_response (GHexGoto *self, gint res, gpointer data)
{
	guint byte = 2, len, i;
	gint is_relative = 0;
	gboolean is_hex;
	const gchar *byte_str = gtk_entry_get_text (GTK_ENTRY (self->entry));
	GHexWindow *win = GHEX_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (self)));

	if (res == GTK_RESPONSE_APPLY) {
		if(win == NULL || win->gh == NULL) {
			display_error_dialog (GTK_WIDGET (win),
								  _("There is no active document to move the "
									"cursor in!"));
			return;
		}

		len = strlen(byte_str);

		if(len > 1 && byte_str[0] == '+') {
			is_relative = 1;
			byte_str++;
			len--;
		} else if(len > 1 && byte_str[0] == '-') {
			is_relative = -1;
			byte_str++;
			len--;
		}

		if(len == 0) {
			display_error_dialog (GTK_WIDGET (win), _("No offset has been specified!"));
			return;
		}

		is_hex = ((len > 2) && (byte_str[0] == '0') && (byte_str[1] == 'x'));

		if(!is_hex) {
			for(i = 0; i < len; i++)
				if(!(byte_str[i] >= '0' && byte_str[i] <= '9'))
					break;
		}
		else {
			for(i = 2; i < len; i++)
				if(!((byte_str[i] >= '0' && byte_str[i] <= '9') ||
					 (byte_str[i] >= 'A' && byte_str[i] <= 'F') ||
					 (byte_str[i] >= 'a' && byte_str[i] <= 'f')))
					break;
		}

		if((i == len) &&
		   ((sscanf(byte_str, "0x%x", &byte) == 1) ||
			(sscanf(byte_str, "%d", &byte) == 1))) {
			if(is_relative) {
				if(is_relative == -1 && byte > win->gh->cursor_pos) {
					display_error_dialog(GTK_WIDGET (win),
									     _("The specified offset is beyond the "
									       " file boundaries!"));
					return;
				}
				byte = byte * is_relative + win->gh->cursor_pos;
			}
			if(byte >= win->gh->document->file_size)
				display_error_dialog (GTK_WIDGET (win),
									  _("Can not position cursor beyond the "
									    "End Of File!"));
			else
				gtk_hex_set_cursor(win->gh, byte);
		} else {
			display_error_dialog (GTK_WIDGET (win),
								 _("You may only give the offset as:\n"
								   "  - a positive decimal number, or\n"
								   "  - a hex number, beginning with '0x', or\n"
								   "  - a '+' or '-' sign, followed by a relative offset"));
		}
	}

	gtk_widget_destroy (GTK_WIDGET (self));
}

static void
g_hex_goto_init (GHexGoto *self)
{
	GtkWidget *ok;
	GtkWidget *cancel;
	GtkWidget *wrap;
	GtkWidget *label;

	g_signal_connect (G_OBJECT (self), "delete-event", G_CALLBACK (delete_event_cb), NULL);
	g_signal_connect (G_OBJECT (self), "response", G_CALLBACK (g_hex_goto_response), NULL);
	g_signal_connect (G_OBJECT (self), "show", G_CALLBACK (g_hex_goto_show), NULL);

	self->entry = gtk_entry_new ();
	g_signal_connect_swapped (G_OBJECT (self->entry),
	                          "activate", G_CALLBACK(gtk_window_activate_default),
	                          GTK_WINDOW (self));
	gtk_widget_show (self->entry);

	ok = gtk_dialog_add_button (GTK_DIALOG (self), _("Goto"), GTK_RESPONSE_APPLY);
	cancel = gtk_dialog_add_button (GTK_DIALOG (self), _("Cancel"), GTK_RESPONSE_CANCEL);

	wrap = g_object_new (GTK_TYPE_BOX,
						 "orientation", GTK_ORIENTATION_VERTICAL,
						 "spacing", 5,
						 "margin", 16,
						 NULL);
	gtk_widget_set_halign (wrap, GTK_ALIGN_FILL);
	gtk_widget_set_valign (wrap, GTK_ALIGN_CENTER);
	gtk_widget_show (wrap);

	label = gtk_label_new (_("Go to Byte"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_END);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (wrap), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (wrap), self->entry, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))),
						wrap, TRUE, TRUE, 0);

	gtk_window_set_default (GTK_WINDOW (self), ok);
	gtk_window_set_position (GTK_WINDOW( self), GTK_WIN_POS_CENTER_ON_PARENT);

	if (GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (self->entry))) {
		add_atk_namedesc (self->entry, _("Jump to byte"), _("Enter the byte to jump to"));
		add_atk_namedesc (ok, _("OK"), _("Jumps to the specified byte"));
		add_atk_namedesc (cancel, _("Cancel"), _("Closes jump to byte window"));
	}
}

GtkWidget *
g_hex_goto_new (GtkWindow *parent)
{
  return g_object_new (G_HEX_TYPE_GOTO,
					   "use-header-bar", TRUE,
					   "modal", TRUE,
					   "transient-for", parent,
					   "resizable", FALSE,
					   NULL);
}
