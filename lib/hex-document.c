/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* hex-document.c - implementation of a hex document

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
#include <glib-object.h>
#include <glib/gi18n.h>

#include <gtkhex.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

static void hex_document_class_init     (HexDocumentClass *);
static void hex_document_init           (HexDocument *doc);
static void hex_document_finalize       (GObject *obj);
static void hex_document_real_changed   (HexDocument *doc,
										 gpointer change_data,
										 gboolean undoable);
static void hex_document_real_redo      (HexDocument   *doc);
static void hex_document_real_undo      (HexDocument   *doc);
static void move_gap_to                 (HexDocument *doc,
										 guint offset,
								  	     gint min_size);
static void free_stack                  (GList *stack);
static gint undo_stack_push             (HexDocument *doc,
									     HexChangeData *change_data);
static void undo_stack_descend          (HexDocument *doc);
static void undo_stack_ascend           (HexDocument *doc);
static void undo_stack_free             (HexDocument *doc);
static gboolean get_document_attributes (HexDocument *doc);

#define DEFAULT_UNDO_DEPTH 1024

G_DEFINE_POINTER_TYPE (HexChangeData, hex_change_data)

typedef struct _HexDocumentPrivate HexDocumentPrivate;

struct _HexDocumentPrivate
{
	GList *views;      /* a list of GtkHex widgets showing this document */

	gchar *file_name;
	gchar *path_end;

	guchar *buffer;    /* data buffer */
	guchar *gap_pos;   /* pointer to the start of insertion gap */
	gint gap_size;     /* insertion gap size */
	guint buffer_size; /* buffer size = file size + gap size */
	guint file_size;   /* real file size */

	gboolean changed;

	GList *undo_stack; /* stack base */
	GList *undo_top;   /* top of the stack (for redo) */
	guint undo_depth;  /* number of els on stack */
	guint undo_max;    /* max undo depth */
};

G_DEFINE_TYPE_WITH_PRIVATE (HexDocument, hex_document, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_CAN_UNDO,
	PROP_CAN_REDO,
	LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP] = { NULL, };

enum {
	DOCUMENT_CHANGED,
	UNDO,
	REDO,
	UNDO_STACK_FORGET,
	LAST_SIGNAL
};

static gint hex_signals[LAST_SIGNAL];

static GList *doc_list = NULL;

static void
free_stack(GList *stack)
{
	HexChangeData *cd;

	while(stack) {
		cd = (HexChangeData *)stack->data;
		if(cd->v_string)
			g_free(cd->v_string);
		stack = g_list_remove(stack, cd);
		g_free(cd);
	}
}

static gint
undo_stack_push(HexDocument *doc, HexChangeData *change_data)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	HexChangeData *cd;
	GList *stack_rest;

#ifdef ENABLE_DEBUG
	g_message("undo_stack_push");
#endif

	if(priv->undo_stack != priv->undo_top) {
		stack_rest = priv->undo_stack;
		priv->undo_stack = priv->undo_top;
		if(priv->undo_top) {
			priv->undo_top->prev->next = NULL;
			priv->undo_top->prev = NULL;
		}
		free_stack(stack_rest);
	}

	if((cd = g_new(HexChangeData, 1)) != NULL) {
		memcpy(cd, change_data, sizeof(HexChangeData));
		if(change_data->v_string) {
			cd->v_string = g_malloc(cd->rep_len);
			memcpy(cd->v_string, change_data->v_string, cd->rep_len);
		}

		priv->undo_depth++;

#ifdef ENABLE_DEBUG
		g_message("depth at: %d", priv->undo_depth);
#endif

		if(priv->undo_depth > priv->undo_max) {
			GList *last;

#ifdef ENABLE_DEBUG
			g_message("forget last undo");
#endif

			last = g_list_last(priv->undo_stack);
			priv->undo_stack = g_list_remove_link(priv->undo_stack, last);
			priv->undo_depth--;
			free_stack(last);
		}

		priv->undo_stack = g_list_prepend(priv->undo_stack, cd);
		priv->undo_top = priv->undo_stack;

		return TRUE;
	}

	return FALSE;
}

static void
undo_stack_descend(HexDocument *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
#ifdef ENABLE_DEBUG
	g_message("undo_stack_descend");
#endif

	if(priv->undo_top == NULL)
		return;

	priv->undo_top = priv->undo_top->next;
	priv->undo_depth--;

#ifdef ENABLE_DEBUG
	g_message("undo depth at: %d", priv->undo_depth);
#endif

	g_object_notify (G_OBJECT (doc), "can-redo");
}

static void
undo_stack_ascend(HexDocument *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
#ifdef ENABLE_DEBUG
	g_message("undo_stack_ascend");
#endif

	if(priv->undo_stack == NULL || priv->undo_top == priv->undo_stack)
		return;

	if(priv->undo_top == NULL)
		priv->undo_top = g_list_last(priv->undo_stack);
	else
		priv->undo_top = priv->undo_top->prev;
	priv->undo_depth++;

	g_object_notify (G_OBJECT (doc), "can-undo");
}

static void
undo_stack_free(HexDocument *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
#ifdef ENABLE_DEBUG
	g_message("undo_stack_free");
#endif

	if(priv->undo_stack == NULL)
		return;

	free_stack(priv->undo_stack);
	priv->undo_stack = NULL;
	priv->undo_top = NULL;
	priv->undo_depth = 0;

	g_signal_emit(G_OBJECT(doc), hex_signals[UNDO_STACK_FORGET], 0);
}

static gboolean
get_document_attributes(HexDocument *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	static struct stat stats;

	if(priv->file_name == NULL)
		return FALSE;

	if(!stat(priv->file_name, &stats) &&
	   S_ISREG(stats.st_mode)) {
		priv->file_size = stats.st_size;

		return TRUE;
	}

	return FALSE;
}


static void
move_gap_to(HexDocument *doc, guint offset, gint min_size)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	guchar *tmp, *buf_ptr, *tmp_ptr;

	if(priv->gap_size < min_size) {
		tmp = g_malloc(sizeof(guchar)*priv->file_size);
		buf_ptr = priv->buffer;
		tmp_ptr = tmp;
		while(buf_ptr < priv->gap_pos)
			*tmp_ptr++ = *buf_ptr++;
		buf_ptr += priv->gap_size;
		while(buf_ptr < priv->buffer + priv->buffer_size)
			*tmp_ptr++ = *buf_ptr++;

		priv->gap_size = MAX(min_size, 32);
		priv->buffer_size = priv->file_size + priv->gap_size;
		priv->buffer = g_realloc(priv->buffer, sizeof(guchar)*priv->buffer_size);
		priv->gap_pos = priv->buffer + offset;

		buf_ptr = priv->buffer;
		tmp_ptr = tmp;
		
		while(buf_ptr < priv->gap_pos)
			*buf_ptr++ = *tmp_ptr++;
		buf_ptr += priv->gap_size;
		while(buf_ptr < priv->buffer + priv->buffer_size)
			*buf_ptr++ = *tmp_ptr++;

		g_free(tmp);
	}
	else {
		if(priv->buffer + offset < priv->gap_pos) {
			buf_ptr = priv->gap_pos + priv->gap_size - 1;
			while(priv->gap_pos > priv->buffer + offset)
				*buf_ptr-- = *(--priv->gap_pos);
		}
		else if(priv->buffer + offset > priv->gap_pos) {
			buf_ptr = priv->gap_pos + priv->gap_size;
			while(priv->gap_pos < priv->buffer + offset)
				*priv->gap_pos++ = *buf_ptr++;
		}
	}
}

GtkWidget *
hex_document_add_view(HexDocument *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	GtkWidget *new_view;
	
	new_view = gtk_hex_new(doc);

#if GTK_CHECK_VERSION (2,18,0)
	gtk_widget_set_has_window (GTK_WIDGET (new_view), TRUE);
#else
	gtk_fixed_set_has_window (GTK_FIXED(new_view), TRUE);
#endif

	g_object_ref(new_view);

	priv->views = g_list_append(priv->views, new_view);

	return new_view;
}

void
hex_document_remove_view(HexDocument *doc, GtkWidget *view)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	if(g_list_index(priv->views, view) == -1)
		return;

	priv->views = g_list_remove(priv->views, view);

	g_object_unref(view);

	if (!priv->views) /* If we have destroyed the last view */
		g_object_unref (G_OBJECT (doc));
}

static void
hex_document_finalize(GObject *obj)
{
	HexDocument *hex;
	HexDocumentPrivate *priv;
	
	hex = HEX_DOCUMENT(obj);
	priv = hex_document_get_instance_private (hex);
	
	if(priv->buffer)
		g_free(priv->buffer);
	
	if(priv->file_name)
		g_free(priv->file_name);

	if(priv->path_end)
		g_free(priv->path_end);

	undo_stack_free(hex);

	while(priv->views)
		hex_document_remove_view(hex, (GtkWidget *)priv->views->data);

	doc_list = g_list_remove(doc_list, hex);

	G_OBJECT_CLASS (hex_document_parent_class)->finalize (obj);
}

static void
hex_document_real_changed(HexDocument *doc, gpointer change_data,
						  gboolean push_undo)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);

	if(push_undo && priv->undo_max > 0)
		undo_stack_push(doc, change_data);
}

static void
hex_document_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
	/*HexDocument *data = HEX_DOCUMENT (object);
	HexDocumentPrivate *priv = hex_document_get_instance_private (data);*/

	switch (property_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_document_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
	HexDocument *data = HEX_DOCUMENT (object);

	switch (property_id) {
		case PROP_CAN_UNDO:
			g_value_set_boolean (value, hex_document_get_can_undo (data));
			break;
		case PROP_CAN_REDO:
			g_value_set_boolean (value, hex_document_get_can_redo (data));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
hex_document_class_init (HexDocumentClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->set_property = hex_document_set_property;
	object_class->get_property = hex_document_get_property;
	object_class->finalize = hex_document_finalize;

	pspecs[PROP_CAN_UNDO] =
		g_param_spec_boolean ("can-undo", "Can undo", NULL, FALSE,
							  G_PARAM_READABLE);

	pspecs[PROP_CAN_REDO] =
		g_param_spec_boolean ("can-redo", "Can redo", NULL, FALSE,
							  G_PARAM_READABLE);

	g_object_class_install_properties (object_class, LAST_PROP, pspecs);
	
	klass->document_changed = hex_document_real_changed;
	klass->undo = hex_document_real_undo;
	klass->redo = hex_document_real_redo;
	klass->undo_stack_forget = NULL;

	hex_signals[DOCUMENT_CHANGED] = 
		g_signal_new ("document_changed",
					  G_OBJECT_CLASS_TYPE (object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (HexDocumentClass, document_changed),
					  NULL,
					  NULL,
					  NULL,
					  G_TYPE_NONE,
					  2, G_TYPE_POINTER, G_TYPE_BOOLEAN);
	hex_signals[UNDO] = 
		g_signal_new ("undo",
					  G_OBJECT_CLASS_TYPE (object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (HexDocumentClass, undo),
					  NULL,
					  NULL,
					  NULL,
					  G_TYPE_NONE, 0);
	hex_signals[REDO] = 
		g_signal_new ("redo",
					  G_OBJECT_CLASS_TYPE (object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (HexDocumentClass, redo),
					  NULL,
					  NULL,
					  NULL,
					  G_TYPE_NONE, 0);
	hex_signals[UNDO_STACK_FORGET] = 
		g_signal_new ("undo_stack_forget",
					  G_OBJECT_CLASS_TYPE (object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (HexDocumentClass, undo_stack_forget),
					  NULL,
					  NULL,
					  NULL,
					  G_TYPE_NONE, 0);
}

static void
hex_document_init (HexDocument *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);

	priv->file_name = NULL;
	priv->file_size = 0;
	priv->gap_size = 100;
	priv->buffer_size = priv->file_size + priv->gap_size;
	priv->gap_pos = priv->buffer = (guchar *)g_malloc(priv->buffer_size);
	priv->changed = FALSE;
	priv->undo_stack = NULL;
	priv->undo_top = NULL;
	priv->undo_depth = 0;
	priv->undo_max = DEFAULT_UNDO_DEPTH;
	priv->path_end = g_strdup(_("New document"));

	doc_list = g_list_append(doc_list, doc);
}

/*-------- public API starts here --------*/

HexDocument *
hex_document_new()
{
	return g_object_new (HEX_TYPE_DOCUMENT, NULL);
}

HexDocument *
hex_document_new_from_file(const gchar *name)
{
	HexDocument *doc;
	HexDocumentPrivate *priv;
	gchar *path_end;

	doc = HEX_DOCUMENT (g_object_new (hex_document_get_type(), NULL));
	priv = hex_document_get_instance_private (doc);
	g_return_val_if_fail (doc != NULL, NULL);

	priv->file_name = (gchar *)g_strdup(name);
	if(get_document_attributes(doc)) {
		priv->gap_size = 100;
		priv->buffer_size = priv->file_size + priv->gap_size;
		priv->buffer = (guchar *)g_malloc(priv->buffer_size);

		/* find the start of the filename without path */
		path_end = g_path_get_basename (priv->file_name);
		priv->path_end = g_filename_to_utf8 (path_end, -1, NULL, NULL, NULL);
		g_free (path_end);

		if(hex_document_read(doc)) {
			doc_list = g_list_append(doc_list, doc);
			return doc;
		}
	}
	g_object_unref(G_OBJECT(doc));
	
	return NULL;
}

guchar
hex_document_get_byte(HexDocument *doc, guint offset)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	if(offset < priv->file_size) {
		if(priv->gap_pos <= priv->buffer + offset)
			offset += priv->gap_size;
		return priv->buffer[offset];
	}
	else
		return 0;
}

guchar *
hex_document_get_data(HexDocument *doc, guint offset, guint len)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	guchar *ptr, *data, *dptr;
	guint i;

	ptr = priv->buffer + offset;
	if(ptr >= priv->gap_pos)
		ptr += priv->gap_size;
	dptr = data = g_malloc(sizeof(guchar)*len);
	i = 0;
	while(i < len) {
		if(ptr >= priv->gap_pos && ptr < priv->gap_pos + priv->gap_size)
			ptr += priv->gap_size;
		*dptr++ = *ptr++;
		i++;
	}

	return data;
}

void
hex_document_set_nibble(HexDocument *doc, guchar val, guint offset,
						gboolean lower_nibble, gboolean insert,
						gboolean undoable)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	static HexChangeData change_data;

	if(offset <= priv->file_size) {
		if(!insert && offset == priv->file_size)
			return;

		priv->changed = TRUE;
		change_data.start = offset;
		change_data.end = offset;
		change_data.v_string = NULL;
		change_data.type = HEX_CHANGE_BYTE;
		change_data.lower_nibble = lower_nibble;
		change_data.insert = insert;
		if(!lower_nibble && insert) {
			move_gap_to(doc, offset, 1);
			priv->gap_size--;
			priv->gap_pos++;
			priv->file_size++;
			change_data.rep_len = 0;
			if(offset == priv->file_size)
				priv->buffer[offset] = 0;
		}
		else {
			if(priv->buffer + offset >= priv->gap_pos)
				offset += priv->gap_size;
			change_data.rep_len = 1;
		}

		change_data.v_byte = priv->buffer[offset];
		priv->buffer[offset] = (priv->buffer[offset] & (lower_nibble?0xF0:0x0F)) | (lower_nibble?val:(val << 4));

	 	hex_document_changed(doc, &change_data, undoable);
	}
}

void
hex_document_set_byte(HexDocument *doc, guchar val, guint offset,
					  gboolean insert, gboolean undoable)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	static HexChangeData change_data;

	if(offset <= priv->file_size) {
		if(!insert && offset == priv->file_size)
			return;

		priv->changed = TRUE;
		change_data.start = offset;
		change_data.end = offset;
		change_data.rep_len = (insert?0:1);
		change_data.v_string = NULL;
		change_data.type = HEX_CHANGE_BYTE;
		change_data.lower_nibble = FALSE;
		change_data.insert = insert;
		if(insert) {
			move_gap_to(doc, offset, 1);
			priv->gap_size--;
			priv->gap_pos++;
			priv->file_size++;
		}
		else if(priv->buffer + offset >= priv->gap_pos)
			offset += priv->gap_size;
			
		change_data.v_byte = priv->buffer[offset];
		priv->buffer[offset] = val;

	 	hex_document_changed(doc, &change_data, undoable);
	}
}

void
hex_document_set_data(HexDocument *doc, guint offset, guint len,
					  guint rep_len, guchar *data, gboolean undoable)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	guint i;
	guchar *ptr;
	static HexChangeData change_data;

	if(offset <= priv->file_size) {
		if(priv->file_size - offset < rep_len)
			rep_len -= priv->file_size - offset;

		priv->changed = TRUE;
		
		change_data.v_string = g_realloc(change_data.v_string, rep_len);
		change_data.start = offset;
		change_data.end = change_data.start + len - 1;
		change_data.rep_len = rep_len;
		change_data.type = HEX_CHANGE_STRING;
		change_data.lower_nibble = FALSE;
		
		i = 0;
		ptr = &priv->buffer[offset];
		if(ptr >= priv->gap_pos)
			ptr += priv->gap_size;
		while(offset + i < priv->file_size && i < rep_len) {
			if(ptr >= priv->gap_pos && ptr < priv->gap_pos + priv->gap_size)
				ptr += priv->gap_size;
			change_data.v_string[i] = *ptr++;
			i++;
		}
		
		if(rep_len == len) {
			if(priv->buffer + offset >= priv->gap_pos)
				offset += priv->gap_size;
		}
		else {
			if(rep_len > len) {
				move_gap_to(doc, offset + rep_len, 1);
			}
			else if(rep_len < len) {
				move_gap_to(doc, offset + rep_len, len - rep_len);
			}
			priv->gap_pos -= (gint)rep_len - (gint)len;
			priv->gap_size += (gint)rep_len - (gint)len;
			priv->file_size += (gint)len - (gint)rep_len;
		}
		
		ptr = &priv->buffer[offset];
		i = 0;
		while(offset + i < priv->buffer_size && i < len) {
			*ptr++ = *data++;
			i++;
		}
		
		hex_document_changed(doc, &change_data, undoable);
	}
}

void
hex_document_delete_data(HexDocument *doc, guint offset, guint len, gboolean undoable)
{
	hex_document_set_data(doc, offset, 0, len, NULL, undoable);
}

gint
hex_document_read(HexDocument *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	FILE *file;
	static HexChangeData change_data;

	if(priv->file_name == NULL)
		return FALSE;

	if(!get_document_attributes(doc))
		return FALSE;

	if((file = fopen(priv->file_name, "r")) == NULL)
		return FALSE;

	priv->gap_size = priv->buffer_size - priv->file_size;
	if(fread(priv->buffer + priv->gap_size, 1, priv->file_size, file) != priv->file_size)
	{
		g_return_val_if_reached(FALSE);
	}
	priv->gap_pos = priv->buffer;
	fclose(file);
	undo_stack_free(doc);

	change_data.start = 0;
	change_data.end = priv->file_size - 1;
	priv->changed = FALSE;
	hex_document_changed(doc, &change_data, FALSE);

	return TRUE;
}

gint
hex_document_write_to_file(HexDocument *doc, FILE *file)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	gint ret = TRUE;
	size_t exp_len;

	if(priv->gap_pos > priv->buffer) {
		exp_len = MIN(priv->file_size, priv->gap_pos - priv->buffer);
		ret = fwrite(priv->buffer, 1, exp_len, file);
		ret = (ret == exp_len)?TRUE:FALSE;
	}
	if(priv->gap_pos < priv->buffer + priv->file_size) {
		exp_len = priv->file_size - (size_t)(priv->gap_pos - priv->buffer);
		ret = fwrite(priv->gap_pos + priv->gap_size, 1, exp_len, file);
		ret = (ret == exp_len)?TRUE:FALSE;
	}

	return ret;
}

gint
hex_document_write(HexDocument *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	FILE *file;
	gint ret = FALSE;

	if(priv->file_name == NULL)
		return FALSE;

	if((file = fopen(priv->file_name, "wb")) != NULL) {
		ret = hex_document_write_to_file(doc, file);
		fclose(file);
		if(ret) {
			priv->changed = FALSE;
		}
	}

	return ret;
}

void
hex_document_changed(HexDocument *doc, gpointer change_data,
					 gboolean push_undo)
{
	g_signal_emit(G_OBJECT(doc), hex_signals[DOCUMENT_CHANGED], 0,
				  change_data, push_undo);
}

gboolean
hex_document_has_changed(HexDocument *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	return priv->changed;
}

void
hex_document_set_max_undo(HexDocument *doc, guint max_undo)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	if(priv->undo_max != max_undo) {
		if(priv->undo_max > max_undo)
			undo_stack_free(doc);
		priv->undo_max = max_undo;
	}
}

static gboolean
ignore_cb(GtkWidget *w, GdkEventAny *e, gpointer user_data)
{
	return TRUE;
}

gint
hex_document_export_html(HexDocument *doc, gchar *html_path, gchar *base_name,
						 guint start, guint end, guint cpl, guint lpp,
						 guint cpw)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	GtkWidget *progress_dialog, *progress_bar;
	FILE *file;
	int page, line, pos, lines, pages, c;
	gchar *page_name, b;
	gint update_pages;
	gchar *progress_str;

	lines = (end - start)/cpl;
	if((end - start)%cpl != 0)
		lines++;
	pages = lines/lpp;
	if(lines%lpp != 0)
		pages++;
	update_pages = pages/1000 + 1;

	/* top page */
	page_name = g_strdup_printf("%s/%s.html", html_path, base_name);
	file = fopen(page_name, "w");
	g_free(page_name);
	if(!file)
		return FALSE;
	fprintf(file, "<HTML>\n<HEAD>\n");
	fprintf(file, "<META HTTP-EQUIV=\"Content-Type\" "
			"CONTENT=\"text/html; charset=UTF-8\">\n");
	fprintf(file, "<META NAME=\"hexdata\" CONTENT=\"GHex export to HTML\">\n");
	fprintf(file, "</HEAD>\n<BODY>\n");

	fprintf(file, "<CENTER>");
	fprintf(file, "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\">\n");
	fprintf(file, "<TR>\n<TD COLSPAN=\"3\"><B>%s</B></TD>\n</TR>\n",
			priv->file_name?priv->file_name:priv->path_end);
	fprintf(file, "<TR>\n<TD COLSPAN=\"3\">&nbsp;</TD>\n</TR>\n");
	for(page = 0; page < pages; page++) {
		fprintf(file, "<TR>\n<TD>\n<A HREF=\"%s%08d.html\"><PRE>", base_name, page);
		fprintf(file, _("Page"));
		fprintf(file, " %d</PRE></A>\n</TD>\n<TD>&nbsp;</TD>\n<TD VALIGN=\"CENTER\"><PRE>%08x -", page+1, page*cpl*lpp);
		fprintf(file, " %08x</PRE></TD>\n</TR>\n", MIN((page+1)*cpl*lpp-1, priv->file_size-1));
	}
	fprintf(file, "</TABLE>\n</CENTER>\n");
	fprintf(file, "<HR WIDTH=\"100%%\">");
	fprintf(file, _("Hex dump generated by"));
	fprintf(file, " <B>"LIBGTKHEX_RELEASE_STRING"</B>\n");
	fprintf(file, "</BODY>\n</HTML>\n");
	fclose(file);

	progress_dialog = gtk_dialog_new();
	gtk_window_set_resizable(GTK_WINDOW(progress_dialog), FALSE);
	gtk_window_set_modal(GTK_WINDOW(progress_dialog), TRUE);
	g_signal_connect(G_OBJECT(progress_dialog), "delete-event",
					 G_CALLBACK(ignore_cb), NULL);
	gtk_window_set_title(GTK_WINDOW(progress_dialog),
						 _("Saving to HTML..."));
	progress_bar = gtk_progress_bar_new();
	gtk_widget_show(progress_bar);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(progress_dialog))),
					  progress_bar);
	gtk_widget_show(progress_dialog);

	pos = start;
	g_object_ref(G_OBJECT(doc));
	for(page = 0; page < pages; page++) {
		if((page%update_pages) == 0) {
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar),
										  (gdouble)page/(gdouble)pages);
			progress_str = g_strdup_printf("%d/%d", page, pages);
			gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar),
									  progress_str);
			g_free(progress_str);
			while(gtk_events_pending())
				gtk_main_iteration();
		}
		/* write page header */
		page_name = g_strdup_printf("%s/%s%08d.html",
									html_path, base_name, page);
		file = fopen(page_name, "w");
		g_free(page_name);
		if(!file)
			break;
		/* write header */
		fprintf(file, "<HTML>\n<HEAD>\n");
		fprintf(file, "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=iso-8859-1\">\n");
		fprintf(file, "<META NAME=\"hexdata\" CONTENT=\"GHex export to HTML\">\n");
		fprintf(file, "</HEAD>\n<BODY>\n");
		/* write top table |previous|filename: page/pages|next| */
		fprintf(file, "<TABLE BORDER=\"0\" CELLSPACING=\"0\" WIDTH=\"100%%\">\n");
		fprintf(file, "<TR>\n<TD WIDTH=\"33%%\">\n");
		if(page > 0) {
			fprintf(file, "<A HREF=\"%s%08d.html\">", base_name, page-1);
			fprintf(file, _("Previous page"));
			fprintf(file, "</A>");
		}
		else
			fprintf(file, "&nbsp;");
		fprintf(file, "\n</TD>\n");
		fprintf(file, "<TD WIDTH=\"33%%\" ALIGN=\"CENTER\">\n");
		fprintf(file, "<A HREF=\"%s.html\">", base_name);
		fprintf(file, "%s:", priv->path_end);
		fprintf(file, "</A>");
		fprintf(file, " %d/%d", page+1, pages);
		fprintf(file, "\n</TD>\n");
		fprintf(file, "<TD WIDTH=\"33%%\" ALIGN=\"RIGHT\">\n");
		if(page < pages - 1) {
			fprintf(file, "<A HREF=\"%s%08d.html\">", base_name, page+1);
			fprintf(file, _("Next page"));
			fprintf(file, "</A>");
		}
		else
			fprintf(file, "&nbsp;");
		fprintf(file, "\n</TD>\n");
		fprintf(file, "</TR>\n</TABLE>\n");
		
		/* now the actual data */
		fprintf(file, "<CENTER>\n");
		fprintf(file, "<TABLE BORDER=\"1\" CELLSPACING=\"2\" CELLPADDING=\"2\">\n");
		fprintf(file, "<TR>\n<TD>\n");
		fprintf(file, "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\">\n");
		for(line = 0; line < lpp && pos + line*cpl < priv->file_size; line++) {
		/* offset of line*/
			fprintf(file, "<TR>\n<TD>\n");
			fprintf(file, "<PRE>%08x</PRE>\n", pos + line*cpl);
			fprintf(file, "</TD>\n</TR>\n");
		}
		fprintf(file, "</TABLE>\n");
		fprintf(file, "</TD>\n<TD>\n");
		fprintf(file, "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\">\n");
		c = 0;
		for(line = 0; line < lpp; line++) {
			/* hex data */
			fprintf(file, "<TR>\n<TD>\n<PRE>");
			while(pos + c < end) {
				fprintf(file, "%02x", hex_document_get_byte(doc, pos + c));
				c++;
				if(c%cpl == 0)
					break;
				if(c%cpw == 0)
					fprintf(file, " ");
			}
			fprintf(file, "</PRE>\n</TD>\n</TR>\n");
		}
		fprintf(file, "</TABLE>\n");
		fprintf(file, "</TD>\n<TD>\n");
		fprintf(file, "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\">\n");
		c = 0;
		for(line = 0; line < lpp; line++) {
			/* ascii data */
			fprintf(file, "<TR>\n<TD>\n<PRE>");
			while(pos + c < end) {
				b = hex_document_get_byte(doc, pos + c);
				if(b >= 0x20)
					fprintf(file, "%c", b);
				else
					fprintf(file, ".");
				c++;
				if(c%cpl == 0)
					break;
			}
			fprintf(file, "</PRE></TD>\n</TR>\n");
			if(pos >= end)
				line = lpp;
		}
		pos += c;
		fprintf(file, "</TD>\n</TR>\n");
		fprintf(file, "</TABLE>\n");
		fprintf(file, "</TABLE>\n</CENTER>\n");
		fprintf(file, "<HR WIDTH=\"100%%\">");
		fprintf(file, _("Hex dump generated by"));
		fprintf(file, " <B>" LIBGTKHEX_RELEASE_STRING "</B>\n");
		fprintf(file, "</BODY>\n</HTML>\n");
		fclose(file);
	}
	g_object_unref(G_OBJECT(doc));
	gtk_widget_destroy(progress_dialog);

	return TRUE;
}

gint
hex_document_compare_data(HexDocument *doc, guchar *s2, gint pos, gint len)
{
	guchar c1;
	guint i;

	for(i = 0; i < len; i++, s2++) {
		c1 = hex_document_get_byte(doc, pos + i);
		if(c1 != (*s2))
			return (c1 - (*s2));
	}
	
	return 0;
}

gint
hex_document_find_forward(HexDocument *doc, guint start, guchar *what,
						  gint len, guint *found)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	guint pos;
	
	pos = start;
	while(pos < priv->file_size) {
		if(hex_document_compare_data(doc, what, pos, len) == 0) {
			*found = pos;
			return TRUE;
		}
		pos++;
	}

	return FALSE;
}

gint
hex_document_find_backward(HexDocument *doc, guint start, guchar *what,
						   gint len, guint *found)
{
	guint pos;
	
	pos = start;

	if(pos == 0)
		return FALSE;

	do {
		pos--;
		if(hex_document_compare_data(doc, what, pos, len) == 0) {
			*found = pos;
			return TRUE;
		}
	} while(pos > 0);

	return FALSE;
}

gboolean
hex_document_undo(HexDocument *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);

	if(priv->undo_top == NULL)
		return FALSE;

	g_signal_emit(G_OBJECT(doc), hex_signals[UNDO], 0);

	return TRUE;
}

static void
hex_document_real_undo(HexDocument *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	HexChangeData *cd;
	gint len;
	guchar *rep_data;
	gchar c_val;

	cd = (HexChangeData *) priv->undo_top->data;

	switch(cd->type) {
	case HEX_CHANGE_BYTE:
		if(cd->start >= 0 && cd->end < priv->file_size) {
			c_val = hex_document_get_byte(doc, cd->start);
			if(cd->rep_len > 0)
				hex_document_set_byte(doc, cd->v_byte, cd->start, FALSE, FALSE);
			else if(cd->rep_len == 0)
				hex_document_delete_data(doc, cd->start, 1, FALSE);
			else
				hex_document_set_byte(doc, cd->v_byte, cd->start, TRUE, FALSE);
			cd->v_byte = c_val;
		}
		break;
	case HEX_CHANGE_STRING:
		len = cd->end - cd->start + 1;
		rep_data = hex_document_get_data(doc, cd->start, len);
		hex_document_set_data(doc, cd->start, cd->rep_len, len, (guchar *) cd->v_string, FALSE);
		g_free(cd->v_string);
		cd->end = cd->start + cd->rep_len - 1;
		cd->rep_len = len;
		cd->v_string = (gchar *) rep_data;
		break;
	}

	hex_document_changed(doc, cd, FALSE);

	undo_stack_descend(doc);
}

gboolean
hex_document_is_writable(HexDocument *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	return (priv->file_name != NULL &&
			access(priv->file_name, W_OK) == 0);
}

gboolean 
hex_document_redo(HexDocument *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	if(priv->undo_stack == NULL || priv->undo_top == priv->undo_stack)
		return FALSE;

	g_signal_emit(G_OBJECT(doc), hex_signals[REDO], 0);

	return TRUE;
}

static void
hex_document_real_redo (HexDocument   *doc)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (doc);
	HexChangeData *cd;
	gint len;
	guchar *rep_data;
	gchar c_val;

	undo_stack_ascend(doc);

	cd = (HexChangeData *) priv->undo_top->data;

	switch(cd->type) {
	case HEX_CHANGE_BYTE:
		if(cd->start >= 0 && cd->end <= priv->file_size) {
			c_val = hex_document_get_byte(doc, cd->start);
			if(cd->rep_len > 0)
				hex_document_set_byte(doc, cd->v_byte, cd->start, FALSE, FALSE);
			else if(cd->rep_len == 0)
				hex_document_set_byte(doc, cd->v_byte, cd->start, cd->insert, FALSE);
#if 0
				hex_document_delete_data(doc, cd->start, 1, FALSE);
#endif
			else
				hex_document_set_byte(doc, cd->v_byte, cd->start, TRUE, FALSE);
			cd->v_byte = c_val;
		}
		break;
	case HEX_CHANGE_STRING:
		len = cd->end - cd->start + 1;
		rep_data = hex_document_get_data(doc, cd->start, len);
		hex_document_set_data(doc, cd->start, cd->rep_len, len, (guchar *) cd->v_string, FALSE);
		g_free(cd->v_string);
		cd->end = cd->start + cd->rep_len - 1;
		cd->rep_len = len;
		cd->v_string = (gchar *) rep_data;
		break;
	}

	hex_document_changed(doc, cd, FALSE);
}

guint
hex_document_get_file_size (HexDocument *self)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (self);
	return priv->file_size;
}

const gchar *
hex_document_get_file_name (HexDocument *self)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (self);
	return priv->file_name;
}

void
hex_document_set_file_name (HexDocument *self,
                            const gchar *file)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (self);

	g_free (priv->file_name);
	priv->file_name = g_strdup (file);
}

const gchar *
hex_document_get_path_end (HexDocument *self)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (self);
	return priv->path_end;
}

void
hex_document_set_path_end (HexDocument *self,
                           const gchar *path_end)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (self);

	g_free (priv->path_end);
	priv->path_end = g_strdup (path_end);
}

void
hex_document_set_changed (HexDocument *self,
                          gboolean     changed)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (self);

	priv->changed = changed;
}


gboolean
hex_document_get_can_undo (HexDocument *self)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (self);
	return priv->undo_top != NULL;
}

gboolean
hex_document_get_can_redo (HexDocument *self)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (self);
	return priv->undo_stack && priv->undo_top != priv->undo_stack;
}

GList *
hex_document_get_views (HexDocument *self)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (self);
	return priv->views;
}

HexChangeData *
hex_document_get_last_change (HexDocument *self)
{
	HexDocumentPrivate *priv = hex_document_get_instance_private (self);
	return priv->undo_top->data;
}

const GList *
hex_document_get_list()
{
	return doc_list;
}

