#include "task_detail.h"
#include "config.h"
#include "data.h"
#include "i18n.h"
#include "theme.h"
#include <pebble.h>

static Window      *s_window = NULL;
static ScrollLayer *s_scroll = NULL;
static TextLayer   *s_title  = NULL;
static TextLayer   *s_due    = NULL;
static TextLayer   *s_desc   = NULL;
static Layer       *s_badges = NULL;
static bool         s_shown  = false;

static char s_title_buf[TASK_TITLE_LEN];
static char s_due_buf[TASK_DUE_LEN + 24];   // "<Due label>: " + due text

// --- Label badges ------------------------------------------------------------
// Draws (or, when draw=false, only measures) the labels as rounded "pill" badges,
// laid out left-to-right at (ox,oy) within `width`, wrapping onto new lines.
// Returns the total height the badges occupy.

#define BADGE_HPAD   6
#define BADGE_GAP    4
#define BADGE_H      20

static int render_badges(GContext *ctx, int ox, int oy, int width, bool draw) {
  const char *src = data_detail_labels();
  if (!src || !src[0] || width <= 0) return 0;

  char buf[DETAIL_LABELS_LEN];
  strncpy(buf, src, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  int x = 0, y = 0;
  bool any = false;

  // Split on commas manually (strtok is unreliable on the watch's libc).
  char *p = buf;
  while (*p) {
    while (*p == ' ') p++;                 // trim leading spaces
    char *start = p;
    while (*p && *p != ',') p++;
    char *end = p;                          // at ',' or '\0'
    bool more = (*p == ',');
    while (end > start && end[-1] == ' ') end--;  // trim trailing spaces
    if (end > start) {
      char saved = *end;
      *end = '\0';
      any = true;

      GSize ts = graphics_text_layout_get_content_size(
          start, font, GRect(0, 0, width, 100),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      int pill_w = ts.w + 2 * BADGE_HPAD;
      if (pill_w > width) pill_w = width;

      if (x > 0 && x + pill_w > width) { x = 0; y += BADGE_H + BADGE_GAP; }

      if (draw) {
        GRect pill = GRect(ox + x, oy + y, pill_w, BADGE_H);
        graphics_context_set_fill_color(ctx, theme_accent());
        graphics_fill_rect(ctx, pill, 4, GCornersAll);
        graphics_context_set_text_color(ctx, GColorBlack);
        graphics_draw_text(ctx, start, font,
                           GRect(pill.origin.x + BADGE_HPAD, pill.origin.y + 1,
                                 pill_w - 2 * BADGE_HPAD, 16),
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      }
      x += pill_w + BADGE_GAP;
      *end = saved;
    }
    if (more) p++;
    else break;
  }
  return any ? y + BADGE_H : 0;
}

static void badges_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  render_badges(ctx, 0, 0, b.size.w, true);
}

// --- Layout ------------------------------------------------------------------

static void clear_layers(void) {
  if (s_title)  { text_layer_destroy(s_title);   s_title  = NULL; }
  if (s_due)    { text_layer_destroy(s_due);     s_due    = NULL; }
  if (s_desc)   { text_layer_destroy(s_desc);    s_desc   = NULL; }
  if (s_badges) { layer_destroy(s_badges);       s_badges = NULL; }
  if (s_scroll) { scroll_layer_destroy(s_scroll); s_scroll = NULL; }
}

static void build(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  const int pad = 6;
  const int w = bounds.size.w - pad * 2;

  s_scroll = scroll_layer_create(bounds);
  scroll_layer_set_click_config_onto_window(s_scroll, window);

  // Title (word-wrapped).
  GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GSize ts = graphics_text_layout_get_content_size(
      s_title_buf, title_font, GRect(0, 0, w, 2000),
      GTextOverflowModeWordWrap, GTextAlignmentLeft);
  int title_h = ts.h + 6;

  s_title = text_layer_create(GRect(pad, pad, w, title_h));
  text_layer_set_font(s_title, title_font);
  text_layer_set_background_color(s_title, GColorClear);
  text_layer_set_text_color(s_title, GColorBlack);
  text_layer_set_overflow_mode(s_title, GTextOverflowModeWordWrap);
  text_layer_set_text(s_title, s_title_buf);
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_title));

  int y = pad + title_h + 8;

  // Due (accent), if any.
  if (s_due_buf[0]) {
    GFont due_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
    GSize ds = graphics_text_layout_get_content_size(
        s_due_buf, due_font, GRect(0, 0, w, 2000),
        GTextOverflowModeWordWrap, GTextAlignmentLeft);
    int due_h = ds.h + 4;
    s_due = text_layer_create(GRect(pad, y, w, due_h));
    text_layer_set_font(s_due, due_font);
    text_layer_set_background_color(s_due, GColorClear);
    text_layer_set_text_color(s_due, theme_accent());
    text_layer_set_overflow_mode(s_due, GTextOverflowModeWordWrap);
    text_layer_set_text(s_due, s_due_buf);
    scroll_layer_add_child(s_scroll, text_layer_get_layer(s_due));
    y += due_h + 6;
  }

  // Description (GOTHIC_24), once the phone's detail reply has arrived.
  const char *desc = data_detail_desc();
  if (data_detail_ready() && desc[0]) {
    GFont desc_font = fonts_get_system_font(FONT_KEY_GOTHIC_24);
    GSize es = graphics_text_layout_get_content_size(
        desc, desc_font, GRect(0, 0, w, 4000),
        GTextOverflowModeWordWrap, GTextAlignmentLeft);
    int desc_h = es.h + 4;
    s_desc = text_layer_create(GRect(pad, y, w, desc_h));
    text_layer_set_font(s_desc, desc_font);
    text_layer_set_background_color(s_desc, GColorClear);
    text_layer_set_text_color(s_desc, GColorBlack);
    text_layer_set_overflow_mode(s_desc, GTextOverflowModeWordWrap);
    text_layer_set_text(s_desc, desc);   // points at data.c's stable static buffer
    scroll_layer_add_child(s_scroll, text_layer_get_layer(s_desc));
    y += desc_h + 8;
  }

  // Label badges.
  int badges_h = render_badges(NULL, 0, 0, w, false);
  if (badges_h > 0) {
    s_badges = layer_create(GRect(pad, y, w, badges_h));
    layer_set_update_proc(s_badges, badges_update);
    scroll_layer_add_child(s_scroll, s_badges);
    y += badges_h;
  }

  scroll_layer_set_content_size(s_scroll, GSize(bounds.size.w, y + pad));
  layer_add_child(root, scroll_layer_get_layer(s_scroll));
}

static void window_load(Window *window) {
  build(window);
}

static void window_unload(Window *window) {
  clear_layers();
  window_destroy(s_window);
  s_window = NULL;
}

static void window_appear(Window *window)    { s_shown = true; }
static void window_disappear(Window *window) { s_shown = false; }

void task_detail_push(const Task *t) {
  if (!t) { return; }
  strncpy(s_title_buf, t->title, sizeof(s_title_buf) - 1);
  s_title_buf[sizeof(s_title_buf) - 1] = '\0';
  if (t->due[0]) {
    snprintf(s_due_buf, sizeof(s_due_buf), "%s: %s", i18n(STR_DUE), t->due);
  } else {
    s_due_buf[0] = '\0';
  }

  // Fetch the description + labels; the reply rebuilds the window (task_detail_reload).
  data_clear_task_detail();
  config_request_task_detail(t->id);

  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
    .appear = window_appear,
    .disappear = window_disappear,
  });
  window_stack_push(s_window, true);
}

void task_detail_reload(void) {
  if (!s_shown || !s_window) { return; }
  clear_layers();
  build(s_window);
  layer_mark_dirty(window_get_root_layer(s_window));
}

bool task_detail_is_shown(void) { return s_shown; }
