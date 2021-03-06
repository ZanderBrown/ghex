/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * ghex-window.c: a ghex window
 *
 * Copyright (C) 2002 - 2004 the Free Software Foundation
 *
 * GHex is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GHex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GHex; see the file COPYING.
 * If not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Author: Jaka Mocnik  <jaka@gnu.org>
 */

#include <config.h>

#include <gio/gio.h>
#include <glib/gi18n.h>

#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h> /* for F_OK */

#include "ghex-window.h"
#include "findreplace.h"
#include "ui.h"
#include "converter.h"
#include "chartable.h"
#include "configuration.h"
#include "hex-dialog.h"

#define GHEX_WINDOW_DEFAULT_WIDTH 320
#define GHEX_WINDOW_DEFAULT_HEIGHT 256

typedef void (*Action)(GSimpleAction *action, GVariant *param, gpointer data);
#define ACTION(f) ((Action) (f))

G_DEFINE_TYPE (GHexWindow, ghex_window, GTK_TYPE_APPLICATION_WINDOW)

/* what can be dragged in us... */
enum {
    TARGET_URI_LIST,
};

static void
ghex_window_drag_data_received(GtkWidget *widget,
                               GdkDragContext *context,
                               gint x, gint y,
                               GtkSelectionData *selection_data,
                               guint info, guint time)
{
    GHexWindow *win = GHEX_WINDOW(widget);
    GtkWidget *newwin;
    gchar **uri;
    gchar **uris_to_open;

    if (info != TARGET_URI_LIST)
        return;

    if (win->gh == NULL)
        newwin = GTK_WIDGET (win);
    else
        newwin = NULL;

    uri = uris_to_open = g_uri_list_extract_uris ((gchar *) gtk_selection_data_get_data (selection_data));
    while (*uri) {
        GError *err = NULL;
        gchar *filename = g_filename_from_uri (*uri, NULL, &err);

        uri++;
        if (filename == NULL) {
            GtkWidget *dlg;
            dlg = gtk_message_dialog_new (GTK_WINDOW (win),
                                          GTK_DIALOG_MODAL,
                                          GTK_MESSAGE_ERROR,
                                          GTK_BUTTONS_OK,
                                          _("Can not open URI:\n%s"),
                                          err->message);
            g_error_free (err);
            gtk_dialog_run (GTK_DIALOG (dlg));
            gtk_widget_destroy (dlg);
            continue;
        }

        if (newwin == NULL)
            newwin = ghex_window_new (G_HEX_APPLICATION (g_application_get_default ()));
        if (ghex_window_load (GHEX_WINDOW (newwin), filename)) {
            if (newwin != GTK_WIDGET (win))
                gtk_widget_show (newwin);
            newwin = NULL;
        }
        else {
            GtkWidget *dlg;
            dlg = gtk_message_dialog_new (GTK_WINDOW (win),
                                          GTK_DIALOG_MODAL,
                                          GTK_MESSAGE_ERROR,
                                          GTK_BUTTONS_OK,
                                          _("Can not open file:\n%s"),
                                          filename);
            gtk_widget_show (dlg);
            gtk_dialog_run (GTK_DIALOG (dlg));
            gtk_widget_destroy (dlg);
        }
        g_free (filename);
    }
    g_strfreev (uris_to_open);
}

gboolean
ghex_window_close(GHexWindow *win)
{
	HexDocument *doc;

	if(win->gh == NULL) {
		gtk_widget_destroy (GTK_WIDGET (win));
		return FALSE;
	}

	doc = gtk_hex_get_document (win->gh);
	if (g_list_next (hex_document_get_views (doc)) == NULL) {
		if (!ghex_window_ok_to_close (win))
			return FALSE;
	}

	/* If we have created the converter window disable the
	 * "Get cursor value" button
	 */
	if (win->converter && GTK_IS_WIDGET (win->converter))
		g_hex_converter_set_can_grap (G_HEX_CONVERTER (win->converter), FALSE);

	if (win->find_advanced)
		gtk_widget_destroy (win->find_advanced);

	gtk_widget_destroy(GTK_WIDGET(win));

	return TRUE;
}

static void
ghex_window_set_toggle_action_active (GHexWindow *win,
                                      const char *name,
                                      gboolean    active)
{
    GAction  *action;
    GVariant *state;

    action = g_action_map_lookup_action (G_ACTION_MAP (win), name);
    state = g_variant_new_boolean (!active);
    g_simple_action_set_state (G_SIMPLE_ACTION (action), state);
}

static gboolean
ghex_window_get_toggle_action_active (GHexWindow *win,
                                      const char *name)
{
    GAction *action;
    GVariant *state;

    action = g_action_map_lookup_action (G_ACTION_MAP (win), name);
    state = g_action_get_state (action);
    return g_variant_get_boolean (state);
}

void
ghex_window_set_sensitivity (GHexWindow *win)
{
    GAction *act;
    gboolean allmenus = (win->gh != NULL);

    win->undo_sens = (allmenus && (hex_document_get_can_undo (gtk_hex_get_document (win->gh))));
    win->redo_sens = (allmenus && (hex_document_get_can_redo (gtk_hex_get_document (win->gh))));

	act = g_action_map_lookup_action (G_ACTION_MAP (win), "open-view");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "group");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    gtk_widget_set_visible (win->statusbar_display_mode_btn, allmenus);

    /* File menu */
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "save");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "save-as");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "export");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "revert");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus && hex_document_has_changed (gtk_hex_get_document (win->gh)));
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "print");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "print-preview");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "close");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);

    /* Edit menu */
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "find");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "replace");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "find-advanced");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "goto");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "insert");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    gtk_widget_set_visible (win->statusbar_insert_mode, allmenus);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "undo");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus && win->undo_sens);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "redo");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus && win->redo_sens);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "cut");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "copy");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
    act = g_action_map_lookup_action (G_ACTION_MAP (win), "paste");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (act), allmenus);
}

static void
ghex_window_doc_changed(HexDocument *doc, HexChangeData *change_data,
                        gboolean push_undo, gpointer user_data)
{
    GHexWindow *win = GHEX_WINDOW(user_data);
    GAction    *act;

    if (!hex_document_has_changed (gtk_hex_get_document (win->gh)))
        return;

    if(!win->changed) {
        ghex_window_set_sensitivity(win);
        win->changed = TRUE;
    } else if(push_undo) {
        if (win->undo_sens != hex_document_get_can_undo (gtk_hex_get_document (win->gh))) {
            win->undo_sens = hex_document_get_can_undo (gtk_hex_get_document (win->gh));
            act = g_action_map_lookup_action (G_ACTION_MAP (win), "undo");
            g_simple_action_set_enabled (G_SIMPLE_ACTION (act), win->undo_sens);
        }
        if(win->redo_sens != hex_document_get_can_redo (gtk_hex_get_document (win->gh))) {
            win->redo_sens = hex_document_get_can_redo (gtk_hex_get_document (win->gh));
            act = g_action_map_lookup_action (G_ACTION_MAP (win), "redo");
            g_simple_action_set_enabled (G_SIMPLE_ACTION (act), win->redo_sens);
        }
    }
}

static void
ghex_window_destroy (GtkWidget *object)
{
	GHexWindow *win;

	g_return_if_fail (object);
	g_return_if_fail (GHEX_IS_WINDOW (object));

	win = GHEX_WINDOW (object);

	if (win->gh) {
		hex_document_remove_view (gtk_hex_get_document (win->gh), GTK_WIDGET (win->gh));
		g_signal_handlers_disconnect_matched (gtk_hex_get_document (win->gh),
											  G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
											  0, 0, NULL,
											  ghex_window_doc_changed,
											  win);
		win->gh = NULL;
	}

	if (win->dialog) {
		g_object_unref (G_OBJECT(win->dialog));
		win->dialog = NULL;
	}

	if (GTK_WIDGET_CLASS (ghex_window_parent_class)->destroy)
		GTK_WIDGET_CLASS (ghex_window_parent_class)->destroy (object);
}

static gboolean
ghex_window_delete_event(GtkWidget *widget, GdkEventAny *e)
{
    ghex_window_close(GHEX_WINDOW(widget));
    return TRUE;
}

/* Actions */
static const GActionEntry gaction_entries [] = {
    // Open a file
    { "open", ACTION (open_cb) },
    // Save the current file
    { "save", ACTION (save_cb) },
    // Save the current file with a different name
    { "save-as", ACTION (save_as_cb) },
    // Export data to HTML source
    { "export", ACTION (export_html_cb) },
    // Revert to a saved version of the file
    { "revert", ACTION (revert_cb) },
    // Print the current file
    { "print", ACTION (print_cb) },
    // Preview printed data
    { "print-preview", ACTION (print_preview_cb) },

    // Undo the last action
    { "undo", ACTION (undo_cb) },
    // Redo the undone action
    { "redo", ACTION (redo_cb) },
    // Cut selection
    { "cut", ACTION (cut_cb) },
    // Copy selection to clipboard
    { "copy", ACTION (copy_cb) },
    // Paste data from clipboard
    { "paste", ACTION (paste_cb) },

    // Search for a string
    { "find", ACTION (find_cb) },
    // Replace a string
    { "replace", ACTION (replace_cb) },
    // Advanced Find
    { "find-advanced", ACTION (advanced_find_cb) },
    // Jump to a certain position
    { "goto", ACTION (jump_cb) },
    // Insert/overwrite data
    { "insert", NULL, NULL, "false", ACTION (insert_mode_cb) },

    // Group data by 8/16/32 bits
    { "group", NULL, "s", "'bytes'", ACTION (group_data_cb) },

    // Show the character table
    { "char-table", ACTION (character_table_cb) },
    // Open base conversion dialog
    { "base-tool", ACTION (converter_cb) },
    // Show the type conversion dialog in the edit window
    { "type-tool", NULL, NULL, "true", ACTION (type_dialog_cb) },

    // Add a new view to the buffer
    { "open-view", ACTION (add_view_cb) },

    // Configure the application
    { "prefs", ACTION (prefs_cb) },
    // Help on this application
    { "help", ACTION (help_cb) },
    // About this application
    { "about", ACTION (about_cb) },
    // Exit the program
    { "quit", ACTION (quit_app_cb) },
    // Close the current file
    { "close", ACTION (close_cb) }
};

void
ghex_window_set_contents (GHexWindow *win,
                          GtkWidget  *child)
{
    if (win->contents)
        gtk_widget_destroy (win->contents);

    win->contents = child;
    gtk_box_pack_start (GTK_BOX (win->vbox), win->contents, TRUE, TRUE, 0);
}

void
ghex_window_destroy_contents (GHexWindow *win)
{
	GtkWidget *placeholder;

	if (G_LIKELY (win->contents))
		gtk_widget_destroy (win->contents);
	win->contents = NULL;

	placeholder = g_object_new (GTK_TYPE_IMAGE,
						  "icon-name", "org.gnome.GHex-symbolic",
						  "pixel-size", 128,
						  "margin", 16,
						  "halign", GTK_ALIGN_CENTER,
						  "valign", GTK_ALIGN_CENTER,
						  "visible", TRUE,
						  NULL);
	ghex_window_set_contents (win, placeholder);
}

static GObject *
ghex_window_constructor (GType                  type,
                         guint                  n_construct_properties,
                         GObjectConstructParam *construct_params)
{
    GObject    *object;
    GHexWindow *window;
    GtkWidget  *header;
    GtkWidget  *btn;
    GtkWidget  *box;
    GtkWidget  *image;
    GMenu      *appmenu;
    GMenu      *section;
	GAction    *save;

    object = G_OBJECT_CLASS (ghex_window_parent_class)->constructor (type,
                             n_construct_properties,
                             construct_params);
    window = GHEX_WINDOW (object);

    g_action_map_add_action_entries (G_ACTION_MAP (window),
                                     gaction_entries,
                                     G_N_ELEMENTS (gaction_entries),
                                     window);

    window->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    window->contents = NULL;
	ghex_window_destroy_contents (window);

    /* Create statusbar */
    window->statusbar = gtk_statusbar_new ();
    window->statusbar_tooltip_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (window->statusbar),
                                                                 "tooltip");
    gtk_box_pack_end (GTK_BOX (window->vbox), window->statusbar, FALSE, TRUE, 0);
    gtk_widget_show (window->statusbar);

    window->statusbar_insert_mode = gtk_label_new ("OVR");
    gtk_box_pack_end (GTK_BOX (window->statusbar), window->statusbar_insert_mode, FALSE, TRUE, 0);
    gtk_widget_show (window->statusbar_insert_mode);

    section = g_menu_new ();
    g_menu_append (section, _("Byte"), "win.group::bytes");
    g_menu_append (section, _("Word"), "win.group::words");
    g_menu_append (section, _("Longword"), "win.group::longwords");

    window->statusbar_display_mode_btn = gtk_menu_button_new ();
    gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (window->statusbar_display_mode_btn),
                                    G_MENU_MODEL (section));
    gtk_button_set_relief (GTK_BUTTON (window->statusbar_display_mode_btn),
                           GTK_RELIEF_NONE);
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    window->statusbar_display_mode = gtk_label_new (_("Byte"));
    gtk_box_pack_start (GTK_BOX (box), window->statusbar_display_mode, FALSE, FALSE, 0);
    gtk_widget_show (window->statusbar_display_mode);
    image = gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_MENU);
    gtk_box_pack_end (GTK_BOX (box), image, FALSE, FALSE, 0);
    gtk_widget_show (image);
    gtk_container_add (GTK_CONTAINER (window->statusbar_display_mode_btn), box);
    gtk_widget_show (box);
    gtk_widget_show (window->statusbar_display_mode_btn);
    gtk_box_pack_end (GTK_BOX (window->statusbar),
                      window->statusbar_display_mode_btn,
                      FALSE,
                      FALSE,
                      5);


    gtk_container_add (GTK_CONTAINER (window), window->vbox);
    gtk_widget_show (window->vbox);

    header = gtk_header_bar_new ();
    gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (header), TRUE);
    gtk_window_set_titlebar (GTK_WINDOW (window), header);
    gtk_widget_show (header);

    btn = gtk_button_new_with_label (_("Open"));
    gtk_actionable_set_action_name (GTK_ACTIONABLE (btn), "win.open");
    gtk_header_bar_pack_start (GTK_HEADER_BAR (header), btn);
    gtk_widget_show (btn);

    appmenu = g_menu_new ();

    section = g_menu_new ();
    g_menu_append (section, _("Revert"), "win.revert");
    g_menu_append_section (appmenu, NULL, G_MENU_MODEL (section));

    section = g_menu_new ();
    g_menu_append (section, _("Save As..."), "win.save-as");
    g_menu_append (section, _("Export..."), "win.export");
    g_menu_append_section (appmenu, NULL, G_MENU_MODEL (section));

    section = g_menu_new ();
    g_menu_append (section, _("Find..."), "win.find");
    g_menu_append (section, _("Find and Replace..."), "win.replace");
    g_menu_append (section, _("Advanced Find..."), "win.find-advanced");
    g_menu_append (section, _("Go to Byte..."), "win.goto");
    g_menu_append_section (appmenu, NULL, G_MENU_MODEL (section));

    section = g_menu_new ();
    g_menu_append (section, _("Print..."), "win.print");
    g_menu_append (section, _("Print Preview..."), "win.print-preview");
    g_menu_append_section (appmenu, NULL, G_MENU_MODEL (section));

    section = g_menu_new ();
    g_menu_append (section, _("Open View"), "win.open-view");
    g_menu_append (section, _("Character Table"), "win.char-table");
    g_menu_append (section, _("Base Converter"), "win.base-tool");
    g_menu_append (section, _("Type Converter"), "win.type-tool");
    g_menu_append_submenu (appmenu, "Tools", G_MENU_MODEL (section));

    section = g_menu_new ();
    g_menu_append (section, _("Preferences"), "win.prefs");
    g_menu_append (section, _("Help"), "win.help");
    g_menu_append (section, _("About GHex"), "win.about");
    g_menu_append_section (appmenu, NULL, G_MENU_MODEL (section));

    btn = gtk_menu_button_new ();
    gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (btn), G_MENU_MODEL (appmenu));
    image = gtk_image_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (btn), image);
    gtk_widget_show (image);
    gtk_header_bar_pack_end (GTK_HEADER_BAR  (header), btn);
    gtk_widget_show (btn);

    btn = gtk_button_new_with_label (_("Save"));
    gtk_actionable_set_action_name (GTK_ACTIONABLE (btn), "win.save");
	save = g_action_map_lookup_action (G_ACTION_MAP (window), "save");
	g_object_bind_property (save, "enabled", btn, "visible", G_BINDING_SYNC_CREATE);
    gtk_header_bar_pack_end (GTK_HEADER_BAR (header), btn);
    gtk_widget_show (btn);

    return object;
}

static void
ghex_window_class_init(GHexWindowClass *class)
{
	GObjectClass   *gobject_class;
	GtkWidgetClass *widget_class;

	gobject_class = (GObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	gobject_class->constructor = ghex_window_constructor;

	widget_class->delete_event = ghex_window_delete_event;
	widget_class->destroy = ghex_window_destroy;
	widget_class->drag_data_received = ghex_window_drag_data_received;
}

static void
ghex_window_init (GHexWindow *window)
{
}

void
ghex_window_sync_converter_item(GHexWindow *win, gboolean state)
{
	GHexApplication *app;
	GList           *win_node;

	app = G_HEX_APPLICATION (gtk_window_get_application (GTK_WINDOW (win)));
	win_node = gtk_application_get_windows (GTK_APPLICATION (app));

	while (win_node) {
		if (GHEX_IS_WINDOW (win_node->data) && GHEX_WINDOW (win_node->data) != win)
			ghex_window_set_toggle_action_active (GHEX_WINDOW (win_node->data), "type-tool", state);
		win_node = g_list_next (win_node);
	}
}

GtkWidget *
ghex_window_new (GHexApplication *application)
{
    GHexWindow *win;

	static const GtkTargetEntry drag_types[] = {
		{ "text/uri-list", 0, TARGET_URI_LIST }
	};

	win = GHEX_WINDOW(g_object_new(GHEX_TYPE_WINDOW,
                                   "application", application,
                                   "title", _("GHex"),
                                   NULL));

	gtk_drag_dest_set (GTK_WIDGET (win),
                       GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                       drag_types,
                       sizeof (drag_types) / sizeof (drag_types[0]),
                       GDK_ACTION_COPY);

    ghex_window_set_sensitivity(win);

    gtk_window_set_default_size(GTK_WINDOW(win),
                                GHEX_WINDOW_DEFAULT_WIDTH,
                                GHEX_WINDOW_DEFAULT_HEIGHT);

	return GTK_WIDGET(win);
}

static GtkWidget *
create_document_view(GHexWindow *win, HexDocument *doc)
{
	PangoFontMetrics     *metrics;
	PangoFontDescription *desc;
	GHexApplication      *app;
	const gchar          *group;
	GtkWidget            *gh = hex_document_add_view(doc);

	app = G_HEX_APPLICATION (gtk_window_get_application (GTK_WINDOW (win)));
	group = g_hex_application_get_group_by (app);
	g_hex_application_get_font (app, &metrics, &desc);

	gtk_hex_set_group_type(GTK_HEX(gh), map_group_nick (group));

	if (metrics && desc) {
		gtk_hex_set_font(GTK_HEX(gh), metrics, desc);
	}

	return gh;
}

GtkWidget *
ghex_window_new_from_doc (GHexApplication *application,
                          HexDocument    *doc)
{
    GtkWidget *win = ghex_window_new (application);
    GtkWidget *gh = create_document_view(GHEX_WINDOW (win), doc);

    gtk_widget_show(gh);
    GHEX_WINDOW(win)->gh = GTK_HEX(gh);
    ghex_window_set_contents (GHEX_WINDOW (win), gh);
    g_signal_connect(G_OBJECT(doc), "document_changed",
                     G_CALLBACK(ghex_window_doc_changed), win);
    ghex_window_set_doc_name(GHEX_WINDOW(win),
                             hex_document_get_path_end (gtk_hex_get_document (GHEX_WINDOW(win)->gh)));
    ghex_window_set_sensitivity(GHEX_WINDOW(win));

    return win;
}

GtkWidget *
ghex_window_new_from_file (GHexApplication *application,
                           const gchar    *filename)
{
    GtkWidget *win = ghex_window_new (application);

    if(!ghex_window_load(GHEX_WINDOW(win), filename)) {
        gtk_widget_destroy(win);
        return NULL;
    }

    return win;
}

void
ghex_window_update_status_message(GHexWindow *win)
{
#define FMT_LEN    128
#define STATUS_LEN 128
	GHexApplication *app;
    gchar fmt[FMT_LEN], status[STATUS_LEN];
    gint current_pos;
    gint ss, se, len;
	const gchar     *offset_format;

	app = G_HEX_APPLICATION (gtk_window_get_application (GTK_WINDOW (win)));

    if(NULL == win->gh) {
        ghex_window_show_status(win, " ");
        return;
    }

	offset_format = g_hex_application_get_offset_format (app);

#if defined(__GNUC__) && (__GNUC__ > 4)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    current_pos = gtk_hex_get_cursor(win->gh);
    if(g_snprintf(fmt, FMT_LEN, _("Offset: %s"), offset_format) < FMT_LEN) {
        g_snprintf(status, STATUS_LEN, fmt, current_pos);
        if(gtk_hex_get_selection(win->gh, &ss, &se)) {
            if(g_snprintf(fmt, FMT_LEN, _("; %s bytes from %s to %s selected"),
                          offset_format, offset_format, offset_format) < FMT_LEN) {
                len = strlen(status);
                if(len < STATUS_LEN) {
                    // Variables 'ss' and 'se' denotes the offsets of the first and
                    // the last bytes that are part of the selection.
                    g_snprintf(status + len, STATUS_LEN - len, fmt, se - ss + 1, ss, se);
                }
            }
        }
#if defined(__GNUC__) && (__GNUC__ > 4)
#pragma GCC diagnostic pop
#endif

        ghex_window_show_status(win, status);
    }
    else
        ghex_window_show_status(win, " ");
}

static void
cursor_moved_cb(GtkHex *gtkhex, gpointer user_data)
{
    int i;
    int current_pos;
    HexDialogVal64 val;
	GHexWindow *win = GHEX_WINDOW(user_data);

    current_pos = gtk_hex_get_cursor(gtkhex);
    ghex_window_update_status_message(win);
    for (i = 0; i < 8; i++)
    {
        /* returns 0 on buffer overflow, which is what we want */
        val.v[i] = gtk_hex_get_byte(gtkhex, current_pos+i);
    }
    hex_dialog_updateview (HEX_DIALOG (win->dialog), &val);
}

gboolean
ghex_window_load(GHexWindow *win, const gchar *filename)
{
    HexDocument     *doc;
    GtkWidget       *gh;
    GtkWidget       *vbox;
    gboolean         active;
	GHexApplication *app;
	gboolean         show_offset;
	guint            undo_depth;

    g_return_val_if_fail(win != NULL, FALSE);
    g_return_val_if_fail(GHEX_IS_WINDOW(win), FALSE);
    g_return_val_if_fail(filename != NULL, FALSE);

	app = G_HEX_APPLICATION (g_application_get_default ());
	show_offset = g_hex_application_get_show_offset (app);
	undo_depth = g_hex_application_get_undo_depth (app);

    doc = hex_document_new_from_file (filename);
    if(!doc)
        return FALSE;
    hex_document_set_max_undo(doc, undo_depth);
    gh = create_document_view(win, doc);
    gtk_hex_show_offsets(GTK_HEX(gh), show_offset);
    g_signal_connect(G_OBJECT(doc), "document_changed",
                     G_CALLBACK(ghex_window_doc_changed), win);
    g_signal_connect(G_OBJECT(doc), "undo",
                     G_CALLBACK(set_doc_menu_sensitivity), win);;
    g_signal_connect(G_OBJECT(doc), "redo",
                     G_CALLBACK(set_doc_menu_sensitivity), win);;
    g_signal_connect(G_OBJECT(doc), "undo_stack_forget",
                     G_CALLBACK(set_doc_menu_sensitivity), win);;
    g_signal_connect(G_OBJECT(gh), "cursor_moved",
                     G_CALLBACK(cursor_moved_cb), win);
    gtk_widget_show(gh);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(win), 4);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(vbox), gh, TRUE, TRUE, 4);

    win->dialog = hex_dialog_new ();
    gtk_box_pack_start (GTK_BOX(vbox), win->dialog, FALSE, FALSE, 4);
    active = ghex_window_get_toggle_action_active (win, "type-tool");
    if (active) {
      gtk_widget_show (win->dialog);
    } else {
      gtk_widget_hide (win->dialog);
    }

    if(win->gh) {
        hex_document_remove_view(gtk_hex_get_document (win->gh), GTK_WIDGET(win->gh));
        g_signal_handlers_disconnect_matched(gtk_hex_get_document (win->gh),
                                             G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL,
                                             ghex_window_doc_changed,
                                             win);
    }
    ghex_window_set_contents (win, vbox);
    win->gh = GTK_HEX(gh);
    win->changed = FALSE;

    ghex_window_set_doc_name(win, hex_document_get_path_end (doc));
    ghex_window_set_sensitivity(win);

    g_signal_emit_by_name(G_OBJECT(gh), "cursor_moved");
   
    return TRUE;
}

void
ghex_window_set_doc_name(GHexWindow *win, const gchar *name)
{
    if(name != NULL) {
        gchar *title = g_strdup_printf(_("%s - GHex"), name);
        gtk_window_set_title(GTK_WINDOW(win), title);
        g_free(title);
    }
    else gtk_window_set_title(GTK_WINDOW(win), _("GHex"));
}

struct _MessageInfo {
  GHexWindow * win;
  guint timeoutid;
  guint handlerid;
};
typedef struct _MessageInfo MessageInfo;

/* Called if the win is destroyed before the timeout occurs. */
static void
remove_timeout_cb (GtkWidget *win, MessageInfo *mi )
{
	g_return_if_fail (mi != NULL);

	g_source_remove (mi->timeoutid);
	g_free (mi);
}

static gint
remove_message_timeout (MessageInfo * mi)
{
	/* Remove the status message */
	/* NOTE : Use space ' ' not an empty string '' */
	ghex_window_update_status_message (mi->win);
    g_signal_handlers_disconnect_by_func(G_OBJECT(mi->win),
                                         remove_timeout_cb, mi);
	g_free (mi);

	return FALSE; /* removes the timeout */
}

static const guint32 flash_length = 3000; /* 3 seconds, I hope */

/**
 * ghex_window_set_status
 * @win: Pointer to GHexWindow object.
 * @msg: Text of message to be shown on the status bar.
 *
 * Description:
 * Show the message in the status bar; if no status bar do nothing.
 * -- SnM
 */
void
ghex_window_show_status (GHexWindow *win, const gchar *msg)
{
	g_return_if_fail (win != NULL);
	g_return_if_fail (GHEX_IS_WINDOW (win));
	g_return_if_fail (msg != NULL);

	gtk_statusbar_pop (GTK_STATUSBAR (win->statusbar), 0);
	gtk_statusbar_push (GTK_STATUSBAR (win->statusbar), 0, msg);
}

/**
 * ghex_window_flash
 * @win: Pointer to GHexWindow object
 * @flash: Text of message to be flashed
 *
 * Description:
 * Flash the message in the statusbar for a few moments; if no
 * statusbar, do nothing. For trivial little status messages,
 * e.g. "Auto saving..."
 **/
void
ghex_window_flash (GHexWindow * win, const gchar * flash)
{
	MessageInfo * mi;
	g_return_if_fail (win != NULL);
	g_return_if_fail (GHEX_IS_WINDOW (win));
	g_return_if_fail (flash != NULL);

	mi = g_new (MessageInfo, 1);

	ghex_window_show_status (win, flash);

	mi->timeoutid =
		g_timeout_add (flash_length,
                       (GSourceFunc) remove_message_timeout,
                       mi);
	mi->handlerid =
		g_signal_connect (G_OBJECT(win),
                          "destroy",
                          G_CALLBACK (remove_timeout_cb),
                          mi );
	mi->win = win;
}

GHexWindow *
ghex_window_find_for_doc(HexDocument *doc)
{
	GHexApplication *app;
	GList           *win_node;
	GHexWindow      *win;

	app = G_HEX_APPLICATION (g_application_get_default ());
	win_node = gtk_application_get_windows (GTK_APPLICATION (app));
	while (win_node) {
		if (GHEX_IS_WINDOW (win_node->data)) {
			win = GHEX_WINDOW(win_node->data);
			if (win->gh && gtk_hex_get_document (win->gh) == doc)
				return win;
			win_node = g_list_next (win_node);
		}
	}
	return NULL;
}

gboolean
ghex_window_save_as(GHexWindow *win)
{
	HexDocument *doc;
	GtkWidget *file_sel;
	gboolean ret_val = TRUE;
    GtkResponseType resp;

	if(win->gh == NULL)
		return ret_val;

	doc = gtk_hex_get_document (win->gh);
	file_sel =
        gtk_file_chooser_dialog_new(_("Select a file to save buffer as"),
                                    GTK_WINDOW(win),
                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                    _("Cancel"), GTK_RESPONSE_CANCEL,
                                    _("Save"), GTK_RESPONSE_OK,
                                    NULL);

	if(hex_document_get_file_name (doc))
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_sel),
                                      hex_document_get_file_name (doc));
	gtk_window_set_modal(GTK_WINDOW(file_sel), TRUE);
	gtk_window_set_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	gtk_widget_show (file_sel);

	resp = gtk_dialog_run (GTK_DIALOG (file_sel));
	if(resp == GTK_RESPONSE_OK) {
		FILE *file;
		gchar *flash;
        gchar *gtk_file_name, *path_end;
        const gchar *filename;

        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_sel));
        if(access(filename, F_OK) == 0) {
            GtkWidget *mbox;

            gtk_file_name = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

			mbox = gtk_message_dialog_new(GTK_WINDOW(win),
										  GTK_DIALOG_MODAL|
										  GTK_DIALOG_DESTROY_WITH_PARENT,
										  GTK_MESSAGE_QUESTION,
										  GTK_BUTTONS_YES_NO,
                                          _("File %s exists.\n"
                                            "Do you want to overwrite it?"),
                                          gtk_file_name);
            g_free(gtk_file_name);

            gtk_dialog_set_default_response(GTK_DIALOG(mbox), GTK_RESPONSE_NO);

            ret_val = (ask_user(GTK_MESSAGE_DIALOG(mbox)) == GTK_RESPONSE_YES);
            gtk_widget_destroy (mbox);
        }

        if(ret_val) {
            if((file = fopen(filename, "wb")) != NULL) {
                if(hex_document_write_to_file(doc, file)) {
					hex_document_set_file_name (doc, filename);
					hex_document_set_changed (doc, FALSE);
                    win->changed = FALSE;

                    path_end = g_path_get_basename (hex_document_get_file_name (doc));
                    hex_document_set_path_end (doc, g_filename_to_utf8(path_end, -1, NULL, NULL, NULL));
                    ghex_window_set_doc_name(win, hex_document_get_path_end (doc));
                    gtk_file_name = g_filename_to_utf8(hex_document_get_file_name (doc), -1, NULL, NULL, NULL);
                    flash = g_strdup_printf(_("Saved buffer to file %s"), gtk_file_name);
                    ghex_window_flash(win, flash);
                    g_free(path_end);
                    g_free(gtk_file_name);
                    g_free(flash);
                }
                else {
                    display_error_dialog (GTK_WIDGET (win), _("Error saving file!"));
                    ret_val = FALSE;
                }
                fclose(file);
            }
            else {
                display_error_dialog (GTK_WIDGET (win), _("Can't open file for writing!"));
                ret_val = TRUE;
            }
        }
	}
	gtk_widget_destroy(file_sel);
	return ret_val;
}

static gboolean
ghex_window_path_exists (const gchar* path)
{
	GFile *file;
	gboolean res;

	g_return_val_if_fail (path != NULL, FALSE);
	file = g_file_new_for_path (path);
	g_return_val_if_fail (file != NULL, FALSE);
	res = g_file_query_exists (file, NULL);

	g_object_unref (file);
	return res;
}

gboolean
ghex_window_ok_to_close(GHexWindow *win)
{
	GtkWidget *mbox;
	gint reply;
	gboolean file_exists;
	HexDocument *doc;

	if (win->gh == NULL)
		return TRUE;

	doc = gtk_hex_get_document (win->gh);
	file_exists = ghex_window_path_exists (hex_document_get_file_name (doc));
	if (!hex_document_has_changed (doc) && file_exists)
		return TRUE;

	mbox = gtk_message_dialog_new (GTK_WINDOW (win),
								   GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
								   GTK_MESSAGE_QUESTION,
								   GTK_BUTTONS_NONE,
								   _("File %s has changed since last save.\n"
								   "Do you want to save changes?"),
								   hex_document_get_path_end (doc));
			
	gtk_dialog_add_button (GTK_DIALOG (mbox), _("Do_n't save"), GTK_RESPONSE_NO);
	gtk_dialog_add_button (GTK_DIALOG (mbox), _("_Cancel"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (mbox), _("_Save"), GTK_RESPONSE_YES);
	gtk_dialog_set_default_response (GTK_DIALOG (mbox), GTK_RESPONSE_YES);
	gtk_window_set_resizable (GTK_WINDOW (mbox), FALSE);
	
	reply = gtk_dialog_run (GTK_DIALOG (mbox));
	
	gtk_widget_destroy (mbox);
		
	if (reply == GTK_RESPONSE_YES) {
		if (!file_exists) {
			if (!ghex_window_save_as (win)) {
				return FALSE;
			}
		} else {
			if (!hex_document_is_writable (doc)) {
				display_error_dialog (GTK_WIDGET (win), _("You don't have the permissions to save the file!"));
				return FALSE;
			} else if (!hex_document_write (doc)) {
				display_error_dialog (GTK_WIDGET (win), _("An error occurred while saving file!"));
				return FALSE;
			}
		}
	} else if (reply == GTK_RESPONSE_CANCEL) {
		return FALSE;
	}

	return TRUE;
}
