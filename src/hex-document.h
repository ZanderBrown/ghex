/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* hex-document.h

   Copyright (C) 1998 - 2002 Free Software Foundation

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

#ifndef __HEX_DOCUMENT_H__
#define __HEX_DOCUMENT_H__

#include <stdio.h>

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _HexChangeData     HexChangeData;

typedef enum {
	HEX_CHANGE_STRING,
	HEX_CHANGE_BYTE
} HexChangeType;

#define HEX_TYPE_CHANGE_DATA (hex_change_data_get_type ())

struct _HexChangeData
{
	guint start, end, rep_len;
	gboolean lower_nibble;
	gboolean insert;
	HexChangeType type;
	gchar *v_string;
	gchar v_byte;
};

#define HEX_TYPE_DOCUMENT (hex_document_get_type ())
G_DECLARE_DERIVABLE_TYPE (HexDocument, hex_document, HEX, DOCUMENT, GObject)

struct _HexDocumentClass
{
	GObjectClass parent_class;

	void (*document_changed)(HexDocument *, gpointer, gboolean);
	void (*undo)(HexDocument *);
	void (*redo)(HexDocument *);
	void (*undo_stack_forget)(HexDocument *);
};

HexDocument *hex_document_new(void);
HexDocument *hex_document_new_from_file(const gchar *name);
void        hex_document_set_data(HexDocument *doc, guint offset,
								  guint len, guint rep_len, guchar *data,
								  gboolean undoable);
void        hex_document_set_byte(HexDocument *doc, guchar val, guint offset,
								  gboolean insert, gboolean undoable);
void        hex_document_set_nibble(HexDocument *doc, guchar val,
									guint offset, gboolean lower_nibble,
									gboolean insert, gboolean undoable);
guchar      hex_document_get_byte(HexDocument *doc, guint offset);
guchar      *hex_document_get_data(HexDocument *doc, guint offset, guint len);
void        hex_document_delete_data(HexDocument *doc, guint offset,
									 guint len, gboolean undoable);
gint        hex_document_read(HexDocument *doc);
gint        hex_document_write(HexDocument *doc);
gint        hex_document_write_to_file(HexDocument *doc, FILE *file);
gint        hex_document_export_html(HexDocument *doc,
									 gchar *html_path, gchar *base_name,
									 guint start, guint end,
									 guint cpl, guint lpp, guint cpw);
gboolean    hex_document_has_changed(HexDocument *doc);
void        hex_document_changed(HexDocument *doc, gpointer change_data,
								 gboolean push_undo);
void        hex_document_set_max_undo(HexDocument *doc, guint max_undo);
gboolean    hex_document_undo(HexDocument *doc);
gboolean    hex_document_redo(HexDocument *doc);
gint        hex_document_compare_data(HexDocument *doc, guchar *s2,
									  gint pos, gint len);
gint        hex_document_find_forward(HexDocument *doc, guint start,
									  guchar *what, gint len, guint *found);
gint        hex_document_find_backward(HexDocument *doc, guint start,
									   guchar *what, gint len, guint *found);
void        hex_document_remove_view(HexDocument *doc, GtkWidget *view);
GtkWidget   *hex_document_add_view(HexDocument *doc);
const GList *hex_document_get_list(void);
gboolean    hex_document_is_writable(HexDocument *doc);

guint
hex_document_get_file_size   (HexDocument *self);
const gchar *
hex_document_get_file_name   (HexDocument *self);
void
hex_document_set_file_name   (HexDocument *self,
                              const gchar *file);
const gchar *
hex_document_get_path_end    (HexDocument *self);
void
hex_document_set_path_end    (HexDocument *self,
                              const gchar *path_end);
void
hex_document_set_changed     (HexDocument *self,
                              gboolean     changed);
gboolean
hex_document_get_can_undo    (HexDocument *self);
gboolean
hex_document_get_can_redo    (HexDocument *self);
GList *
hex_document_get_views       (HexDocument *self);
HexChangeData *
hex_document_get_last_change (HexDocument *self);

G_END_DECLS

#endif /* __HEX_DOCUMENT_H__ */
