/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gtkhex.c - a GtkHex widget, modified for use in GHex

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

#include <gdk/gdkkeysyms.h>

#include "hex-document.h"
#include "gtkhex.h"
#include "gtkhex-private.h"

#define DISPLAY_BORDER 4

#define DEFAULT_CPL 32
#define DEFAULT_LINES 10

#define SCROLL_TIMEOUT 100

#define DEFAULT_FONT "Monospace 12"
#define FONT_CSS "hex-editor { font-family: %s; font-size: %ipt; }"

#define is_displayable(c) (((c) >= 0x20) && ((c) < 0x7f))

enum {
	CURSOR_MOVED_SIGNAL,
	DATA_CHANGED_SIGNAL,
	CUT_CLIPBOARD_SIGNAL,
	COPY_CLIPBOARD_SIGNAL,
	PASTE_CLIPBOARD_SIGNAL,
	LAST_SIGNAL
};

enum {
  TARGET_STRING,
};

struct _GtkHex_AutoHighlight
{
	gint search_view;
	gchar *search_string;
	gint search_len;

	gchar *colour;

	gint view_min;
	gint view_max;

	GtkHex_Highlight *highlights;
	GtkHex_AutoHighlight *next, *prev;
};

typedef struct _GtkHexPrivate GtkHexPrivate;

struct _GtkHexPrivate
{
	/* Buffer for storing formatted data for rendering;
	   dynamically adjusts its size to the display size */
	guchar *disp_buffer;

	gint default_cpl;
	gint default_lines;


	/* ========== */
	HexDocument *document;

	GtkWidget *xdisp, *adisp, *scrollbar;
	GtkWidget *offsets;

	PangoLayout *xlayout, *alayout, *olayout; /* Changes for Gnome 2.0 */

	GtkAdjustment *adj;

	PangoFontMetrics *disp_font_metrics;
	PangoFontDescription *font_desc;
	GtkCssProvider *font_provider;

	gint active_view;

	guint char_width, char_height;
	guint button;

	guint cursor_pos;
	GtkHex_Highlight selection;
	gint lower_nibble;

	guint group_type;

	gint lines, vis_lines, cpl, top_line;
	gint cursor_shown;

	gint xdisp_width, adisp_width, extra_width;

	GtkHex_AutoHighlight *auto_highlight;

	gint scroll_dir;
	guint scroll_timeout;
	gboolean show_offsets;
	gint starting_offset;
	gboolean insert;
	gboolean selecting;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkHex, gtk_hex, GTK_TYPE_FIXED)

enum {
	PROP_0,
	PROP_DOCUMENT,
	LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP] = { NULL, };


static gint gtkhex_signals[LAST_SIGNAL] = { 0 };

static gchar *char_widths = NULL;

static void render_hex_highlights (GtkHex *gh, cairo_t *cr, gint cursor_line);
static void render_ascii_highlights (GtkHex *gh, cairo_t *cr, gint cursor_line);
static void render_hex_lines (GtkHex *gh, cairo_t *cr, gint, gint);
static void render_ascii_lines (GtkHex *gh, cairo_t *cr, gint, gint);

static void gtk_hex_validate_highlight(GtkHex *gh, GtkHex_Highlight *hl);
static void gtk_hex_invalidate_highlight(GtkHex *gh, GtkHex_Highlight *hl);
static void gtk_hex_invalidate_all_highlights(GtkHex *gh);

static void gtk_hex_update_all_auto_highlights(GtkHex *gh, gboolean delete,
											   gboolean add);

static GtkHex_Highlight *gtk_hex_insert_highlight (GtkHex *gh,
												   GtkHex_AutoHighlight *ahl,
												   gint start, gint end);
static void gtk_hex_delete_highlight (GtkHex *gh, GtkHex_AutoHighlight *ahl,
									  GtkHex_Highlight *hl);
static void gtk_hex_update_auto_highlight(GtkHex *gh, GtkHex_AutoHighlight *ahl,
									  gboolean delete, gboolean add);

/*
 * simply forces widget w to redraw itself
 */
static void redraw_widget(GtkWidget *w) {
	if(!gtk_widget_get_realized(w))
		return;

	gdk_window_invalidate_rect (gtk_widget_get_window(w), NULL, FALSE);
}

/*
 * ?_to_pointer translates mouse coordinates in hex/ascii view
 * to cursor coordinates.
 */
static void hex_to_pointer(GtkHex *gh, guint mx, guint my) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	guint cx, cy, x;
	
	cy = priv->top_line + my/priv->char_height;
	
	cx = 0; x = 0;
	while(cx < 2*priv->cpl) {
		x += priv->char_width;
		
		if(x > mx) {
			gtk_hex_set_cursor_xy(gh, cx/2, cy);
			gtk_hex_set_nibble(gh, ((cx%2 == 0)?UPPER_NIBBLE:LOWER_NIBBLE));
			
			cx = 2*priv->cpl;
		}
		
		cx++;
		
		if(cx%(2*priv->group_type) == 0)
			x += priv->char_width;
	}
}

static void ascii_to_pointer(GtkHex *gh, gint mx, gint my) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	int cy;
	
	cy = priv->top_line + my/priv->char_height;
	
	gtk_hex_set_cursor_xy(gh, mx/priv->char_width, cy);
}

static guint get_max_char_width(GtkHex *gh, PangoFontMetrics *font_metrics) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	/* this is, I guess, a rather dirty trick, but
	   right now i can't think of anything better */
	guint i;
	guint maxwidth = 0;
	PangoRectangle logical_rect;
	PangoLayout *layout;
	gchar str[2]; 

	if (char_widths == NULL)
		char_widths = (gchar*)g_malloc(0x100);

	char_widths[0] = 0;

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (gh), "");
	pango_layout_set_font_description (layout, priv->font_desc);

	for(i = 1; i < 0x100; i++) {
		logical_rect.width = 0;
		/* Check if the char is displayable. Caused trouble to pango */
		if (is_displayable((guchar)i)) {
			sprintf (str, "%c", (gchar)i);
			pango_layout_set_text(layout, str, -1);
			pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
		}
		char_widths[i] = logical_rect.width;
	}

	for(i = '0'; i <= 'z'; i++)
		maxwidth = MAX(maxwidth, char_widths[i]);

	g_object_unref (G_OBJECT (layout));
	return maxwidth;
}

void format_xbyte(GtkHex *gh, gint pos, gchar buf[2]) {
	guint low, high;
	guchar c;

	c = gtk_hex_get_byte(gh, pos);
	low = c & 0x0F;
	high = (c & 0xF0) >> 4;
	
	buf[0] = ((high < 10)?(high + '0'):(high - 10 + 'A'));
	buf[1] = ((low < 10)?(low + '0'):(low - 10 + 'A'));
}

/*
 * format_[x|a]block() formats contents of the buffer
 * into displayable text in hex or ascii, respectively
 */
gint format_xblock(GtkHex *gh, gchar *out, guint start, guint end) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	int i, j, low, high;
	guchar c;

	for(i = start + 1, j = 0; i <= end; i++) {
		c = gtk_hex_get_byte(gh, i - 1);
		low = c & 0x0F;
		high = (c & 0xF0) >> 4;
		
		out[j++] = ((high < 10)?(high + '0'):(high - 10 + 'A'));
		out[j++] = ((low < 10)?(low + '0'):(low - 10 + 'A'));
		
		if(i%priv->group_type == 0)
			out[j++] = ' ';
	}
	
	return j;
}

gint format_ablock(GtkHex *gh, gchar *out, guint start, guint end) {
	int i, j;
	guchar c;

	for(i = start, j = 0; i < end; i++, j++) {
		c = gtk_hex_get_byte(gh, i);
		if(is_displayable(c))
			out[j] = c;
		else
			out[j] = '.';
	}

	return end - start;
}

/*
 * get_[x|a]coords() translates offset from the beginning of
 * the block into x,y coordinates of the xdisp/adisp, respectively
 */
static gint get_xcoords(GtkHex *gh, gint pos, gint *x, gint *y) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	gint cx, cy, spaces;
	
	if(priv->cpl == 0)
		return FALSE;
	
	cy = pos / priv->cpl;
	cy -= priv->top_line;
	if(cy < 0)
		return FALSE;
	
	cx = 2*(pos % priv->cpl);
	spaces = (pos % priv->cpl) / priv->group_type;
	
	cx *= priv->char_width;
	cy *= priv->char_height;
	spaces *= priv->char_width;
	*x = cx + spaces;
	*y = cy;
	
	return TRUE;
}

static gint get_acoords(GtkHex *gh, gint pos, gint *x, gint *y) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	gint cx, cy;
	
	if(priv->cpl == 0)
		return FALSE;

	cy = pos / priv->cpl;
	cy -= priv->top_line;
	if(cy < 0)
		return FALSE;
	cy *= priv->char_height;
	
	cx = priv->char_width*(pos % priv->cpl);
	
	*x = cx;
	*y = cy;
	
	return TRUE;
}

static void
invalidate_xc (GtkHex *gh)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
    GtkWidget *widget = priv->xdisp;
    gint cx, cy;

    if (get_xcoords (gh, priv->cursor_pos, &cx, &cy)) {
        if (priv->lower_nibble)
            cx += priv->char_width;

        gtk_widget_queue_draw_area (widget,
                                    cx, cy,
                                    priv->char_width + 1,
                                    priv->char_height);
    }
}

static void
invalidate_ac (GtkHex *gh)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
    GtkWidget *widget = priv->adisp;
    gint cx, cy;

    if (get_acoords (gh, priv->cursor_pos, &cx, &cy)) {
        gtk_widget_queue_draw_area (widget,
                                    cx, cy,
                                    priv->char_width + 1,
                                    priv->char_height);
    }
}

/*
 * the cursor rendering stuff...
 */
static void
render_ac (GtkHex *gh,
           cairo_t *cr)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GdkRGBA fg_color;
	GtkStateFlags state;
	GtkStyleContext *context;
	gint cx, cy;
	static gchar c[2] = "\0\0";

	if(!gtk_widget_get_realized(priv->adisp))
		return;

	context = gtk_widget_get_style_context (priv->adisp);
	state = gtk_widget_get_state_flags (priv->adisp);

	if(get_acoords(gh, priv->cursor_pos, &cx, &cy)) {
		c[0] = gtk_hex_get_byte(gh, priv->cursor_pos);
		if(!is_displayable(c[0]))
			c[0] = '.';

		if(priv->active_view == VIEW_ASCII) {
			gtk_widget_set_state_flags (priv->adisp, GTK_STATE_FLAG_SELECTED, FALSE);
			context = gtk_widget_get_style_context (priv->adisp);
			state = gtk_widget_get_state_flags (priv->adisp);
			gtk_render_background (context, cr, cx, cy,
								   priv->char_width, priv->char_height - 1);
			gtk_style_context_get_color (context, state, &fg_color);
			gtk_widget_unset_state_flags (priv->adisp, GTK_STATE_FLAG_SELECTED);
		} else {
			gtk_style_context_get_color (context, state, &fg_color);
			gdk_cairo_set_source_rgba (cr, &fg_color);
			cairo_set_line_width (cr, 1.0);
			cairo_rectangle (cr, cx + 0.5, cy + 0.5, priv->char_width, priv->char_height - 1);
			cairo_stroke (cr);
		}
		gdk_cairo_set_source_rgba (cr, &fg_color);
		cairo_move_to (cr, cx, cy);
		pango_layout_set_text (priv->alayout, (char *) c, 1);
		pango_cairo_show_layout (cr, priv->alayout);
	}
}

static void
render_xc (GtkHex *gh,
           cairo_t *cr)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GdkRGBA fg_color;
	GtkStateFlags state;
	GtkStyleContext *context;
	gint cx, cy, i;
	static gchar c[2];

	if(!gtk_widget_get_realized(priv->xdisp))
		return;

	context = gtk_widget_get_style_context (priv->xdisp);
	state = gtk_widget_get_state_flags (priv->xdisp);

	if(get_xcoords(gh, priv->cursor_pos, &cx, &cy)) {
		format_xbyte(gh, priv->cursor_pos, c);
		if(priv->lower_nibble) {
			cx += priv->char_width;
			i = 1;
		} else {
			c[1] = 0;
			i = 0;
		}

		if(priv->active_view == VIEW_HEX) {
			gtk_widget_set_state_flags (priv->xdisp, GTK_STATE_FLAG_SELECTED, FALSE);
			context = gtk_widget_get_style_context (priv->xdisp);
			state = gtk_widget_get_state_flags (priv->xdisp);
			gtk_render_background (context, cr, cx, cy,
								   priv->char_width, priv->char_height - 1);
			gtk_style_context_get_color (context, state, &fg_color);
			gtk_widget_unset_state_flags (priv->xdisp, GTK_STATE_FLAG_SELECTED);
			context = gtk_widget_get_style_context (priv->xdisp);
		} else {
			gtk_style_context_get_color (context, state, &fg_color);
			gdk_cairo_set_source_rgba (cr, &fg_color);
			cairo_set_line_width (cr, 1.0);
			cairo_rectangle (cr, cx + 0.5, cy + 0.5, priv->char_width, priv->char_height - 1);
			cairo_stroke (cr);
		}
		gdk_cairo_set_source_rgba (cr, &fg_color);
		cairo_move_to (cr, cx, cy);
		pango_layout_set_text (priv->xlayout, &c[i], 1);
		pango_cairo_show_layout (cr, priv->xlayout);
	}
}

static void show_cursor(GtkHex *gh) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	if(!priv->cursor_shown) {
		if (gtk_widget_get_realized (priv->xdisp) || gtk_widget_get_realized (priv->adisp)) {
			invalidate_xc (gh);
			invalidate_ac (gh);
		}
		priv->cursor_shown = TRUE;
	}
}

static void hide_cursor(GtkHex *gh) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	if(priv->cursor_shown) {
		if (gtk_widget_get_realized (priv->xdisp) || gtk_widget_get_realized (priv->adisp)) {
			invalidate_xc (gh);
			invalidate_ac (gh);
		}
		priv->cursor_shown = FALSE;
	}
}

void
get_highlight_colour (GtkWidget        *widget,
                      GtkHex_Highlight *hlight,
                      GdkRGBA          *colour)
{
	GdkRGBA         *c;
	GtkStyleContext *context;
	GtkStateFlags    state;

	if (hlight && hlight->bg_color) {
		*colour = *hlight->bg_color;
		return;
	}

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);
	gtk_style_context_get (context, state, "background-color", &c, NULL);

	*colour = *c;
	gdk_rgba_free (c);
}


static void
render_hex_highlights (GtkHex *gh,
                       cairo_t *cr,
                       gint cursor_line)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkHex_Highlight *curHighlight = &priv->selection;
	gint xcpl = priv->cpl*2 + priv->cpl/priv->group_type;
	   /* would be nice if we could cache that */

	GtkHex_AutoHighlight *nextList = priv->auto_highlight;

	gtk_widget_set_state_flags (priv->xdisp, GTK_STATE_FLAG_SELECTED, FALSE);

	cairo_save (cr);

	while (curHighlight)
	{
		if (ABS(curHighlight->start - curHighlight->end) >= curHighlight->min_select)
		{
			GdkRGBA bg_color;
			gint start, end;
			gint sl, el;
			gint cursor_off = 0;
			gint len;

			gtk_hex_validate_highlight(gh, curHighlight);

			start = MIN(curHighlight->start, curHighlight->end);
			end = MAX(curHighlight->start, curHighlight->end);
			sl = curHighlight->start_line;
			el = curHighlight->end_line;

			get_highlight_colour (priv->xdisp, curHighlight, &bg_color);
			gdk_cairo_set_source_rgba (cr, &bg_color);

			if (cursor_line == sl)
			{
				cursor_off = 2*(start%priv->cpl) + (start%priv->cpl)/priv->group_type;
				if (cursor_line == el)
					len = 2*(end%priv->cpl + 1) + (end%priv->cpl)/priv->group_type;
				else
					len = xcpl;
				len = len - cursor_off;
				if (len > 0)
					cairo_rectangle (cr,
									 cursor_off * priv->char_width,
									 cursor_line * priv->char_height,
									 len * priv->char_width,
									 priv->char_height);
			}
			else if (cursor_line == el)
			{
				cursor_off = 2*(end%priv->cpl + 1) + (end%priv->cpl)/priv->group_type;
				if (cursor_off > 0)
					cairo_rectangle (cr,
									 0,
									 cursor_line * priv->char_height,
									 cursor_off * priv->char_width,
									 priv->char_height);
			}
			else if (cursor_line > sl && cursor_line < el)
			{
				cairo_rectangle (cr,
								 0,
								 cursor_line * priv->char_height,
								 xcpl * priv->char_width,
								 priv->char_height);
			}

			cairo_fill (cr);
		}
		curHighlight = curHighlight->next;
		while (curHighlight == NULL && nextList)
		{
			curHighlight = nextList->highlights;
			nextList = nextList->next;
		}
	}

	cairo_restore (cr);
	gtk_widget_unset_state_flags (priv->xdisp, GTK_STATE_FLAG_SELECTED);
}

static void
render_ascii_highlights (GtkHex *gh,
                         cairo_t *cr,
                         gint cursor_line)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkHex_Highlight *curHighlight = &priv->selection;
	GtkHex_AutoHighlight *nextList = priv->auto_highlight;

	gtk_widget_set_state_flags (priv->adisp, GTK_STATE_FLAG_SELECTED, FALSE);

	cairo_save (cr);

	while (curHighlight)
	{
		if (ABS(curHighlight->start - curHighlight->end) >= curHighlight->min_select)
		{
			GdkRGBA bg_color;
			gint start, end;
			gint sl, el;
			gint cursor_off = 0;
			gint len;

			gtk_hex_validate_highlight(gh, curHighlight);

			start = MIN(curHighlight->start, curHighlight->end);
			end = MAX(curHighlight->start, curHighlight->end);
			sl = curHighlight->start_line;
			el = curHighlight->end_line;

			get_highlight_colour (priv->adisp, curHighlight, &bg_color);
			gdk_cairo_set_source_rgba (cr, &bg_color);

			if (cursor_line == sl)
			{
				cursor_off = start % priv->cpl;
				if (cursor_line == el)
					len = end - start + 1;
				else
					len = priv->cpl - cursor_off;
				if (len > 0)
					cairo_rectangle (cr,
									 cursor_off * priv->char_width,
									 cursor_line * priv->char_height,
									 len * priv->char_width,
									 priv->char_height);
			}
			else if (cursor_line == el)
			{
				cursor_off = end % priv->cpl + 1;
				if (cursor_off > 0)
					cairo_rectangle (cr,
									 0,
									 cursor_line * priv->char_height,
									 cursor_off * priv->char_width,
									 priv->char_height);
			}
			else if (cursor_line > sl && cursor_line < el)
			{
				cairo_rectangle (cr,
								 0,
								 cursor_line * priv->char_height,
								 priv->cpl * priv->char_width,
								 priv->char_height);
			}

			cairo_fill (cr);
		}
		curHighlight = curHighlight->next;
		while (curHighlight == NULL && nextList)
		{
			curHighlight = nextList->highlights;
			nextList = nextList->next;
		}
	}

	cairo_restore (cr);
	gtk_widget_unset_state_flags (priv->adisp, GTK_STATE_FLAG_SELECTED);
}

/*
 * when calling invalidate_*_lines() the imin and imax arguments are the
 * numbers of the first and last line TO BE INVALIDATED in the range
 * [0 .. priv->vis_lines-1] AND NOT [0 .. priv->lines]!
 */
static void
invalidate_lines (GtkHex *gh,
                  GtkWidget *widget,
                  gint imin,
                  gint imax)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
    GtkAllocation allocation;

    gtk_widget_get_allocation (widget, &allocation);
    gtk_widget_queue_draw_area (widget,
                                0,
                                imin * priv->char_height,
                                allocation.width,
                                (imax - imin + 1) * priv->char_height);
}

static void
invalidate_hex_lines (GtkHex *gh,
                      gint imin,
                      gint imax)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
    invalidate_lines (gh, priv->xdisp, imin, imax);
}

static void
invalidate_ascii_lines (GtkHex *gh,
                        gint imin,
                        gint imax)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
    invalidate_lines (gh, priv->adisp, imin, imax);
}

static void
invalidate_offsets (GtkHex *gh,
                    gint imin,
                    gint imax)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
    invalidate_lines (gh, priv->offsets, imin, imax);
}

/*
 * when calling render_*_lines() the imin and imax arguments are the
 * numbers of the first and last line TO BE DISPLAYED in the range
 * [0 .. priv->vis_lines-1] AND NOT [0 .. priv->lines]!
 */
static void
render_hex_lines (GtkHex *gh,
                  cairo_t *cr,
                  gint imin,
                  gint imax)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkWidget *w = priv->xdisp;
	GdkRGBA fg_color;
	GtkAllocation allocation;
	GtkStateFlags state;
	GtkStyleContext *context;
	gint i, cursor_line;
	gint xcpl = priv->cpl*2 + priv->cpl/priv->group_type;
	gint frm_len, tmp;

	if( (!gtk_widget_get_realized(GTK_WIDGET (gh))) || (priv->cpl == 0) )
		return;

	context = gtk_widget_get_style_context (w);
	state = gtk_widget_get_state_flags (w);

	gtk_style_context_get_color (context, state, &fg_color);

	cursor_line = priv->cursor_pos / priv->cpl - priv->top_line;

	gtk_widget_get_allocation(w, &allocation);
	gtk_render_background (context, cr,
						   0, imin * priv->char_height,
						   allocation.width, (imax - imin + 1) * priv->char_height);
	cairo_fill (cr);
  
	imax = MIN(imax, priv->vis_lines);
	imax = MIN(imax, priv->lines);

	gdk_cairo_set_source_rgba (cr, &fg_color);

	frm_len = format_xblock (gh, (gchar *) priv->disp_buffer, (priv->top_line+imin)*priv->cpl,
							MIN((priv->top_line+imax+1)*priv->cpl, hex_document_get_file_size (priv->document)));
	
	for(i = imin; i <= imax; i++) {
		tmp = (gint)frm_len - (gint)((i - imin)*xcpl);
		if(tmp <= 0)
			break;

		render_hex_highlights (gh, cr, i);
		cairo_move_to (cr, 0, i * priv->char_height);
		pango_layout_set_text (priv->xlayout, (char *) priv->disp_buffer + (i - imin) * xcpl, MIN(xcpl, tmp));
		pango_cairo_show_layout (cr, priv->xlayout);
	}
	
	if((cursor_line >= imin) && (cursor_line <= imax) && (priv->cursor_shown))
		render_xc (gh, cr);
}

static void
render_ascii_lines (GtkHex *gh,
                    cairo_t *cr,
                    gint imin,
                    gint imax)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkWidget *w = priv->adisp;
	GdkRGBA fg_color;
	GtkAllocation allocation;
	GtkStateFlags state;
	GtkStyleContext *context;
	gint i, tmp, frm_len;
	guint cursor_line;

	if( (!gtk_widget_get_realized(GTK_WIDGET(gh))) || (priv->cpl == 0) )
		return;

	context = gtk_widget_get_style_context (w);
	state = gtk_widget_get_state_flags (w);

	gtk_style_context_get_color (context, state, &fg_color);

	cursor_line = priv->cursor_pos / priv->cpl - priv->top_line;

	gtk_widget_get_allocation(w, &allocation);
	gtk_render_background (context, cr,
						   0, imin * priv->char_height,
						   allocation.width, (imax - imin + 1) * priv->char_height);
	cairo_fill (cr);
	
	imax = MIN(imax, priv->vis_lines);
	imax = MIN(imax, priv->lines);

	gdk_cairo_set_source_rgba (cr, &fg_color);
	
	frm_len = format_ablock (gh, (gchar *) priv->disp_buffer, (priv->top_line+imin)*priv->cpl,
							MIN((priv->top_line+imax+1)*priv->cpl, hex_document_get_file_size (priv->document)) );
	
	for(i = imin; i <= imax; i++) {
		tmp = (gint)frm_len - (gint)((i - imin)*priv->cpl);
		if(tmp <= 0)
			break;

		render_ascii_highlights (gh, cr, i);

		cairo_move_to (cr, 0, i * priv->char_height);
		pango_layout_set_text (priv->alayout, (gchar *) priv->disp_buffer + (i - imin)*priv->cpl, MIN(priv->cpl, tmp));
		pango_cairo_show_layout (cr, priv->alayout);
	}
	
	if((cursor_line >= imin) && (cursor_line <= imax) && (priv->cursor_shown))
		render_ac (gh, cr);
}

static void
render_offsets (GtkHex *gh,
                cairo_t *cr,
                gint imin,
                gint imax)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkWidget *w = priv->offsets;
	GdkRGBA fg_color;
	GtkAllocation allocation;
	GtkStateFlags state;
	GtkStyleContext *context;
	gint i;
	gchar offstr[9];

	if(!gtk_widget_get_realized(GTK_WIDGET(gh)))
		return;

	context = gtk_widget_get_style_context (w);
	state = gtk_widget_get_state_flags (w);

	gtk_style_context_get_color (context, state, &fg_color);

	gtk_widget_get_allocation(w, &allocation);
	gtk_render_background (context, cr, 0,
						   imin * priv->char_height,
						   allocation.width, (imax - imin + 1) * priv->char_height);
	cairo_fill (cr);
  
	imax = MIN(imax, priv->vis_lines);
	imax = MIN(imax, priv->lines - priv->top_line - 1);

	gdk_cairo_set_source_rgba (cr, &fg_color);
	
	for(i = imin; i <= imax; i++) {
		sprintf(offstr, "%08X", (priv->top_line + i)*priv->cpl + priv->starting_offset);
		cairo_move_to (cr, 0, i * priv->char_height);
		pango_layout_set_text (priv->olayout, offstr, 8);
		pango_cairo_show_layout (cr, priv->olayout);
	}
}

/*
 * draw signal handlers for both displays
 */
static void
hex_draw (GtkWidget *w,
          cairo_t *cr,
          GtkHex *gh)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GdkRectangle rect;
	gint imin, imax;

	gdk_cairo_get_clip_rectangle (cr, &rect);

	imin = (rect.y) / priv->char_height;
	imax = (rect.y + rect.height) / priv->char_height;
	if ((rect.y + rect.height) % priv->char_height)
		imax++;

	imax = MIN(imax, priv->vis_lines);
	
	render_hex_lines (gh, cr, imin, imax);
}

static void
ascii_draw (GtkWidget *w,
            cairo_t *cr,
            GtkHex *gh)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GdkRectangle rect;
	gint imin, imax;

	gdk_cairo_get_clip_rectangle (cr, &rect);

	imin = (rect.y) / priv->char_height;
	imax = (rect.y + rect.height) / priv->char_height;
	if ((rect.y + rect.height) % priv->char_height)
		imax++;
	
	imax = MIN(imax, priv->vis_lines);

	render_ascii_lines (gh, cr, imin, imax);
}

static void
offsets_draw (GtkWidget *w,
              cairo_t *cr,
              GtkHex *gh)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GdkRectangle rect;
	gint imin, imax;

	gdk_cairo_get_clip_rectangle (cr, &rect);

	imin = (rect.y) / priv->char_height;
	imax = (rect.y + rect.height) / priv->char_height;
	if ((rect.y + rect.height) % priv->char_height)
		imax++;

	imax = MIN(imax, priv->vis_lines);
	
	render_offsets (gh, cr, imin, imax);
}

/*
 * draw signal handler for the GtkHex itself: draws shadows around both displays
 */
static void
draw_shadow (GtkWidget *widget,
             cairo_t *cr)
{
	GtkHex *gh = GTK_HEX(widget);
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkAllocation allocation;
	GtkBorder padding;
	GtkStateFlags state;
	GtkStyleContext *context;
	gint x;
	gint sb_width;
	gint border = gtk_container_get_border_width(GTK_CONTAINER(widget));

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);
	gtk_style_context_get_padding (context, state, &padding);

	x = border;
	gtk_widget_get_allocation(widget, &allocation);
	if(priv->show_offsets) {
		gtk_render_frame (context,
		                  cr,
		                  border,
		                  border,
		                  8 * priv->char_width + padding.left + padding.right,
		                  allocation.height - 2 * border);
		x += 8 * priv->char_width + padding.left + padding.right + priv->extra_width/2;
	}

	gtk_render_frame (context,
	                  cr,
	                  x,
	                  border,
	                  priv->xdisp_width + padding.left + padding.right,
	                  allocation.height - 2 * border);

	/* Draw a frame around the ascii display + scrollbar */
	gtk_widget_get_preferred_width (priv->scrollbar, NULL, &sb_width);
	gtk_render_frame (context,
	                  cr,
	                  allocation.width - border - priv->adisp_width - sb_width - padding.left - padding.right,
	                  border,
	                  priv->adisp_width + sb_width + padding.left + padding.right,
	                  allocation.height - 2 * border);
}

/*
 * this calculates how many bytes we can stuff into one line and how many
 * lines we can display according to the current size of the widget
 */
static void recalc_displays(GtkHex *gh, guint width, guint height) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	gboolean scroll_to_cursor;
	gdouble value;
	gint total_width = width;
	gint total_cpl, xcpl;
	gint old_cpl = priv->cpl;
	GtkBorder padding;
	GtkStateFlags state;
	GtkStyleContext *context;
	GtkRequisition req;

	context = gtk_widget_get_style_context (GTK_WIDGET (gh));
	state = gtk_widget_get_state_flags (GTK_WIDGET (gh));
	gtk_style_context_get_padding (context, state, &padding);

	/*
	 * Only change the value of the adjustment to put the cursor on screen
	 * if the cursor is currently within the displayed portion.
	 */
	scroll_to_cursor = (priv->cpl == 0) ||
	                   ((priv->cursor_pos / priv->cpl >= gtk_adjustment_get_value (priv->adj)) &&
	                    (priv->cursor_pos / priv->cpl <= gtk_adjustment_get_value (priv->adj) + priv->vis_lines - 1));

	gtk_widget_get_preferred_size (priv->scrollbar, &req, NULL);
	
	priv->xdisp_width = 1;
	priv->adisp_width = 1;

	total_width -= 2*gtk_container_get_border_width(GTK_CONTAINER(gh)) +
	               2 * padding.left + 2 * padding.right + req.width;

	if(priv->show_offsets)
		total_width -= padding.left + padding.right + 8 * priv->char_width;

	total_cpl = total_width / priv->char_width;

	if((total_cpl == 0) || (total_width < 0)) {
		priv->cpl = priv->lines = priv->vis_lines = 0;
		return;
	}
	
	/* calculate how many bytes we can stuff in one line */
	priv->cpl = 0;
	do {
		if(priv->cpl % priv->group_type == 0 && total_cpl < priv->group_type*3)
			break;
		
		priv->cpl++;        /* just added one more char */
		total_cpl -= 3;   /* 2 for xdisp, 1 for adisp */
		
		if(priv->cpl % priv->group_type == 0) /* just ended a group */
			total_cpl--;
	} while(total_cpl > 0);

	if(priv->cpl == 0)
		return;

	if(hex_document_get_file_size (priv->document) == 0)
		priv->lines = 1;
	else {
		priv->lines = hex_document_get_file_size (priv->document) / priv->cpl;
		if(hex_document_get_file_size (priv->document) % priv->cpl)
			priv->lines++;
	}

	priv->vis_lines = ((gint) (height - 2 * gtk_container_get_border_width (GTK_CONTAINER (gh)) - padding.top - padding.bottom)) / ((gint) priv->char_height);

	priv->adisp_width = priv->cpl*priv->char_width;
	xcpl = priv->cpl*2 + (priv->cpl - 1)/priv->group_type;
	priv->xdisp_width = xcpl*priv->char_width;

	priv->extra_width = total_width - priv->xdisp_width - priv->adisp_width;

	if (priv->disp_buffer)
		g_free (priv->disp_buffer);
	
	priv->disp_buffer = g_malloc ((xcpl + 1) * (priv->vis_lines + 1));

	/* calculate new display position */
	value = MIN (priv->top_line * old_cpl / priv->cpl, priv->lines - priv->vis_lines);
	value = MAX (0, value);

	/* keep cursor on screen if it was on screen before */
	if (scroll_to_cursor &&
	    ((priv->cursor_pos / priv->cpl < value) ||
	     (priv->cursor_pos / priv->cpl > value + priv->vis_lines - 1))) {
		value = MIN (priv->cursor_pos / priv->cpl, priv->lines - priv->vis_lines);
		value = MAX (0, value);
	}

	/* adjust the scrollbar and display position to new values */
	gtk_adjustment_configure (priv->adj,
	                          value,             /* value */
	                          0,                 /* lower */
	                          priv->lines,         /* upper */
	                          1,                 /* step increment */
	                          priv->vis_lines - 1, /* page increment */
	                          priv->vis_lines      /* page size */);

	g_signal_emit_by_name(G_OBJECT(priv->adj), "changed");
	g_signal_emit_by_name(G_OBJECT(priv->adj), "value_changed");
}

/*
 * takes care of xdisp and adisp scrolling
 * connected to value_changed signal of scrollbar's GtkAdjustment
 */
static void display_scrolled(GtkAdjustment *adj, GtkHex *gh) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	gint dx;
	gint dy;

	if ((!gtk_widget_is_drawable(priv->xdisp)) ||
	    (!gtk_widget_is_drawable(priv->adisp)))
		return;

	dx = 0;
	dy = (priv->top_line - (gint)gtk_adjustment_get_value (adj)) * priv->char_height;

	priv->top_line = (gint)gtk_adjustment_get_value(adj);

	gdk_window_scroll (gtk_widget_get_window (priv->xdisp), dx, dy);
	gdk_window_scroll (gtk_widget_get_window (priv->adisp), dx, dy);
	if (priv->offsets)
		gdk_window_scroll (gtk_widget_get_window (priv->offsets), dx, dy);

	gtk_hex_update_all_auto_highlights(gh, TRUE, TRUE);
	gtk_hex_invalidate_all_highlights(gh);
}

/*
 * mouse signal handlers (button 1 and motion) for both displays
 */
static gboolean scroll_timeout_handler(GtkHex *gh) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	if(priv->scroll_dir < 0)
		gtk_hex_set_cursor(gh, MAX(0, (int)(priv->cursor_pos - priv->cpl)));
	else if(priv->scroll_dir > 0)
		gtk_hex_set_cursor(gh, MIN(hex_document_get_file_size (priv->document) - 1,
								   priv->cursor_pos + priv->cpl));
	return TRUE;
}

static void hex_scroll_cb(GtkWidget *w, GdkEventScroll *event, GtkHex *gh) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	gtk_widget_event(priv->scrollbar, (GdkEvent *)event);
}

static void hex_button_cb(GtkWidget *w, GdkEventButton *event, GtkHex *gh) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	if( (event->type == GDK_BUTTON_RELEASE) &&
		(event->button == GDK_BUTTON_PRIMARY) ) {
		if(priv->scroll_timeout != -1) {
			g_source_remove(priv->scroll_timeout);
			priv->scroll_timeout = -1;
			priv->scroll_dir = 0;
		}
		priv->selecting = FALSE;
		gtk_grab_remove(w);
		priv->button = 0;
	}
	else if((event->type == GDK_BUTTON_PRESS) && (event->button == GDK_BUTTON_PRIMARY)) {
		if (!gtk_widget_has_focus (GTK_WIDGET (gh)))
			gtk_widget_grab_focus (GTK_WIDGET(gh));
		
		gtk_grab_add(w);

		priv->button = event->button;
		
		if(priv->active_view == VIEW_HEX) {
			hex_to_pointer(gh, event->x, event->y);

			if(!priv->selecting) {
				priv->selecting = TRUE;
				gtk_hex_set_selection(gh, priv->cursor_pos, priv->cursor_pos);
			}
		}
		else {
			hide_cursor(gh);
			priv->active_view = VIEW_HEX;
			show_cursor(gh);
			hex_button_cb(w, event, gh);
		}
	}
	else if((event->type == GDK_BUTTON_PRESS) && (event->button == GDK_BUTTON_MIDDLE)) {
		GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));
		gchar *text;

		priv->active_view = VIEW_HEX;
		hex_to_pointer(gh, event->x, event->y);

		text = gtk_clipboard_wait_for_text(klass->primary);
		if(text) {
			hex_document_set_data(priv->document, priv->cursor_pos,
								  strlen(text), 0, (guchar *)  text, TRUE);
			gtk_hex_set_cursor(gh, priv->cursor_pos + strlen(text));
			g_free(text);
		}
		priv->button = 0;
	}
	else
		priv->button = 0;
}

static void hex_motion_cb(GtkWidget *w, GdkEventMotion *event, GtkHex *gh) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkAllocation  allocation;
	GdkSeat       *seat;
	GdkDevice     *pointer;
	gint           x, y;

	gtk_widget_get_allocation(w, &allocation);

	seat = gdk_display_get_default_seat (gtk_widget_get_display (w));
	pointer = gdk_seat_get_pointer (seat);
	gdk_window_get_device_position (gtk_widget_get_window (w), pointer, &x, &y, NULL);

	if(y < 0)
		priv->scroll_dir = -1;
	else if(y >= allocation.height)
		priv->scroll_dir = 1;
	else
		priv->scroll_dir = 0;

	if(priv->scroll_dir != 0) {
		if(priv->scroll_timeout == -1)
			priv->scroll_timeout =
				g_timeout_add(SCROLL_TIMEOUT,
							  (GSourceFunc)scroll_timeout_handler, gh);
		return;
	}
	else {
		if(priv->scroll_timeout != -1) {
			g_source_remove(priv->scroll_timeout);
			priv->scroll_timeout = -1;
		}
	}
			
	if(event->window != gtk_widget_get_window(w))
		return;

	if((priv->active_view == VIEW_HEX) && (priv->button == 1)) {
		hex_to_pointer(gh, x, y);
	}
}

static void ascii_scroll_cb(GtkWidget *w, GdkEventScroll *event, GtkHex *gh) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	gtk_widget_event(priv->scrollbar, (GdkEvent *)event);
}

static void ascii_button_cb(GtkWidget *w, GdkEventButton *event, GtkHex *gh) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	if( (event->type == GDK_BUTTON_RELEASE) &&
		(event->button == GDK_BUTTON_PRIMARY) ) {
		if(priv->scroll_timeout != -1) {
			g_source_remove(priv->scroll_timeout);
			priv->scroll_timeout = -1;
			priv->scroll_dir = 0;
		}
		priv->selecting = FALSE;
		gtk_grab_remove(w);
		priv->button = 0;
	}
	else if( (event->type == GDK_BUTTON_PRESS) && (event->button == GDK_BUTTON_PRIMARY) ) {
		if (!gtk_widget_has_focus (GTK_WIDGET (gh)))
			gtk_widget_grab_focus (GTK_WIDGET(gh));
		
		gtk_grab_add(w);
		priv->button = event->button;
		if(priv->active_view == VIEW_ASCII) {
			ascii_to_pointer(gh, event->x, event->y);
			if(!priv->selecting) {
				priv->selecting = TRUE;
				gtk_hex_set_selection(gh, priv->cursor_pos, priv->cursor_pos);
			}
		}
		else {
			hide_cursor(gh);
			priv->active_view = VIEW_ASCII;
			show_cursor(gh);
			ascii_button_cb(w, event, gh);
		}
	}
	else if((event->type == GDK_BUTTON_PRESS) && (event->button == GDK_BUTTON_MIDDLE)) {
		GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));
		gchar *text;

		priv->active_view = VIEW_ASCII;
		ascii_to_pointer(gh, event->x, event->y);

		text = gtk_clipboard_wait_for_text(klass->primary);
		if(text) {
			hex_document_set_data(priv->document, priv->cursor_pos,
								  strlen(text), 0, (guchar *) text, TRUE);
			gtk_hex_set_cursor(gh, priv->cursor_pos + strlen(text));
			g_free(text);
		}
		priv->button = 0;
	}
	else
		priv->button = 0;
}

static void ascii_motion_cb(GtkWidget *w, GdkEventMotion *event, GtkHex *gh) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkAllocation  allocation;
	GdkSeat       *seat;
	GdkDevice     *pointer;
	gint           x, y;

	gtk_widget_get_allocation(w, &allocation);

	seat = gdk_display_get_default_seat (gtk_widget_get_display (w));
	pointer = gdk_seat_get_pointer (seat);
	gdk_window_get_device_position (gtk_widget_get_window (w), pointer, &x, &y, NULL);

	if(y < 0)
		priv->scroll_dir = -1;
	else if(y >= allocation.height)
		priv->scroll_dir = 1;
	else
		priv->scroll_dir = 0;

	if(priv->scroll_dir != 0) {
		if(priv->scroll_timeout == -1)
			priv->scroll_timeout =
				g_timeout_add(SCROLL_TIMEOUT,
							  (GSourceFunc)scroll_timeout_handler, gh);
		return;
	}
	else {
		if(priv->scroll_timeout != -1) {
			g_source_remove(priv->scroll_timeout);
			priv->scroll_timeout = -1;
		}
	}

	if(event->window != gtk_widget_get_window(w))
		return;

	if((priv->active_view == VIEW_ASCII) && (priv->button == 1)) {
		ascii_to_pointer(gh, x, y);
	}
}

static void show_offsets_widget(GtkHex *gh) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkStyleContext *context;

	priv->offsets = gtk_drawing_area_new();

	/* Create the pango layout for the widget */
	priv->olayout = gtk_widget_create_pango_layout (priv->offsets, "");

	gtk_widget_set_events (priv->offsets, GDK_EXPOSURE_MASK);
	g_signal_connect (G_OBJECT(priv->offsets), "draw",
	                  G_CALLBACK (offsets_draw), gh);

	context = gtk_widget_get_style_context (GTK_WIDGET (priv->xdisp));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_HEADER);

	gtk_fixed_put(GTK_FIXED(gh), priv->offsets, 0, 0);
	gtk_widget_show(priv->offsets);
}

static void hide_offsets_widget(GtkHex *gh) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	if(priv->offsets) {
		gtk_container_remove(GTK_CONTAINER(gh), priv->offsets);
		priv->offsets = NULL;
	}
}

/*
 * default data_changed signal handler
 */
static void gtk_hex_real_data_changed(GtkHex *gh, gpointer data) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	HexChangeData *change_data = (HexChangeData *)data;
	gint start_line, end_line;
	guint lines;

	if(priv->cpl == 0)
		return;

	if(change_data->start - change_data->end + 1 != change_data->rep_len) {
		lines = hex_document_get_file_size (priv->document) / priv->cpl;
		if(hex_document_get_file_size (priv->document) % priv->cpl)
			lines++;
		if(lines != priv->lines) {
			priv->lines = lines;
			gtk_adjustment_set_value(priv->adj, MIN(gtk_adjustment_get_value(priv->adj), priv->lines - priv->vis_lines));
			gtk_adjustment_set_value(priv->adj, MAX(0, gtk_adjustment_get_value(priv->adj)));
			if((priv->cursor_pos/priv->cpl < gtk_adjustment_get_value(priv->adj)) ||
			   (priv->cursor_pos/priv->cpl > gtk_adjustment_get_value(priv->adj) + priv->vis_lines - 1)) {
				gtk_adjustment_set_value(priv->adj, MIN(priv->cursor_pos/priv->cpl, priv->lines - priv->vis_lines));
				gtk_adjustment_set_value(priv->adj, MAX(0, gtk_adjustment_get_value(priv->adj)));
			}
			gtk_adjustment_set_lower(priv->adj, 0);
			gtk_adjustment_set_upper(priv->adj, priv->lines);
			gtk_adjustment_set_step_increment(priv->adj, 1);
			gtk_adjustment_set_page_increment(priv->adj, priv->vis_lines - 1);
			gtk_adjustment_set_page_size(priv->adj, priv->vis_lines);
			g_signal_emit_by_name(G_OBJECT(priv->adj), "changed");
			g_signal_emit_by_name(G_OBJECT(priv->adj), "value_changed");
		}
	}

	start_line = change_data->start/priv->cpl - priv->top_line;
	end_line = change_data->end/priv->cpl - priv->top_line;

	if(end_line < 0 ||
	   start_line > priv->vis_lines)
		return;

	start_line = MAX(start_line, 0);
	if(change_data->rep_len - change_data->end + change_data->start - 1 != 0)
		end_line = priv->vis_lines;
	else
		end_line = MIN(end_line, priv->vis_lines);

    invalidate_hex_lines (gh, start_line, end_line);
    invalidate_ascii_lines (gh, start_line, end_line);
    if (priv->show_offsets)
    {
        invalidate_offsets (gh, start_line, end_line);
    }
}

static void bytes_changed(GtkHex *gh, gint start, gint end)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	gint start_line = start/priv->cpl - priv->top_line;
	gint end_line = end/priv->cpl - priv->top_line;

	if(end_line < 0 ||
	   start_line > priv->vis_lines)
		return;

	start_line = MAX(start_line, 0);

    invalidate_hex_lines (gh, start_line, end_line);
    invalidate_ascii_lines (gh, start_line, end_line);
    if (priv->show_offsets)
    {
        invalidate_offsets (gh, start_line, end_line);
    }
}

static void primary_get_cb(GtkClipboard *clipboard,
						   GtkSelectionData *data, guint info,
						   gpointer user_data) {
	GtkHex *gh = GTK_HEX(user_data);
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	if(priv->selection.start != priv->selection.end) {
		gint start_pos; 
		gint end_pos;
		guchar *text;

		start_pos = MIN(priv->selection.start, priv->selection.end);
		end_pos = MAX(priv->selection.start, priv->selection.end);
 
		text = hex_document_get_data(priv->document, start_pos,
									 end_pos - start_pos);
		gtk_selection_data_set_text(data, (gchar *) text, end_pos - start_pos);
		g_free(text);
	}
}

static void primary_clear_cb(GtkClipboard *clipboard,
							 gpointer user_data_or_owner) {
}

void gtk_hex_set_selection(GtkHex *gh, gint start, gint end)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	gint length = hex_document_get_file_size (priv->document);
	gint oe, os, ne, ns;
	GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));

	static const GtkTargetEntry targets[] = {
		{ "STRING", 0, TARGET_STRING }
	};
	static const gint n_targets = sizeof(targets) / sizeof(targets[0]);

	if (end < 0)
		end = length;

	if (priv->selection.start != priv->selection.end)
		gtk_clipboard_clear(klass->primary);

	os = MIN(priv->selection.start, priv->selection.end);
	oe = MAX(priv->selection.start, priv->selection.end);

	priv->selection.start = CLAMP(start, 0, length);
	priv->selection.end = MIN(end, length);

	gtk_hex_invalidate_highlight(gh, &priv->selection);

	ns = MIN(priv->selection.start, priv->selection.end);
	ne = MAX(priv->selection.start, priv->selection.end);

	if(ns != os && ne != oe) {
		bytes_changed(gh, MIN(ns, os), MAX(ne, oe));
	}
	else if(ne != oe) {
		bytes_changed(gh, MIN(ne, oe), MAX(ne, oe));
	}
	else if(ns != os) {
		bytes_changed(gh, MIN(ns, os), MAX(ns, os));
	}

	if(priv->selection.start != priv->selection.end)
		gtk_clipboard_set_with_data(klass->primary, targets, n_targets,
									primary_get_cb, primary_clear_cb,
									gh);
}

gboolean gtk_hex_get_selection(GtkHex *gh, gint *start, gint *end)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	gint ss, se;

	if(priv->selection.start > priv->selection.end) {
		se = priv->selection.start;
		ss = priv->selection.end;
	}
	else {
		ss = priv->selection.start;
		se = priv->selection.end;
	}

	if(NULL != start)
		*start = ss;
	if(NULL != end)
		*end = se;

	return !(ss == se);
}

void gtk_hex_clear_selection(GtkHex *gh)
{
	gtk_hex_set_selection(gh, 0, 0);
}

void gtk_hex_delete_selection(GtkHex *gh)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	guint start;
	guint end;

	start = MIN(priv->selection.start, priv->selection.end);
	end = MAX(priv->selection.start, priv->selection.end);

	gtk_hex_set_selection(gh, 0, 0);

	if(start != end) {
		if(start < priv->cursor_pos)
			gtk_hex_set_cursor(gh, priv->cursor_pos - end + start);
		hex_document_delete_data(priv->document, MIN(start, end), end - start, TRUE);
	}
}

static void gtk_hex_validate_highlight(GtkHex *gh, GtkHex_Highlight *hl)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	if (!hl->valid)
	{
		hl->start_line = MIN(hl->start, hl->end) / priv->cpl - priv->top_line;
		hl->end_line = MAX(hl->start, hl->end) / priv->cpl - priv->top_line;
		hl->valid = TRUE;
	}
}

static void gtk_hex_invalidate_highlight(GtkHex *gh, GtkHex_Highlight *hl)
{
	hl->valid = FALSE;
}

static void gtk_hex_invalidate_all_highlights(GtkHex *gh)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkHex_Highlight *cur = &priv->selection;
	GtkHex_AutoHighlight *nextList = priv->auto_highlight;
	while (cur)
	{
		gtk_hex_invalidate_highlight(gh, cur);
		cur = cur->next;
		while (cur == NULL && nextList)
		{
			cur = nextList->highlights;
			nextList = nextList->next;
		}
	}
}

static GtkHex_Highlight *gtk_hex_insert_highlight (GtkHex *gh,
												   GtkHex_AutoHighlight *ahl,
												   gint start, gint end)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GdkRGBA rgba;
	gint length = hex_document_get_file_size (priv->document);

	GtkHex_Highlight *new = g_malloc0(sizeof(GtkHex_Highlight));

	new->start = CLAMP(MIN(start, end), 0, length);
	new->end = MIN(MAX(start, end), length);

	new->valid = FALSE;

	new->min_select = 0;

	if (gtk_hex_autohighlight_get_colour (ahl, &rgba))
		new->bg_color = gdk_rgba_copy (&rgba);
	else
		new->bg_color = NULL;


	new->prev = NULL;
	new->next = ahl->highlights;
	if (new->next) new->next->prev = new;
	ahl->highlights = new;

	bytes_changed(gh, new->start, new->end);

	return new;
}

static void gtk_hex_delete_highlight (GtkHex *gh, GtkHex_AutoHighlight *ahl,
									  GtkHex_Highlight *hl)
{
	int start, end;
	start = hl->start;
	end = hl->end;
	if (hl->prev) hl->prev->next = hl->next;
	if (hl->next) hl->next->prev = hl->prev;

	if (hl == ahl->highlights) ahl->highlights = hl->next;

	if (hl->bg_color)
		gdk_rgba_free (hl->bg_color);

	g_free(hl);
	bytes_changed(gh, start, end);
}

/* stolen from hex_document_compare_data - but uses
 * gtk_hex_* stuff rather than hex_document_* directly
 * and simply returns a gboolean.
 */
static gboolean gtk_hex_compare_data(GtkHex *gh, guchar *cmp, guint pos, gint len)
{
	int i;
	for (i = 0; i < len; i++)
	{
		guchar c = gtk_hex_get_byte(gh, pos + i);
		if (c != *(cmp + i))
			return FALSE;
	}
	return TRUE;
}

static gboolean gtk_hex_find_limited(GtkHex *gh, gchar *find, int findlen,
									 guint lower, guint upper,
									 guint *found)
{
	guint pos = lower;
	while (pos < upper)
	{
		if (gtk_hex_compare_data(gh, (guchar *)find, pos, findlen))
		{
			*found = pos;
			return TRUE;
		}
		pos++;
	}
	return FALSE;
}

/* removes any highlights that arn't visible
 * adds any new highlights that became visible
 */
static void gtk_hex_update_auto_highlight(GtkHex *gh, GtkHex_AutoHighlight *ahl,
										  gboolean delete, gboolean add)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	gint del_min, del_max;
	gint add_min, add_max;
	guint foundpos = -1;
	gint prev_min = ahl->view_min;
	gint prev_max = ahl->view_max;
	GtkHex_Highlight *cur;

	ahl->view_min = priv->top_line * priv->cpl;
	ahl->view_max = (priv->top_line + priv->vis_lines) * priv->cpl;

	if (prev_min < ahl->view_min && prev_max < ahl->view_max)
	{
		del_min = prev_min - ahl->search_len;
		del_max = ahl->view_min - ahl->search_len;
		add_min = prev_max;
		add_max = ahl->view_max;
	}
	else if (prev_min > ahl->view_min && prev_max > ahl->view_max)
	{
		add_min = ahl->view_min - ahl->search_len;
		add_max = prev_min - ahl->search_len;
		del_min = ahl->view_max;
		del_max = prev_max;
	}
	else /* just refresh the lot */
	{
		del_min = 0;
		del_max = priv->cpl * priv->lines;
		add_min = ahl->view_min;
		add_max = ahl->view_max;
	}

	add_min = MAX(add_min, 0);
	del_min = MAX(del_min, 0);

	cur = ahl->highlights;
	while (delete && cur)
	{
		if (cur->start >= del_min && cur->start <= del_max)
		{
			GtkHex_Highlight *next = cur->next;
			gtk_hex_delete_highlight(gh, ahl, cur);
			cur = next;
		}
		else cur = cur->next;
	}
	while (add &&
		   gtk_hex_find_limited(gh, ahl->search_string, ahl->search_len,
								MAX(add_min, foundpos+1), add_max, &foundpos))
	{
		gtk_hex_insert_highlight(gh, ahl, foundpos, foundpos+(ahl->search_len)-1);
	}
}

static void gtk_hex_update_all_auto_highlights(GtkHex *gh, gboolean delete, gboolean add)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkHex_AutoHighlight *cur = priv->auto_highlight;
	while (cur)
	{
		gtk_hex_update_auto_highlight(gh, cur, delete, add);
		cur = cur->next;
	}
}

void gtk_hex_copy_to_clipboard(GtkHex *gh)
{
	g_signal_emit_by_name(G_OBJECT(gh), "copy_clipboard");
}

void gtk_hex_cut_to_clipboard(GtkHex *gh)
{
	g_signal_emit_by_name(G_OBJECT(gh), "cut_clipboard");
}

void gtk_hex_paste_from_clipboard(GtkHex *gh)
{
	g_signal_emit_by_name(G_OBJECT(gh), "paste_clipboard");
}

static void gtk_hex_real_copy_to_clipboard(GtkHex *gh)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	gint start_pos; 
	gint end_pos;
	GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));

	start_pos = MIN(priv->selection.start, priv->selection.end);
	end_pos = MAX(priv->selection.start, priv->selection.end);
 
	if(start_pos != end_pos) {
		guchar *text = hex_document_get_data(priv->document, start_pos,
											 end_pos - start_pos);
		gtk_clipboard_set_text(klass->clipboard, (gchar *) text, end_pos - start_pos);
		g_free(text);
	}
}

static void gtk_hex_real_cut_to_clipboard(GtkHex *gh)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	if(priv->selection.start != -1 && priv->selection.end != -1) {
		gtk_hex_real_copy_to_clipboard(gh);
		gtk_hex_delete_selection(gh);
	}
}

static void gtk_hex_real_paste_from_clipboard(GtkHex *gh)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkHexClass *klass = GTK_HEX_CLASS(GTK_WIDGET_GET_CLASS(gh));
	gchar *text;

	text = gtk_clipboard_wait_for_text(klass->clipboard);
	if(text) {
		hex_document_set_data(priv->document, priv->cursor_pos,
							  strlen(text), 0, (guchar *) text, TRUE);
		gtk_hex_set_cursor(gh, priv->cursor_pos + strlen(text));
		g_free(text);
	}
}

static void gtk_hex_finalize(GObject *o) {
	GtkHex *gh = GTK_HEX(o);
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	
	if (priv->disp_buffer)
		g_free (priv->disp_buffer);

	if (priv->disp_font_metrics)
		pango_font_metrics_unref (priv->disp_font_metrics);

	if (priv->font_desc)
		pango_font_description_free (priv->font_desc);

	if (priv->xlayout)
		g_object_unref (G_OBJECT (priv->xlayout));
	
	if (priv->alayout)
		g_object_unref (G_OBJECT (priv->alayout));
	
	if (priv->olayout)
		g_object_unref (G_OBJECT (priv->olayout));
	
	/* Changes for Gnome 2.0 -- SnM */	
	if (G_OBJECT_CLASS (gtk_hex_parent_class)->finalize)
		(* G_OBJECT_CLASS (gtk_hex_parent_class)->finalize)(G_OBJECT(o));
}

static gboolean gtk_hex_key_press(GtkWidget *w, GdkEventKey *event) {
	GtkHex *gh = GTK_HEX(w);
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	gint ret = TRUE;

	hide_cursor(gh);

	if(!(event->state & GDK_SHIFT_MASK)) {
		priv->selecting = FALSE;
	}
	else {
		priv->selecting = TRUE;
	}
	switch(event->keyval) {
	case GDK_KEY_BackSpace:
		if(priv->cursor_pos > 0) {
			hex_document_set_data(priv->document, priv->cursor_pos - 1,
								  0, 1, NULL, TRUE);
			if (priv->selecting)
				priv->selecting = FALSE;
			gtk_hex_set_cursor(gh, priv->cursor_pos - 1);
		}
		break;
	case GDK_KEY_Tab:
	case GDK_KEY_KP_Tab:
		if (priv->active_view == VIEW_ASCII) {
			priv->active_view = VIEW_HEX;
		}
		else {
			priv->active_view = VIEW_ASCII;
		}
		break;
	case GDK_KEY_Delete:
		if(priv->cursor_pos < hex_document_get_file_size (priv->document)) {
			hex_document_set_data(priv->document, priv->cursor_pos,
								  0, 1, NULL, TRUE);
			gtk_hex_set_cursor(gh, priv->cursor_pos);
		}
		break;
	case GDK_KEY_Up:
		gtk_hex_set_cursor(gh, priv->cursor_pos - priv->cpl);
		break;
	case GDK_KEY_Down:
		gtk_hex_set_cursor(gh, priv->cursor_pos + priv->cpl);
		break;
	case GDK_KEY_Page_Up:
		gtk_hex_set_cursor(gh, MAX(0, (gint)priv->cursor_pos - priv->vis_lines*priv->cpl));
		break;
	case GDK_KEY_Page_Down:
		gtk_hex_set_cursor(gh, MIN((gint)hex_document_get_file_size (priv->document), (gint)priv->cursor_pos + priv->vis_lines*priv->cpl));
		break;
	default:
		if (event->state & GDK_MOD1_MASK) {
			show_cursor(gh);
			return FALSE;
		}
		if(priv->active_view == VIEW_HEX)
			switch(event->keyval) {
			case GDK_KEY_Left:
				if(!(event->state & GDK_SHIFT_MASK)) {
					priv->lower_nibble = !priv->lower_nibble;
					if(priv->lower_nibble)
						gtk_hex_set_cursor(gh, priv->cursor_pos - 1);
				}
				else {
					gtk_hex_set_cursor(gh, priv->cursor_pos - 1);
				}
				break;
			case GDK_KEY_Right:
				if(priv->cursor_pos >= hex_document_get_file_size (priv->document))
					break;
				if(!(event->state & GDK_SHIFT_MASK)) {
					priv->lower_nibble = !priv->lower_nibble;
					if(!priv->lower_nibble)
						gtk_hex_set_cursor(gh, priv->cursor_pos + 1);
				}
				else {
					gtk_hex_set_cursor(gh, priv->cursor_pos + 1);
				}
				break;
			default:
				if(event->length != 1)
					ret = FALSE;
				else if((event->keyval >= '0')&&(event->keyval <= '9')) {
					hex_document_set_nibble(priv->document, event->keyval - '0',
											priv->cursor_pos, priv->lower_nibble,
											priv->insert, TRUE);
					if (priv->selecting)
						priv->selecting = FALSE;
					priv->lower_nibble = !priv->lower_nibble;
					if(!priv->lower_nibble)
						gtk_hex_set_cursor(gh, priv->cursor_pos + 1);
				}
				else if((event->keyval >= 'A')&&(event->keyval <= 'F')) {
					hex_document_set_nibble(priv->document, event->keyval - 'A' + 10,
											priv->cursor_pos, priv->lower_nibble,
											priv->insert, TRUE);
					if (priv->selecting)
						priv->selecting = FALSE;
					priv->lower_nibble = !priv->lower_nibble;
					if(!priv->lower_nibble)
						gtk_hex_set_cursor(gh, priv->cursor_pos + 1);
				}
				else if((event->keyval >= 'a')&&(event->keyval <= 'f')) {
					hex_document_set_nibble(priv->document, event->keyval - 'a' + 10,
											priv->cursor_pos, priv->lower_nibble,
											priv->insert, TRUE);
					if (priv->selecting)
						priv->selecting = FALSE;
					priv->lower_nibble = !priv->lower_nibble;
					if(!priv->lower_nibble)
						gtk_hex_set_cursor(gh, priv->cursor_pos + 1);
				}
				else if((event->keyval >= GDK_KEY_KP_0)&&(event->keyval <= GDK_KEY_KP_9)) {
					hex_document_set_nibble(priv->document, event->keyval - GDK_KEY_KP_0,
											priv->cursor_pos, priv->lower_nibble,
											priv->insert, TRUE);
					if (priv->selecting)
						priv->selecting = FALSE;
					priv->lower_nibble = !priv->lower_nibble;
					if(!priv->lower_nibble)
						gtk_hex_set_cursor(gh, priv->cursor_pos + 1);
				}
				else
					ret = FALSE;

				break;      
			}
		else if(priv->active_view == VIEW_ASCII)
			switch(event->keyval) {
			case GDK_KEY_Left:
				gtk_hex_set_cursor(gh, priv->cursor_pos - 1);
				break;
			case GDK_KEY_Right:
				gtk_hex_set_cursor(gh, priv->cursor_pos + 1);
				break;
			default:
				if(event->length != 1)
					ret = FALSE;
				else if(is_displayable(event->keyval)) {
					hex_document_set_byte(priv->document, event->keyval,
										  priv->cursor_pos, priv->insert, TRUE);
					if (priv->selecting)
						priv->selecting = FALSE;
					gtk_hex_set_cursor(gh, priv->cursor_pos + 1);
				}
				else if((event->keyval >= GDK_KEY_KP_0)&&(event->keyval <= GDK_KEY_KP_9)) {
					hex_document_set_byte(priv->document, event->keyval - GDK_KEY_KP_0 + '0',
											priv->cursor_pos, priv->insert, TRUE);
					if (priv->selecting)
						priv->selecting = FALSE;
					gtk_hex_set_cursor(gh, priv->cursor_pos + 1);
				}
				else
					ret = FALSE;

				break;
			}
		break;
	}

	show_cursor(gh);
	
	return ret;
}

static gboolean gtk_hex_key_release(GtkWidget *w, GdkEventKey *event) {
	GtkHex *gh = GTK_HEX(w);
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);

	if (event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R) {
		priv->selecting = FALSE;
	}

	return TRUE;
}

static gboolean gtk_hex_button_release(GtkWidget *w, GdkEventButton *event) {
	GtkHex *gh = GTK_HEX(w);
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);

	if(event->state & GDK_SHIFT_MASK) {
		priv->selecting = FALSE;
	}

	return TRUE;
}

/* 
 * recalculate the width of both displays and reposition and resize all
 * the children widgets and adjust the scrollbar after resizing
 * connects to the size_allocate signal of the GtkHex widget
 */
static void gtk_hex_size_allocate(GtkWidget *w, GtkAllocation *alloc) {
	GtkHex *gh;
	GtkAllocation my_alloc;
	GtkBorder padding;
	GtkStateFlags state;
	GtkStyleContext *context;
	gint border_width;
	gint sb_width;

	gh = GTK_HEX(w);
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	hide_cursor(gh);
	
	recalc_displays(gh, alloc->width, alloc->height);

	gtk_widget_set_allocation(w, alloc);
	if(gtk_widget_get_realized(w))
		gdk_window_move_resize (gtk_widget_get_window(w),
								alloc->x, 
								alloc->y,
								alloc->width, 
								alloc->height);

	border_width = gtk_container_get_border_width(GTK_CONTAINER(w));

	context = gtk_widget_get_style_context (w);
	state = gtk_widget_get_state_flags (w);
	gtk_style_context_get_padding (context, state, &padding);

	my_alloc.x = border_width + padding.left;
	my_alloc.y = border_width + padding.top;
	my_alloc.height = MAX (alloc->height - 2 * border_width - padding.top - padding.bottom, 1);
	if(priv->show_offsets) {
		my_alloc.width = 8*priv->char_width;
		gtk_widget_size_allocate(priv->offsets, &my_alloc);
		gtk_widget_queue_draw(priv->offsets);
		my_alloc.x += padding.left + padding.right + my_alloc.width + priv->extra_width/2;
	}

	gtk_widget_get_preferred_width (priv->scrollbar, NULL, &sb_width);

	my_alloc.width = priv->xdisp_width;
	gtk_widget_size_allocate(priv->xdisp, &my_alloc);
	my_alloc.x = alloc->width - border_width - sb_width;
	my_alloc.y = border_width;
	my_alloc.width = sb_width;
	my_alloc.height = MAX(alloc->height - 2*border_width, 1);
	gtk_widget_size_allocate(priv->scrollbar, &my_alloc);
	my_alloc.x -= priv->adisp_width + padding.left;
	my_alloc.y = border_width + padding.top;
	my_alloc.width = priv->adisp_width;
	my_alloc.height = MAX (alloc->height - 2 * border_width - padding.top - padding.bottom, 1);
	gtk_widget_size_allocate(priv->adisp, &my_alloc);
	
	show_cursor(gh);
}

static gboolean
gtk_hex_draw (GtkWidget *w,
              cairo_t *cr)
{
	if (GTK_WIDGET_CLASS (gtk_hex_parent_class)->draw)
		(* GTK_WIDGET_CLASS (gtk_hex_parent_class)->draw) (w, cr);

	draw_shadow (w, cr);

	return TRUE;
}

static void gtk_hex_document_changed(HexDocument* doc, gpointer change_data,
        gboolean push_undo, gpointer data)
{
    gtk_hex_real_data_changed (GTK_HEX(data), change_data);
}


static void gtk_hex_size_request(GtkWidget *w, GtkRequisition *req) {
	GtkBorder padding;
	GtkHex *gh = GTK_HEX(w);
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkStateFlags state;
	GtkStyleContext *context;
	gint sb_width;

	context = gtk_widget_get_style_context (w);
	state = gtk_widget_get_state_flags (w);
	gtk_style_context_get_padding (context, state, &padding);

	gtk_widget_get_preferred_width (priv->scrollbar, NULL, &sb_width);
	req->width = 2 * padding.left + 2 * padding.right + 2 * gtk_container_get_border_width (GTK_CONTAINER (w)) +
		sb_width + priv->char_width * (priv->default_cpl + (priv->default_cpl - 1) /
										 priv->group_type);
	if(priv->show_offsets)
		req->width += padding.left + padding.right + 8 * priv->char_width;
	req->height = priv->default_lines * priv->char_height + padding.top + padding.bottom +
		2*gtk_container_get_border_width(GTK_CONTAINER(w));
}

static void
gtk_hex_get_preferred_width (GtkWidget *widget,
                             gint      *minimal_width,
                             gint      *natural_width)
{
    GtkRequisition requisition;

    gtk_hex_size_request (widget, &requisition);

    *minimal_width = *natural_width = requisition.width;
}

static void
gtk_hex_get_preferred_height (GtkWidget *widget,
                              gint      *minimal_height,
                              gint      *natural_height)
{
    GtkRequisition requisition;

    gtk_hex_size_request (widget, &requisition);

    *minimal_height = *natural_height = requisition.height;
}

static void
gtk_hex_set_property (GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
	GtkHex *data = GTK_HEX (object);
	GtkHexPrivate *priv = gtk_hex_get_instance_private (data);

	switch (property_id) {
		case PROP_DOCUMENT:
			priv->document = g_value_get_object (value);
			g_signal_connect (G_OBJECT (priv->document), "document_changed",
							  G_CALLBACK (gtk_hex_document_changed), data);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
gtk_hex_get_property (GObject    *object,
                      guint       property_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
	GtkHex *data = GTK_HEX (object);

	switch (property_id) {
		case PROP_DOCUMENT:
			g_value_set_object (value, gtk_hex_get_document (data));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}


static void
gtk_hex_class_init (GtkHexClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->set_property = gtk_hex_set_property;
	object_class->get_property = gtk_hex_get_property;
	object_class->finalize = gtk_hex_finalize;

	pspecs[PROP_DOCUMENT] =
		g_param_spec_object ("document", "Document", NULL, HEX_TYPE_DOCUMENT,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, LAST_PROP, pspecs);

	gtkhex_signals[CURSOR_MOVED_SIGNAL] =
		g_signal_new ("cursor_moved",
					  G_OBJECT_CLASS_TYPE (object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (GtkHexClass, cursor_moved),
					  NULL, NULL, NULL, G_TYPE_NONE, 0);

	gtkhex_signals[DATA_CHANGED_SIGNAL] = 
		g_signal_new ("data_changed",
					  G_OBJECT_CLASS_TYPE (object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (GtkHexClass, data_changed),
					  NULL, NULL, NULL, G_TYPE_NONE, 1,
					  G_TYPE_POINTER);

	gtkhex_signals[CUT_CLIPBOARD_SIGNAL] = 
		g_signal_new ("cut_clipboard",
					  G_OBJECT_CLASS_TYPE (object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (GtkHexClass, cut_clipboard),
					  NULL, NULL, NULL, G_TYPE_NONE, 0);

	gtkhex_signals[COPY_CLIPBOARD_SIGNAL] = 
		g_signal_new ("copy_clipboard",
					  G_OBJECT_CLASS_TYPE (object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (GtkHexClass, copy_clipboard),
					  NULL, NULL, NULL, G_TYPE_NONE, 0);


	gtkhex_signals[PASTE_CLIPBOARD_SIGNAL] = 
		g_signal_new ("paste_clipboard",
					  G_OBJECT_CLASS_TYPE (object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (GtkHexClass, paste_clipboard),
					  NULL, NULL, NULL, G_TYPE_NONE, 0);
	
	klass->cursor_moved = NULL;
	klass->data_changed = gtk_hex_real_data_changed;
	klass->cut_clipboard = gtk_hex_real_cut_to_clipboard;
	klass->copy_clipboard = gtk_hex_real_copy_to_clipboard;
	klass->paste_clipboard = gtk_hex_real_paste_from_clipboard;

	klass->primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	klass->clipboard = gtk_clipboard_get(GDK_NONE);

	widget_class->size_allocate = gtk_hex_size_allocate;
	widget_class->get_preferred_width = gtk_hex_get_preferred_width;
	widget_class->get_preferred_height = gtk_hex_get_preferred_height;
	widget_class->draw = gtk_hex_draw;
	widget_class->key_press_event = gtk_hex_key_press;
	widget_class->key_release_event = gtk_hex_key_release;
	widget_class->button_release_event = gtk_hex_button_release;

	gtk_widget_class_set_css_name (widget_class, "hex-editor");
}

static void
gtk_hex_init (GtkHex *gh)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkCssProvider *provider;
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (GTK_WIDGET (gh));

	priv->disp_buffer = NULL;
	priv->default_cpl = DEFAULT_CPL;
	priv->default_lines = DEFAULT_LINES;

	priv->scroll_timeout = -1;

	priv->document = NULL;
	priv->starting_offset = 0;

	priv->xdisp_width = priv->adisp_width = 200;
	priv->extra_width = 0;
	priv->active_view = VIEW_HEX;
	priv->group_type = GROUP_BYTE;
	priv->lines = priv->vis_lines = priv->top_line = priv->cpl = 0;
	priv->cursor_pos = 0;
	priv->lower_nibble = FALSE;
	priv->cursor_shown = FALSE;
	priv->button = 0;
	priv->insert = FALSE; /* default to overwrite mode */
	priv->selecting = FALSE;

	priv->selection.start = priv->selection.end = 0;
	priv->selection.bg_color = NULL;
	priv->selection.min_select = 1;
	priv->selection.next = priv->selection.prev = NULL;
	priv->selection.valid = FALSE;

	priv->auto_highlight = NULL;

	/* get ourselves a decent monospaced font for rendering text */
	priv->disp_font_metrics = gtk_hex_load_font (DEFAULT_FONT);
	priv->font_desc = pango_font_description_from_string (DEFAULT_FONT);
	priv->font_provider = gtk_css_provider_new ();
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (priv->font_provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);


	priv->char_width = get_max_char_width(gh, priv->disp_font_metrics);
	priv->char_height = PANGO_PIXELS (pango_font_metrics_get_ascent (priv->disp_font_metrics)) + PANGO_PIXELS (pango_font_metrics_get_descent (priv->disp_font_metrics)) + 2;

	
	gtk_widget_set_can_focus(GTK_WIDGET(gh), TRUE);
	gtk_widget_set_events(GTK_WIDGET(gh), GDK_KEY_PRESS_MASK);
	gtk_container_set_border_width(GTK_CONTAINER(gh), DISPLAY_BORDER);
	
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
	                                 "hex-editor {\n"
	                                 "   border-style: solid;\n"
	                                 "   border-width: 1px;\n"
	                                 "   padding: 1px;\n"
	                                 "}\n", -1, NULL);
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);


	priv->adj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
	priv->xdisp = gtk_drawing_area_new();

	/* Create the pango layout for the widget */
	priv->xlayout = gtk_widget_create_pango_layout (priv->xdisp, "");

	gtk_widget_set_events (priv->xdisp, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
						   GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK | GDK_SCROLL_MASK);
	g_signal_connect(G_OBJECT(priv->xdisp), "draw",
					 G_CALLBACK(hex_draw), gh);
	g_signal_connect(G_OBJECT(priv->xdisp), "button_press_event",
					 G_CALLBACK(hex_button_cb), gh);
	g_signal_connect(G_OBJECT(priv->xdisp), "button_release_event",
					 G_CALLBACK(hex_button_cb), gh);
	g_signal_connect(G_OBJECT(priv->xdisp), "motion_notify_event",
					 G_CALLBACK(hex_motion_cb), gh);
	g_signal_connect(G_OBJECT(priv->xdisp), "scroll_event",
					 G_CALLBACK(hex_scroll_cb), gh);

	context = gtk_widget_get_style_context (GTK_WIDGET (priv->xdisp));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);

	gtk_fixed_put(GTK_FIXED(gh), priv->xdisp, 0, 0);
	gtk_widget_show(priv->xdisp);
	
	priv->adisp = gtk_drawing_area_new();

	/* Create the pango layout for the widget */
	priv->alayout = gtk_widget_create_pango_layout (priv->adisp, "");

	gtk_widget_set_events (priv->adisp, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
						   GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK | GDK_SCROLL_MASK);
	g_signal_connect(G_OBJECT(priv->adisp), "draw",
					 G_CALLBACK(ascii_draw), gh);
	g_signal_connect(G_OBJECT(priv->adisp), "button_press_event",
					 G_CALLBACK(ascii_button_cb), gh);
	g_signal_connect(G_OBJECT(priv->adisp), "button_release_event",
					 G_CALLBACK(ascii_button_cb), gh);
	g_signal_connect(G_OBJECT(priv->adisp), "motion_notify_event",
					 G_CALLBACK(ascii_motion_cb), gh);
	g_signal_connect(G_OBJECT(priv->adisp), "scroll_event",
					 G_CALLBACK(ascii_scroll_cb), gh);

	context = gtk_widget_get_style_context (GTK_WIDGET (priv->adisp));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);

	gtk_fixed_put(GTK_FIXED(gh), priv->adisp, 0, 0);
	gtk_widget_show(priv->adisp);
	
	g_signal_connect(G_OBJECT(priv->adj), "value_changed",
					 G_CALLBACK(display_scrolled), gh);

	priv->scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, priv->adj);
	gtk_fixed_put(GTK_FIXED(gh), priv->scrollbar, 0, 0);
	gtk_widget_show(priv->scrollbar);
}

GtkWidget *
gtk_hex_new (HexDocument *owner)
{
	return g_object_new (GTK_TYPE_HEX,
						 "document", owner,
						 NULL);
}


/*-------- public API starts here --------*/


/*
 * moves cursor to UPPER_NIBBLE or LOWER_NIBBLE of the current byte
 */
void gtk_hex_set_nibble(GtkHex *gh, gint lower_nibble) {
	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);

	if(priv->selecting) {
		bytes_changed(gh, priv->cursor_pos, priv->cursor_pos);
		priv->lower_nibble = lower_nibble;
	}
	else if(priv->selection.end != priv->selection.start) {
		guint start = MIN(priv->selection.start, priv->selection.end);
		guint end = MAX(priv->selection.start, priv->selection.end);
		priv->selection.end = priv->selection.start = 0;
		bytes_changed(gh, start, end);
		priv->lower_nibble = lower_nibble;
	}
	else {
		hide_cursor(gh);
		priv->lower_nibble = lower_nibble;
		show_cursor(gh);
	}
}

/*
 * moves cursor to byte index
 */
void gtk_hex_set_cursor(GtkHex *gh, gint index) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	guint y;
	guint old_pos = priv->cursor_pos;

	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	if((index >= 0) && (index <= hex_document_get_file_size (priv->document))) {
		if(!priv->insert && index == hex_document_get_file_size (priv->document))
			index--;

		index = MAX(index, 0);

		hide_cursor(gh);
		
		priv->cursor_pos = index;

		if(priv->cpl == 0)
			return;
		
		y = index / priv->cpl;
		if(y >= priv->top_line + priv->vis_lines) {
			gtk_adjustment_set_value(priv->adj, MIN(y - priv->vis_lines + 1, priv->lines - priv->vis_lines));
			gtk_adjustment_set_value(priv->adj, MAX(gtk_adjustment_get_value(priv->adj), 0));
			g_signal_emit_by_name(G_OBJECT(priv->adj), "value_changed");
		}
		else if (y < priv->top_line) {
			gtk_adjustment_set_value(priv->adj, y);
			g_signal_emit_by_name(G_OBJECT(priv->adj), "value_changed");
		}      

		if(index == hex_document_get_file_size (priv->document))
			priv->lower_nibble = FALSE;
		
		if(priv->selecting) {
			gtk_hex_set_selection(gh, priv->selection.start, priv->cursor_pos);
			bytes_changed(gh, MIN(priv->cursor_pos, old_pos), MAX(priv->cursor_pos, old_pos));
		}
		else {
			guint start = MIN(priv->selection.start, priv->selection.end);
			guint end = MAX(priv->selection.start, priv->selection.end);
			bytes_changed(gh, start, end);
			priv->selection.end = priv->selection.start = priv->cursor_pos;
		}

		g_signal_emit_by_name(G_OBJECT(gh), "cursor_moved");

		bytes_changed(gh, old_pos, old_pos);
		show_cursor(gh);
	}
}

/*
 * moves cursor to column x in line y (in the whole buffer, not just the currently visible part)
 */
void gtk_hex_set_cursor_xy(GtkHex *gh, gint x, gint y) {
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	gint cp;
	guint old_pos = priv->cursor_pos;

	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	cp = y*priv->cpl + x;

	if((y >= 0) && (y < priv->lines) && (x >= 0) &&
	   (x < priv->cpl) && (cp <= hex_document_get_file_size (priv->document))) {
		if(!priv->insert && cp == hex_document_get_file_size (priv->document))
			cp--;

		cp = MAX(cp, 0);

		hide_cursor(gh);

		priv->cursor_pos = cp;

		if(y >= priv->top_line + priv->vis_lines) {
			gtk_adjustment_set_value(priv->adj, MIN(y - priv->vis_lines + 1, priv->lines - priv->vis_lines));
			gtk_adjustment_set_value(priv->adj, MAX(0, gtk_adjustment_get_value(priv->adj)));
			g_signal_emit_by_name(G_OBJECT(priv->adj), "value_changed");
		}
		else if (y < priv->top_line) {
			gtk_adjustment_set_value(priv->adj, y);
			g_signal_emit_by_name(G_OBJECT(priv->adj), "value_changed");
		}      
		
		g_signal_emit_by_name(G_OBJECT(gh), "cursor_moved");

		if(priv->selecting) {
			gtk_hex_set_selection(gh, priv->selection.start, priv->cursor_pos);
			bytes_changed(gh, MIN(priv->cursor_pos, old_pos), MAX(priv->cursor_pos, old_pos));
		}
		else if(priv->selection.end != priv->selection.start) {
			guint start = MIN(priv->selection.start, priv->selection.end);
			guint end = MAX(priv->selection.start, priv->selection.end);
			priv->selection.end = priv->selection.start = 0;
			bytes_changed(gh, start, end);
		}
		bytes_changed(gh, old_pos, old_pos);
		show_cursor(gh);
	}
}

/*
 * returns cursor position
 */
guint gtk_hex_get_cursor(GtkHex *gh) {
	g_return_val_if_fail(gh != NULL, -1);
	g_return_val_if_fail(GTK_IS_HEX(gh), -1);

	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);

	return priv->cursor_pos;
}

/*
 * returns value of the byte at position offset
 */
guchar gtk_hex_get_byte(GtkHex *gh, guint offset) {
	g_return_val_if_fail(gh != NULL, 0);
	g_return_val_if_fail(GTK_IS_HEX(gh), 0);

	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);

	if((offset >= 0) && (offset < hex_document_get_file_size (priv->document)))
		return hex_document_get_byte(priv->document, offset);

	return 0;
}

/*
 * sets data group type (see GROUP_* defines in gtkhex.h)
 */
void gtk_hex_set_group_type(GtkHex *gh, guint gt) {
	GtkAllocation allocation;

	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);

	hide_cursor(gh);
	priv->group_type = gt;
	gtk_widget_get_allocation(GTK_WIDGET(gh), &allocation);
	recalc_displays(gh, allocation.width, allocation.height);
	gtk_widget_queue_resize(GTK_WIDGET(gh));
	show_cursor(gh);
}

/*
 * sets font for displaying data
 */
void gtk_hex_set_font(GtkHex *gh, PangoFontMetrics *font_metrics, const PangoFontDescription *font_desc) {
	GtkAllocation    allocation;
	const gchar     *font;
	gint             size;

	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);

	if (priv->disp_font_metrics)
		pango_font_metrics_unref (priv->disp_font_metrics);

	if (priv->font_desc)
		pango_font_description_free (priv->font_desc);

	priv->disp_font_metrics = pango_font_metrics_ref (font_metrics);
	priv->font_desc = pango_font_description_copy (font_desc);

	font = pango_font_description_get_family (priv->font_desc);
	if (pango_font_description_get_size_is_absolute (priv->font_desc))
		size = pango_font_description_get_size (priv->font_desc);
	else
		size = pango_font_description_get_size (priv->font_desc) / PANGO_SCALE;

	gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (priv->font_provider),
	                                 g_strdup_printf (FONT_CSS, font, size), -1, NULL);

	priv->char_width = get_max_char_width(gh, priv->disp_font_metrics);
	priv->char_height = PANGO_PIXELS (pango_font_metrics_get_ascent (priv->disp_font_metrics)) + PANGO_PIXELS (pango_font_metrics_get_descent (priv->disp_font_metrics)) + 2;
	gtk_widget_get_allocation(GTK_WIDGET(gh), &allocation);
	recalc_displays(gh, allocation.width, allocation.height);
	
	redraw_widget(GTK_WIDGET(gh));
}

/*
 *  do we show the offsets of lines?
 */
void gtk_hex_show_offsets(GtkHex *gh, gboolean show)
{
	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);

	if(priv->show_offsets == show)
		return;

	priv->show_offsets = show;
	if(show)
		show_offsets_widget(gh);
	else
		hide_offsets_widget(gh);
}

void gtk_hex_set_starting_offset(GtkHex *gh, gint starting_offset)
{
	g_return_if_fail (gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	priv->starting_offset = starting_offset;
}

void gtk_hex_set_insert_mode(GtkHex *gh, gboolean insert)
{
	g_return_if_fail(gh != NULL);
	g_return_if_fail(GTK_IS_HEX(gh));

	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	priv->insert = insert;

	if(!priv->insert && priv->cursor_pos > 0) {
		if(priv->cursor_pos >= hex_document_get_file_size (priv->document))
			priv->cursor_pos = hex_document_get_file_size (priv->document) - 1;
	}
}

PangoFontMetrics* gtk_hex_load_font (const char *font_name)
{
	PangoContext *context;
	PangoFont *new_font;
	PangoFontDescription *new_desc;
	PangoFontMetrics *new_metrics;

	new_desc = pango_font_description_from_string (font_name);

	context = gdk_pango_context_get();

	/* FIXME - Should get the locale language here */
	pango_context_set_language (context, gtk_get_default_language());

	new_font = pango_context_load_font (context, new_desc);

	new_metrics = pango_font_get_metrics (new_font, pango_context_get_language (context));

	pango_font_description_free (new_desc);
	g_object_unref (G_OBJECT (context));
	g_object_unref (G_OBJECT (new_font));

	return new_metrics;
}

GtkHex_AutoHighlight *gtk_hex_insert_autohighlight(GtkHex *gh,
												   const gchar *search,
												   gint len,
												   const gchar *colour)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
	GtkHex_AutoHighlight *new = g_malloc0(sizeof(GtkHex_AutoHighlight));

	new->search_string = g_memdup(search, len);
	new->search_len = len;

	new->colour = g_strdup(colour);

	new->highlights = NULL;

	new->next = priv->auto_highlight;
	new->prev = NULL;
	if (new->next) new->next->prev = new;
	priv->auto_highlight = new;

	new->view_min = 0;
	new->view_max = 0;

	gtk_hex_update_auto_highlight(gh, new, FALSE, TRUE);

	return new;
}

void gtk_hex_delete_autohighlight(GtkHex *gh, GtkHex_AutoHighlight *ahl)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);

	g_free(ahl->search_string);
	g_free(ahl->colour);

	while (ahl->highlights)
	{
		gtk_hex_delete_highlight(gh, ahl, ahl->highlights);
	}

	if (ahl->next) ahl->next->prev = ahl->prev;
	if (ahl->prev) ahl->prev->next = ahl->next;

	if (priv->auto_highlight == ahl) priv->auto_highlight = ahl->next;

	g_free(ahl);
}

void gtk_hex_set_geometry(GtkHex *gh, gint cpl, gint vis_lines)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (gh);
    priv->default_cpl = cpl;
    priv->default_lines = vis_lines;
}

HexDocument *
gtk_hex_get_document (GtkHex *self)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (self);
	return priv->document;
}

guint
gtk_hex_get_cursor_pos (GtkHex *self)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (self);
	return priv->cursor_pos;
}

guint
gtk_hex_get_group_type (GtkHex *self)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (self);
	return priv->group_type;
}

gboolean
gtk_hex_get_insert (GtkHex *self)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (self);
	return priv->insert;
}

gint
gtk_hex_get_cpl (GtkHex *self)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (self);
	return priv->cpl;
}

gint
gtk_hex_get_visible_lines (GtkHex *self)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (self);
	return priv->vis_lines;
}

gint
gtk_hex_get_active_view (GtkHex *self)
{
	GtkHexPrivate *priv = gtk_hex_get_instance_private (self);
	return priv->active_view;
}

void add_atk_namedesc (GtkWidget *widget, const gchar *name, const gchar *desc)
{
        AtkObject *atk_widget;

        g_return_if_fail (GTK_IS_WIDGET (widget));
        atk_widget = gtk_widget_get_accessible (widget);

        if (name)
                atk_object_set_name (atk_widget, name);
        if (desc)
                atk_object_set_description (atk_widget, desc);
}

void add_atk_relation (GtkWidget *obj1, GtkWidget *obj2, AtkRelationType type)
{

        AtkObject *atk_obj1, *atk_obj2;
        AtkRelationSet *relation_set;
        AtkRelation *relation;

        g_return_if_fail (GTK_IS_WIDGET (obj1));
        g_return_if_fail (GTK_IS_WIDGET (obj2));

        atk_obj1 = gtk_widget_get_accessible (obj1);
        atk_obj2 = gtk_widget_get_accessible (obj2);

        relation_set = atk_object_ref_relation_set (atk_obj1);
        relation = atk_relation_new (&atk_obj2, 1, type);
        atk_relation_set_add (relation_set, relation);
        g_object_unref (G_OBJECT (relation));

}

gboolean
gtk_hex_autohighlight_get_colour (GtkHex_AutoHighlight *hlight,
								  GdkRGBA              *colour)
{
	return gdk_rgba_parse (colour, hlight->colour);
}

