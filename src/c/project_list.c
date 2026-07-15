#include "project_list.h"
#include "task_list.h"
#include "data.h"
#include "config.h"
#include "i18n.h"
#include "theme.h"
#include "dictation_flow.h"

static Window         *s_window = NULL;
static MenuLayer      *s_menu = NULL;
static StatusBarLayer *s_status_bar = NULL;
static Layer          *s_refresh_bar = NULL;   // thin accent line shown while refreshing

// True when the overview should show its rows: loaded, or refreshing while cached
// projects are still present (stale-while-revalidate — avoids a loading flash).
static bool list_ready(void) {
  int s = data_load_state();
  return s == LOAD_OK || (s == LOAD_LOADING && data_project_count() > 0);
}

// Holds the dictated text while the post-dictation project picker is open.
static char            s_pending_add[TASK_TITLE_LEN];
static ActionMenuLevel *s_pick_root = NULL;

// --- Post-dictation project picker (Quick-Launch "Direct inspreken") ---------

static void pick_performed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  int idx = (int)(uintptr_t)action_menu_item_get_action_data(item);
  Project *p = data_project(idx);
  if (p && p->id[0]) {
    config_add_task(p->id, s_pending_add);
  }
}

static void pick_did_close(ActionMenu *menu, const ActionMenuItem *performed, void *context) {
  action_menu_hierarchy_destroy((ActionMenuLevel *)context, NULL, NULL);
  s_pick_root = NULL;
}

static void open_add_picker(const char *content) {
  strncpy(s_pending_add, content ? content : "", sizeof(s_pending_add) - 1);
  s_pending_add[sizeof(s_pending_add) - 1] = '\0';

  int n = data_project_count();
  if (n <= 0) {
    // Projects not loaded yet — fall back to the configured default project.
    const char *def = config_default_project_id();
    if (def[0] && strcmp(def, TODAY_PROJECT_ID) != 0) {
      config_add_task(def, s_pending_add);
    }
    return;
  }

  s_pick_root = action_menu_level_create(n);
  for (int i = 0; i < n; i++) {
    Project *p = data_project(i);
    action_menu_level_add_action(s_pick_root, p->name, pick_performed, (void *)(uintptr_t)i);
  }
  ActionMenuConfig cfg = {
    .root_level = s_pick_root,
    .colors = { .background = theme_accent(), .foreground = GColorBlack },
    .align = ActionMenuAlignCenter,
    .did_close = pick_did_close,
    .context = s_pick_root,
  };
  action_menu_open(&cfg);
}

static void quick_add_result(DictationResult result, const char *transcript) {
  if (result == DICTATION_RESULT_SUCCESS && transcript && transcript[0]) {
    open_add_picker(transcript);
  }
}

void project_list_begin_quick_add(void) {
  dictation_flow_start(quick_add_result);
}

// --- ActionMenu: refresh -----------------------------------------------------

static void refresh_performed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  data_set_load_state(LOAD_LOADING);
  project_list_reload();
  config_request_refresh();
}

static void options_did_close(ActionMenu *menu, const ActionMenuItem *performed, void *context) {
  action_menu_hierarchy_destroy((ActionMenuLevel *)context, NULL, NULL);
}

static void open_options_menu(void) {
  ActionMenuLevel *root = action_menu_level_create(1);
  action_menu_level_add_action(root, i18n(STR_REFRESH), refresh_performed, NULL);
  ActionMenuConfig cfg = {
    .root_level = root,
    .colors = { .background = theme_accent(), .foreground = GColorBlack },
    .align = ActionMenuAlignCenter,
    .did_close = options_did_close,
    .context = root,
  };
  action_menu_open(&cfg);
}

// --- MenuLayer callbacks -----------------------------------------------------

// Row layout when loaded: 0 = Vandaag, 1..N = projects, N+1 = "Nieuwe taak".
static int add_row_index(void) { return 1 + data_project_count(); }

static uint16_t get_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  if (list_ready()) {
    return 2 + data_project_count();  // Vandaag + projects + "Nieuwe taak"
  }
  return 1;  // status row
}

static int16_t get_cell_height(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  return 44;
}

static void draw_text_row(GContext *ctx, const Layer *cell, const char *title, const char *sub) {
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  // Dark text on the red highlight reads better in low light than white.
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
    default:                title = i18n(STR_NO_PROJECTS); sub = "";                break;
  }
  draw_text_row(ctx, cell, title, sub);
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *ci, void *c) {
  if (!list_ready()) {
    draw_status_row(ctx, cell);
    return;
  }
  if (ci->row == 0) {
    draw_text_row(ctx, cell, i18n(STR_TODAY), i18n(STR_TODAY_SUB));
    return;
  }
  if (ci->row == add_row_index()) {
    draw_text_row(ctx, cell, i18n(STR_NEW_TASK), i18n(STR_DICTATE));
    return;
  }
  Project *p = data_project(ci->row - 1);
  if (!p) return;
  char sub[24];
  snprintf(sub, sizeof(sub), i18n(p->task_count == 1 ? STR_N_TASK : STR_N_TASKS), p->task_count);
  draw_text_row(ctx, cell, p->name, sub);
}

static void select_click(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  if (!list_ready()) {
    if (data_load_state() == LOAD_ERROR) {
      data_set_load_state(LOAD_LOADING);
      config_request_refresh();
      project_list_reload();
    }
    return;
  }
  if (ci->row == 0) {
    task_list_push(TODAY_PROJECT_ID, i18n(STR_TODAY), true);
    return;
  }
  if (ci->row == add_row_index()) {
    project_list_begin_quick_add();  // dictate, then pick a project
    return;
  }
  Project *p = data_project(ci->row - 1);
  if (p && p->id[0]) {
    task_list_push(p->id, p->name, false);
  }
}

static void select_long_click(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  open_options_menu();
}

// --- Refresh indicator -------------------------------------------------------

static void refresh_bar_update(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, theme_accent());
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

// Shows the thin accent line only while a background refresh runs over cached rows.
static void update_refresh_bar(void) {
  if (!s_refresh_bar) { return; }
  bool refreshing = (data_load_state() == LOAD_LOADING && data_project_count() > 0);
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
    .draw_row = draw_row,
    .select_click = select_click,
    .select_long_click = select_long_click,
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
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  if (s_refresh_bar) { layer_destroy(s_refresh_bar); s_refresh_bar = NULL; }
  status_bar_layer_destroy(s_status_bar);
  s_status_bar = NULL;
}

Window *project_list_window(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, GColorWhite);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = window_load,
      .unload = window_unload,
    });
  }
  return s_window;
}

void project_list_reload(void) {
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
  update_refresh_bar();
}

void project_list_destroy(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}
