/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* main.c - genesis of a GHex application

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
#include <glib/gi18n.h>

#include "application.h"

#ifdef G_OS_WIN32
static gchar *
_ghex_win32_locale_dir (void)
{
    gchar *install_dir;
    gchar *locale_dir = NULL;
    gchar *utf8_locale_dir;

    install_dir = g_win32_get_package_installation_directory_of_module (NULL);

    if (install_dir) {
        utf8_locale_dir = g_build_filename (install_dir, "share", "locale", NULL);
        locale_dir = g_win32_locale_filename_from_utf8 (utf8_locale_dir);

        g_free (install_dir);
        g_free (utf8_locale_dir);
    }

    return locale_dir;
}
#endif

static gchar *
ghex_locale_dir (void)
{
#ifdef G_OS_WIN32
    return _ghex_win32_locale_dir ();
#else
    return g_strdup (LOCALEDIR);
#endif
}

int
main(int argc, char **argv)
{
	GHexApplication *application;
	gchar *locale_dir;
	gint retval;

	locale_dir = ghex_locale_dir ();
	bindtextdomain (GETTEXT_PACKAGE, locale_dir);
	g_free (locale_dir);

	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_application_name (_("GHex"));

	application = g_hex_application_new ();

	retval = g_application_run (G_APPLICATION (application), argc, argv);
	g_object_unref (application);

	return retval;
}
