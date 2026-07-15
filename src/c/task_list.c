#include "task_list.h"
#include "data.h"
#include "cache.h"
#include "config.h"
#include "i18n.h"
#include "theme.h"
#include "dictation_flow.h"
#include "task_detail.h"

static Window         *s_window = NULL;
static MenuLayer      *s_menu = NULL;
static StatusBarLayer *s_status_bar = NULL;
static Layer          *s_refresh_bar = NULL;   // thin accent line shown while refreshing
static bool            s_shown = false;

static char s_project_id[PROJ_ID_LEN];
static char s_title[PROJ_NAME_LEN];
static bool s_is_today;

static char s_sel_task_id[TASK_ID_LEN];   // task targeted by the open ActionMenu
static Task s_sel_task;                    // snapshot for the detail view

// Marquee state for the selected task row's title (see marquee_tick).
static AppTimer *s_marq_timer = NULL;
static int       s_marq_offset = 0;
static int       s_marq_pause  = 0;
#define MARQ_TICK_MS 60
#define MARQ_STEP    3
#define MARQ_PAUSE   14

// Number of non-task rows above the task rows ("new task" row for real projects).
static int add_rows(void) { return s_is_today ? 0 : 1; }

// True when the list should show its rows: loaded, or refreshing while cached
// rows are still present (stale-while-revalidate — avoids a loading flash).
static bool list_ready(void) {
  int s = data_load_state();
  return s == LOAD_OK || (s == LOAD_LOADING && data_task_count() > 0);
}

// --- Afvinken (close) ActionMenu ---------------------------------------------

static void close_performed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  if (s_sel_task_id[0]) {
    config_close_task(s_sel_task_id);
    data_set_load_state(LOAD_LOADING);
    task_list_reload();
  }
}

static void delete_performed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  if (s_sel_task_id[0]) {
    config_delete_task(s_sel_task_id);
    data_set_load_state(LOAD_LOADING);
    task_list_reload();
  }
}

static void detail_performed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  task_detail_push(&s_sel_task);
}

static void close_menu_did_close(ActionMenu *menu, const ActionMenuItem *performed, void *context) {
  action_menu_hierarchy_destroy((ActionMenuLevel *)context, NULL, NULL);
}

static void open_task_menu(Task *t) {
  s_sel_task = *t;   // snapshot for the detail view (survives a background refresh)
  strncpy(s_sel_task_id, t->id, sizeof(s_sel_task_id) - 1);
  s_sel_task_id[sizeof(s_sel_task_id) - 1] = '\0';

  ActionMenuLevel *root = action_menu_level_create(3);
  action_menu_level_add_action(root, i18n(STR_DETAILS), detail_performed, NULL);
  action_menu_level_add_action(root, i18n(STR_COMPLETE), close_performed, NULL);
  action_menu_level_add_action(root, i18n(STR_DELETE), delete_performed, NULL);
  ActionMenuConfig cfg = {
    .root_level = root,
    .colors = { .background = theme_accent(), .foreground = GColorBlack },
    .align = ActionMenuAlignCenter,
    .did_close = close_menu_did_close,
    .context = root,
  };
  action_menu_open(&cfg);
}

// --- Dictation add -----------------------------------------------------------

static void add_result(DictationResult result, const char *transcript) {
  if (result == DICTATION_RESULT_SUCCESS && transcript && transcript[0]) {
    config_add_task(s_project_id, transcript);
    data_set_load_state(LOAD_LOADING);
    data_clear_tasks();
    task_list_reload();
  }
}

// --- MenuLayer callbacks -----------------------------------------------------

static uint16_t get_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  if (!list_ready()) return 1;                     // status row
  int tc = data_task_count();
  if (tc > 0) return add_rows() + tc;
  return add_rows() + (s_is_today ? 1 : 0);        // empty state
}

static int16_t get_cell_height(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  return 44;
}

static int16_t get_header_height(MenuLayer *ml, uint16_t section, void *ctx) {
  return 26;
}

static void draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *c) {
  GRect b = layer_get_bounds(cell);
  graphics_context_set_fill_color(ctx, theme_accent());
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(8, -2, b.size.w - 12, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void draw_add_row(GContext *ctx, const Layer *cell) {
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  // Dark text on the red highlight reads better in low light than white.
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, i18n(STR_NEW_TASK), fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(8, 0, b.size.w - 12, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, hl ? GColorBlack : GColorDarkGray);
  graphics_draw_text(ctx, i18n(STR_DICTATE), fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(8, 26, b.size.w - 12, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void draw_task_row(GContext *ctx, const Layer *cell, Task *t) {
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  GFont f = fonts_get_system_font(FONT_KEY_GOTHIC_18);

  graphics_context_set_text_color(ctx, GColorBlack);
  if (hl) {
    // Selected row: marquee long titles horizontally (no ellipsis).
    graphics_draw_text(ctx, t->title, f, GRect(34 - s_marq_offset, 4, 4000, 36),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    // Re-paint the checkbox column so scrolled text never runs under it.
    graphics_context_set_fill_color(ctx, theme_accent());
    graphics_fill_rect(ctx, GRect(0, 0, 34, b.size.h), 0, GCornerNone);
  } else {
    graphics_draw_text(ctx, t->title, f, GRect(34, 4, b.size.w - 40, 36),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  // Empty checkbox, drawn last so it stays visible on the scrolling row.
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_round_rect(ctx, GRect(10, b.size.h / 2 - 8, 16, 16), 3);
}

static void draw_status_row(GContext *ctx, const Layer *cell) {
  GRect b = layer_get_bounds(cell);
  const char *title, *sub;
  if (data_load_state() != LOAD_OK) {
    switch (data_load_state()) {
      case LOAD_LOADING: title = i18n(STR_LOADING_T); sub = i18n(STR_LOADING_S); break;
      case LOAD_ERROR:   title = i18n(STR_ERROR_T);   sub = i18n(STR_ERROR_S);   break;
      default:           title = i18n(STR_UNCONF_T);  sub = i18n(STR_UNCONF_S);  break;
    }
  } else {
    title = s_is_today ? i18n(STR_NOTHING_TODAY) : i18n(STR_NO_TASKS);
    sub = "";
  }
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(8, 2, b.size.w - 12, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, GColorDarkGray);
  graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(8, 26, b.size.w - 12, 16),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *ci, void *c) {
  if (!list_ready()) { draw_status_row(ctx, cell); return; }
  if (!s_is_today && ci->row == 0) { draw_add_row(ctx, cell); return; }
  if (data_task_count() == 0) { draw_status_row(ctx, cell); return; }
  Task *t = data_task(ci->row - add_rows());
  if (t) draw_task_row(ctx, cell, t);
}

static void select_click(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  if (!list_ready()) {
    if (data_load_state() == LOAD_ERROR) {
      data_set_load_state(LOAD_LOADING);
      config_request_tasks(s_project_id);
      task_list_reload();
    }
    return;
  }
  if (!s_is_today && ci->row == 0) {
    dictation_flow_start(add_result);
    return;
  }
  if (data_task_count() == 0) return;
  Task *t = data_task(ci->row - add_rows());
  if (t && t->id[0]) open_task_menu(t);
}

// --- Marquee (selected task row) ---------------------------------------------

static void marquee_tick(void *ctx) {
  s_marq_timer = NULL;
  if (!s_shown || !s_menu) { return; }

  int prev = s_marq_offset;
  bool scrolled = false;
  if (list_ready() && data_task_count() > 0) {
    MenuIndex mi = menu_layer_get_selected_index(s_menu);
    int trow = mi.row - add_rows();
    if (trow >= 0 && trow < data_task_count()) {
      Task *t = data_task(trow);
      if (t && t->id[0]) {
        GRect fr = layer_get_frame(menu_layer_get_layer(s_menu));
        int avail = fr.size.w - 40;
        GSize ts = graphics_text_layout_get_content_size(
            t->title, fonts_get_system_font(FONT_KEY_GOTHIC_18),
            GRect(0, 0, 4000, 36), GTextOverflowModeFill, GTextAlignmentLeft);
        int maxoff = ts.w - avail;
        if (maxoff > 0) {
          scrolled = true;
          if (s_marq_pause > 0) {
            s_marq_pause--;
            if (s_marq_pause == 0 && s_marq_offset >= maxoff) {
              s_marq_offset = 0; s_marq_pause = MARQ_PAUSE;   // pause again at the start
            }
          } else {
            s_marq_offset += MARQ_STEP;
            if (s_marq_offset >= maxoff) { s_marq_offset = maxoff; s_marq_pause = MARQ_PAUSE; }
          }
        }
      }
    }
  }
  if (!scrolled) { s_marq_offset = 0; }
  if (s_marq_offset != prev) { layer_mark_dirty(menu_layer_get_layer(s_menu)); }
  s_marq_timer = app_timer_register(MARQ_TICK_MS, marquee_tick, NULL);
}

static void marquee_start(void) {
  s_marq_offset = 0;
  s_marq_pause = MARQ_PAUSE;   // brief pause before the first scroll
  if (!s_marq_timer) { s_marq_timer = app_timer_register(MARQ_TICK_MS, marquee_tick, NULL); }
}

static void marquee_stop(void) {
  if (s_marq_timer) { app_timer_cancel(s_marq_timer); s_marq_timer = NULL; }
  s_marq_offset = 0;
  s_marq_pause = 0;
}

static void selection_changed(MenuLayer *ml, MenuIndex now, MenuIndex old, void *ctx) {
  s_marq_offset = 0;
  s_marq_pause = MARQ_PAUSE;
}

// --- Refresh indicator -------------------------------------------------------

static void refresh_bar_update(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, theme_accent());
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

// Shows the thin accent line only while a background refresh runs over cached rows.
static void update_refresh_bar(void) {
  if (!s_refresh_bar) { return; }
  bool refreshing = (data_load_state() == LOAD_LOADING && data_task_count() > 0);
  layer_set_hidden(s_refresh_bar, !refreshing);
}

// --- Window ------------------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorWhite, GColorBlack);
  status_bar_layer_set_separator_mode(s_status_bar, StatusBarLayerSeparatorModeDotted);
  layer_add_child(root, status_bar_layer_get_layer(s_status_bar));

  int top = STATUS_BAR_LAYER_HEIGHT;
  s_menu = menu_layer_create(GRect(0, top, b.size.w, b.size.h - top));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_num_rows,
    .get_cell_height = get_cell_height,
    .get_header_height = get_header_height,
    .draw_header = draw_header,
    .draw_row = draw_row,
    .select_click = select_click,
    .selection_changed = selection_changed,
  });
  menu_layer_set_normal_colors(s_menu, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu, theme_accent(), GColorBlack);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));

  // Thin accent line just under the status bar, shown only while refreshing.
  s_refresh_bar = layer_create(GRect(0, top - 3, b.size.w, 3));
  layer_set_update_proc(s_refresh_bar, refresh_bar_update);
  layer_set_hidden(s_refresh_bar, true);
  layer_add_child(root, s_refresh_bar);
}

static void window_unload(Window *window) {
  marquee_stop();
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  if (s_refresh_bar) { layer_destroy(s_refresh_bar); s_refresh_bar = NULL; }
  status_bar_layer_destroy(s_status_bar);
  s_status_bar = NULL;
  window_destroy(s_window);
  s_window = NULL;
  s_shown = false;
}

static void window_appear(Window *window) {
  s_shown = true;
  update_refresh_bar();
  marquee_start();
}
static void window_disappear(Window *window) {
  s_shown = false;
  marquee_stop();
}

void task_list_push(const char *project_id, const char *title, bool is_today) {
  strncpy(s_project_id, project_id ? project_id : "", sizeof(s_project_id) - 1);
  s_project_id[sizeof(s_project_id) - 1] = '\0';
  strncpy(s_title, title ? title : "", sizeof(s_title) - 1);
  s_title[sizeof(s_title) - 1] = '\0';
  s_is_today = is_today;

  // Paint the Vandaag list instantly from cache (other lists still fetch cold);
  // the background fetch overwrites it when it lands.
  if (cache_load_tasks(s_project_id)) {
    data_set_load_state(LOAD_OK);
  } else {
    data_clear_tasks();
    data_set_load_state(LOAD_LOADING);
  }
  config_request_tasks(s_project_id);

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

void task_list_reload(void) {
  if (s_menu) menu_layer_reload_data(s_menu);
  update_refresh_bar();
}

bool task_list_is_shown(void) { return s_shown; }

void task_list_destroy(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}
