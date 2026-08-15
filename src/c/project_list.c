#include "project_list.h"
#include "task_list.h"
#include "label_list.h"
#include "data.h"
#include "config.h"
#include "i18n.h"
#include "theme.h"
#include "header_bar.h"
#include "dictation_flow.h"
#include "touch_nav.h"

static Window    *s_window = NULL;
static MenuLayer *s_menu = NULL;
static Layer     *s_header = NULL;
static Layer     *s_refresh_bar = NULL;   // thin accent line shown while refreshing

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
  // .background colours the rail down the left edge, .foreground only the
  // small crumb dot on it -- verified on the emulator. The sheet is always
  // black with white focused / grey unfocused text, not configurable here.
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
  // .background colours the rail down the left edge, .foreground only the
  // small crumb dot on it -- verified on the emulator. The sheet is always
  // black with white focused / grey unfocused text, not configurable here.
    .colors = { .background = theme_accent(), .foreground = GColorBlack },
    .align = ActionMenuAlignCenter,
    .did_close = options_did_close,
    .context = root,
  };
  action_menu_open(&cfg);
}

// --- MenuLayer callbacks -----------------------------------------------------

// Row layout when loaded: 0 = Vandaag, 1..N = projects, N+1 = Labels, N+2 = "Nieuwe taak".
static int labels_row_index(void) { return 1 + data_project_count(); }
static int add_row_index(void)    { return 2 + data_project_count(); }

static uint16_t get_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  if (list_ready()) {
    return 3 + data_project_count();  // Vandaag + projects + Labels + "Nieuwe taak"
  }
  return 1;  // status row
}

#define ROW_HEIGHT 44

static int16_t get_cell_height(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  return ROW_HEIGHT;
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
  if (ci->row == labels_row_index()) {
    draw_text_row(ctx, cell, i18n(STR_LABELS), i18n(STR_LABELS_SUB));
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
  touch_tap_note_action();
  if (!list_ready()) {
    // Retry from the loading row too: if the phone swallowed the first
    // request, pressing the only row on screen should re-issue it.
    if (data_load_state() == LOAD_ERROR || data_load_state() == LOAD_LOADING) {
      data_set_load_state(LOAD_LOADING);
      config_request_refresh();
      project_list_reload();
    }
    return;
  }
  if (ci->row == 0) {
    task_list_push(TODAY_PROJECT_ID, i18n(STR_TODAY), TASK_LIST_TODAY);
    return;
  }
  if (ci->row == labels_row_index()) {
    label_list_push();
    return;
  }
  if (ci->row == add_row_index()) {
    project_list_begin_quick_add();  // dictate, then pick a project
    return;
  }
  Project *p = data_project(ci->row - 1);
  if (p && p->id[0]) {
    task_list_push(p->id, p->name, TASK_LIST_PROJECT);
  }
}

static void select_long_click(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  open_options_menu();
}

// --- Touch -------------------------------------------------------------------
//
// The system touch bridge (app_touch_navigation_enable) is unusable on firmware
// 4.33.1: it faults inside firmware on the very first touch and kills the app.
// So this window takes touch itself, from the raw event stream — see touch_nav.

// After a drag or glide the highlight can be off screen, which would make SELECT
// act on an invisible row and the next UP/DOWN yank the list back. Move the
// selection to the row in the middle of the window: always fully visible, and
// where button navigation puts it anyway, so the first button press does not
// jump. MenuRowAlignNone leaves the list exactly where the finger left it.
static void touch_settled(void) {
  if (!s_menu) { return; }
  GRect frame = layer_get_frame(menu_layer_get_layer(s_menu));
  int row = touch_nav_row_at(s_menu, GPoint(frame.size.w / 2, frame.origin.y + frame.size.h / 2),
                             get_num_rows(s_menu, 0, NULL), get_cell_height);
  if (row >= 0) {
    menu_layer_set_selected_index(s_menu, MenuIndex(0, (uint16_t)row), MenuRowAlignNone, false);
    // MenuRowAlignNone does not scroll, and without a scroll the menu never
    // repaints — the highlight would stay invisible until the next button press.
    layer_mark_dirty(menu_layer_get_layer(s_menu));
  }
}

bool project_list_is_top(void) {
  return s_window && window_stack_get_top_window() == s_window;
}

void project_list_handle_touch(const TouchEvent *event) {
  if (!s_menu) { return; }
  GPoint point;
  switch (touch_nav_feed(menu_layer_get_scroll_layer(s_menu), event, &point)) {
    case TOUCH_NAV_TAP: {
      int row = touch_nav_row_at(s_menu, point, get_num_rows(s_menu, 0, NULL), get_cell_height);
      if (row < 0) { return; }   // the header bar, or past the last row
      // Select the row the finger hit, then run what SELECT would have run.
      MenuIndex index = MenuIndex(0, (uint16_t)row);
      menu_layer_set_selected_index(s_menu, index, MenuRowAlignNone, false);
      select_click(s_menu, &index, NULL);
      break;
    }
    case TOUCH_NAV_BACK:
      window_stack_pop(true);   // root window, so this leaves the app — same as BACK
    default:
      break;
  }
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

#if TOUCH_NAV_SYSTEM_BRIDGE
// The system only moves the selection on the first tap and opens the row on a
// second one; open the row the finger actually hit, straight away.
static void tap_at(GPoint point) {
  if (!s_menu || touch_tap_action_just_ran()) { return; }
  int row = touch_nav_row_at(s_menu, point, get_num_rows(s_menu, 0, NULL), get_cell_height);
  if (row < 0) { return; }   // the header bar, or past the last row
  MenuIndex index = MenuIndex(0, (uint16_t)row);
  menu_layer_set_selected_index(s_menu, index, MenuRowAlignNone, false);
  select_click(s_menu, &index, NULL);
}
#endif

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
    .select_long_click = select_long_click,
  });
  menu_layer_set_normal_colors(s_menu, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu, theme_accent(), GColorBlack);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));

  // Thin accent line just under the header bar (over the white list), while refreshing.
  s_refresh_bar = layer_create(GRect(0, top, b.size.w, 3));
  layer_set_update_proc(s_refresh_bar, refresh_bar_update);
  layer_set_hidden(s_refresh_bar, true);
  layer_add_child(root, s_refresh_bar);

  // Branded accent top bar (icon + name + open-task count + time).
  s_header = header_bar_create(b.size.w);
  layer_add_child(root, s_header);

#if TOUCH_NAV_SYSTEM_BRIDGE
  touch_tap_attach(window, tap_at);
  touch_settle_attach(window, touch_settled);
#else
  // Take touch away from the system bridge and handle it here (see above).
  window_set_touch_bridge_disabled(window, true);
#endif
}

static void window_unload(Window *window) {
  touch_gestures_release(window);
  touch_nav_reset();   // stop any glide before its scroll layer goes away
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  if (s_refresh_bar) { layer_destroy(s_refresh_bar); s_refresh_bar = NULL; }
  if (s_header) { layer_destroy(s_header); s_header = NULL; }
}

static void window_appear(Window *window) {
  header_bar_set_active(s_header, "PebbleDoist", HEADER_COUNT_PROJECTS_TOTAL);
  touch_nav_set_settled_handler(touch_settled);
}

static void window_disappear(Window *window) {
  touch_nav_reset();
}

Window *project_list_window(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, GColorWhite);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = window_load,
      .unload = window_unload,
      .appear = window_appear,
      .disappear = window_disappear,
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
