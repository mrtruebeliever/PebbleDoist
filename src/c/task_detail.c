#include "task_detail.h"
#include "i18n.h"
#include "theme.h"
#include <pebble.h>

static Window      *s_window = NULL;
static ScrollLayer *s_scroll = NULL;
static TextLayer   *s_title  = NULL;
static TextLayer   *s_due    = NULL;

static char s_title_buf[TASK_TITLE_LEN];
static char s_due_buf[TASK_DUE_LEN + 24];   // "<Due label>: " + due text

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  const int pad = 6;
  const int w = b.size.w - pad * 2;

  s_scroll = scroll_layer_create(b);
  scroll_layer_set_click_config_onto_window(s_scroll, window);

  // Title (word-wrapped); measure its height so scrolling fits long tasks.
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
    y += due_h;
  }

  scroll_layer_set_content_size(s_scroll, GSize(b.size.w, y + pad));
  layer_add_child(root, scroll_layer_get_layer(s_scroll));
}

static void window_unload(Window *window) {
  if (s_title)  { text_layer_destroy(s_title);   s_title  = NULL; }
  if (s_due)    { text_layer_destroy(s_due);     s_due    = NULL; }
  if (s_scroll) { scroll_layer_destroy(s_scroll); s_scroll = NULL; }
  window_destroy(s_window);
  s_window = NULL;
}

void task_detail_push(const Task *t) {
  if (!t) { return; }
  strncpy(s_title_buf, t->title, sizeof(s_title_buf) - 1);
  s_title_buf[sizeof(s_title_buf) - 1] = '\0';
  if (t->due[0]) {
    snprintf(s_due_buf, sizeof(s_due_buf), "%s: %s", i18n(STR_DUE), t->due);
  } else {
    s_due_buf[0] = '\0';
  }

  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
