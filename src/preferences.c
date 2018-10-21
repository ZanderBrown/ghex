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

#define MAX_MAX_UNDO_DEPTH 100000

static void select_font_cb(GtkWidget *w, GHexPreferences *pui);
static void select_display_font_cb(GtkWidget *w, GHexPreferences *pui);
static void max_undo_changed_cb(GtkAdjustment *adj, GHexPreferences *pui);
static void box_size_changed_cb(GtkAdjustment *adj, GHexPreferences *pui);
static void offset_cb(GtkWidget *w, GHexPreferences *pui);
static void prefs_response_cb(GHexPreferences *self, gint response, gpointer data);
static void offsets_col_cb(GtkToggleButton *tb, GHexPreferences *pui);
static void group_type_cb(GtkRadioButton *rd, GHexPreferences *pui);
static void format_activated_cb(GtkEntry *entry, GHexPreferences *pui);
static gboolean format_focus_out_event_cb(GtkEntry *entry, GdkEventFocus *event,
										  GHexPreferences *pui);

struct _GHexPreferences
{
	GHexDialog      parent;

	GtkRadioButton *group_type[3];
	GtkWidget      *font_button, *undo_spin, *box_size_spin;
	GtkWidget      *offset_menu, *offset_choice[3];
	GtkWidget      *format, *offsets_col;
	GtkWidget      *paper_sel, *print_font_sel;
	GtkWidget      *df_button, *hf_button;
	GtkWidget      *df_label, *hf_label;
};

G_DEFINE_TYPE (GHexPreferences, g_hex_prefrences, G_HEX_TYPE_DIALOG)

static void
g_hex_prefrences_class_init (GHexPreferencesClass *klass)
{
}

static void
g_hex_prefrences_init (GHexPreferences *self)
{
	GtkWidget *vbox, *label, *frame, *box, *fbox, *flabel, *grid;
	GtkAdjustment *undo_adj, *box_adj;
	GtkWidget *notebook;
	GHexApplication      *app;
	const gchar          *font;
	gboolean              show_offset;
	guint                 undo_depth;

	GSList *group;

	int i;

	gboolean gail_up;

	app = G_HEX_APPLICATION (g_application_get_default ());
	show_offset = g_hex_application_get_show_offset (app);
	undo_depth = g_hex_application_get_undo_depth (app);


	gtk_dialog_add_button (GTK_DIALOG (self), _("Close"), GTK_RESPONSE_CLOSE);
	gtk_dialog_add_button (GTK_DIALOG (self), _("Help"), GTK_RESPONSE_HELP);

	g_signal_connect(G_OBJECT(self), "response", G_CALLBACK(prefs_response_cb), NULL);

	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(self))), notebook);

	/* editing page */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox);

	/* max undo levels */
	undo_adj = GTK_ADJUSTMENT (gtk_adjustment_new (MIN (undo_depth, MAX_MAX_UNDO_DEPTH),
												   0, MAX_MAX_UNDO_DEPTH, 1, 10, 0));

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (box);

	label = gtk_label_new_with_mnemonic (_("_Maximum number of undo levels:"));
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_label_set_yalign (GTK_LABEL (label), 0.5);
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 8);
	gtk_widget_show (label);
						  
	self->undo_spin = gtk_spin_button_new (undo_adj, 1, 0);
	gtk_box_pack_end (GTK_BOX (box), GTK_WIDGET (self->undo_spin), FALSE, TRUE, 8);
	gtk_widget_show (self->undo_spin);

	gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, TRUE, 8);

	/* cursor offset format */
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (box);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->undo_spin);

	gail_up = GTK_IS_ACCESSIBLE(gtk_widget_get_accessible(label)) ;

	if (gail_up) {
		add_atk_namedesc (self->undo_spin, _("Undo levels"), _("Select maximum number of undo levels"));
		add_atk_relation (self->undo_spin, label, ATK_RELATION_LABELLED_BY);
	}

	label = gtk_label_new_with_mnemonic (_("_Show cursor offset in statusbar as:"));
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_label_set_yalign (GTK_LABEL (label), 0.5);
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 8);
	gtk_widget_show (label);

	self->format = gtk_entry_new();
	g_signal_connect (G_OBJECT (self->format), "activate",
					  G_CALLBACK (format_activated_cb), self);
	g_signal_connect (G_OBJECT (self->format), "focus_out_event",
					  G_CALLBACK (format_focus_out_event_cb), self);
	gtk_box_pack_start (GTK_BOX (box), self->format, TRUE, TRUE, 8);
	gtk_widget_show (self->format);

	self->offset_menu = gtk_combo_box_text_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL(label), self->offset_menu);
	gtk_widget_show (self->offset_menu);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->offset_menu),
								    _("Decimal"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->offset_menu),
								    _("Hexadecimal"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->offset_menu),
								    _("Custom"));
	g_signal_connect (G_OBJECT(self->offset_menu), "changed",
					  G_CALLBACK(offset_cb), self);
	gtk_box_pack_end (GTK_BOX(box), GTK_WIDGET (self->offset_menu),
					  FALSE, TRUE, 8);

	gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, TRUE, 4);

	if (gail_up) {
		add_atk_namedesc (self->format, "format_entry", _("Enter the cursor offset format"));
		add_atk_namedesc (self->offset_menu, "format_combobox", _("Select the cursor offset format"));
		add_atk_relation (label, self->format, ATK_RELATION_LABEL_FOR);
		add_atk_relation (self->format, label, ATK_RELATION_LABELLED_BY);
		add_atk_relation (label, self->offset_menu, ATK_RELATION_LABEL_FOR);
		add_atk_relation (self->offset_menu, label, ATK_RELATION_LABELLED_BY);
		add_atk_relation (self->format, self->offset_menu, ATK_RELATION_CONTROLLED_BY);
		add_atk_relation (self->offset_menu, self->format, ATK_RELATION_CONTROLLER_FOR);
	}

	/* show offsets check button */
	self->offsets_col = gtk_check_button_new_with_mnemonic (_("Sh_ow offsets column"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(self->offsets_col), show_offset);
	gtk_box_pack_start (GTK_BOX (vbox), self->offsets_col, FALSE, TRUE, 4);
	gtk_widget_show (self->offsets_col);

	label = gtk_label_new (_("Editing"));
	gtk_widget_show (label);
	gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, label);

	/* display page */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show(vbox);
	
	/* display font */
	frame = gtk_frame_new (_("Font"));
	gtk_container_set_border_width (GTK_CONTAINER(frame), 4);
	gtk_widget_show (frame);
	
	fbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	self->font_button = gtk_font_button_new ();
	font = g_hex_application_get_font_name (app);
	gtk_font_chooser_set_font (GTK_FONT_CHOOSER (self->font_button), font);
	g_signal_connect (self->font_button, "font-set",
	                  G_CALLBACK (select_display_font_cb), self);
	flabel = gtk_label_new ("");
	gtk_label_set_mnemonic_widget (GTK_LABEL (flabel), self->font_button);
	gtk_widget_show (flabel);
	gtk_widget_show (GTK_WIDGET (self->font_button));
	gtk_container_set_border_width (GTK_CONTAINER (fbox), 4);
	gtk_box_pack_start (GTK_BOX (fbox), GTK_WIDGET (self->font_button), FALSE, TRUE, 12);
	
	gtk_widget_show (fbox);
	gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (fbox));
	
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 4);
	
	/* default group type */
	frame = gtk_frame_new (_("Default Group Type"));
	gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
	gtk_widget_show (frame);

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (box);
	group = NULL;
	for(i = 0; i < 3; i++) {
		self->group_type[i] = GTK_RADIO_BUTTON (gtk_radio_button_new_with_mnemonic (group, _(group_type_label[i])));
		gtk_widget_show (GTK_WIDGET (self->group_type[i]));
		gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (self->group_type[i]), TRUE, TRUE, 2);
		group = gtk_radio_button_get_group (self->group_type[i]);
	}
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 4);
	
	label = gtk_label_new (_("Display"));
	gtk_widget_show (label);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
	
	/* printing page */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox);

	/* paper selection */
	frame = gtk_frame_new (_("Paper size"));
	gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
	gtk_widget_show (frame);

	/* data & header font selection */
	frame = gtk_frame_new (_("Fonts"));
	gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
	gtk_widget_show (frame);

	grid = gtk_grid_new ();
	g_object_set (grid,
	              "orientation", GTK_ORIENTATION_VERTICAL,
	              "row-spacing", 6,
	              "column-spacing", 12,
	              NULL);
	gtk_widget_show (grid);

	label = gtk_label_new_with_mnemonic (_("_Data font:"));
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_label_set_yalign (GTK_LABEL (label), 0.5);
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (grid), label);

	font = g_hex_application_get_data_font (app);
	self->df_button = gtk_font_button_new_with_font (font);
	g_signal_connect (G_OBJECT (self->df_button), "font_set",
					  G_CALLBACK (select_font_cb), self);
	self->df_label = gtk_label_new ("");
	gtk_label_set_mnemonic_widget (GTK_LABEL (self->df_label), self->df_button);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->df_button);

	if (gail_up) {
		add_atk_namedesc (self->df_button, _("Data font"), _("Select the data font"));
		add_atk_relation (self->df_button, label, ATK_RELATION_LABELLED_BY);
	}	
	
	gtk_widget_set_hexpand (self->df_button, TRUE);
	gtk_widget_show (self->df_button);
	gtk_grid_attach_next_to (GTK_GRID (grid), self->df_button, label,
	                         GTK_POS_RIGHT, 1, 1);

	label = gtk_label_new_with_mnemonic (_("Header fo_nt:"));
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_label_set_yalign (GTK_LABEL (label), 0.5);
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (grid), label);
	font = g_hex_application_get_header_font (app);
	self->hf_button = gtk_font_button_new_with_font (font);
	g_signal_connect (G_OBJECT (self->hf_button), "font_set",
					  G_CALLBACK (select_font_cb), self);
	self->hf_label = gtk_label_new("");
	gtk_label_set_mnemonic_widget (GTK_LABEL (self->hf_label), self->hf_button);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->hf_button);

	if (gail_up) {
		add_atk_namedesc (self->hf_button, _("Header font"), _("Select the header font"));
		add_atk_relation (self->hf_button, label, ATK_RELATION_LABELLED_BY);
	}

	gtk_widget_set_hexpand (self->hf_button, TRUE);
	gtk_widget_show(self->hf_button);
	gtk_grid_attach_next_to (GTK_GRID (grid), self->hf_button, label,
	                         GTK_POS_RIGHT, 1, 1);

	label = gtk_label_new ("");
	gtk_widget_set_hexpand (label, TRUE);
	gtk_widget_show (label);
	gtk_grid_attach (GTK_GRID (grid), label, 2, 1, 1, 1);

	gtk_container_add (GTK_CONTAINER (frame), grid);

	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
					    4);

	/* shaded box entry */
	box_adj = GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 1000, 1, 10, 0));

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (box);

	label = gtk_label_new_with_mnemonic (_("_Print shaded box over:"));
	gtk_label_set_xalign (GTK_LABEL (label), 1.0);
	gtk_label_set_yalign (GTK_LABEL (label), 0.5);
	gtk_box_pack_start (GTK_BOX(box), label, TRUE, TRUE, 4);
	gtk_widget_show (label);
						  
	self->box_size_spin = gtk_spin_button_new (box_adj, 1, 0);
	gtk_box_pack_start (GTK_BOX(box), GTK_WIDGET (self->box_size_spin), FALSE, TRUE, 8);
	gtk_widget_show (self->box_size_spin);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->box_size_spin);

	if (gail_up) {
		add_atk_namedesc (self->box_size_spin, _("Box size"), _("Select size of box (in number of lines)"));
		add_atk_relation (self->box_size_spin, label, ATK_RELATION_LABELLED_BY);
	}

	label = gtk_label_new (_("lines (0 for no box)"));
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_label_set_yalign (GTK_LABEL (label), 0.5);
	gtk_box_pack_start (GTK_BOX(box), label, FALSE, TRUE, 4);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 4);

	label = gtk_label_new (_("Printing"));
	gtk_widget_show (label);
	gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, label);

	for(i = 0; i < 3; i++)
		g_signal_connect (G_OBJECT (self->group_type[i]), "clicked",
						  G_CALLBACK(group_type_cb), self);
	g_signal_connect (G_OBJECT(self->offsets_col), "toggled",
					  G_CALLBACK(offsets_col_cb), self);
	g_signal_connect (G_OBJECT(undo_adj), "value_changed",
					  G_CALLBACK(max_undo_changed_cb), self);
	g_signal_connect (G_OBJECT(box_adj), "value_changed",
					  G_CALLBACK(box_size_changed_cb), self);
}

GtkWidget *
g_hex_prefrences_new (GHexWindow *parent)
{
	return g_object_new (G_HEX_TYPE_PREFERENCES,
						 "use-header-bar", TRUE,
						 "modal", TRUE,
						 "transient-for", parent,
						 "title", _("GHex Preferences"),
						 NULL);
}

void set_current_prefs (GHexPreferences *pui) {
	int i;
	GHexApplication *app;
	gint             group;
	guint            shaded_box;
	const gchar     *font;
	gboolean         show_offset;
	guint            undo_depth;
	const gchar     *offset_format;

	app = G_HEX_APPLICATION (g_application_get_default ());
	group = map_group_nick (g_hex_application_get_group_by (app));
	shaded_box = g_hex_application_get_shaded_box (app);
	show_offset = g_hex_application_get_show_offset (app);
	undo_depth = g_hex_application_get_undo_depth (app);
	offset_format = g_hex_application_get_offset_format (app);

	for(i = 0; i < 3; i++) {
		if(group == group_type[i]) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pui->group_type[i]), TRUE);
			break;
		}
	}
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pui->offsets_col), show_offset);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(pui->undo_spin), (gfloat) undo_depth);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pui->box_size_spin), (gfloat) shaded_box);

	gtk_widget_set_sensitive(pui->format, FALSE);
	gtk_entry_set_text (GTK_ENTRY (pui->format), offset_format);
	if (strcmp (offset_format, "%d") == 0) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (pui->offset_menu), 0);
	} else if (strcmp (offset_format, "0x%X") == 0) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (pui->offset_menu), 1);
	} else {
		gtk_combo_box_set_active (GTK_COMBO_BOX (pui->offset_menu), 2);
		gtk_widget_set_sensitive (pui->format, TRUE);
	}

	font = g_hex_application_get_header_font (app);
	gtk_font_chooser_set_font (GTK_FONT_CHOOSER (pui->hf_button), font);

	font = g_hex_application_get_data_font (app);
	gtk_font_chooser_set_font (GTK_FONT_CHOOSER (pui->df_button), font);

	font = g_hex_application_get_font_name (app);
	gtk_font_chooser_set_font (GTK_FONT_CHOOSER (pui->font_button), font);

	gtk_dialog_set_default_response (GTK_DIALOG (pui), GTK_RESPONSE_CLOSE);
}

static void
max_undo_changed_cb(GtkAdjustment *adj, GHexPreferences *pui)
{
	GHexApplication *app;
	guint undo_depth;

	app = G_HEX_APPLICATION (g_application_get_default ());
	undo_depth = g_hex_application_get_undo_depth (app);

	if ((guint) gtk_adjustment_get_value (adj) != undo_depth) {
		undo_depth = gtk_spin_button_get_value_as_int
			(GTK_SPIN_BUTTON (pui->undo_spin));
		g_hex_application_set_undo_depth (app, undo_depth);
	}
}

static void
box_size_changed_cb(GtkAdjustment *adj, GHexPreferences *pui)
{
	GHexApplication *app;
	guint shaded_box;

	app = G_HEX_APPLICATION (g_application_get_default ());
	shaded_box = g_hex_application_get_shaded_box (app);

	if ((guint) gtk_adjustment_get_value(adj) != shaded_box) {
		shaded_box = gtk_spin_button_get_value_as_int
			(GTK_SPIN_BUTTON (pui->box_size_spin));
		g_hex_application_set_shaded_box (app, shaded_box);
	}
}

static void
offsets_col_cb(GtkToggleButton *tb, GHexPreferences *pui)
{
	GHexApplication *app;
	gboolean show_offsets;

	app = G_HEX_APPLICATION (g_application_get_default ());
	show_offsets = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pui->offsets_col));
	g_hex_application_set_show_offset (app, show_offsets);
}

static void
group_type_cb(GtkRadioButton *rd, GHexPreferences *pui)
{
	int i;
	GHexApplication *app;
	gint group;

	app = G_HEX_APPLICATION (g_application_get_default ());
	group = map_group_nick (g_hex_application_get_group_by (app));

	for(i = 0; i < 3; i++) {
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pui->group_type[i]))) {
			group = group_type[i];
			break;
		}
	}

	g_hex_application_set_group_by (app, map_nick_group (group));
}

static void
prefs_response_cb(GHexPreferences *self, gint response, gpointer data)
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
select_display_font_cb(GtkWidget *w, GHexPreferences *pui)
{
	GHexApplication *app;
	const gchar *font;

	app = G_HEX_APPLICATION (g_application_get_default ());
	font = g_hex_application_get_font_name (app);

	if (strcmp (gtk_font_chooser_get_font (GTK_FONT_CHOOSER (pui->font_button)), font) != 0) {
		font = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (pui->font_button));
		g_hex_application_set_font_name (app, font);
	}
}

static void
select_font_cb(GtkWidget *w, GHexPreferences *pui)
{
	GHexApplication *app;
	const gchar *font;

	app = G_HEX_APPLICATION (g_application_get_default ());

	if(w == pui->df_button) {
		font = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (pui->df_button));
		g_hex_application_set_data_font (app, font);
	}
	else if(w == pui->hf_button) {
		font = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (pui->hf_button));
		g_hex_application_set_header_font (app, font);
	}
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
