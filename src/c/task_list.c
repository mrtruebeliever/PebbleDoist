#include "task_list.h"
#include "data.h"
#include "cache.h"
#include "config.h"
#include "i18n.h"
#include "theme.h"
#include "dictation_flow.h"
#include "task_detail.h"
#include "header_bar.h"

static Window    *s_window = NULL;
static MenuLayer *s_menu = NULL;
static Layer     *s_header = NULL;
static Layer     *s_refresh_bar = NULL;   // thin accent line shown while refreshing
static bool       s_shown = false;

static char s_project_id[LIST_ID_LEN];
static char s_title[PROJ_NAME_LEN];
static TaskListMode s_mode;

static char s_sel_task_id[TASK_ID_LEN];   // task targeted by the open ActionMenu
static Task s_sel_task;                    // snapshot for the detail view

// Quick-complete deferred-undo: a Select-completed task is shown "done" for a short
// window before it is actually completed on the server, so a second Select undoes it
// (nothing was sent yet). Only one task can be pending at a time.
static char      s_pending_id[TASK_ID_LEN] = "";
static AppTimer *s_undo_timer = NULL;
#define UNDO_WINDOW_MS 3000

// Marquee state for the selected task row's title (see marquee_tick).
static AppTimer *s_marq_timer = NULL;
static int       s_marq_offset = 0;
static int       s_marq_pause  = 0;
#define MARQ_TICK_MS 60
#define MARQ_STEP    3
#define MARQ_PAUSE   14

// Number of non-task rows above the task rows ("new task" row for real projects).
static int add_rows(void) { return s_mode == TASK_LIST_PROJECT ? 1 : 0; }

// Layout of a task row for the configured text size. The title is drawn full width
// on one line (ellipsized when too long); when the task has a due date it sits on a
// small line at the bottom of the row (like the "dictate" subtitle under "+ New
// task"). Rows are a fixed height per text size (compact, like the add row); a task
// with no due date centres its title vertically instead of leaving a gap.
#define DUE_LINE_H 16
typedef struct {
  GFont   font;
  int16_t row_h;     // total row height
  int16_t text_y;    // title top when a due line is present
  int16_t text_h;    // title line height (also the marquee measure height)
} RowMetrics;

static RowMetrics row_metrics(void) {
  switch (config_font_size()) {
    case FONT_SIZE_SMALL:
      return (RowMetrics){ fonts_get_system_font(FONT_KEY_GOTHIC_18), 38, 2, 20 };
    case FONT_SIZE_LARGE:
      return (RowMetrics){ fonts_get_system_font(FONT_KEY_GOTHIC_28), 50, 3, 30 };
    default:
      return (RowMetrics){ fonts_get_system_font(FONT_KEY_GOTHIC_24), 44, 2, 26 };
  }
}

// True when the list should show its rows: loaded, or refreshing while cached
// rows are still present (stale-while-revalidate — avoids a loading flash).
static bool list_ready(void) {
  int s = data_load_state();
  return s == LOAD_OK || (s == LOAD_LOADING && data_task_count() > 0);
}

// --- Quick-complete deferred undo --------------------------------------------

// Returns the task with this id (NULL if not present), for toggling its done flag.
static Task *task_by_id(const char *id) {
  int n = data_task_count();
  for (int i = 0; i < n; i++) {
    Task *t = data_task(i);
    if (t && strcmp(t->id, id) == 0) return t;
  }
  return NULL;
}

// Actually complete the pending task on the server and drop it from the list.
static void pending_commit(void) {
  if (s_undo_timer) { app_timer_cancel(s_undo_timer); s_undo_timer = NULL; }
  if (s_pending_id[0]) {
    config_close_task(s_pending_id);            // server completes; re-stream reconciles
    data_remove_task_by_id(s_pending_id);       // optimistic removal (no loading flash)
    s_pending_id[0] = '\0';
    task_list_reload();
    header_bar_refresh();
  }
}

// Cancel the pending completion within the undo window — nothing was sent yet.
static void pending_undo(void) {
  if (s_undo_timer) { app_timer_cancel(s_undo_timer); s_undo_timer = NULL; }
  if (s_pending_id[0]) {
    Task *t = task_by_id(s_pending_id);
    if (t) t->done = false;
    s_pending_id[0] = '\0';
    task_list_reload();
    header_bar_refresh();
  }
}

static void undo_timer_cb(void *ctx) {
  s_undo_timer = NULL;   // fired
  pending_commit();
}

// Mark a task done and start the undo window; the completion is only sent when the
// window elapses (undo_timer_cb) — so a second Select before then cancels it.
static void quick_complete(int trow) {
  Task *t = data_task(trow);
  if (!t || !t->id[0]) return;
  if (s_pending_id[0] && strcmp(s_pending_id, t->id) != 0) { pending_commit(); }
  t->done = true;
  strncpy(s_pending_id, t->id, sizeof(s_pending_id) - 1);
  s_pending_id[sizeof(s_pending_id) - 1] = '\0';
  if (s_undo_timer) { app_timer_cancel(s_undo_timer); }
  s_undo_timer = app_timer_register(UNDO_WINDOW_MS, undo_timer_cb, NULL);
  task_list_reload();
}

// --- Afvinken (close) ActionMenu ---------------------------------------------

static void close_performed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  if (s_sel_task_id[0]) {
    config_close_task(s_sel_task_id);
    data_remove_task_by_id(s_sel_task_id);   // optimistic — no loading flash
    task_list_reload();
    header_bar_refresh();
  }
}

static void delete_performed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  if (s_sel_task_id[0]) {
    config_delete_task(s_sel_task_id);
    data_remove_task_by_id(s_sel_task_id);   // optimistic — no loading flash
    task_list_reload();
    header_bar_refresh();
  }
}

static void detail_performed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  task_detail_push(&s_sel_task);
}

static void close_menu_did_close(ActionMenu *menu, const ActionMenuItem *performed, void *context) {
  action_menu_hierarchy_destroy((ActionMenuLevel *)context, NULL, NULL);
}

static void open_task_menu(Task *t) {
  pending_commit();  // finalize any quick-complete before showing the menu
  s_sel_task = *t;   // snapshot for the detail view (survives a background refresh)
  strncpy(s_sel_task_id, t->id, sizeof(s_sel_task_id) - 1);
  s_sel_task_id[sizeof(s_sel_task_id) - 1] = '\0';

  // Complete first (the most common action, so it's the default-highlighted item);
  // Delete opens a one-item confirm level so it can't fire on an accidental scroll.
  ActionMenuLevel *root = action_menu_level_create(3);
  action_menu_level_add_action(root, i18n(STR_COMPLETE), close_performed, NULL);
  action_menu_level_add_action(root, i18n(STR_DETAILS), detail_performed, NULL);
  ActionMenuLevel *confirm = action_menu_level_create(1);
  action_menu_level_add_action(confirm, i18n(STR_CONFIRM_DEL), delete_performed, NULL);
  action_menu_level_add_child(root, confirm, i18n(STR_DELETE));
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
  // Empty: a project shows only its add row; Today/Label show an empty-state row.
  return add_rows() + (s_mode == TASK_LIST_PROJECT ? 0 : 1);
}

// Only task rows follow the text-size setting; the add and status rows keep the
// fixed height their two-line (title + subtitle) layout needs. A task row grows to
// two lines when its (unselected) title is too long for one line.
static int16_t get_cell_height(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  if (!list_ready()) return 44;
  if (s_mode == TASK_LIST_PROJECT && ci->row == 0) return 44;
  if (data_task_count() == 0) return 44;
  // Fixed height per text size, so a row doesn't change when a due date is
  // added/removed or when it is quick-completed.
  return row_metrics().row_h;
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

// Draws a task that was just quick-completed and is inside its undo window: a
// ticked checkbox, the title struck through, and an "Undo" hint on the right.
static void draw_done_row(GContext *ctx, GRect b, Task *t, bool hl, RowMetrics m) {
  GFont hint_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  const char *undo = i18n(STR_UNDO);
  GSize us = graphics_text_layout_get_content_size(
      undo, hint_font, GRect(0, 0, 90, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  int uw = us.w;
  int title_w = b.size.w - 40 - uw - 6;   // leave a small gap before the hint

  int ty = (b.size.h - m.text_h) / 2;   // no due line here — centre it vertically
  GColor dim = hl ? GColorBlack : GColorDarkGray;
  graphics_context_set_text_color(ctx, dim);
  graphics_draw_text(ctx, t->title, m.font, GRect(34, ty, title_w, m.text_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  // Strike-through, only as wide as the (clamped) title text.
  GSize tsz = graphics_text_layout_get_content_size(
      t->title, m.font, GRect(0, 0, 4000, m.text_h), GTextOverflowModeFill, GTextAlignmentLeft);
  int strike_w = tsz.w < title_w ? tsz.w : title_w;
  int sy = ty + m.text_h / 2;
  graphics_context_set_stroke_color(ctx, dim);
  graphics_draw_line(ctx, GPoint(34, sy), GPoint(34 + strike_w, sy));

  // "Undo" hint on the right.
  graphics_context_set_text_color(ctx, hl ? GColorBlack : theme_accent());
  graphics_draw_text(ctx, undo, hint_font, GRect(b.size.w - 4 - uw, (b.size.h - 16) / 2, uw, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Ticked checkbox.
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_round_rect(ctx, GRect(10, b.size.h / 2 - 8, 16, 16), 3);
  graphics_draw_line(ctx, GPoint(13, b.size.h / 2), GPoint(16, b.size.h / 2 + 4));
  graphics_draw_line(ctx, GPoint(16, b.size.h / 2 + 4), GPoint(23, b.size.h / 2 - 5));
}

static void draw_task_row(GContext *ctx, const Layer *cell, Task *t) {
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  RowMetrics m = row_metrics();

  if (t->done) { draw_done_row(ctx, b, t, hl, m); return; }

  // Title: full width on one line. With a due date it sits near the top (due goes on
  // the bottom line); without one it is vertically centred so the row has no gap.
  bool has_due = t->due[0];
  int ty = has_due ? m.text_y : (b.size.h - m.text_h) / 2;
  graphics_context_set_text_color(ctx, GColorBlack);
  if (hl) {
    // Selected row: marquee the title on one line (no ellipsis).
    graphics_draw_text(ctx, t->title, m.font, GRect(34 - s_marq_offset, ty, 4000, m.text_h),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    // Re-paint the checkbox column so scrolled text never runs under it.
    graphics_context_set_fill_color(ctx, theme_accent());
    graphics_fill_rect(ctx, GRect(0, 0, 34, b.size.h), 0, GCornerNone);
  } else {
    graphics_draw_text(ctx, t->title, m.font, GRect(34, ty, b.size.w - 40, m.text_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  // Due date: a small gray line at the bottom of the row (shown selected or not).
  if (has_due) {
    graphics_context_set_text_color(ctx, hl ? GColorBlack : GColorDarkGray);
    graphics_draw_text(ctx, t->due, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(34, b.size.h - DUE_LINE_H - 1, b.size.w - 38, DUE_LINE_H),
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
    title = (s_mode == TASK_LIST_TODAY) ? i18n(STR_NOTHING_TODAY) : i18n(STR_NO_TASKS);
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
  if (s_mode == TASK_LIST_PROJECT && ci->row == 0) { draw_add_row(ctx, cell); return; }
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
  if (s_mode == TASK_LIST_PROJECT && ci->row == 0) {
    dictation_flow_start(add_result);
    return;
  }
  if (data_task_count() == 0) return;
  int trow = ci->row - add_rows();
  Task *t = data_task(trow);
  if (!t || !t->id[0]) return;
  // A second Select on the pending row undoes the quick-complete.
  if (s_pending_id[0] && strcmp(s_pending_id, t->id) == 0) { pending_undo(); return; }
  if (config_quick_complete()) { quick_complete(trow); }   // Select ticks it off
  else { open_task_menu(t); }                              // Select opens the menu
}

// Long-press always opens the menu (Complete / Details / Delete), so those actions
// stay reachable whether or not quick-complete is on.
static void select_long_click(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  if (!list_ready()) return;
  if (s_mode == TASK_LIST_PROJECT && ci->row == 0) return;
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
        RowMetrics m = row_metrics();
        GRect fr = layer_get_frame(menu_layer_get_layer(s_menu));
        int avail = fr.size.w - 40;
        GSize ts = graphics_text_layout_get_content_size(
            t->title, m.font,
            GRect(0, 0, 4000, m.text_h), GTextOverflowModeFill, GTextAlignmentLeft);
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

  int top = HEADER_BAR_HEIGHT;
  s_menu = menu_layer_create(GRect(0, top, b.size.w, b.size.h - top));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_num_rows,
    .get_cell_height = get_cell_height,
    .draw_row = draw_row,
    .select_click = select_click,
    .select_long_click = select_long_click,
    .selection_changed = selection_changed,
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

  s_header = header_bar_create(b.size.w);
  layer_add_child(root, s_header);
}

static void window_unload(Window *window) {
  marquee_stop();
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  if (s_refresh_bar) { layer_destroy(s_refresh_bar); s_refresh_bar = NULL; }
  if (s_header) { layer_destroy(s_header); s_header = NULL; }
  window_destroy(s_window);
  s_window = NULL;
  s_shown = false;
}

static void window_appear(Window *window) {
  s_shown = true;
  header_bar_set_active(s_header, s_title, HEADER_COUNT_TASKS);   // project / Today / label name
  update_refresh_bar();
  marquee_start();
}
static void window_disappear(Window *window) {
  s_shown = false;
  marquee_stop();
  pending_commit();   // finalize a quick-complete if the user navigates away mid-window
}

void task_list_push(const char *list_id, const char *title, TaskListMode mode) {
  strncpy(s_project_id, list_id ? list_id : "", sizeof(s_project_id) - 1);
  s_project_id[sizeof(s_project_id) - 1] = '\0';
  strncpy(s_title, title ? title : "", sizeof(s_title) - 1);
  s_title[sizeof(s_title) - 1] = '\0';
  s_mode = mode;

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
