/*
 * gtkhex.c - a GtkHex widget
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */

#include <gdk/gdkkeysyms.h>

#include "gtkhex.h"

#define DISPLAY_BORDER 4
#define DISPLAY_SPACING 2
#define SHADOW_WIDTH 2

#define DEFAULT_FONT "-*-courier-medium-r-normal--12-*-*-*-*-*-*-*"

#define TOP_SCROLL_BORDER 2
#define BOTTOM_SCROLL_BORDER 2

#define is_printable(c) (((c>=0x20) && (c<=0xFF))?1:0)

#define DEBUG YES!

typedef void (*DataChangedSignal)(GtkObject *, guint, guint, gpointer);

enum {
  CURSOR_MOVED_SIGNAL,
  DATA_CHANGED_SIGNAL,
  LAST_SIGNAL
};

static gint gtkhex_signals[LAST_SIGNAL] = { 0, 0 };

static void render_hex_lines(GtkHex *, gint, gint);
static void render_ascii_lines(GtkHex *, gint, gint);

static void gtk_hex_marshaller (GtkObject *object, GtkSignalFunc func,
				gpointer func_data, GtkArg *args) {
  DataChangedSignal rfunc;
  
  rfunc = (DataChangedSignal) func;
  
  (* rfunc)(object, GTK_VALUE_INT (args[0]), GTK_VALUE_INT (args[1]), func_data);
}

/*
 * simply forces widget w to redraw itself
 */
static void redraw_widget(GtkWidget *w) {
  GdkRectangle rect;

  rect.x = rect.y = 0;
  rect.width = w->allocation.width;
  rect.height = w->allocation.height;

  gtk_widget_draw(w, &rect);
}

/*
 * ?_to_pointer translates mouse coordinates in hex/ascii view
 * to cursor coordinates.
 */
static void hex_to_pointer(GtkHex *gh, guint mx, guint my) {
  guint cx, cy, x;

  if(my < TOP_SCROLL_BORDER)
    cy = MAX(0, gh->top_line - 1);
  else if(gh->xdisp->allocation.height - my < BOTTOM_SCROLL_BORDER)
    cy = MIN(gh->lines, gh->top_line + gh->vis_lines) + 1;
  else
    cy = gh->top_line + my/gh->char_height;
  
  cx = 0; x = 0;
  while(cx < 2*gh->cpl) {
    x += gh->char_width;
    
    if(x > mx) {
      gtk_hex_set_cursor_xy(gh, cx/2, cy);
      gtk_hex_set_nibble(gh, ((cx%2 == 0)?UPPER_NIBBLE:LOWER_NIBBLE));

      cx = 2*gh->cpl;
    }
    
    cx++;
    
    if(cx%(2*gh->group_type) == 0)
      x += gh->char_width;
  }
}

static void ascii_to_pointer(GtkHex *gh, gint mx, gint my) {
  int cy;

  if(my < TOP_SCROLL_BORDER)
    cy = MAX(0, gh->top_line - 1);
  else if(gh->xdisp->allocation.height - my < BOTTOM_SCROLL_BORDER)
    cy = MIN(gh->lines, gh->top_line + gh->vis_lines + 1);
  else
    cy = gh->top_line + my/gh->char_height;

  gtk_hex_set_cursor_xy(gh, mx/gh->char_width, cy);
}

static guint get_max_char_width(GdkFont *font) {
  /* this is, I guess, a rather dirty trick, but
     right now I cant think of anything better */
  guint i;
  guint w, maxwidth = 0;

  for(i = '0'; i <= 'z'; i++) {
    w = gdk_char_width(font, (gchar)i);
    maxwidth = MAX(maxwidth, w);
  }

  return maxwidth;
}

static void format_xbyte(GtkHex *gh, gint pos, gchar buf[2]) {
  /* actually does a sprintf(buf, "%x", gh->buffer[pos]) */
  guint low, high;

  low = gh->buffer[pos] & 0x0F;
  high = (gh->buffer[pos] & 0xF0) >> 4;

  buf[0] = ((high < 10)?(high + '0'):(high - 10 + 'A'));
  buf[1] = ((low < 10)?(low + '0'):(low - 10 + 'A'));
}

/*
 * format_[x|a]block() formats contents of the buffer
 * into displayable text in hex or ascii, respectively
 */
static gint format_xblock(GtkHex *gh, gchar *out, guint start, guint end) {
  int i, j, low, high;

  for(i = start + 1, j = 0; i <= end; i++) {
    low = gh->buffer[i-1] & 0x0F;
    high = (gh->buffer[i-1] & 0xF0) >> 4;

    out[j++] = ((high < 10)?(high + '0'):(high - 10 + 'A'));
    out[j++] = ((low < 10)?(low + '0'):(low - 10 + 'A'));

    if(i%gh->group_type == 0)
      out[j++] = ' ';
  }

  return j;
}

static gint format_ablock(GtkHex *gh, gchar *out, guint start, guint end) {
  int i, j;

  for(i = start, j = 0; i < end; i++, j++) {
    if(is_printable(gh->buffer[i]))
      out[j] = gh->buffer[i];
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
  gint cx, cy, spaces;

  cy = pos / gh->cpl;
  cy -= gh->top_line;
  if(cy < 0)
    return FALSE;

  cx = 2*(pos % gh->cpl);
  spaces = (pos % gh->cpl) / gh->group_type;

  cx *= gh->char_width;
  cy *= gh->char_height;
  spaces *= gh->char_width;
  *x = cx + spaces;
  *y = cy;

  return TRUE;
}

static gint get_acoords(GtkHex *gh, gint pos, gint *x, gint *y) {
  gint cx, cy;

  cy = pos / gh->cpl;
  cy -= gh->top_line;
  if(cy < 0)
    return FALSE;
  cy *= gh->char_height;

  cx = gh->char_width*(pos % gh->cpl);

  *x = cx;
  *y = cy;

  return TRUE;
}

/*
 * the cursor rendering stuff...
 */
static void render_ac(GtkHex *gh) {
  gint cx, cy;

  if(gh->adisp_gc == NULL) {
    gh->adisp_gc = gdk_gc_new(gh->adisp->window);
    gdk_gc_set_exposures(gh->adisp_gc, TRUE);
  }

  gdk_gc_set_function(gh->adisp_gc, GDK_INVERT);
  
  if(get_acoords(gh, gh->cursor_pos, &cx, &cy))
    gdk_draw_rectangle(gh->adisp->window, gh->adisp_gc, (gh->active_view == VIEW_ASCII),
		       cx, cy, gh->char_width, gh->char_height - 1);

  gdk_gc_set_function(gh->adisp_gc, GDK_COPY);
}

static void render_xc(GtkHex *gh) {
  gint cx, cy;

  if(gh->xdisp_gc == NULL) {
    gh->xdisp_gc = gdk_gc_new(gh->xdisp->window);
    gdk_gc_set_exposures(gh->xdisp_gc, TRUE);
  }

  gdk_gc_set_function(gh->xdisp_gc, GDK_INVERT);

  if(get_xcoords(gh, gh->cursor_pos, &cx, &cy)) {
    if(gh->lower_nibble)
      cx += gh->char_width;
    gdk_draw_rectangle(gh->xdisp->window, gh->xdisp_gc, (gh->active_view == VIEW_HEX),
		       cx, cy, gh->char_width, gh->char_height - 1);
  }

  gdk_gc_set_function(gh->xdisp_gc, GDK_COPY);
}

static void show_cursor(GtkHex *gh) {
  if(!gh->cursor_shown) {
    render_xc(gh);
    render_ac(gh);
  }
  gh->cursor_shown = TRUE;
}

static void hide_cursor(GtkHex *gh) {
  if(gh->cursor_shown) {
    render_xc(gh);
    render_ac(gh);
  }
  gh->cursor_shown = FALSE;
}

/*
 * renders a byte at offset pos in both displays
 */ 

static void render_byte(GtkHex *gh, gint pos) {
  gint cx, cy;
  gchar buf[2];

  get_xcoords(gh, pos, &cx, &cy);
  format_xbyte(gh, pos, buf);

  gdk_gc_set_foreground(gh->xdisp_gc, &GTK_WIDGET(gh)->style->base[GTK_STATE_NORMAL]);
  gdk_draw_rectangle(gh->xdisp->window, gh->xdisp_gc, TRUE,
		     cx, cy, 2*gh->char_width, gh->char_height);
  gdk_gc_set_foreground(gh->xdisp_gc, &GTK_WIDGET(gh)->style->text[GTK_STATE_NORMAL]);

  /*  gdk_window_clear_area (gh->xdisp->window,
      cx, cy,
      2*gh->char_width, gh->char_height); */

  gdk_draw_text(gh->xdisp->window, gh->disp_font, gh->xdisp_gc,
		cx, cy + gh->disp_font->ascent, buf, 2);

  get_acoords(gh, pos, &cx, &cy);

  gdk_gc_set_foreground(gh->adisp_gc, &GTK_WIDGET(gh)->style->base[GTK_STATE_NORMAL]);
  gdk_draw_rectangle(gh->adisp->window, gh->adisp_gc, TRUE,
		     cx, cy, gh->char_width, gh->char_height);
  gdk_gc_set_foreground(gh->adisp_gc, &GTK_WIDGET(gh)->style->text[GTK_STATE_NORMAL]);

  /*  gdk_window_clear_area (gh->adisp->window,
      cx, cy,
      gh->char_width, gh->char_height); */

  gdk_draw_text(gh->adisp->window, gh->disp_font, gh->adisp_gc,
		cx, cy + gh->disp_font->ascent, &gh->buffer[pos], 1);
}


/*
 * when calling render_*_lines() the imin and imax arguments are the
 * numbers of the first and last line TO BE DISPLAYES in the range
 * [0 .. gh->vis_lines-1] AND NOT [0 .. gh->lines]!
 */
static void render_hex_lines(GtkHex *gh, gint imin, gint imax) {
  GtkWidget *w = gh->xdisp;
  gint i, cursor_line = gh->cursor_pos / gh->cpl - gh->top_line;
  gint xcpl = gh->cpl*2 + gh->cpl/gh->group_type;
  gint frm_len, tmp;

  imax = MIN(imax, gh->vis_lines);

  /* gdk_window_clear_area (w->window, 0, imin*gh->char_height,
                            w->allocation.width,
			    (imax - imin + 1)*gh->char_height); */

  gdk_gc_set_foreground(gh->xdisp_gc, &GTK_WIDGET(gh)->style->base[GTK_STATE_NORMAL]);
  gdk_draw_rectangle(gh->xdisp->window, gh->xdisp_gc, TRUE,
		     0, imin*gh->char_height, w->allocation.width,
		     (imax - imin + 1)*gh->char_height);
  gdk_gc_set_foreground(gh->xdisp_gc, &GTK_WIDGET(gh)->style->text[GTK_STATE_NORMAL]);

  frm_len = format_xblock(gh, gh->disp_buffer, (gh->top_line+imin)*gh->cpl,
			  MIN((gh->top_line+imax+1)*gh->cpl, gh->buffer_size) );

  for(i = imin; i <= imax; i++) {
    tmp = (gint)frm_len - (gint)((i - imin)*xcpl);
    if(tmp <= 0)
      return;
    gdk_draw_text(w->window, gh->disp_font, gh->xdisp_gc,
		  0, gh->disp_font->ascent + i*gh->char_height,
		  gh->disp_buffer + (i - imin)*xcpl, 
		  MIN(xcpl, tmp));
  }

  if((cursor_line >= imin) && (cursor_line <= imax) && (gh->cursor_shown))
    render_xc(gh);
}

static void render_ascii_lines(GtkHex *gh, gint imin, gint imax) {
  GtkWidget *w = gh->adisp;
  gint i, tmp, frm_len;
  guint cx, cy, cursor_line = gh->cursor_pos / gh->cpl - gh->top_line;

  imax = MIN(imax, gh->vis_lines);

  /* gdk_window_clear_area (w->window, 0, imin*gh->char_height, w->allocation.width,
			 (imax - imin + 1)*gh->char_height); */

  gdk_gc_set_foreground(gh->xdisp_gc, &GTK_WIDGET(gh)->style->base[GTK_STATE_NORMAL]);
  gdk_draw_rectangle(w->window, gh->xdisp_gc, TRUE,
		     0, imin*gh->char_height, w->allocation.width,
		     (imax - imin + 1)*gh->char_height);
  gdk_gc_set_foreground(gh->xdisp_gc, &GTK_WIDGET(gh)->style->text[GTK_STATE_NORMAL]);

  frm_len = format_ablock(gh, gh->disp_buffer, (gh->top_line+imin)*gh->cpl,
			  MIN((gh->top_line+imax+1)*gh->cpl, gh->buffer_size) );

  for(i = imin; i <= imax; i++) {
    tmp = (gint)frm_len - (gint)((i - imin)*gh->cpl);
    if(tmp <= 0)
      return;
    gdk_draw_text(w->window, gh->disp_font, gh->adisp_gc,
		  0, gh->disp_font->ascent + i*gh->char_height,
		  gh->disp_buffer + (i - imin)*gh->cpl,
		  MIN(gh->cpl, tmp));
  }

  if((cursor_line >= imin) && (cursor_line <= imax) && (gh->cursor_shown))
    render_ac(gh);
}

/*
 * expose signal handlers for both displays
 */
static void hex_expose(GtkWidget *w, GdkEventExpose *event, GtkHex *gh) {
  gint imin, imax;

  imin = (event->area.y) / gh->char_height;
  if(imin > gh->vis_lines)
    return;
  imax = (event->area.y + event->area.height) / gh->char_height;
  if((event->area.y + event->area.height) % gh->char_height)
    imax++;

  render_hex_lines(gh, imin, imax);
}

static void ascii_expose(GtkWidget *w, GdkEventExpose *event, GtkHex *gh) {
  gint imin, imax;

  imin = (event->area.y) / gh->char_height;
  if(imin > gh->vis_lines)
    return;
  imax = (event->area.y + event->area.height) / gh->char_height;
  if((event->area.y + event->area.height) % gh->char_height)
    imax++;

  render_ascii_lines(gh, imin, imax);
}

/*
 * expose signal handler for the GtkHex itself: draws shadows around both displays
 */
static void draw_shadow(GtkWidget *widget, GdkEventExpose *event) {
  GtkHex *gh = GTK_HEX(widget);
  gint border = GTK_CONTAINER(widget)->border_width;

  gtk_draw_shadow(widget->style, widget->window,
		  GTK_STATE_NORMAL, GTK_SHADOW_IN,
		  border, border, gh->xdisp_width + 2*SHADOW_WIDTH,
		  widget->allocation.height - 2*border);

  gtk_draw_shadow(widget->style, widget->window,
		  GTK_STATE_NORMAL, GTK_SHADOW_IN,
		  border + gh->xdisp_width + 2*SHADOW_WIDTH + DISPLAY_SPACING, border,
		  gh->adisp_width + 2*SHADOW_WIDTH,
		  widget->allocation.height - 2*border);
}

/*
 * this calculates how many bytes we can stuff into one line and how many
 * lines we can display according to the current size of the widget
 */
static void recalc_displays(GtkHex *gh, guint width, guint height) {
  gint sep_width, total_width = width;
  gint total_cpl, xcpl;
  gint old_cpl = gh->cpl;

  total_width -= 2*DISPLAY_SPACING + 2*GTK_CONTAINER(gh)->border_width + 4*SHADOW_WIDTH +
                 gh->scrollbar->allocation.width + 6;
  /* the 6 at the end of the above line comes from experimenting. otherwise the code below
     thinks it has too much space to divide ;) */

  total_cpl = total_width / gh->char_width;

  if((total_cpl == 0) || (total_width < 0)) {
    gh->cpl = gh->lines = gh->vis_lines = 0;
    return;
  }

  /* calculate how many bytes we can stuff in one line */
  gh->cpl = 0;
  do {
    gh->cpl++;        /* just added one more char */
    total_cpl -= 3;   /* 2 for xdisp, 1 for adisp */

    if(gh->cpl % gh->group_type == 0) /* just ended a group */
      if(total_cpl < gh->group_type*3) {
	/* there is no more space for another group */
	total_cpl = 0;
      }
      else {
	total_cpl--;
      }
  } while(total_cpl > 0);

  gh->lines = gh->buffer_size / gh->cpl;
  if(gh->buffer_size % gh->cpl)
    gh->lines++;

  gh->vis_lines = (height - 2*GTK_CONTAINER(gh)->border_width - 2*SHADOW_WIDTH) / gh->char_height;
  gh->vis_lines = MIN(gh->vis_lines, gh->lines);
  
  gh->adisp_width = gh->cpl*gh->char_width + 1;
  xcpl = gh->cpl*2 + gh->cpl/gh->group_type;
  if(gh->cpl % gh->group_type == 0)
    xcpl--;
  gh->xdisp_width = xcpl*gh->char_width + 1;

  if(gh->disp_buffer)
    free(gh->disp_buffer);

  gh->disp_buffer = malloc(xcpl * (gh->vis_lines + 3));

  /* adjust the scrollbar and display position to
     new sizes */
  gh->adj->value = MIN(gh->top_line*old_cpl / gh->cpl, gh->lines - gh->vis_lines);
  if((gh->cursor_pos/gh->cpl < gh->adj->value) ||
     (gh->cursor_pos/gh->cpl > gh->adj->value + gh->vis_lines - 1))
    gh->adj->value = MIN(gh->cursor_pos/gh->cpl, gh->lines - gh->vis_lines);
  gh->adj->lower = 0;
  gh->adj->upper = gh->lines;
  gh->adj->step_increment = 1;
  gh->adj->page_increment = gh->vis_lines - 1;
  gh->adj->page_size = gh->vis_lines;

  gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "changed");
  gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "value_changed");
}

/* 
 * recalculate the with of both displays and reposition and resize all
 * the children widgetsand adjust the scrollbar after resizing
 * connects to the size_allocate signal of the GtkHex widget
 */
static void resize_displays(GtkWidget *w, GtkAllocation *alloc) {
  GtkHex *gh = GTK_HEX(w);
  GtkAllocation my_alloc;

  hide_cursor(gh);

  recalc_displays(gh, alloc->width, alloc->height);

  my_alloc.x = GTK_CONTAINER(gh)->border_width + SHADOW_WIDTH;
  my_alloc.y = GTK_CONTAINER(gh)->border_width + SHADOW_WIDTH;
  my_alloc.height = alloc->height - 2*GTK_CONTAINER(gh)->border_width - 2*SHADOW_WIDTH;
  my_alloc.width = gh->xdisp_width;
  gtk_signal_emit_by_name(GTK_OBJECT(gh->xdisp), "size_allocate", &my_alloc);
  /*  my_alloc.x += my_alloc.width + DISPLAY_SPACING;
      my_alloc.width = gh->separator->allocation.width;
      gtk_signal_emit_by_name(GTK_OBJECT(gh->separator), "size_allocate", &my_alloc); */
  my_alloc.x += my_alloc.width + 2*SHADOW_WIDTH + DISPLAY_SPACING;
  my_alloc.width = gh->adisp_width;
  gtk_signal_emit_by_name(GTK_OBJECT(gh->adisp), "size_allocate", &my_alloc);
  my_alloc.x += my_alloc.width + SHADOW_WIDTH + DISPLAY_SPACING;
  my_alloc.y = GTK_CONTAINER(gh)->border_width;
  my_alloc.width = gh->scrollbar->allocation.width;
  my_alloc.height = alloc->height - 2*GTK_CONTAINER(gh)->border_width;
  gtk_signal_emit_by_name(GTK_OBJECT(gh->scrollbar), "size_allocate", &my_alloc);

  show_cursor(gh);
}

/*
 * takes care of xdisp and adisp scrolling
 * connected to value_changed signal of scrollbar's GtkAdjustment
 * I cant really remember anymore, but I think it was mostly copied
 * from testgtk.c ;)
*/
static void display_scrolled(GtkAdjustment *adj, GtkHex *gh) {
  gint source_min = ((int)adj->value - gh->top_line) * gh->char_height;
  gint source_max = source_min + gh->xdisp->allocation.height;
  gint dest_min = 0;
  gint dest_max = gh->xdisp->allocation.height;

  GdkRectangle rect;
  GdkEvent *event;

  if((!GTK_WIDGET_REALIZED(gh)) || (!GTK_WIDGET_DRAWABLE(gh->xdisp)) ||
     (!GTK_WIDGET_DRAWABLE(gh->adisp))) {
    return;
  }

  if(gh->xdisp_gc == NULL) {
    gh->xdisp_gc = gdk_gc_new(gh->xdisp->window);
    gdk_gc_set_exposures(gh->xdisp_gc, TRUE);
  }
  if(gh->adisp_gc == NULL) {
    gh->adisp_gc = gdk_gc_new(gh->adisp->window);
    gdk_gc_set_exposures(gh->adisp_gc, TRUE);
  }

  hide_cursor(gh);

  gh->top_line = adj->value;

  if (source_min < 0) {
    rect.y = 0;
    rect.height = -source_min;
    if (rect.height > gh->xdisp->allocation.height)
      rect.height = gh->xdisp->allocation.height;

    source_min = 0;
    dest_min = rect.height;
  }
  else {
    rect.y = 2*gh->xdisp->allocation.height - source_max;
    if (rect.y < 0)
      rect.y = 0;
    rect.height = gh->xdisp->allocation.height - rect.y;
    
    source_max = gh->xdisp->allocation.height;
    dest_max = rect.y;
  }

  if (source_min != source_max) {
    gdk_draw_pixmap (gh->xdisp->window,
		     gh->xdisp_gc,
		     gh->xdisp->window,
		     0, source_min,
		     0, dest_min,
		     gh->xdisp->allocation.width,
		     source_max - source_min);
    gdk_draw_pixmap (gh->adisp->window,
		     gh->adisp_gc,
		     gh->adisp->window,
		     0, source_min,
		     0, dest_min,
		     gh->adisp->allocation.width,
		     source_max - source_min);
    
    while ((event = gdk_event_get_graphics_expose (gh->xdisp->window)) != NULL) {
      gtk_widget_event (gh->xdisp, event);
      if (event->expose.count == 0) {
	gdk_event_free (event);
	break;
      }
      gdk_event_free (event);
    }

    while ((event = gdk_event_get_graphics_expose (gh->adisp->window)) != NULL) {
      gtk_widget_event (gh->adisp, event);
      if (event->expose.count == 0) {
	gdk_event_free (event);
	break;
      }
      gdk_event_free (event);
    }
  }
  
  if (rect.height != 0) {
    gtk_widget_draw(gh->xdisp, &rect);
    gtk_widget_draw(gh->adisp, &rect);
  }

  show_cursor(gh);
}

/*
 * mouse signal handlers (button 1 and motion) for both displays
 */
static void hex_button_cb(GtkWidget *w, GdkEventButton *event, GtkHex *gh) {
  guint mx, my, cx, cy, x;

  if((event->type == GDK_BUTTON_PRESS) && (event->button == 1)) {
    if (!GTK_WIDGET_HAS_FOCUS (gh))
      gtk_widget_grab_focus (GTK_WIDGET(gh));

    gh->button = event->button;

    if(gh->active_view == VIEW_HEX)
      hex_to_pointer(gh, event->x, event->y);
    else {
      hide_cursor(gh);
      gh->active_view = VIEW_HEX;
      show_cursor(gh);
    }
  }
}

static void hex_motion_cb(GtkWidget *w, GdkEventMotion *event, GtkHex *gh) {
  if((gh->active_view == VIEW_HEX) && (gh->button == 1))
    hex_to_pointer(gh, event->x, event->y);
}

static void ascii_button_cb(GtkWidget *w, GdkEventButton *event, GtkHex *gh) {
  if((event->type == GDK_BUTTON_PRESS) && (event->button == 1)) {
    if (!GTK_WIDGET_HAS_FOCUS (gh))
      gtk_widget_grab_focus (GTK_WIDGET(gh));

    gh->button = event->button;
    if(gh->active_view == VIEW_ASCII)
      ascii_to_pointer(gh, event->x, event->y);
    else {
      hide_cursor(gh);
      gh->active_view = VIEW_ASCII;
      show_cursor(gh);
    }
  }
}

static void ascii_motion_cb(GtkWidget *w, GdkEventMotion *event, GtkHex *gh) {
  if((gh->active_view == VIEW_ASCII) && (gh->button == 1))
    ascii_to_pointer(gh, event->x, event->y);
}

/*
 * default data_changed signal handler
 */
static void gtk_hex_data_changed(GtkHex *gh, guint start, guint end) {
  gint start_line, end_line;

  if(gh->cpl == 0)
    return;

  start_line = start/gh->cpl - gh->top_line;
  end_line = end/gh->cpl - gh->top_line;
  start_line = MAX(start_line, 0);
  end_line = MIN(end_line, gh->vis_lines);

  render_hex_lines(gh, start_line, end_line);
  render_ascii_lines(gh, start_line, end_line);
}

static void gtk_hex_destroy(GtkObject *o) {
  GtkHex *gh = GTK_HEX(o);

  if(gh->disp_buffer)
    free(gh->disp_buffer);
}

static gint gtk_hex_key_press(GtkWidget *w, GdkEventKey *event) {
  GtkHex *gh = GTK_HEX(w);
  guint old_cp = gh->cursor_pos;

  hide_cursor(gh);

  switch(event->keyval) {
  case GDK_Up:
  case GDK_KP_8:
    gtk_hex_set_cursor(gh, gh->cursor_pos - gh->cpl);
    break;
  case GDK_Down:
  case GDK_KP_2:
    gtk_hex_set_cursor(gh, gh->cursor_pos + gh->cpl);
    break; 
  default:
    if(gh->active_view == VIEW_HEX)
      switch(event->keyval) {
      case GDK_Left:
      case GDK_KP_4:
	gh->lower_nibble = !gh->lower_nibble;
	if(gh->lower_nibble)
	  gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
	break;
      case GDK_Right:
      case GDK_KP_6:
	gh->lower_nibble = !gh->lower_nibble;
	if(!gh->lower_nibble)
	  gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
	break;
      default:
	if((event->keyval >= '0')&&(event->keyval <= '9')) {
	  gh->buffer[gh->cursor_pos] = (gh->buffer[gh->cursor_pos] &
					(0x0F << 4*gh->lower_nibble)) |
	    ((event->keyval - '0') << ((gh->lower_nibble)?0:4));
	  render_byte(gh, gh->cursor_pos);
	  gh->lower_nibble = !gh->lower_nibble;
	  if((!gh->lower_nibble) && (gh->cursor_pos < gh->buffer_size-1))
	    gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
	  gtk_signal_emit_by_name(GTK_OBJECT(gh), "data_changed", old_cp, old_cp);
	}
	if((event->keyval >= 'A')&&(event->keyval <= 'F')) {
	  gh->buffer[gh->cursor_pos] = (gh->buffer[gh->cursor_pos] &
					(0x0F << 4*gh->lower_nibble)) | 
	    ((event->keyval - 'A' + 10) << ((gh->lower_nibble)?0:4));
	  render_byte(gh, gh->cursor_pos);
	  gh->lower_nibble = !gh->lower_nibble;
	  if((!gh->lower_nibble) && (gh->cursor_pos < gh->buffer_size-1))
	    gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
	  gtk_signal_emit_by_name(GTK_OBJECT(gh), "data_changed", old_cp, old_cp);
	}
	if((event->keyval >= 'a')&&(event->keyval <= 'f')) {
	  gh->buffer[gh->cursor_pos] = (gh->buffer[gh->cursor_pos] &
					(0x0F << 4*gh->lower_nibble)) | 
	    ((event->keyval - 'a' + 10) << ((gh->lower_nibble)?0:4));
	  render_byte(gh, gh->cursor_pos);
	  gh->lower_nibble = !gh->lower_nibble;
	  if((!gh->lower_nibble) && (gh->cursor_pos < gh->buffer_size-1))
	    gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
	  gtk_signal_emit_by_name(GTK_OBJECT(gh), "data_changed", old_cp, old_cp);
	}
	break;      
      }
    else if(gh->active_view == VIEW_ASCII)
      switch(event->keyval) {
      case GDK_Left:
      case GDK_KP_4:
	gtk_hex_set_cursor(gh, gh->cursor_pos - 1);
	break;
      case GDK_Right:
      case GDK_KP_6:
	gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
	break;
      default:
	if(is_printable(event->keyval)) {
	  gh->buffer[gh->cursor_pos] = event->keyval;
	  render_byte(gh, gh->cursor_pos);
	  gtk_hex_set_cursor(gh, gh->cursor_pos + 1);
	}
	break;
      }
    break;
  }

  show_cursor(gh);

  return TRUE;
}

static void gtk_hex_class_init(GtkHexClass *class) {
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;
        
  gtkhex_signals[CURSOR_MOVED_SIGNAL] = gtk_signal_new ("cursor_moved",
							GTK_RUN_FIRST,
							object_class->type,
							GTK_SIGNAL_OFFSET (GtkHexClass, cursor_moved),
							gtk_signal_default_marshaller, GTK_TYPE_NONE, 0);
  gtkhex_signals[DATA_CHANGED_SIGNAL] = gtk_signal_new ("data_changed",
							GTK_RUN_FIRST,
							object_class->type,
							GTK_SIGNAL_OFFSET (GtkHexClass, data_changed),
							gtk_hex_marshaller, GTK_TYPE_NONE, 2,
							GTK_TYPE_INT, GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, gtkhex_signals, LAST_SIGNAL);

  class->cursor_moved = NULL;
  class->data_changed = gtk_hex_data_changed;

  GTK_WIDGET_CLASS(class)->key_press_event = gtk_hex_key_press;
  GTK_OBJECT_CLASS(class)->destroy = gtk_hex_destroy;
}

void gtk_hex_init(GtkHex *gh) {
  gh->buffer_size = 0;
  gh->buffer = NULL;
  gh->disp_buffer = NULL;

  gh->xdisp_width = gh->adisp_width = 200;
  gh->adisp_gc = gh->xdisp_gc = NULL;
  gh->active_view = VIEW_HEX;
  gh->group_type = GROUP_BYTE;
  gh->lines = gh->vis_lines = gh->top_line = gh->cpl = 0;
  gh->cursor_pos = 0;
  gh->lower_nibble = FALSE;
  gh->cursor_shown = FALSE;
  gh->button = 0;

  /* get ourselves a decent monospaced font for rendering text */
  gh->disp_font = gdk_font_load(DEFAULT_FONT);
  gh->char_width = get_max_char_width(gh->disp_font);
  gh->char_height = gh->disp_font->ascent + gh->disp_font->descent + 2;

  GTK_WIDGET_SET_FLAGS(gh, GTK_CAN_FOCUS);
  gtk_widget_set_events(GTK_WIDGET(gh), GDK_KEY_PRESS_MASK);
  gtk_container_border_width(GTK_CONTAINER(gh), DISPLAY_BORDER);
  gtk_signal_connect(GTK_OBJECT(gh), "expose_event",
		     GTK_SIGNAL_FUNC(draw_shadow), NULL);
  gtk_signal_connect(GTK_OBJECT(gh), "size_allocate",
		     GTK_SIGNAL_FUNC(resize_displays), NULL);

  gh->adj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

  gh->xdisp = gtk_drawing_area_new();
  gtk_widget_set_events (gh->xdisp, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
			            GDK_BUTTON_MOTION_MASK);
  gtk_signal_connect(GTK_OBJECT(gh->xdisp), "expose_event",
		     GTK_SIGNAL_FUNC(hex_expose), gh);
  gtk_signal_connect(GTK_OBJECT(gh->xdisp), "button_press_event",
		     GTK_SIGNAL_FUNC(hex_button_cb), gh);
  gtk_signal_connect(GTK_OBJECT(gh->xdisp), "motion_notify_event",
		     GTK_SIGNAL_FUNC(hex_motion_cb), gh);
  gtk_fixed_put(GTK_FIXED(gh), gh->xdisp, 0, 0);
  gtk_widget_show(gh->xdisp);

  gh->adisp = gtk_drawing_area_new();
  gtk_widget_set_events (gh->adisp, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
			            GDK_BUTTON_MOTION_MASK);
  gtk_signal_connect(GTK_OBJECT(gh->adisp), "expose_event",
		     GTK_SIGNAL_FUNC(ascii_expose), gh);
  gtk_signal_connect(GTK_OBJECT(gh->adisp), "button_press_event",
		     GTK_SIGNAL_FUNC(ascii_button_cb), gh);
  gtk_signal_connect(GTK_OBJECT(gh->adisp), "motion_notify_event",
		     GTK_SIGNAL_FUNC(ascii_motion_cb), gh);
  gtk_fixed_put(GTK_FIXED(gh), gh->adisp, 0, 0);
  gtk_widget_show(gh->adisp);

  gtk_signal_connect(GTK_OBJECT(gh->adj), "value_changed",
		     GTK_SIGNAL_FUNC(display_scrolled), gh);

  gh->scrollbar = gtk_vscrollbar_new(gh->adj);
  gtk_fixed_put(GTK_FIXED(gh), gh->scrollbar, 0, 0);
  gtk_widget_show(gh->scrollbar);
}

/*
 * "public" functions follow. only these should be used when
 * manipulating a GtkHex widget outside this module
 */

/*
 * moves cursor to UPPER_NIBBLE or LOWER_NIBBLE of the current byte
 */
void gtk_hex_set_nibble(GtkHex *gh, gint lower_nibble) {
  hide_cursor(gh);
  gh->lower_nibble = lower_nibble;
  show_cursor(gh);
}

/*
 * moves cursor to byte index
 */
void gtk_hex_set_cursor(GtkHex *gh, gint index) {
  guint y;

  if((index >= 0) && (index < gh->buffer_size)) {
    hide_cursor(gh);

    gh->cursor_pos = index;

    if(gh->cpl == 0)
      return;

    y = index / gh->cpl;
    if(y >= gh->top_line + gh->vis_lines) {
      gh->adj->value = MIN(y - gh->vis_lines + 1, gh->lines - gh->vis_lines);
      gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "value_changed");
    }
    else if (y < gh->top_line) {
      gh->adj->value = y;
      gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "value_changed");
    }      

    gtk_signal_emit_by_name(GTK_OBJECT(gh), "cursor_moved");

    show_cursor(gh);
  }
}

/*
 * moves cursor to column x in line y (in the whole buffer, not just the currently visible part)
 */
void gtk_hex_set_cursor_xy(GtkHex *gh, gint x, gint y) {
  gint cp = y*gh->cpl + x;
  if((y >= 0) && (y < gh->lines) && (x >= 0) && (x < gh->cpl) && (cp < gh->buffer_size)) {
    hide_cursor(gh);

    gh->cursor_pos = cp;

    if(y >= gh->top_line + gh->vis_lines) {
      gh->adj->value = MIN(y - gh->vis_lines + 1, gh->lines - gh->vis_lines);
      gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "value_changed");
    }
    else if (y < gh->top_line) {
      gh->adj->value = y;
      gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "value_changed");
    }      

    gtk_signal_emit_by_name(GTK_OBJECT(gh), "cursor_moved");

    show_cursor(gh);
  }
}

/*
 * returns cursor position
 */
gint gtk_hex_get_cursor(GtkHex *gh) {
  return gh->cursor_pos;
}

/*
 * returns value of the byte at position offset
 */
guchar gtk_hex_get_byte(GtkHex *gh, guint offset) {
  if((offset >= 0) && (offset < gh->buffer_size))
    return gh->buffer[offset];
}

/*
 * sets value of the byte at position offset to val
 */
void gtk_hex_set_byte(GtkHex *gh, guchar val, guint offset) {
  if((offset >= 0) && (offset < gh->buffer_size))
    gh->buffer[offset] = val;

  gtk_signal_emit_by_name(GTK_OBJECT(gh), "data_changed", offset, offset);
}

/*
 * copies len bytes from address data into buffer begining at position offset
 */
void gtk_hex_set_data(GtkHex *gh, guint offset, guint len, guchar *data) {
  guint i;

  for(i = 0; (offset < gh->buffer_size) && (i < len); offset++, i++)
    gh->buffer[offset] = data[i];

  gtk_signal_emit_by_name(GTK_OBJECT(gh), "data_changed", offset, offset + i - 1);
}

/*
 * sets data group type (see GROUP_* defines in gtkhex.h)
 */
void gtk_hex_set_group_type(GtkHex *gh, guint gt) {
  GtkAllocation alloc;

  memcpy(&alloc, &(GTK_WIDGET(gh)->allocation), sizeof(GtkAllocation));

  gh->group_type = gt;
  gtk_signal_emit_by_name(GTK_OBJECT(gh), "size_allocate", &alloc);
}

/*
 * replaces the whole buffer (note that the buffer (de)allocation is NOT handled by
 * the GtkHex widget!)
 */
void gtk_hex_set_buffer(GtkHex *gh, guchar *new_buf, guint new_size) {
  gh->buffer = new_buf;
  gh->buffer_size = new_size;

  if(!GTK_WIDGET_REALIZED(gh))
    return;

  gh->lines = gh->buffer_size / gh->cpl;
  if(gh->buffer_size % gh->cpl)
    gh->lines++;
  gh->vis_lines = MIN(gh->vis_lines, gh->lines);
  
  /* adjust the scrollbar and display position to
     new sizes */
  gh->adj->value = 0;
  gh->adj->lower = 0;
  gh->adj->upper = gh->lines;
  gh->adj->step_increment = 1;
  gh->adj->page_increment = gh->vis_lines - 1;
  gh->adj->page_size = gh->vis_lines;

  gtk_hex_set_cursor(gh, 0);

  gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "changed");
  gtk_signal_emit_by_name(GTK_OBJECT(gh->adj), "value_changed");

  gtk_signal_emit_by_name(GTK_OBJECT(gh), "data_changed", 0, new_size - 1);
}

/*
 * sets font for displaying data
 */
void gtk_hex_set_font(GtkHex *gh, GdkFont *font) {
  if(gh->disp_font)
    gdk_font_unref(gh->disp_font);

  gh->disp_font = gdk_font_ref(font);
  gh->char_width = get_max_char_width(gh->disp_font);
  gh->char_height = gh->disp_font->ascent + gh->disp_font->descent + 2;

  recalc_displays(gh, GTK_WIDGET(gh)->allocation.width, GTK_WIDGET(gh)->allocation.height);

  redraw_widget(GTK_WIDGET(gh));
}

guint gtk_hex_get_type() {
  static guint gh_type = 0;

  if(!gh_type) {
    GtkTypeInfo gh_info = {
      "GtkHex", sizeof(GtkHex), sizeof(GtkHexClass),
      (GtkClassInitFunc) gtk_hex_class_init,
      (GtkObjectInitFunc) gtk_hex_init,
      (GtkArgSetFunc) NULL,
      (GtkArgGetFunc) NULL,
    };

    gh_type = gtk_type_unique(gtk_fixed_get_type(), &gh_info);
  }

  return gh_type;
}

GtkWidget *gtk_hex_new() {
  return GTK_WIDGET(gtk_type_new(gtk_hex_get_type()));
}














