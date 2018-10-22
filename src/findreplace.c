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


struct _GHexFindCondition
{
	GObject               parent;

	GString              *cond;
	GtkHex_AutoHighlight *highlight;
};

G_DEFINE_TYPE (GHexFindCondition, g_hex_find_condition, G_TYPE_OBJECT)

enum {
	COND_PROP_0,
	COND_PROP_CONDITION,
	COND_PROP_HIGHLIGHT,
	COND_LAST_PROP
};

static GParamSpec *cond_pspecs[COND_LAST_PROP] = { NULL, };

void
g_hex_find_condition_set_condition (GHexFindCondition *self,
                                    GString           *condition)
{
	if (G_LIKELY (condition)) {
		self->cond = g_boxed_copy (G_TYPE_GSTRING, condition);
	} else {
		self->cond = g_string_new (NULL);
	}
}

GString *
g_hex_find_condition_get_condition (GHexFindCondition *self)
{
	return self->cond;
}

void
g_hex_find_condition_set_highlight (GHexFindCondition    *self,
                                    GtkHex_AutoHighlight *hlight)
{
	self->highlight = hlight;
}

GtkHex_AutoHighlight *
g_hex_find_condition_get_highlight (GHexFindCondition *self)
{
	return self->highlight;
}

static void
g_hex_find_condition_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
	GHexFindCondition *data = G_HEX_FIND_CONDITION (object);

	switch (property_id) {
		case COND_PROP_CONDITION:
			g_hex_find_condition_set_condition (data, g_value_get_boxed (value));
			break;
		case COND_PROP_HIGHLIGHT:
			g_hex_find_condition_set_highlight (data, g_value_get_pointer (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
g_hex_find_condition_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
	GHexFindCondition *data = G_HEX_FIND_CONDITION (object);

	switch (property_id) {
		case COND_PROP_CONDITION:
			g_value_set_boxed (value, g_hex_find_condition_get_condition (data));
			break;
		case COND_PROP_HIGHLIGHT:
			g_value_set_pointer (value, g_hex_find_condition_get_highlight (data));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
g_hex_find_condition_class_init (GHexFindConditionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = g_hex_find_condition_set_property;
	object_class->get_property = g_hex_find_condition_get_property;

	cond_pspecs[COND_PROP_CONDITION] =
		g_param_spec_boxed ("condition", "Matching condition", NULL,
							G_TYPE_GSTRING, G_PARAM_READWRITE);

	cond_pspecs[COND_PROP_HIGHLIGHT] =
		g_param_spec_pointer ("highlight", "Highlight", NULL,
							 G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, COND_LAST_PROP, cond_pspecs);
}

static void
g_hex_find_condition_init (GHexFindCondition *self)
{
	self->cond = g_string_new (NULL);
	self->highlight = NULL;
}

GHexFindCondition *
g_hex_find_condition_new (GString              *cond,
						  GtkHex_AutoHighlight *hlight)
{
  return g_object_new (G_HEX_TYPE_FIND_CONDITION,
					   "condition", cond,
					   "highlight", hlight,
					   NULL);
}


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
					 "visible", TRUE,
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

struct _GHexFindRow
{
	GtkListBoxRow      parent;

	GHexFindCondition *cond;

	GtkWidget         *label;
	GtkWidget         *colour;
};

G_DEFINE_TYPE (GHexFindRow, g_hex_find_row, GTK_TYPE_LIST_BOX_ROW)

enum {
	ROW_PROP_0,
	ROW_PROP_CONDITION,
	ROW_LAST_PROP
};

static GParamSpec *row_pspecs[ROW_LAST_PROP] = { NULL };

enum {
	ROW_NEXT_MATCH,
	ROW_PREV_MATCH,
	ROW_REMOVE_COND,
	ROW_LAST_SIGNAL
};

static guint row_signals[ROW_LAST_SIGNAL] = { 0 };

static gboolean
render_string (GBinding     *binding,
               const GValue *from,
               GValue       *to,
               gpointer      data)
{
	GString *src;
	gchar   *dest;

	src = g_value_get_boxed (from);
	dest = g_strndup (src->str, src->len);

	for (gsize i = 0; i < src->len; i++) {
		if (G_UNLIKELY (!g_ascii_isprint (dest[i]))) {
			dest[i] = '.';
		}
	}

	g_value_set_string (to, dest);

	g_free (dest);

	return TRUE;
}

GHexFindCondition *
g_hex_find_row_get_condition (GHexFindRow *self)
{
	return self->cond;
}

static void
g_hex_find_row_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
	GHexFindRow *data = G_HEX_FIND_ROW (object);

	switch (property_id) {
		case ROW_PROP_CONDITION:
			data->cond = g_value_get_object (value);
			g_object_bind_property_full (G_OBJECT (data->cond), "condition", G_OBJECT (data->label), "label",
										 G_BINDING_SYNC_CREATE, render_string, NULL, NULL, NULL);
			g_signal_connect_swapped (G_OBJECT (data->cond), "notify::colour", G_CALLBACK (gtk_widget_queue_draw), data->colour);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
g_hex_find_row_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
	GHexFindRow *data = G_HEX_FIND_ROW (object);

	switch (property_id) {
		case ROW_PROP_CONDITION:
			g_value_set_object (value, g_hex_find_row_get_condition (data));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
g_hex_find_row_finalize (GObject *object)
{
	GHexFindRow *data = G_HEX_FIND_ROW (object);

	g_object_unref (data->cond);

	G_OBJECT_CLASS (g_hex_find_row_parent_class)->finalize (object);
}

static void
g_hex_find_row_class_init (GHexFindRowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = g_hex_find_row_set_property;
	object_class->get_property = g_hex_find_row_get_property;
	object_class->finalize = g_hex_find_row_finalize;

	row_pspecs[ROW_PROP_CONDITION] =
		g_param_spec_object ("condition", "Matching condition", NULL,
							 G_HEX_TYPE_FIND_CONDITION, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, ROW_LAST_PROP, row_pspecs);

	row_signals[ROW_NEXT_MATCH] = g_signal_new ("next-match",
												G_OBJECT_CLASS_TYPE (object_class),
												G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
												0, NULL, NULL, NULL, G_TYPE_NONE, 0);

	row_signals[ROW_PREV_MATCH] = g_signal_new ("prev-match",
												G_OBJECT_CLASS_TYPE (object_class),
												G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
												0, NULL, NULL, NULL, G_TYPE_NONE, 0);

	row_signals[ROW_REMOVE_COND] = g_signal_new ("remove-cond",
											G_OBJECT_CLASS_TYPE (object_class),
											G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
											0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static gboolean
g_hex_find_row_color_draw (GtkWidget   *widget,
                           cairo_t     *cr,
                           GHexFindRow *self)
{
	guint                 width, height;
	GtkHex_AutoHighlight *hlight;
	GdkRGBA               colour;

	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);

	hlight = g_hex_find_condition_get_highlight (self->cond);
	gtk_hex_autohighlight_get_colour (hlight, &colour);

	gdk_cairo_set_source_rgba (cr, &colour);
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	return TRUE;
}

static void
g_hex_find_row_next (GtkButton   *btn,
					 GHexFindRow *row)
{
	g_signal_emit (G_OBJECT (row), row_signals[ROW_NEXT_MATCH], 0);
}

static void
g_hex_find_row_prev (GtkButton   *btn,
					 GHexFindRow *row)
{
	g_signal_emit (G_OBJECT (row), row_signals[ROW_PREV_MATCH], 0);
}

static void
g_hex_find_row_remove (GtkButton   *btn,
					   GHexFindRow *row)
{
	g_signal_emit (G_OBJECT (row), row_signals[ROW_REMOVE_COND], 0);
}

static void
g_hex_find_row_init (GHexFindRow *self)
{
	GtkWidget *row;
	GtkWidget *btn;
	GtkWidget *wrap;

	row = g_object_new (GTK_TYPE_BOX,
						"orientation", GTK_ORIENTATION_HORIZONTAL,
						"spacing", 8,
						"margin", 8,
						"visible", TRUE,
						NULL);

	self->colour = g_object_new (GTK_TYPE_DRAWING_AREA,
								 "width_request", 64,
								 "visible", TRUE,
								 NULL);
	g_signal_connect (G_OBJECT (self->colour), "draw", G_CALLBACK (g_hex_find_row_color_draw), self);
	gtk_box_pack_start (GTK_BOX (row), self->colour, FALSE, FALSE, 0);

	self->label = g_object_new (GTK_TYPE_LABEL,
								"label", NULL,
								"halign", GTK_ALIGN_START,
								"valign", GTK_ALIGN_CENTER,
								"ellipsize", PANGO_ELLIPSIZE_MIDDLE,
								"visible", TRUE,
								NULL);
	gtk_style_context_add_class (gtk_widget_get_style_context (self->label), GTK_STYLE_CLASS_MONOSPACE);
	gtk_box_pack_start (GTK_BOX (row), self->label, TRUE, TRUE, 0);

	btn = gtk_button_new_from_icon_name ("edit-delete-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_halign (btn, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (btn, GTK_ALIGN_CENTER);
	gtk_style_context_add_class (gtk_widget_get_style_context (btn), "circular");
	g_signal_connect (G_OBJECT (btn), "clicked", G_CALLBACK (g_hex_find_row_remove), self);
	gtk_box_pack_end (GTK_BOX (row), btn, FALSE, FALSE, 0);
	gtk_widget_show (btn);

	wrap = g_object_new (GTK_TYPE_BOX,
						"orientation", GTK_ORIENTATION_HORIZONTAL,
						"spacing", 0,
						"visible", TRUE,
						NULL);
	gtk_style_context_add_class (gtk_widget_get_style_context (wrap), GTK_STYLE_CLASS_LINKED);

	btn = gtk_button_new_from_icon_name ("go-previous-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_halign (btn, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (btn, GTK_ALIGN_CENTER);
	g_signal_connect (G_OBJECT (btn), "clicked", G_CALLBACK (g_hex_find_row_prev), self);
	gtk_box_pack_start (GTK_BOX (wrap), btn, FALSE, FALSE, 0);
	gtk_widget_show (btn);

	btn = gtk_button_new_from_icon_name ("go-next-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_halign (btn, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (btn, GTK_ALIGN_CENTER);
	g_signal_connect (G_OBJECT (btn), "clicked", G_CALLBACK (g_hex_find_row_next), self);
	gtk_box_pack_start (GTK_BOX (wrap), btn, FALSE, FALSE, 0);
	gtk_widget_show (btn);

	gtk_box_pack_end (GTK_BOX (row), wrap, FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER (self), row);
}

GtkWidget *
g_hex_find_row_new (GHexFindCondition *cond)
{
  return g_object_new (G_HEX_TYPE_FIND_ROW,
					   "condition", cond,
					   NULL);
}

struct _GHexFindAdvanced
{
	GHexDialog parent;

	GListStore *store;
};

G_DEFINE_TYPE (GHexFindAdvanced, g_hex_find_advanced, G_HEX_TYPE_DIALOG)

static void
g_hex_find_advanced_class_init (GHexFindAdvancedClass *klass)
{
}

static void
g_hex_find_advanced_response (GHexFindAdvanced *self, gint res, gpointer data)
{
	GHexFindCondition *cond;
	GtkHex_AutoHighlight *hlight;
	guint i;
	GHexWindow *win = g_hex_dialog_get_window (G_HEX_DIALOG (self));
	i = 0;
	while ((cond = g_list_model_get_item (G_LIST_MODEL (self->store), i))) {
		hlight = g_hex_find_condition_get_highlight (cond);
		gtk_hex_delete_autohighlight(win->gh, hlight);
		i++;
	}
	g_list_store_remove_all (self->store);
}

static void
g_hex_find_advanced_prev (GHexFindAdvanced *self,
                          GHexFindRow      *row)
{
	GHexFindCondition *cond;
	GHexWindow *win = g_hex_dialog_get_window (G_HEX_DIALOG (self));
	GtkHex *gh = win->gh;
	GString *str;
	guint offset;

	cond = g_hex_find_row_get_condition (row);
	str = g_hex_find_condition_get_condition (cond);

	if (hex_document_find_backward (gtk_hex_get_document (gh), gtk_hex_get_cursor_pos (gh),
									(guchar *) str->str, str->len, &offset)) {
		gtk_hex_set_cursor (gh, offset);
	} else {
		ghex_window_flash (win, _("Beginning Of File reached"));
		display_info_dialog (GTK_WINDOW (self), _("String was not found!\n"));
	}
}

static void
g_hex_find_advanced_next (GHexFindAdvanced *self,
                          GHexFindRow      *row)
{
	GHexFindCondition *cond;
	GHexWindow *win = g_hex_dialog_get_window (G_HEX_DIALOG (self));
	GtkHex *gh = win->gh;
	GString *str;
	guint offset;

	cond = g_hex_find_row_get_condition (row);
	str = g_hex_find_condition_get_condition (cond);

	if (hex_document_find_forward (gtk_hex_get_document (gh), gtk_hex_get_cursor_pos (gh) + 1,
								   (guchar *) str->str, str->len, &offset)) {
		gtk_hex_set_cursor (gh, offset);
	} else {
		ghex_window_flash (win, _("End Of File reached"));
		display_info_dialog (GTK_WINDOW (self), _("String was not found!\n"));
	}
}

static void
g_hex_find_advanced_remove (GHexFindAdvanced *self,
                            GHexFindRow      *row)
{
	gint                  index;
	GHexFindCondition    *cond;
	GtkHex_AutoHighlight *hlight;
	GHexWindow *win = g_hex_dialog_get_window (G_HEX_DIALOG (self));

	cond = g_hex_find_row_get_condition (row);
	hlight = g_hex_find_condition_get_highlight (cond);
	gtk_hex_delete_autohighlight(win->gh, hlight);

	index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row));
	g_list_store_remove (self->store, index);
}

static void
g_hex_find_advanced_add (GHexFindAdvanced *self)
{
	GtkWidget *dlg;
	gint       ret;

	dlg = g_hex_find_add_new (GTK_WINDOW (self));

	gtk_widget_show (dlg);
	ret = gtk_dialog_run (GTK_DIALOG (dlg));

	if (ret == GTK_RESPONSE_APPLY) {
		GHexFindCondition *cond;
		gchar             *colour;
		GdkRGBA            rgba;
		GHexWindow *win = g_hex_dialog_get_window (G_HEX_DIALOG (self));
		GtkHex            *gh = win->gh;
		GtkHex_AutoHighlight *highlight;
		GString           *str;
		gchar             *search;
		gsize              search_len;

		g_return_if_fail (gh != NULL);

		if ((search_len = get_search_string (G_HEX_FIND_ADD (dlg)->f_doc, &search)) == 0) {
			display_error_dialog (dlg, _("No string to search for!"));
			gtk_widget_destroy (dlg);
			return;
		}

		gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (G_HEX_FIND_ADD (dlg)->colour), &rgba);
		colour = gdk_rgba_to_string (&rgba);

		highlight = gtk_hex_insert_autohighlight (gh, search, search_len, colour);
		str = g_string_new_len (search, search_len);
		cond = g_hex_find_condition_new (str, highlight);
		g_list_store_append (G_LIST_STORE (self->store), cond);

		g_free(colour);
	}

	gtk_widget_destroy (dlg);
}


static GtkWidget *
g_hex_find_advanced_render_row (gpointer item, gpointer data)
{
	GtkWidget *row;

	row = g_hex_find_row_new (G_HEX_FIND_CONDITION (item));
	g_signal_connect_swapped (G_OBJECT (row), "prev-match", G_CALLBACK (g_hex_find_advanced_prev), data);
	g_signal_connect_swapped (G_OBJECT (row), "next-match", G_CALLBACK (g_hex_find_advanced_next), data);
	g_signal_connect_swapped (G_OBJECT (row), "remove-cond", G_CALLBACK (g_hex_find_advanced_remove), data);

	return row;
}

void
g_hex_find_advanced_header_func (GtkListBoxRow *row,
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
g_hex_find_advanced_init (GHexFindAdvanced *self)
{
	GtkWidget *scroll;
	GtkWidget *wrap;
	GtkWidget *frame;
	GtkWidget *list;
	GtkWidget *btn;
	GtkWidget *placeholder;

	g_signal_connect (G_OBJECT (self), "response", G_CALLBACK (g_hex_find_advanced_response), NULL);

	self->store = g_list_store_new (G_HEX_TYPE_FIND_CONDITION);

	scroll = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
						   "hscrollbar-policy", GTK_POLICY_NEVER,
						   "expand", TRUE,
						   "max-content-height", 300,
						   "propagate-natural-height", TRUE,
						   NULL);

	wrap = g_object_new (GTK_TYPE_BOX,
						 "orientation", GTK_ORIENTATION_VERTICAL,
						 "spacing", 8,
						 "margin", 16,
						 "visible", TRUE,
						 NULL);

	frame = gtk_frame_new (NULL);

	placeholder = g_object_new (GTK_TYPE_LABEL,
								"label", _("Add searches"),
								"margin", 32,
								"visible", TRUE,
								NULL);

	list = g_object_new (GTK_TYPE_LIST_BOX,
						 "selection-mode", GTK_SELECTION_NONE,
						 "visible", TRUE,
						 "width-request", 300,
						 NULL);
	gtk_list_box_set_header_func (GTK_LIST_BOX (list), g_hex_find_advanced_header_func, NULL, NULL);
	gtk_list_box_set_placeholder (GTK_LIST_BOX (list), placeholder);
	gtk_list_box_bind_model (GTK_LIST_BOX (list), G_LIST_MODEL (self->store),
							 g_hex_find_advanced_render_row, self, NULL);
	gtk_container_add (GTK_CONTAINER (frame), list);

	gtk_widget_show (frame);

	gtk_box_pack_start (GTK_BOX (wrap), frame, FALSE, FALSE, 0);

	btn = g_object_new (GTK_TYPE_BUTTON,
						"label", _("Add"),
						"halign", GTK_ALIGN_END,
						"valign", GTK_ALIGN_START,
						"can-default", TRUE,
						"visible", TRUE,
						NULL);
	gtk_box_pack_start (GTK_BOX(wrap), btn, FALSE, FALSE, 0);
	g_signal_connect_swapped (G_OBJECT (btn), "clicked",
							  G_CALLBACK(g_hex_find_advanced_add), self);

	gtk_container_add (GTK_CONTAINER (scroll), wrap);
	gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area (GTK_DIALOG (self))),
						scroll, TRUE, TRUE, 0);
	gtk_widget_show (scroll);

	gtk_window_set_position (GTK_WINDOW(self), GTK_WIN_POS_CENTER_ON_PARENT);
}

GtkWidget *
g_hex_find_advanced_new (GHexWindow *parent)
{
  return g_object_new (G_HEX_TYPE_FIND_ADVANCED,
					   "use-header-bar", TRUE,
					   "modal", FALSE,
					   "transient-for", parent,
					   "resizable", TRUE,
					   "height-request", 300,
					   "title", _("Advanced Find"),
					   NULL);
}

