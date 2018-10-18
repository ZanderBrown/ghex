/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* findreplace.c - finding & replacing data

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
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "findreplace.h"
#include "ui.h"
#include "gtkhex.h"
#include "configuration.h"


struct _GHexFindAdd
{
	GtkDialog    parent;

	GtkWidget   *f_gh;
	HexDocument *f_doc;
	GtkWidget   *colour;
};

G_DEFINE_TYPE (GHexFindAdd, g_hex_find_add, GTK_TYPE_DIALOG)

static void
g_hex_find_add_class_init (GHexFindAddClass *klass)
{
}

static void
g_hex_find_add_init (GHexFindAdd *self)
{
	GtkWidget *wrap;
	GtkWidget *label;
	GtkWidget *ok;

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
	gtk_widget_set_vexpand (self->f_gh, TRUE);
	gtk_widget_set_hexpand (self->f_gh, TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (self->f_gh), 0);
	gtk_box_pack_start (GTK_BOX (wrap), self->f_gh, TRUE, TRUE, 0);
	gtk_widget_show (self->f_gh);

	label = gtk_label_new (_("Hightlight"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_END);
	gtk_box_pack_start (GTK_BOX (wrap), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	self->colour = gtk_color_button_new ();
	gtk_widget_set_halign (self->colour, GTK_ALIGN_START);
	gtk_widget_set_valign (self->colour, GTK_ALIGN_END);
	gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (self->colour), FALSE);
	gtk_box_pack_start (GTK_BOX (wrap), self->colour, FALSE, FALSE, 0);
	gtk_widget_show (self->colour);

	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))),
					    wrap, TRUE, TRUE, 0);
	gtk_widget_show (wrap);

	ok = gtk_dialog_add_button (GTK_DIALOG (self), _("Done"), GTK_RESPONSE_APPLY);
	gtk_window_set_default (GTK_WINDOW (self), ok);
	gtk_dialog_add_button (GTK_DIALOG (self), _("Cancel"), GTK_RESPONSE_CANCEL);

	gtk_window_set_position (GTK_WINDOW (self), GTK_WIN_POS_CENTER_ON_PARENT);
}

GtkWidget *
g_hex_find_add_new (GtkWindow *parent)
{
  return g_object_new (G_HEX_TYPE_FIND_ADD,
					   "use-header-bar", TRUE,
					   "transient-for", parent,
					   "title", _("Add search"),
					   NULL);
}


static gint advanced_find_delete_event_cb(GtkWidget *w, GdkEventAny *e,
										  AdvancedFindDialog *dialog);
static void advanced_find_close_cb(GtkWidget *w, AdvancedFindDialog *dialog);

static void advanced_find_add_cb(GtkButton *button, AdvancedFindDialog *);
static void advanced_find_delete_cb(GtkButton *button, AdvancedFindDialog *dialog);
static void advanced_find_next_cb(GtkButton *button, AdvancedFindDialog *dialog);
static void advanced_find_prev_cb(GtkButton *button, AdvancedFindDialog *dialog);


/* basic structure to hold private information to be stored in the
 * gtk list.
 */
typedef struct
{
	gchar *str;
	gint str_len;
	GtkHex_AutoHighlight *auto_highlight;
} AdvancedFind_ListData;

AdvancedFindDialog *create_advanced_find_dialog(GHexWindow *parent)
{
	AdvancedFindDialog *dialog;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	GtkWidget *sep;

	dialog = g_new0(AdvancedFindDialog, 1);

	dialog->parent = parent;

	dialog->window = gtk_dialog_new();
	g_signal_connect(G_OBJECT(dialog->window), "delete_event",
					 G_CALLBACK(advanced_find_delete_event_cb), dialog);

	gtk_window_set_default_size(GTK_WINDOW(dialog->window), 300, 350);

	create_dialog_title(dialog->window, _("GHex (%s): Find Data"));

	dialog->hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog->window))),
					   dialog->hbox, TRUE, TRUE, 4);
	gtk_widget_show(dialog->hbox);

	dialog->list = gtk_list_store_new(3,
									  G_TYPE_STRING, G_TYPE_STRING,
			                          G_TYPE_POINTER, G_TYPE_POINTER);
	dialog->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (dialog->list));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Search String"),
													   renderer,
													   "text", 0,
													   "foreground", 1,
													   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->tree), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Highlight Colour"),
													   renderer,
													   "background", 1,
													   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->tree), column);

	gtk_box_pack_start(GTK_BOX(dialog->hbox), dialog->tree,
					   TRUE, TRUE, 4);
	gtk_widget_show (dialog->tree);

	dialog->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(dialog->hbox), dialog->vbox,
					   FALSE, FALSE, 4);
	gtk_widget_show(dialog->vbox);

	dialog->f_next = gtk_button_new_with_label (_("Find Next"));
	gtk_box_pack_start(GTK_BOX(dialog->vbox), dialog->f_next,
					   FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (dialog->f_next),
					  "clicked", G_CALLBACK(advanced_find_next_cb),
					  dialog);
	gtk_widget_set_can_default(dialog->f_next, TRUE);
	gtk_widget_show(dialog->f_next);

	dialog->f_prev = gtk_button_new_with_label (_("Find Previous"));
	gtk_box_pack_start(GTK_BOX(dialog->vbox), dialog->f_prev,
					   FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (dialog->f_prev),
					  "clicked", G_CALLBACK(advanced_find_prev_cb),
					  dialog);
	gtk_widget_set_can_default(dialog->f_prev, TRUE);
	gtk_widget_show(dialog->f_prev);

	sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(dialog->vbox), sep, FALSE, FALSE, 4);
	gtk_widget_show(sep);

	dialog->f_new = gtk_button_new_with_label(_("Add New"));
	gtk_box_pack_start(GTK_BOX(dialog->vbox), dialog->f_new,
					   FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (dialog->f_new),
					  "clicked", G_CALLBACK(advanced_find_add_cb),
					  dialog);
	gtk_widget_set_can_default(dialog->f_new, TRUE);
	gtk_widget_show(dialog->f_new);

	dialog->f_remove = gtk_button_new_with_label( _("Remove Selected"));
	gtk_box_pack_start(GTK_BOX(dialog->vbox), dialog->f_remove,
					   FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (dialog->f_remove),
					  "clicked", G_CALLBACK(advanced_find_delete_cb),
					  dialog);
	gtk_widget_set_can_default(dialog->f_remove, TRUE);
	gtk_widget_show(dialog->f_remove);

	dialog->f_close = gtk_dialog_add_button (GTK_DIALOG (dialog->window),
											 _("Close"),
											 GTK_RESPONSE_CLOSE);
	g_signal_connect(G_OBJECT(dialog->f_close),
					 "clicked", G_CALLBACK(advanced_find_close_cb),
					 dialog);

	if (GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (dialog->f_close)))
	{
		add_atk_namedesc(dialog->f_close, _("Close"), _("Closes advanced find window"));
	}

	return dialog;
}

static gboolean advanced_find_foreachfunc_cb (GtkTreeModel *model,
                                              GtkTreePath  *path,
                                              GtkTreeIter  *iter,
                                              gpointer      data)
{
	AdvancedFind_ListData *udata;
	GtkHex *gh = (GtkHex *)data;
	gtk_tree_model_get(model, iter, 2, &udata, -1);
	gtk_hex_delete_autohighlight(gh, udata->auto_highlight);
	if(NULL != udata->str)
		g_free(udata->str);
	g_free(udata);
	return FALSE;
}

void delete_advanced_find_dialog(AdvancedFindDialog *dialog)
{
	gtk_tree_model_foreach(GTK_TREE_MODEL(dialog->list),
						   advanced_find_foreachfunc_cb, (gpointer *)dialog->parent->gh);
	g_free(dialog);
}

static gint advanced_find_delete_event_cb(GtkWidget *w, GdkEventAny *e,
										  AdvancedFindDialog *dialog)
{
	gtk_widget_hide(w);

	return TRUE;
}

static void advanced_find_close_cb(GtkWidget *w, AdvancedFindDialog *dialog)
{
	gtk_widget_hide(dialog->window);
}

static void advanced_find_add_cb(GtkButton *button, AdvancedFindDialog *dialog)
{
	GtkWidget *dlg;
	gint       ret;

	dlg = g_hex_find_add_new (GTK_WINDOW (dialog->window));

	gtk_widget_show (dlg);
	ret = gtk_dialog_run (GTK_DIALOG (dlg));

	if (ret == GTK_RESPONSE_APPLY) {
		gchar *colour;
		GdkRGBA rgba;
		AdvancedFind_ListData *data = g_new0(AdvancedFind_ListData, 1);
		GtkHex *gh = dialog->parent->gh;
		GtkTreeIter iter;
		
		g_return_if_fail (gh != NULL);
		
		if ((data->str_len = get_search_string (G_HEX_FIND_ADD (dlg)->f_doc, &data->str)) == 0) {
			display_error_dialog (dlg, _("No string to search for!"));
			gtk_widget_destroy (dlg);
			return;
		}
		gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (G_HEX_FIND_ADD (dlg)->colour), &rgba);
		colour = gdk_rgba_to_string (&rgba);
		data->auto_highlight = gtk_hex_insert_autohighlight (gh, data->str, data->str_len, colour);
		gtk_list_store_append (dialog->list, &iter);
		gtk_list_store_set (dialog->list, &iter,
						    0, data->str,
						    1, colour,
						    2, data,
						    -1);
		g_free(colour);
	}

	gtk_widget_destroy (dlg);
}

static void advanced_find_delete_cb(GtkButton *button, AdvancedFindDialog *dialog)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tree));
	GtkTreeIter iter;
	GtkTreeModel *model;
	AdvancedFind_ListData *data;
	GtkHex *gh = dialog->parent->gh;

	if (gtk_tree_selection_get_selected(selection, &model, &iter) != TRUE)
		return;
	
	gtk_tree_model_get(model, &iter, 2, &data, -1);
	gtk_hex_delete_autohighlight(gh, data->auto_highlight);
	if(NULL != data->str)
		g_free(data->str);
	g_free(data);
	gtk_list_store_remove(dialog->list, &iter);
}

static void advanced_find_next_cb(GtkButton *button, AdvancedFindDialog *dialog)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tree));
	GtkTreeIter iter;
	GtkTreeModel *model;
	AdvancedFind_ListData *data;
	GtkHex *gh = dialog->parent->gh;
	guint offset;
	GHexWindow *win = ghex_window_get_active();

	if (gtk_tree_selection_get_selected(selection, &model, &iter) != TRUE)
		return;
	
	gtk_tree_model_get(model, &iter, 2, &data, -1);
	if(hex_document_find_forward(gh->document,
								 gh->cursor_pos+1, (guchar *) data->str, data->str_len, &offset))
	{
		gtk_hex_set_cursor(gh, offset);
	}
	else {
		ghex_window_flash(win, _("End Of File reached"));
		display_info_dialog(win, _("String was not found!\n"));
	}
}

static void advanced_find_prev_cb(GtkButton *button, AdvancedFindDialog *dialog)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tree));
	GtkTreeIter iter;
	GtkTreeModel *model;
	AdvancedFind_ListData *data;
	GtkHex *gh = dialog->parent->gh;
	guint offset;
	GHexWindow *win = ghex_window_get_active();

	if (gtk_tree_selection_get_selected(selection, &model, &iter) != TRUE)
		return;
	
	gtk_tree_model_get(model, &iter, 2, &data, -1);
	if(hex_document_find_backward(gh->document,
								  gh->cursor_pos, (guchar *) data->str, data->str_len, &offset))
		gtk_hex_set_cursor(gh, offset);
	else {
		ghex_window_flash(win, _("Beginning Of File reached"));
		display_info_dialog(win, _("String was not found!\n"));
	}
}

