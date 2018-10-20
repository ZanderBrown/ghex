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
	GtkApplication parent;
};

G_DEFINE_TYPE (GHexApplication, g_hex_application, GTK_TYPE_APPLICATION)

static void
g_hex_application_startup (GApplication *self)
{
	G_APPLICATION_CLASS (g_hex_application_parent_class)->startup (self);

	/* Set default window icon */
	gtk_window_set_default_icon_name ("ghex");

	/* load preferences */
	ghex_init_configuration();

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
	GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

	application_class->startup = g_hex_application_startup;
	application_class->activate = g_hex_application_activate;
	application_class->open = g_hex_application_open;
}

static void
g_hex_application_init (GHexApplication *self)
{
}

GHexApplication *
g_hex_application_new ()
{
  return g_object_new (G_HEX_TYPE_APPLICATION,
					   "application-id", "org.gnome.GHexApplication",
					   "flags", G_APPLICATION_NON_UNIQUE | G_APPLICATION_HANDLES_OPEN,
					   NULL);
}
