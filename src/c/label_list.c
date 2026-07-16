#include "label_list.h"
#include "task_list.h"
#include "data.h"
#include "config.h"
#include "i18n.h"
#include "theme.h"
#include "header_bar.h"

static Window    *s_window = NULL;
static MenuLayer *s_menu = NULL;
static Layer     *s_header = NULL;
static Layer     *s_refresh_bar = NULL;   // thin accent line shown while refreshing
static bool       s_shown = false;

// True when the list should show its rows: loaded, or refreshing while cached
// labels are still present (stale-while-revalidate — avoids a loading flash).
static bool list_ready(void) {
  int s = data_load_state();
  return s == LOAD_OK || (s == LOAD_LOADING && data_label_count() > 0);
}

// --- MenuLayer callbacks -----------------------------------------------------

static uint16_t get_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  if (!list_ready()) return 1;                 // status row
  int n = data_label_count();
  return n > 0 ? n : 1;                        // 1 = empty-state row
}

static int16_t get_cell_height(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  return 44;
}

static void draw_text_row(GContext *ctx, const Layer *cell, const char *title, const char *sub) {
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(8, 0, b.size.w - 12, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  if (sub && sub[0]) {
    graphics_context_set_text_color(ctx, hl ? GColorBlack : GColorDarkGray);
    graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(8, 26, b.size.w - 12, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void draw_status_row(GContext *ctx, const Layer *cell) {
  const char *title, *sub;
  switch (data_load_state()) {
    case LOAD_LOADING:      title = i18n(STR_LOADING_T); sub = i18n(STR_LOADING_S); break;
    case LOAD_ERROR:        title = i18n(STR_ERROR_T);   sub = i18n(STR_ERROR_S);   break;
    case LOAD_UNCONFIGURED: title = i18n(STR_UNCONF_T);  sub = i18n(STR_UNCONF_S);  break;
    default:                title = i18n(STR_NO_LABELS);  sub = "";                 break;
  }
  draw_text_row(ctx, cell, title, sub);
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *ci, void *c) {
  if (!list_ready()) { draw_status_row(ctx, cell); return; }
  if (data_label_count() == 0) { draw_status_row(ctx, cell); return; }
  Label *l = data_label(ci->row);
  if (l) draw_text_row(ctx, cell, l->name, "");
}

static void select_click(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  if (!list_ready()) {
    if (data_load_state() == LOAD_ERROR) {
      data_set_load_state(LOAD_LOADING);
      config_request_labels();
      label_list_reload();
    }
    return;
  }
  if (data_label_count() == 0) return;
  Label *l = data_label(ci->row);
  if (l && l->name[0]) {
    char list_id[LIST_ID_LEN];
    snprintf(list_id, sizeof(list_id), "label:%s", l->name);
    task_list_push(list_id, l->name, TASK_LIST_LABEL);
  }
}

// --- Refresh indicator -------------------------------------------------------

static void refresh_bar_update(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, theme_accent());
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void update_refresh_bar(void) {
  if (!s_refresh_bar) { return; }
  bool refreshing = (data_load_state() == LOAD_LOADING && data_label_count() > 0);
  layer_set_hidden(s_refresh_bar, !refreshing);
}

// --- Window ------------------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  int top = HEADER_BAR_HEIGHT;
  s_menu = menu_layer_create(GRect(0, top, b.size.w, b.size.h - top));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_num_rows,
    .get_cell_height = get_cell_height,
    .draw_row = draw_row,
    .select_click = select_click,
  });
  menu_layer_set_normal_colors(s_menu, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu, theme_accent(), GColorBlack);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));

  s_refresh_bar = layer_create(GRect(0, top, b.size.w, 3));
  layer_set_update_proc(s_refresh_bar, refresh_bar_update);
  layer_set_hidden(s_refresh_bar, true);
  layer_add_child(root, s_refresh_bar);

  s_header = header_bar_create(b.size.w);
  layer_add_child(root, s_header);
}

static void window_unload(Window *window) {
  if (s_menu) { menu_layer_destroy(s_menu); s_menu = NULL; }
  if (s_refresh_bar) { layer_destroy(s_refresh_bar); s_refresh_bar = NULL; }
  if (s_header) { layer_destroy(s_header); s_header = NULL; }
  window_destroy(s_window);
  s_window = NULL;
}

static void window_appear(Window *window)    { s_shown = true; header_bar_set_active(s_header, i18n(STR_LABELS), HEADER_COUNT_NONE); update_refresh_bar(); }
static void window_disappear(Window *window) { s_shown = false; }

void label_list_push(void) {
  data_clear_labels();
  data_set_load_state(LOAD_LOADING);
  config_request_labels();

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

void label_list_reload(void) {
  if (s_menu) menu_layer_reload_data(s_menu);
  update_refresh_bar();
}

bool label_list_is_shown(void) { return s_shown; }

void label_list_destroy(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}
