/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ui.c - main menus and callbacks; utility functions

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

#include <config.h>
#include <string.h>
#include <unistd.h> /* for F_OK and W_OK */

#include <gtk/gtk.h>
#include <glib/gi18n.h>


#include "ui.h"
#include "ghex-window.h"
#include "findreplace.h"
#include "converter.h"
#include "print.h"
#include "chartable.h"
#include "configuration.h"

static void ghex_print(GtkHex *gh, gboolean preview);

guint group_type[3] = {
	GROUP_BYTE,
	GROUP_WORD,
	GROUP_LONG,
};

gchar *group_type_label[3] = {
	N_("_Bytes"),
	N_("_Words"),
	N_("_Longwords"),
};

guint search_type = 0;
gchar *search_type_label[] = {
	N_("hex data"),
	N_("ASCII data"),
};

gint
ask_user(GtkMessageDialog *message_box)
{
	return gtk_dialog_run(GTK_DIALOG(message_box));
}

/*
 * callbacks for global menus
 */
void
about_cb (GSimpleAction *action,
          GVariant *value,
          GHexWindow *window)
{
	gchar *copyright;
	gchar *license_translated;

	const gchar *authors[] = {
		"Jaka Mo\304\215nik",
		"Chema Celorio",
		"Shivram Upadhyayula",
		"Rodney Dawes",
		"Jonathon Jongsma",
		"Kalev Lember",
		NULL
	};

	const gchar *documentation_credits[] = {
		"Jaka Mo\304\215nik",
		"Sun GNOME Documentation Team",
		NULL
	};

	const gchar *license[] = {
		N_("This program is free software; you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation; either version 2 of the License, or "
		   "(at your option) any later version."),
		N_("This program is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License "
		   "along with this program; if not, write to the Free Software Foundation, Inc., "
		   "51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA")
	};
	license_translated = g_strjoin ("\n\n",
	                                _(license[0]),
	                                _(license[1]),
	                                _(license[2]),
	                                NULL);

	/* Translators: these two strings here indicate the copyright time span,
	   e.g. 1998-2018. */
	copyright = g_strdup_printf (_("Copyright © %Id–%Id The GHex authors"), 1998, 2018);

	gtk_show_about_dialog (GTK_WINDOW (window),
	                       "authors", authors,
	                       "comments", _("A binary file editor"),
	                       "copyright", copyright,
	                       "documenters", documentation_credits,
	                       "license", license_translated,
	                       "logo-icon-name", "org.gnome.GHex",
	                       "program-name", "GHex",
	                       "title", _("About GHex"),
	                       "translator-credits", _("translator-credits"),
	                       "version", PACKAGE_VERSION,
	                       "website", "https://wiki.gnome.org/Apps/Ghex",
	                       "website-label", _("GHex Website"),
	                       "wrap-license", TRUE,
	                       NULL);

	g_free (license_translated);
	g_free (copyright);
}

void
help_cb (GSimpleAction *action,
         GVariant *value,
         GHexWindow *window)
{
	GError *error = NULL;

	gtk_show_uri_on_window (GTK_WINDOW (window),
	                        "help:ghex",
	                        GDK_CURRENT_TIME,
	                        &error);

	if (error != NULL) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						_("There was an error displaying help: \n%s"),
						error->message);

		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_window_present (GTK_WINDOW (dialog));

		g_error_free (error);
	}
}

void 
paste_cb (GSimpleAction *action,
          GVariant *value,
          GHexWindow *window)
{
	GHexWindow *win = GHEX_WINDOW(window);

	if(win->gh)
		gtk_hex_paste_from_clipboard(win->gh);
}

void 
copy_cb (GSimpleAction *action,
         GVariant *value,
         GHexWindow *window)
{
	GHexWindow *win = GHEX_WINDOW(window);

	if(win->gh)
		gtk_hex_copy_to_clipboard(win->gh);
}

void 
cut_cb (GSimpleAction *action,
        GVariant *value,
        GHexWindow *window)
{
	GHexWindow *win = GHEX_WINDOW(window);

	if(win->gh)
		gtk_hex_cut_to_clipboard(win->gh);
}

void
quit_app_cb (GSimpleAction *action,
             GVariant *value,
             GHexWindow *window)
{
	const GList *doc_node;
	GHexWindow *win;
	HexDocument *doc;

	doc_node = hex_document_get_list();
	while(doc_node) {
		doc = HEX_DOCUMENT(doc_node->data);
		win = ghex_window_find_for_doc(doc);
		if(win && !ghex_window_ok_to_close(win))
			return;
		doc_node = doc_node->next;
	}
	g_application_quit (g_application_get_default ());
}

void
save_cb (GSimpleAction *action,
         GVariant *value,
         GHexWindow *window)
{
	GHexWindow *win = GHEX_WINDOW(window);
	HexDocument *doc;

	if(win->gh)
		doc = gtk_hex_get_document (win->gh);
	else
		doc = NULL;

	if(doc == NULL)
		return;

	if(!hex_document_is_writable(doc)) {
		display_error_dialog (GTK_WIDGET (win), _("You don't have the permissions to save the file!"));
		return;
	}

	if(!hex_document_write(doc))
		display_error_dialog (GTK_WIDGET (win), _("An error occurred while saving file!"));
	else {
		gchar *flash;
		gchar *gtk_file_name;

		gtk_file_name = g_filename_to_utf8 (hex_document_get_file_name (doc), -1,
											NULL, NULL, NULL);
		flash = g_strdup_printf(_("Saved buffer to file %s"), gtk_file_name);

		ghex_window_flash (win, flash);
		g_free(gtk_file_name);
		g_free(flash);
	}
}

void
open_cb (GSimpleAction *action,
         GVariant *value,
         GHexWindow *window)
{
	GHexApplication *app;
	GHexWindow      *win;
	GtkWidget       *file_sel;
	GtkResponseType  resp;

	app = G_HEX_APPLICATION (gtk_window_get_application (GTK_WINDOW (window)));
	win = window;

	file_sel = gtk_file_chooser_dialog_new (_("Select a file to open"),
										    GTK_WINDOW (window),
										    GTK_FILE_CHOOSER_ACTION_OPEN,
										    _("Cancel"), GTK_RESPONSE_CANCEL,
										    _("Open"), GTK_RESPONSE_OK,
										    NULL);
	gtk_window_set_modal (GTK_WINDOW (file_sel), TRUE);
	gtk_window_set_position (GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	gtk_widget_show (file_sel);

	resp = gtk_dialog_run (GTK_DIALOG (file_sel));

	if (resp == GTK_RESPONSE_OK) {
		gchar *flash;

		if (GHEX_WINDOW (win)->gh != NULL) {
			win = GHEX_WINDOW (ghex_window_new_from_file (G_HEX_APPLICATION (g_application_get_default ()),
			                                              gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_sel))));
			if (win)
				gtk_widget_show (GTK_WIDGET (win));
		} else {
			if (!ghex_window_load (GHEX_WINDOW (win),
								   gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_sel))))
				win = NULL;
		}

		if (win) {
			gchar *gtk_file_name;
			gtk_file_name = g_filename_to_utf8
				(hex_document_get_file_name (gtk_hex_get_document (GHEX_WINDOW (win)->gh)), -1,
				 NULL, NULL, NULL);
			flash = g_strdup_printf (_("Loaded file %s"), gtk_file_name);
			ghex_window_flash (GHEX_WINDOW (win), flash);
			g_free (gtk_file_name);
			g_free (flash);
			if (win->converter && GTK_IS_WIDGET (win->converter))
				g_hex_converter_set_can_grap (G_HEX_CONVERTER (win->converter), TRUE);
		} else {
			win = GHEX_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (app)));
			display_error_dialog (GTK_WIDGET (win), _("Can not open file!"));
		}
	}

	gtk_widget_destroy(file_sel);
}

void
save_as_cb (GSimpleAction *action,
            GVariant *value,
            GHexWindow *window)
{
	GHexWindow *win = GHEX_WINDOW(window);
	HexDocument *doc;

	if(win->gh)
		doc = gtk_hex_get_document (win->gh);
	else
		doc = NULL;

	if(doc == NULL)
		return;

	ghex_window_save_as(win);
}

void
print_cb (GSimpleAction *action,
          GVariant *value,
          GHexWindow *window)
{
	GHexWindow *win = GHEX_WINDOW(window);

	if(win->gh == NULL)
		return;

	ghex_print(win->gh, FALSE);
}

void
print_preview_cb (GSimpleAction *action,
                  GVariant *value,
                  GHexWindow *window)
{
	GHexWindow *win = GHEX_WINDOW(window);

	if(win->gh == NULL)
		return;

	ghex_print(win->gh, TRUE);
}

void
export_html_cb (GSimpleAction *action,
                GVariant *value,
                GHexWindow *window)
{
	GHexWindow *win = GHEX_WINDOW(window);
	HexDocument *doc;
	GtkWidget *file_sel;
	GtkResponseType resp;

	if(win->gh)
		doc = gtk_hex_get_document (win->gh);
	else
		doc = NULL;

	if(doc == NULL)
		return;

	file_sel = gtk_file_chooser_dialog_new(_("Select path and file name for the HTML source"),
										   GTK_WINDOW(win),
										   GTK_FILE_CHOOSER_ACTION_SAVE,
										   _("Cancel"), GTK_RESPONSE_CANCEL,
										   _("Save"), GTK_RESPONSE_OK,
										   NULL);
#if 0
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_sel), hex_document_get_file_name (doc));
#endif
	gtk_window_set_modal(GTK_WINDOW(file_sel), TRUE);
	gtk_window_set_position(GTK_WINDOW (file_sel), GTK_WIN_POS_MOUSE);
	gtk_widget_show(file_sel);

	resp = gtk_dialog_run(GTK_DIALOG(file_sel));

	if(resp == GTK_RESPONSE_OK) {
		gchar *html_path;
		gchar *sep, *base_name, *check_path;
		GtkHex *view = win->gh;

		html_path = g_path_get_dirname (gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_sel)));
		base_name = g_path_get_basename (gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_sel)));

		gtk_widget_destroy(file_sel);

		sep = strstr(base_name, ".htm");
		if(sep)
			*sep = 0;

		if(*base_name == 0) {
			g_free(html_path);
			g_free(base_name);
			display_error_dialog(GTK_WIDGET (win), _("You need to specify a base name for "
										"the HTML files."));
			return;
		}

		check_path = g_strdup_printf("%s/%s.html", html_path, base_name);
		if(access(check_path, F_OK) == 0) {
			gint reply;
			GtkWidget *mbox;

			if(access(check_path, W_OK) != 0) {
				display_error_dialog(GTK_WIDGET (win), _("You don't have the permission to write to the selected path.\n"));
				g_free(html_path);
				g_free(base_name);
				g_free(check_path);
				return;
			}

			mbox = gtk_message_dialog_new(GTK_WINDOW(win),
										  GTK_DIALOG_MODAL|
										  GTK_DIALOG_DESTROY_WITH_PARENT,
										  GTK_MESSAGE_QUESTION,
										  GTK_BUTTONS_YES_NO,
										  _("Saving to HTML will overwrite some files.\n"
											"Do you want to proceed?"));
			gtk_dialog_set_default_response(GTK_DIALOG(mbox), GTK_RESPONSE_NO);
			reply = ask_user(GTK_MESSAGE_DIALOG(mbox));
			gtk_widget_destroy(mbox);
			if(reply != GTK_RESPONSE_YES) {
				g_free(html_path);
				g_free(base_name);
				g_free(check_path);
				return;
			}
		}
		else {
			if(access(html_path, W_OK) != 0) {
				display_error_dialog(GTK_WIDGET (win), _("You don't have the permission to write to the selected path.\n"));
				g_free(html_path);
				g_free(base_name);
				g_free(check_path);
				return;
			}
		}
		g_free(check_path);

		hex_document_export_html(doc, html_path, base_name, 0,
								 hex_document_get_file_size (doc),
								 gtk_hex_get_cpl (view),
								 gtk_hex_get_visible_lines (view),
								 gtk_hex_get_group_type (view));
		g_free(html_path);
		g_free(base_name);
	}
	else
		gtk_widget_destroy(GTK_WIDGET(file_sel));
}

void
close_cb (GSimpleAction *action,
          GVariant *value,
          GHexWindow *window)
{
	GHexApplication *app;
	GHexWindow      *other_win;
	HexDocument     *doc;
	GList           *window_list;

	app = G_HEX_APPLICATION (gtk_window_get_application (GTK_WINDOW (window)));
	window_list = gtk_application_get_windows (GTK_APPLICATION (app));

	if (window->gh == NULL) {
		if (window_list->next && GHEX_IS_WINDOW (window_list->data))
			gtk_widget_destroy (GTK_WIDGET (window));
		return;
	}

	doc = gtk_hex_get_document (window->gh);
	
	if (!ghex_window_ok_to_close (window))
		return;
	
	window_list = gtk_application_get_windows (GTK_APPLICATION (app));
	while(window_list) {
		if (GHEX_IS_WINDOW (window_list->data)) {
			other_win = GHEX_WINDOW(window_list->data);
			window_list = window_list->next;
			if(other_win->gh && gtk_hex_get_document (other_win->gh) == doc && other_win != window)
				gtk_widget_destroy(GTK_WIDGET(other_win));
		}
	}


	/* If we have created the converter window disable the 
	 * "Get cursor value" button
	 */
	if (window->converter && GTK_IS_WIDGET (window->converter))
		g_hex_converter_set_can_grap (G_HEX_CONVERTER (window->converter), FALSE);

	window_list = gtk_application_get_windows (GTK_APPLICATION (app));
	if (window_list->next == NULL) {
		ghex_window_destroy_contents (window);
		window->gh = NULL;
		ghex_window_set_sensitivity (window);
		ghex_window_set_doc_name (window, NULL);

        /* Clear the contents of status bar after closing the files */
        ghex_window_show_status (window, " ");
    } else {
        gtk_widget_destroy (GTK_WIDGET (window));
	}

    /* this implicitly destroys all views including this one */
    g_object_unref (G_OBJECT (doc));
}

void
insert_mode_cb (GSimpleAction *action,
                GVariant *value,
                GHexWindow *window)
{
    GHexWindow *win;
    GVariant   *state;
    gboolean    active;

    win = GHEX_WINDOW (window);
    state = g_action_get_state (G_ACTION (action));
    active = !g_variant_get_boolean (state);
    if (active) {
        gtk_label_set_text (GTK_LABEL (win->statusbar_insert_mode), "INS");
    } else {
        gtk_label_set_text (GTK_LABEL (win->statusbar_insert_mode), "OVR");
    }
    state = g_variant_new_boolean (active);
    g_simple_action_set_state (action, state);

    if (win->gh != NULL)
        gtk_hex_set_insert_mode (win->gh, active);
}

void
character_table_cb (GSimpleAction *action,
                    GVariant      *value,
                    GHexWindow    *window)
{
	if (G_LIKELY (!window->characters || !GTK_IS_WIDGET (window->characters))) {
		window->characters = g_hex_characters_new (window);
	}
	gtk_window_present (GTK_WINDOW (window->characters));
}

void
converter_cb (GSimpleAction *action,
              GVariant *value,
              GHexWindow *window)
{
	if (G_LIKELY (!window->converter || !GTK_IS_WIDGET (window->converter))) {
		window->converter = g_hex_converter_new (window);
	}
	// Can't grab if placeholder is open
	g_hex_converter_set_can_grap (G_HEX_CONVERTER (window->converter), !GTK_IS_IMAGE (window->contents));
	gtk_window_present (GTK_WINDOW (window->converter));
}

void
type_dialog_cb (GSimpleAction *action,
                GVariant *value,
                GHexWindow *window)
{
    GHexWindow *win;
    GVariant   *state;
    gboolean    active;

    win = GHEX_WINDOW (window);

    state = g_action_get_state (G_ACTION (action));
    active = !g_variant_get_boolean (state);
    state = g_variant_new_boolean (active);
    g_simple_action_set_state (action, state);

    if (!win->dialog)
        return;
    if (active) {
        if (!gtk_widget_get_visible (win->dialog)) {
            gtk_widget_show (win->dialog);
        }
    }
    else if (gtk_widget_get_visible (win->dialog)) {
        gtk_widget_hide (GTK_WIDGET (win->dialog));
    }
}

gint
map_group_nick (const gchar* nick)
{
	if (g_strcmp0(nick, "words") == 0) {
		return GROUP_WORD;
	} else if (g_strcmp0(nick, "longwords") == 0) {
		return GROUP_LONG;
	}
	return GROUP_BYTE;
}

const gchar*
map_nick_group (gint group)
{
	if (group == GROUP_WORD) {
		return "words";
	} else if (group == GROUP_LONG) {
		return "longwords";
	}
	return "bytes";
}

void
group_data_cb (GSimpleAction *action,
               GVariant *value,
               GHexWindow *window)
{
	const gchar *arg;
	guint        mode;

	arg = g_variant_get_string (value, NULL);
	mode = map_group_nick (arg);

	switch (mode) {
		case GROUP_WORD:
			gtk_label_set_text (GTK_LABEL (window->statusbar_display_mode), "Word");
			break;
		case GROUP_LONG:
			gtk_label_set_text (GTK_LABEL (window->statusbar_display_mode), "Longword");
			break;
		default:
			gtk_label_set_text (GTK_LABEL (window->statusbar_display_mode), "Byte");
	}

    g_simple_action_set_state (action, value);

    if (window->gh != NULL)
        gtk_hex_set_group_type (window->gh, mode);
}

void
prefs_cb (GSimpleAction *action,
          GVariant *value,
          GHexWindow *window)
{
	GtkWidget *dialog;

	dialog = g_hex_preferences_new (window);

	gtk_window_present (GTK_WINDOW (dialog));
}

void
revert_cb (GSimpleAction *action,
           GVariant *value,
           GHexWindow *window)
{
	GHexWindow *win;
   	HexDocument *doc;
	GtkWidget *mbox;
	gint reply;
	
	win = GHEX_WINDOW(window);
	if(win->gh)
		doc = gtk_hex_get_document (win->gh);
	else
		doc = NULL;

	if(doc == NULL)
		return;

	if (hex_document_has_changed (doc)) {
		mbox = gtk_message_dialog_new(GTK_WINDOW(win),
									  GTK_DIALOG_MODAL|
									  GTK_DIALOG_DESTROY_WITH_PARENT,
									  GTK_MESSAGE_QUESTION,
									  GTK_BUTTONS_YES_NO,
									  _("Really revert file %s?"),
									  hex_document_get_path_end (doc));
		gtk_dialog_set_default_response(GTK_DIALOG(mbox), GTK_RESPONSE_NO);

		reply = ask_user(GTK_MESSAGE_DIALOG(mbox));
		
		if(reply == GTK_RESPONSE_YES) {
			gchar *flash;
			gchar *gtk_file_name;

			gtk_file_name = g_filename_to_utf8 (hex_document_get_file_name (doc), -1,
												NULL, NULL, NULL);
			win->changed = FALSE;
			hex_document_read(doc);
			flash = g_strdup_printf(_("Reverted buffer from file %s"), gtk_file_name);
			ghex_window_flash(win, flash);
			ghex_window_set_sensitivity(win);
			g_free(gtk_file_name);
			g_free(flash);
		}

		gtk_widget_destroy (mbox);
	}
}

/**
 * ghex_print
 * @preview: Indicates whether to show only a print preview (TRUE) or
to display the print dialog.
 *
 * Prints or previews the current document.
 **/
static void
ghex_print(GtkHex *gh, gboolean preview)
{
	GHexPrintJobInfo *pji;
	HexDocument *doc;
	GtkPrintOperationResult result;
	GError *error = NULL;
	gchar *basename;
	gchar *gtk_file_name;

	doc = gtk_hex_get_document (gh);

	gtk_file_name = g_filename_to_utf8 (hex_document_get_file_name (doc), -1, NULL, NULL, NULL);
	basename = g_filename_display_basename (gtk_file_name);

	pji = ghex_print_job_info_new(doc, gtk_hex_get_group_type (gh));
	pji->master = gtk_print_operation_new ();
	pji->config = gtk_print_settings_new ();
	gtk_print_settings_set (pji->config, GTK_PRINT_SETTINGS_OUTPUT_BASENAME, basename);
	gtk_print_settings_set_paper_size (pji->config, gtk_paper_size_new (GTK_PAPER_NAME_A4));
	gtk_print_operation_set_unit (pji->master, GTK_UNIT_POINTS);
	gtk_print_operation_set_print_settings (pji->master, pji->config);
	gtk_print_operation_set_embed_page_setup (pji->master, TRUE);
	gtk_print_operation_set_show_progress (pji->master, TRUE);
	g_signal_connect (pji->master, "draw-page",
	                  G_CALLBACK (print_page), pji);
	g_signal_connect (pji->master, "begin-print",
	                  G_CALLBACK (begin_print), pji);

	if (!pji)
		return;

	pji->preview = preview;

	if (!pji->preview)
		result = gtk_print_operation_run (pji->master, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, NULL, &error);
	else
		result = gtk_print_operation_run (pji->master, GTK_PRINT_OPERATION_ACTION_PREVIEW, NULL, &error);

	if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
		g_print ("%s\n", error->message);
		g_error_free (error);
	}
	ghex_print_job_info_destroy (pji);
	g_free (basename);
	g_free (gtk_file_name);
}

void
display_error_dialog (GtkWidget *win, const gchar *msg)
{
	GtkWidget *error_dlg;

	g_return_if_fail (win != NULL);
	g_return_if_fail (msg != NULL);
	error_dlg = gtk_message_dialog_new (
			GTK_WINDOW (win),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_OK,
			"%s",
			msg);

	gtk_dialog_set_default_response (GTK_DIALOG (error_dlg), GTK_RESPONSE_OK);
	gtk_window_set_resizable (GTK_WINDOW (error_dlg), FALSE);
	gtk_dialog_run (GTK_DIALOG (error_dlg));
	gtk_widget_destroy (error_dlg);
}

void
display_info_dialog (GtkWindow *win, const gchar *msg, ...)
{
	GtkWidget *info_dlg;
	gchar *real_msg;
	va_list args;

	g_return_if_fail (win != NULL);
	g_return_if_fail (msg != NULL);
	va_start(args, msg);
	real_msg = g_strdup_vprintf(msg, args);
	va_end(args);
	info_dlg = gtk_message_dialog_new (
			GTK_WINDOW (win),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_OK,
			"%s",
			real_msg);
	g_free(real_msg);

	gtk_dialog_set_default_response (GTK_DIALOG (info_dlg), GTK_RESPONSE_OK);
	gtk_window_set_resizable (GTK_WINDOW (info_dlg), FALSE);
	gtk_dialog_run (GTK_DIALOG (info_dlg));
	gtk_widget_destroy (info_dlg);
}

void
add_view_cb (GSimpleAction *action,
             GVariant *value,
             GHexWindow *window)
{
	GHexWindow *win = GHEX_WINDOW(window);
	GtkWidget *newwin;

	if(win->gh == NULL)
		return;

	newwin = ghex_window_new_from_doc (G_HEX_APPLICATION (g_application_get_default ()),
	                                   gtk_hex_get_document (win->gh));
	gtk_widget_show(newwin);
}

GtkWidget *create_hex_view(HexDocument *doc)
{
	PangoFontMetrics     *metrics;
	PangoFontDescription *desc;
	GHexApplication      *app;
	gint group;
	GtkWidget *gh = hex_document_add_view(doc);

	app = G_HEX_APPLICATION (g_application_get_default ());

	group = map_group_nick (g_hex_application_get_group_by (app));
	gtk_hex_set_group_type(GTK_HEX(gh), group);

	g_hex_application_get_font (app, &metrics, &desc);
	if (metrics && desc) {
		gtk_hex_set_font(GTK_HEX(gh), metrics, desc);
	}
	gtk_hex_set_insert_mode(GTK_HEX(gh), TRUE);
	gtk_hex_set_geometry(GTK_HEX(gh), 16, 4);

	return gh;
}

gint get_search_string(HexDocument *doc, gchar **str)
{
	guint size = hex_document_get_file_size (doc);

	if(size > 0)
		*str = (gchar *)hex_document_get_data(doc, 0, size);
	else
		*str = NULL;
	return size;
}
