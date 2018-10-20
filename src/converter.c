/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* converter.c - conversion dialog

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

   Authors:
   Jaka Mocnik <jaka@gnu.org>
   Chema Celorio <chema@gnome.org>
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <ctype.h>      /* for isdigit */
#include <stdlib.h>     /* for strtoul */
#include <string.h>     /* for strncpy */

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "gtkhex.h"
#include "converter.h"
#include "ghex-window.h"

#define BUFFER_LEN 40
#define CONV_BUFFER_LEN 32

static gboolean
is_char_ok(signed char c, gint base)
{
	/* ASCII which is base 0 is always ok */
	if(base == 0)
		return TRUE;

	/* Normalize A-F to 10-15 */
	if(isalpha(c))
		c = toupper(c) - 'A' + '9' + 1;
	else if(!isdigit(c))
		return FALSE;

	c = c - '0';

	return((c < 0) ||(c >(base - 1))) ? FALSE : TRUE;
}

static void
entry_filter(GtkEditable *editable, const gchar *text, gint length,
			 gint *pos, gpointer data)
{
	int i, l = 0;
	char *s = NULL;
	gint base = GPOINTER_TO_INT(data);

	/* thou shalt optimize for the common case */
	if(length == 1) {
		if(!is_char_ok(*text, base)) {
			gdk_display_beep (gdk_display_get_default ());
			g_signal_stop_emission_by_name(G_OBJECT(editable), "insert_text");
		}
		return;
	}

	/* if it is a paste we have to check all of things */
	s = g_new0(gchar, length);

	for(i=0; i<length; i++) {
		if(is_char_ok(text[i], base)) {
			s[l++] = text[i];
		}
	}

	if(l == length)
		return;

	gdk_display_beep (gdk_display_get_default ());
	g_signal_stop_emission_by_name(G_OBJECT(editable), "insert_text");
#if 0 /* Pasting is not working. gtk is doing weird things. Chema */
	g_signal_handler_block_by_func (editable, entry_filter, data);
	g_print("Inserting -->%s<--- of length %i in pos %i\n", s, l, *pos);
	gtk_editable_insert_text(editable, s, l, pos);
	g_signal_handler_unblock_by_func (editable, entry_filter, data);
#endif
}

static gint
entry_key_press_event_cb(GtkWidget *widget, GdkEventKey *event,
						 gpointer not_used)
{
	/* For gtk_entries don't let the gtk handle the event if
	 * the alt key was pressed. Otherwise the accelerators do not work cause
	 * gtk takes care of this event
	 */
	if(event->state & GDK_MOD1_MASK)
		g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");

	return FALSE;
}


static gchar *
clean (gchar *ptr)
{
	while (*ptr == '0')
		ptr++;
	return ptr;
}

struct _GHexConverter
{
	GtkDialog  parent;

	GtkWidget *grid;

	GtkWidget *entry[5];
	GtkWidget *close;
	GtkWidget *get;

	gulong     value;
};

G_DEFINE_TYPE (GHexConverter, g_hex_converter, G_HEX_TYPE_DIALOG)

enum {
	PROP_0,
	PROP_CAN_GRAB,
	LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP] = { NULL, };


void
g_hex_converter_set_can_grap (GHexConverter *self,
                              gboolean       can_grab)
{
	gtk_widget_set_sensitive (self->get, can_grab);
}

gboolean
g_hex_converter_get_can_grap (GHexConverter *self)
{
	return gtk_widget_get_sensitive (self->get);
}

static void
g_hex_converter_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
	GHexConverter *data = G_HEX_CONVERTER (object);

	switch (property_id) {
		case PROP_CAN_GRAB:
			g_hex_converter_set_can_grap (data, g_value_get_boolean (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
g_hex_converter_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
	GHexConverter *data = G_HEX_CONVERTER (object);

	switch (property_id) {
		case PROP_CAN_GRAB:
			g_value_set_boolean (value, g_hex_converter_get_can_grap (data));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}


static void
g_hex_converter_update_values (GHexConverter *self, gulong val)
{
	gchar buffer[CONV_BUFFER_LEN + 1];
	gint i, nhex, nbytes;
	gulong tmp = val;

	nhex = 0;
	while(tmp > 0) {
		tmp = tmp >> 4;
		nhex++;
	}
	if(nhex == 0)
		nhex = 1;
	nbytes = nhex/2; 

	self->value = val;
	
	for(i = 0; i < 32; i++)
		buffer[i] =((val & (1L << (31 - i)))?'1':'0');
	buffer[i] = 0;
	gtk_entry_set_text(GTK_ENTRY(self->entry[0]), clean(buffer));

	g_snprintf(buffer, CONV_BUFFER_LEN, "%o",(unsigned int)val);
	gtk_entry_set_text(GTK_ENTRY(self->entry[1]), buffer);
	
	g_snprintf(buffer, CONV_BUFFER_LEN, "%lu", val);
	gtk_entry_set_text(GTK_ENTRY(self->entry[2]), buffer);

	for(i = 0, tmp = val; i < nhex; i++) {
		buffer[nhex - i - 1] = (tmp & 0x0000000FL);
		if(buffer[nhex - i - 1] < 10)
			buffer[nhex - i - 1] += '0';
		else
			buffer[nhex - i - 1] += 'A' - 10;
		tmp = tmp >> 4;
	}
	buffer[i] = '\0';
	gtk_entry_set_text(GTK_ENTRY(self->entry[3]), buffer);
	
	for(i = 0, tmp = val; i < nbytes; i++) {
		buffer[nbytes - i - 1] = tmp & 0x000000FF;
		if(buffer[nbytes - i - 1] < 0x20 || buffer[nbytes - i - 1] >= 0x7F)
			buffer[nbytes - i - 1] = '_';
		tmp = tmp >> 8;
	}
	buffer[i] = 0;
	gtk_entry_set_text(GTK_ENTRY(self->entry[4]), buffer);
}

static void
g_hex_converter_response (GtkDialog *self,
						  gint       res)
{
	if (res == GTK_RESPONSE_APPLY) {
		GtkWidget *view;
		guint val, start;
		GHexWindow *win = g_hex_dialog_get_window (G_HEX_DIALOG (self));

		if (win == NULL || win->gh == NULL)
			return;

		view = GTK_WIDGET (win->gh);

		if (view) {
			start = gtk_hex_get_cursor (GTK_HEX (view));
			start = start - start % GTK_HEX (view)->group_type;
			val = 0;
			do {
				val <<= 8;
				val |= gtk_hex_get_byte (GTK_HEX (view), start);
				start++;
			} while ((start % GTK_HEX (view)->group_type != 0) &&
					 (start < GTK_HEX (view)->document->file_size) );

			g_hex_converter_update_values (G_HEX_CONVERTER (self), val);
		}
	} else {
		gtk_widget_destroy (GTK_WIDGET (self));
	}
}

static void
g_hex_converter_class_init (GHexConverterClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	object_class->set_property = g_hex_converter_set_property;
	object_class->get_property = g_hex_converter_get_property;

	pspecs[PROP_CAN_GRAB] =
		g_param_spec_boolean ("can-grab", "If a document is available", NULL, FALSE,
							 G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, LAST_PROP, pspecs);

	dialog_class->response = g_hex_converter_response;
}

static void
g_hex_converter_update_by (GHexConverter *self, GtkEntry *entry)
{
	gchar buffer[33];
	const gchar *text;
	gchar *endptr;
	gulong val = self->value;
	int i, len;
	gint base;
	
	text = gtk_entry_get_text(entry);
	base = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (entry), "gh-base"));

	switch(base) {
	case 0:
		strncpy(buffer, text, 4);
		buffer[4] = 0;
		for(val = 0, i = 0, len = strlen(buffer); i < len; i++) {
			val <<= 8;
			val |= text[i];
		}
		break;
	case 2:
		strncpy(buffer, text, 32);
		buffer[32] = 0;
		break;
	case 8:
		strncpy(buffer, text, 12);
		buffer[12] = 0;
		break;
	case 10:
		strncpy(buffer, text, 10);
		buffer[10] = 0;
		break;
	case 16:
		strncpy(buffer, text, 8);
		buffer[8] = 0;
		break;
	}

	if(base != 0) {
		val = strtoul(buffer, &endptr, base);
		if(*endptr != 0) {
			self->value = 0;
			for(i = 0; i < 5; i++)
				gtk_entry_set_text(GTK_ENTRY(self->entry[i]), _("ERROR"));
			gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
			return;
		}
	}

	if(val == self->value)
		return;

	g_hex_converter_update_values (self, val);
}

static void
g_hex_converter_add_entry (GHexConverter *self,
                           const gchar *name,
                           gint pos,
                           gint base)
{
	GtkWidget *label;
    GtkWidget *entry;
    gchar str[BUFFER_LEN + 1];

	/* label */
	label = gtk_label_new_with_mnemonic(name);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_label_set_yalign (GTK_LABEL (label), 0.5);
	gtk_widget_set_halign (GTK_WIDGET (label), GTK_ALIGN_END);
	gtk_grid_attach (GTK_GRID (self->grid), label, 0, pos, 1, 1);
	gtk_widget_show(label);

	/* entry */
	entry = gtk_entry_new();
	g_object_set_data (G_OBJECT (entry), "gh-base", GINT_TO_POINTER(base));
	g_signal_connect_swapped (G_OBJECT(entry), "activate",
					 G_CALLBACK(g_hex_converter_update_by), self);
	g_signal_connect(G_OBJECT(entry), "key_press_event",
					 G_CALLBACK(entry_key_press_event_cb), NULL);
	g_signal_connect(G_OBJECT(entry), "insert_text",
					 G_CALLBACK(entry_filter), GINT_TO_POINTER(base));
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_widget_set_hexpand (entry, TRUE);
	gtk_grid_attach (GTK_GRID (self->grid), entry, 1, pos, 1, 1);
	gtk_widget_show(entry);

	if (GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (entry))) {
		g_snprintf (str, BUFFER_LEN, "Displays the value at cursor in %s", name+1);
		add_atk_namedesc (entry, name+1, str);
		add_atk_relation (entry, label, ATK_RELATION_LABELLED_BY);
	}

	self->entry[pos] = entry;
}

static void
g_hex_converter_init (GHexConverter *self)
{
	self->grid = g_object_new (GTK_TYPE_GRID,
							   "row-spacing", 8,
							   "column-spacing", 8,
							   "margin", 16,
							   "visible", TRUE,
							   NULL);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))), self->grid,
					   TRUE, TRUE, 0);

	/* entries */
	g_hex_converter_add_entry (self, _("_Binary"), 0, 2);
	g_hex_converter_add_entry (self, _("_Octal"), 1, 8);
	g_hex_converter_add_entry (self, _("_Decimal"), 2, 10);
	g_hex_converter_add_entry (self, _("_Hex"), 3, 16);
	g_hex_converter_add_entry (self, _("_ASCII"), 4, 0);

	self->get = gtk_dialog_add_button (GTK_DIALOG (self), _("Get value"), GTK_RESPONSE_APPLY);

	if (GTK_IS_ACCESSIBLE(gtk_widget_get_accessible(self->get))) {
		add_atk_namedesc (self->get, _("Get cursor value"), _("Gets the value at cursor in binary, octal, decimal, hex and ASCII"));
		for (gint i=0; i<5; i++) {
			add_atk_relation (self->entry[i], self->get, ATK_RELATION_CONTROLLED_BY);
			add_atk_relation (self->get, self->entry[i], ATK_RELATION_CONTROLLER_FOR);
		}
	}
}

GtkWidget *
g_hex_converter_new (GHexWindow *parent)
{
	return g_object_new (G_HEX_TYPE_CONVERTER,
						 "use-header-bar", TRUE,
						 "modal", FALSE,
						 "transient-for", parent,
						 "resizable", FALSE,
						 "title", _("Base Converter"),
						 NULL);
}

