/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* preferences.c - setting the preferences

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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "preferences.h"
#include "configuration.h"
#include "ghex-window.h"
#include "ui.h"
#include "application.h"

static void offset_cb(GtkWidget *w, GHexPreferences *pui);
static void format_activated_cb(GtkEntry *entry, GHexPreferences *pui);
static gboolean format_focus_out_event_cb(GtkEntry *entry, GdkEventFocus *event,
										  GHexPreferences *pui);

struct _GHexPreferences
{
	GHexDialog      parent;

	GtkWidget      *offset_menu;
	GtkWidget      *format;
};

G_DEFINE_TYPE (GHexPreferences, g_hex_preferences, G_HEX_TYPE_DIALOG)

static void
g_hex_preferences_response (GtkDialog *self, gint response)
{
	GError *error = NULL;

	switch(response) {
	case GTK_RESPONSE_HELP:
		gtk_show_uri_on_window (GTK_WINDOW (self), "help:ghex/ghex-prefs", GDK_CURRENT_TIME, &error);
		if(NULL != error) {
			GtkWidget *dialog;
			dialog = gtk_message_dialog_new
				(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				 _("There was an error displaying help: \n%s"),
				 error->message);
			g_signal_connect (G_OBJECT (dialog), "response",
							  G_CALLBACK (gtk_widget_destroy),
							  NULL);
			gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
			gtk_window_present (GTK_WINDOW (dialog));

			g_error_free(error);
		}
		break;
	default:
		gtk_widget_destroy (GTK_WIDGET (self));
		break;
	}
}

static void
g_hex_preferences_class_init (GHexPreferencesClass *klass)
{
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	dialog_class->response = g_hex_preferences_response;
}

static GtkWidget *
g_hex_preferences_add_row (GtkWidget   *list,
                            const gchar *title,
                            GtkWidget   *child)
{
	GtkWidget *row;
	GtkWidget *wrap;
	GtkWidget *label;

	row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
						"activatable", FALSE,
						"visible", TRUE,
						NULL);

	wrap = g_object_new (GTK_TYPE_BOX,
						"orientation", GTK_ORIENTATION_HORIZONTAL,
						"spacing", 8,
						"margin", 8,
						"visible", TRUE,
						NULL);

	label = g_object_new (GTK_TYPE_LABEL,
						  "label", title,
						  "use-underline", TRUE,
						  "mnemonic-widget", child,
						  "halign", GTK_ALIGN_START,
						  "valign", GTK_ALIGN_CENTER,
						  "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
						  "visible", TRUE,
						  NULL);
	gtk_box_pack_start (GTK_BOX (wrap), label, TRUE, TRUE, 0);

	gtk_box_pack_end (GTK_BOX (wrap), child, FALSE, FALSE, 0);

	add_atk_relation (child, label, ATK_RELATION_LABELLED_BY);

	gtk_container_add (GTK_CONTAINER (row), wrap);

	gtk_container_add (GTK_CONTAINER (list), row);

	return row;
}

void
g_hex_preferences_header_func (GtkListBoxRow *row,
                              GtkListBoxRow *before,
                              gpointer user_data)
{
	GtkWidget *current;

	if (G_UNLIKELY (!before)) {
		gtk_list_box_row_set_header (row, NULL);
		return;
	}

	current = gtk_list_box_row_get_header (row);
	if (!current) {
		current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
		gtk_widget_show (current);
		gtk_list_box_row_set_header (row, current);
	}
}

static void
g_hex_preferences_init (GHexPreferences *self)
{
	GtkWidget *vbox, *label, *frame;
	GHexApplication      *app;

	GtkWidget     *list;
	GtkWidget     *control;
	GtkAdjustment *adj;

	gboolean gail_up;

	const gchar *offset_format;

	app = G_HEX_APPLICATION (g_application_get_default ());
	offset_format = g_hex_application_get_offset_format (app);

	gtk_dialog_add_button (GTK_DIALOG (self), _("_Done"), GTK_RESPONSE_CLOSE);
	gtk_dialog_add_button (GTK_DIALOG (self), _("_Help"), GTK_RESPONSE_HELP);
	gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_CLOSE);

	vbox = g_object_new (GTK_TYPE_BOX,
						 "orientation", GTK_ORIENTATION_VERTICAL,
						 "spacing", 8,
						 "margin", 16,
						 "visible", TRUE,
						 NULL);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (self))), vbox);

	frame = gtk_frame_new (NULL);

	list = g_object_new (GTK_TYPE_LIST_BOX,
						 "selection-mode", GTK_SELECTION_NONE,
						 "visible", TRUE,
						 "width-request", 450,
						 NULL);
	gtk_list_box_set_header_func (GTK_LIST_BOX (list), g_hex_preferences_header_func, NULL, NULL);

	/* max undo levels */
	adj = GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, G_HEX_MAX_MAX_UNDO_DEPTH, 1, 10, 0));
	g_object_bind_property (G_OBJECT (app), "undo-depth", G_OBJECT (adj),
						"value", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	control = gtk_spin_button_new (adj, 1, 0);
	gtk_widget_show (control);

	gail_up = GTK_IS_ACCESSIBLE (GTK_WIDGET (self));

	if (gail_up) {
		add_atk_namedesc (control, _("Undo levels"), _("Select maximum number of undo levels"));
	}
	g_hex_preferences_add_row (list, _("_Max undo levels"), control);

	/* cursor offset format */
	control = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_style_context_add_class (gtk_widget_get_style_context (control), GTK_STYLE_CLASS_LINKED);
	gtk_widget_show (control);

	self->format = gtk_entry_new();
	gtk_widget_set_sensitive (self->format, FALSE);
	gtk_entry_set_text (GTK_ENTRY (self->format), offset_format);
	g_signal_connect (G_OBJECT (self->format), "activate",
					  G_CALLBACK (format_activated_cb), self);
	g_signal_connect (G_OBJECT (self->format), "focus_out_event",
					  G_CALLBACK (format_focus_out_event_cb), self);
	gtk_box_pack_start (GTK_BOX (control), self->format, TRUE, TRUE, 0);
	gtk_widget_show (self->format);

	self->offset_menu = gtk_combo_box_text_new ();
	gtk_widget_show (self->offset_menu);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->offset_menu),
								    _("Decimal"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->offset_menu),
								    _("Hexadecimal"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->offset_menu),
								    _("Custom"));
	if (strcmp (offset_format, "%d") == 0) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->offset_menu), 0);
	} else if (strcmp (offset_format, "0x%X") == 0) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->offset_menu), 1);
	} else {
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->offset_menu), 2);
		gtk_widget_set_sensitive (self->format, TRUE);
	}
	g_signal_connect (G_OBJECT(self->offset_menu), "changed",
					  G_CALLBACK(offset_cb), self);
	gtk_box_pack_end (GTK_BOX (control), GTK_WIDGET (self->offset_menu),
					  FALSE, TRUE, 0);
	if (gail_up) {
		add_atk_namedesc (self->format, "format_entry", _("Enter the cursor offset format"));
		add_atk_namedesc (self->offset_menu, "format_combobox", _("Select the cursor offset format"));
		add_atk_relation (self->format, self->offset_menu, ATK_RELATION_CONTROLLED_BY);
		add_atk_relation (self->offset_menu, self->format, ATK_RELATION_CONTROLLER_FOR);
	}
	g_hex_preferences_add_row (list, _("Off_set format"), control);

	/* Offset column */
	control = gtk_switch_new ();
	gtk_widget_show (control);
	g_object_bind_property (G_OBJECT (app), "show-offset", G_OBJECT (control),
							"active", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_hex_preferences_add_row (list, _("Offsets c_olumn"), control);

	/* Display font */
	control = gtk_font_button_new ();
	gtk_widget_show (control);
	g_object_bind_property (G_OBJECT (app), "font-name", G_OBJECT (control),
							"font", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_hex_preferences_add_row (list, _("_Font"), control);
	
	/* Grouping type */
	control = gtk_combo_box_text_new ();
	gtk_widget_show (control);
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (control), "bytes",_("Bytes"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (control), "words",_("Words"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (control), "longwords", _("Longwords"));
	g_object_bind_property (G_OBJECT (app), "group-by", G_OBJECT (control),
							"active-id", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_hex_preferences_add_row (list, _("Default _grouping"), control);


	gtk_container_add (GTK_CONTAINER (frame), list);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
	gtk_widget_show (frame);


	label = g_object_new (GTK_TYPE_LABEL,
						  "label", _("Printing"),
						  "halign", GTK_ALIGN_START,
						  "margin-top", 8,
						  NULL);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
	gtk_widget_show (label);

	frame = gtk_frame_new (NULL);

	list = g_object_new (GTK_TYPE_LIST_BOX,
						 "selection-mode", GTK_SELECTION_NONE,
						 "visible", TRUE,
						 "width-request", 450,
						 NULL);
	gtk_list_box_set_header_func (GTK_LIST_BOX (list), g_hex_preferences_header_func, NULL, NULL);

	/* Data print font */
	control = gtk_font_button_new ();
	gtk_widget_show (control);
	g_object_bind_property (G_OBJECT (app), "data-font", G_OBJECT (control),
							"font", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	if (gail_up) {
		add_atk_namedesc (control, _("Data font"), _("Select the data font"));
	}
	g_hex_preferences_add_row (list, _("_Data font"), control);

	/* Header print font */
	control = gtk_font_button_new ();
	gtk_widget_show (control);
	g_object_bind_property (G_OBJECT (app), "header-font", G_OBJECT (control),
							"font", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	if (gail_up) {
		add_atk_namedesc (control, _("Header font"), _("Select the header font"));
	}
	g_hex_preferences_add_row (list, _("Header fo_nt"), control);

	/* shaded box entry */
	adj = GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 1000, 1, 10, 0));
	g_object_bind_property (G_OBJECT (app), "shaded-box", G_OBJECT (adj),
						"value", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	control = gtk_spin_button_new (adj, 1, 0);
	gtk_widget_show (control);
	if (gail_up) {
		add_atk_namedesc (control, _("Box size"), _("Select size of box (in number of lines)"));
	}
	g_hex_preferences_add_row (list, _("Lines to _print shaded box over"), control);

	gtk_container_add (GTK_CONTAINER (frame), list);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
	gtk_widget_show (frame);
}

GtkWidget *
g_hex_preferences_new (GHexWindow *parent)
{
	return g_object_new (G_HEX_TYPE_PREFERENCES,
						 "use-header-bar", TRUE,
						 "modal", TRUE,
						 "transient-for", parent,
						 "resizable", FALSE,
						 "title", _("GHex Preferences"),
						 NULL);
}

static void
update_offset_fmt_from_entry(GtkEntry *entry, GHexPreferences *pui)
{
	int i, len;
	gboolean expect_spec;
	GList *win_list;
	GHexApplication *app;
	const gchar *format;
	const gchar *new_format;

	app = G_HEX_APPLICATION (g_application_get_default ());
	format = g_hex_application_get_offset_format (app);

	new_format = g_strdup(gtk_entry_get_text(GTK_ENTRY(pui->format)));

	/* check for a valid format string */
	len = strlen(new_format);
	expect_spec = FALSE;
	for(i = 0; i < len; i++) {
		if(new_format[i] == '%')
			expect_spec = TRUE;
		if( expect_spec &&
			( (new_format[i] >= 'a' && new_format[i] <= 'z') ||
			  (new_format[i] >= 'A' && new_format[i] <= 'Z') ) ) {
			expect_spec = FALSE;
			if(new_format[i] != 'x' && new_format[i] != 'd' &&
			   new_format[i] != 'o' && new_format[i] != 'X' &&
			   new_format[i] != 'P' && new_format[i] != 'p') {
				GtkWidget *msg_dialog;

				new_format = format;
				gtk_entry_set_text(GTK_ENTRY(pui->format), format);
				msg_dialog =
					gtk_message_dialog_new (GTK_WINDOW (pui),
										    GTK_DIALOG_MODAL|
										    GTK_DIALOG_DESTROY_WITH_PARENT,
										    GTK_MESSAGE_ERROR,
										    GTK_BUTTONS_OK,
										    _("The offset format string contains invalid format specifier.\n"
										      "Only 'x', 'X', 'p', 'P', 'd' and 'o' are allowed."));
				gtk_dialog_run(GTK_DIALOG(msg_dialog));
				gtk_widget_destroy(msg_dialog);
				gtk_widget_grab_focus(pui->format);
				break;
			}
		}
	}

	g_hex_application_set_offset_format (app, new_format);

	win_list = gtk_application_get_windows (GTK_APPLICATION (app));
	while (win_list) {
		if (GHEX_IS_WINDOW (win_list->data)) {
			ghex_window_update_status_message(GHEX_WINDOW (win_list->data));
		}
		win_list = g_list_next (win_list);
	}
}

static gboolean
format_focus_out_event_cb(GtkEntry *entry, GdkEventFocus *event, GHexPreferences *pui)
{
	update_offset_fmt_from_entry(entry, pui);
	return FALSE;
}

static void
format_activated_cb(GtkEntry *entry, GHexPreferences *pui)
{
	update_offset_fmt_from_entry(entry, pui);
}

static void
offset_cb(GtkWidget *w, GHexPreferences *pui)
{
	int i = gtk_combo_box_get_active(GTK_COMBO_BOX(w)); 

	switch(i) {
	case 0:
		gtk_entry_set_text(GTK_ENTRY(pui->format), "%d");
		gtk_widget_set_sensitive(pui->format, FALSE);
		break;
	case 1:
		gtk_entry_set_text(GTK_ENTRY(pui->format), "0x%X");
		gtk_widget_set_sensitive(pui->format, FALSE);
		break;
	case 2:
		gtk_widget_set_sensitive(pui->format, TRUE);
		break;
	}
	update_offset_fmt_from_entry(GTK_ENTRY(pui->format), pui);
}
