/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* application.c - application class

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

#include "application.h"
#include "ghex-window.h"
#include "configuration.h"
#include "factory.h"
#include "hex-document.h"

const char * const open_accels[] = { "<primary>o", NULL };
const char * const save_accels[] = { "<primary>s", NULL };
const char * const save_as_accels[] = { "<primary><shift>s", NULL };
const char * const print_accels[] = { "<primary>p", NULL };
const char * const print_pre_accels[] = { "<primary><shift>p", NULL };
const char * const undo_accels[] = { "<primary>z", NULL };
const char * const redo_accels[] = { "<primary><shift>z", NULL };
const char * const cut_accels[] = { "<primary>x", NULL };
const char * const copy_accels[] = { "<primary>c", NULL };
const char * const paste_accels[] = { "<primary>v", NULL };
const char * const find_accels[] = { "<primary>f", NULL };
const char * const replace_accels[] = { "<primary>h", NULL };
const char * const jump_accels[] = { "<primary>j", NULL };
const char * const insert_accels[] = { "Insert", NULL };
const char * const help_accels[] = { "F1", NULL };
const char * const quit_accels[] = { "<primary>q", NULL };
const char * const close_accels[] = { "<primary>w", NULL };

struct _GHexApplication
{
	GtkApplication  parent;

	GSettings      *settings;

	const gchar    *group_by;
	guint           shaded_box;
	const gchar    *data_font;
	const gchar    *header_font;
	const gchar    *font_name;
	gboolean        show_offset;
	guint           undo_depth;
	const gchar    *offset_format;
};

G_DEFINE_TYPE (GHexApplication, g_hex_application, GTK_TYPE_APPLICATION)

enum {
	PROP_0,
	PROP_GROUP_BY,
	PROP_SHADED_BOX,
	PROP_DATA_FONT,
	PROP_HEADER_FONT,
	PROP_FONT_NAME,
	PROP_SHOW_OFFSET,
	PROP_UNDO_DEPTH,
	PROP_OFFSET_FORMAT,
	LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP] = { NULL, };

void
g_hex_application_set_group_by (GHexApplication *self,
                                const gchar     *group_by)
{
	self->group_by = g_strdup (group_by);
	g_object_notify (G_OBJECT (self), "group-by");
}

const gchar *
g_hex_application_get_group_by (GHexApplication *self)
{
	return self->group_by;
}

void
g_hex_application_set_shaded_box (GHexApplication *self,
                                  guint            shaded_box)
{
	self->shaded_box = shaded_box;
	g_object_notify (G_OBJECT (self), "shaded-box");
}

guint
g_hex_application_get_shaded_box (GHexApplication *self)
{
	return self->shaded_box;
}

void
g_hex_application_set_data_font (GHexApplication *self,
                                 const gchar     *data_font)
{
	self->data_font = g_strdup (data_font);
	g_object_notify (G_OBJECT (self), "data-font");
}

const gchar *
g_hex_application_get_data_font (GHexApplication *self)
{
	return self->data_font;
}

void
g_hex_application_set_header_font (GHexApplication *self,
                                   const gchar     *header_font)
{
	self->header_font = g_strdup (header_font);
	g_object_notify (G_OBJECT (self), "header-font");
}

const gchar *
g_hex_application_get_header_font (GHexApplication *self)
{
	return self->header_font;
}

void
g_hex_application_set_font_name (GHexApplication *self,
                                 const gchar     *font)
{
	GList *windows;
	PangoFontMetrics *metrics;
	PangoFontDescription *desc;

	self->font_name = g_strdup (font);

	g_hex_application_get_font (self, &metrics, &desc);

	windows = gtk_application_get_windows (GTK_APPLICATION (self));
	while (windows) {
		if (GHEX_IS_WINDOW (windows->data) && GHEX_WINDOW (windows->data)->gh) {
			gtk_hex_set_font (GHEX_WINDOW (windows->data)->gh, metrics, desc);
		}
		windows = g_list_next (windows);
	}

	g_object_notify (G_OBJECT (self), "font-name");
}

const gchar *
g_hex_application_get_font_name (GHexApplication *self)
{
	return self->font_name;
}

void
g_hex_application_set_show_offset (GHexApplication *self,
                                   gboolean         show_offset)
{
	GList *windows;

	self->show_offset = show_offset;

	windows = gtk_application_get_windows (GTK_APPLICATION (self));
	while (windows) {
		if (GHEX_IS_WINDOW (windows->data) && GHEX_WINDOW (windows->data)->gh) {
			gtk_hex_show_offsets (GHEX_WINDOW (windows->data)->gh, show_offset);
		}
		windows = g_list_next (windows);
	}

	g_object_notify (G_OBJECT (self), "show-offset");
}

gboolean
g_hex_application_get_show_offset (GHexApplication *self)
{
	return self->show_offset;
}

void
g_hex_application_set_undo_depth (GHexApplication *self,
                                  guint            undo_depth)
{
	const GList *docn;

	self->undo_depth = undo_depth;

	docn = hex_document_get_list ();
	while (docn) {
		hex_document_set_max_undo (HEX_DOCUMENT (docn->data), undo_depth);
		docn = g_list_next (docn);
	}

	g_object_notify (G_OBJECT (self), "undo-depth");
}

guint
g_hex_application_get_undo_depth (GHexApplication *self)
{
	return self->undo_depth;
}

void
g_hex_application_set_offset_format (GHexApplication *self,
                                     const gchar     *offset_format)
{
	gint len, i;
	gboolean expect_spec;
	const gchar* format = g_strdup (offset_format);

	/* check for a valid format string */
	len = strlen (format);
	expect_spec = FALSE;
	for (i = 0; i < len; i++) {
		if (format[i] == '%')
			expect_spec = TRUE;
		if (expect_spec && ((format[i] >= 'a' && format[i] <= 'z') ||
			  (format[i] >= 'A' && format[i] <= 'Z'))) {
			expect_spec = FALSE;
			if (format[i] != 'x' && format[i] != 'd' &&
				  format[i] != 'o' && format[i] != 'X' &&
				  format[i] != 'P' && format[i] != 'p') {
				// Bad format
				format = self->offset_format;
			}
		}
	}
	self->offset_format = format;

	g_object_notify (G_OBJECT (self), "offset-format");
}

const gchar *
g_hex_application_get_offset_format (GHexApplication *self)
{
	return self->offset_format;
}

static void
g_hex_application_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	GHexApplication *data = G_HEX_APPLICATION (object);

	switch (property_id) {
		case PROP_GROUP_BY:
			g_hex_application_set_group_by (data, g_value_get_string (value));
			break;
		case PROP_SHADED_BOX:
			g_hex_application_set_shaded_box (data, g_value_get_uint (value));
			break;
		case PROP_DATA_FONT:
			g_hex_application_set_data_font (data, g_value_get_string (value));
			break;
		case PROP_HEADER_FONT:
			g_hex_application_set_header_font (data, g_value_get_string (value));
			break;
		case PROP_FONT_NAME:
			g_hex_application_set_font_name (data, g_value_get_string (value));
			break;
		case PROP_SHOW_OFFSET:
			g_hex_application_set_show_offset (data, g_value_get_boolean (value));
			break;
		case PROP_UNDO_DEPTH:
			g_hex_application_set_undo_depth (data, g_value_get_uint (value));
			break;
		case PROP_OFFSET_FORMAT:
			g_hex_application_set_offset_format (data, g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
g_hex_application_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	GHexApplication *data = G_HEX_APPLICATION (object);

	switch (property_id) {
		case PROP_GROUP_BY:
			g_value_set_static_string (value, g_hex_application_get_group_by (data));
			break;
		case PROP_SHADED_BOX:
			g_value_set_uint (value, g_hex_application_get_shaded_box (data));
			break;
		case PROP_DATA_FONT:
			g_value_set_static_string (value, g_hex_application_get_data_font (data));
			break;
		case PROP_HEADER_FONT:
			g_value_set_static_string (value, g_hex_application_get_header_font (data));
			break;
		case PROP_FONT_NAME:
			g_value_set_static_string (value, g_hex_application_get_font_name (data));
			break;
		case PROP_SHOW_OFFSET:
			g_value_set_boolean (value, g_hex_application_get_show_offset (data));
			break;
		case PROP_UNDO_DEPTH:
			g_value_set_uint (value, g_hex_application_get_undo_depth (data));
			break;
		case PROP_OFFSET_FORMAT:
			g_value_set_static_string (value, g_hex_application_get_offset_format (data));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
g_hex_application_startup (GApplication *app)
{
	GHexApplication *self;

	G_APPLICATION_CLASS (g_hex_application_parent_class)->startup (app);

	self = G_HEX_APPLICATION (app);

	/* Set default window icon */
	gtk_window_set_default_icon_name ("org.gnome.GHex");

	self->settings = g_settings_new ("org.gnome.GHex");

	if (G_LIKELY (self->settings)) {
		g_settings_bind (self->settings, GHEX_PREF_OFFSETS_COLUMN, self, "show-offset", G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (self->settings, GHEX_PREF_GROUP, self, "group-by", G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (self->settings, GHEX_PREF_MAX_UNDO_DEPTH, self, "undo-depth", G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (self->settings, GHEX_PREF_BOX_SIZE, self, "shaded-box", G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (self->settings, GHEX_PREF_OFFSET_FORMAT, self, "offset-format", G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (self->settings, GHEX_PREF_FONT, self, "font-name", G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (self->settings, GHEX_PREF_DATA_FONT, self, "data-font", G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (self->settings, GHEX_PREF_HEADER_FONT, self, "header-font", G_SETTINGS_BIND_DEFAULT);
	}

	/* accessibility setup */
	setup_factory();

	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.open", open_accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.save", save_accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.save-as", save_as_accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.print", print_accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.print-preview", print_pre_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.undo", undo_accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.redo", redo_accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.cut", cut_accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.copy", copy_accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.paste", paste_accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.insert", insert_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.find", find_accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.replace", replace_accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.goto", jump_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.help", help_accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.quit", quit_accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.close", close_accels);
}

static void
g_hex_application_activate (GApplication *self)
{
	GtkWidget *window = GTK_WIDGET (gtk_application_get_active_window (GTK_APPLICATION (self)));
	if (!window)
		window = ghex_window_new (G_HEX_APPLICATION (self));
	gtk_window_present (GTK_WINDOW (window));
}

static void
g_hex_application_open (GApplication   *application,
                        GFile         **files,
                        gint            n_files,
                        const gchar    *hint)
{
	GtkWidget *win;
	for (gint i = 0; i < n_files; i++) {
		gchar *filename = g_file_get_path (files[i]);
		if (filename && g_file_test (filename, G_FILE_TEST_EXISTS)) {
			win = ghex_window_new_from_file (G_HEX_APPLICATION (application), filename);
			if(win) {
				gtk_window_present (GTK_WINDOW (win));
			}
		}
		g_free (filename);
	}
}

static void
g_hex_application_class_init (GHexApplicationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = g_hex_application_set_property;
	object_class->get_property = g_hex_application_get_property;

	pspecs[PROP_GROUP_BY] =
		g_param_spec_string ("group-by", "Byte grouping", NULL, NULL,
							G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	pspecs[PROP_SHADED_BOX] =
		g_param_spec_uint ("shaded-box", "Byte grouping", NULL, 0, G_MAXUINT, 0,
							G_PARAM_READWRITE);

	pspecs[PROP_DATA_FONT] =
		g_param_spec_string ("data-font", "Data font", NULL, NULL,
							G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	pspecs[PROP_HEADER_FONT] =
		g_param_spec_string ("header-font", "Header font", NULL, NULL,
							G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	pspecs[PROP_FONT_NAME] =
		g_param_spec_string ("font-name", "Editor font", NULL, NULL,
							G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	pspecs[PROP_SHOW_OFFSET] =
		g_param_spec_boolean ("show-offset", "Shows offsets", NULL, TRUE,
							G_PARAM_READWRITE);

	pspecs[PROP_UNDO_DEPTH] =
		g_param_spec_uint ("undo-depth", "Undo depth", NULL, 0,
						   G_HEX_MAX_MAX_UNDO_DEPTH, 100, G_PARAM_READWRITE);

	pspecs[PROP_OFFSET_FORMAT] =
		g_param_spec_string ("offset-format", "Statusbar format", NULL, NULL,
							G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, LAST_PROP, pspecs);

	GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

	application_class->startup = g_hex_application_startup;
	application_class->activate = g_hex_application_activate;
	application_class->open = g_hex_application_open;
}

static void
g_hex_application_init (GHexApplication *self)
{
}

void
g_hex_application_get_font        (GHexApplication       *self,
								   PangoFontMetrics     **metrics,
								   PangoFontDescription **desc)
{
	const gchar          *font;

	font = g_hex_application_get_font_name (self);

	*metrics = gtk_hex_load_font (font);
	*desc = pango_font_description_from_string (font);
}

GHexApplication *
g_hex_application_new ()
{
  return g_object_new (G_HEX_TYPE_APPLICATION,
					   "application-id", "org.gnome.GHexApplication",
					   "flags", G_APPLICATION_NON_UNIQUE | G_APPLICATION_HANDLES_OPEN,
					   NULL);
}
