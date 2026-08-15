#include "task_detail.h"
#include "config.h"
#include "data.h"
#include "dictation_flow.h"
#include "i18n.h"
#include "theme.h"
#include "touch_nav.h"
#include <pebble.h>

static Window      *s_window = NULL;
static ScrollLayer *s_scroll = NULL;
static TextLayer   *s_title  = NULL;
static TextLayer   *s_due    = NULL;
static TextLayer   *s_desc   = NULL;
static Layer       *s_badges = NULL;
static Layer       *s_subs   = NULL;
static bool         s_shown  = false;

static char s_title_buf[TASK_TITLE_LEN];
static char s_due_buf[TASK_DUE_LEN + 24];   // "<Due label>: " + due text
static char s_task_id[TASK_ID_LEN];         // the task these subtasks belong to

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

// --- Subtasks ----------------------------------------------------------------
// Todoist subtasks are tasks with a parent. The phone keeps them out of the task
// list and streams the open ones here instead, so a task's children live in one
// place. A row can be ticked off (by tap, or from the Select menu); like the
// task list's quick-complete, the completion is only sent to the server once a
// short undo window elapses, so a mis-tap costs nothing.

#define SUB_HEADER_H  28   // separator + section header above the rows
#define SUB_ROW_PAD    5
#define SUB_BOX       14   // checkbox size
#define SUB_TEXT_X    22

static char      s_sub_pending[TASK_ID_LEN] = "";
static AppTimer *s_sub_timer = NULL;
#define SUB_UNDO_WINDOW_MS 3000

// Which subtask the buttons are on: -1 while reading the text above (Up/Down
// page through it), 0.. once the list has been entered. Select ticks the
// focused row off; with nothing focused it opens the action menu instead.
static int s_focus = -1;

static int16_t s_sub_h[MAX_SUBTASKS];   // row heights, for hit-testing a tap

static char s_sub_hdr[40];              // "Subtasks (3)"

static GFont sub_font(void) { return fonts_get_system_font(FONT_KEY_GOTHIC_18); }

// Lays out (and, when draw=true, paints) the subtask section in `width`,
// starting at (ox,oy). Returns its total height, 0 when there are no subtasks.
static int render_subtasks(GContext *ctx, int ox, int oy, int width, bool draw) {
  int n = data_subtask_count();
  if (n <= 0 || width <= 0) { return 0; }

  if (draw) {
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_draw_line(ctx, GPoint(ox, oy + 3), GPoint(ox + width, oy + 3));
    graphics_context_set_text_color(ctx, theme_accent());
    graphics_draw_text(ctx, s_sub_hdr, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(ox, oy + 6, width, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  GFont font = sub_font();
  GFont hint_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  int y = SUB_HEADER_H;

  for (int i = 0; i < n; i++) {
    Subtask *s = data_subtask(i);
    if (!s) { continue; }

    bool pending = (s_sub_pending[0] && strcmp(s_sub_pending, s->id) == 0);
    int hint_w = 0;
    if (pending) {
      GSize hs = graphics_text_layout_get_content_size(
          i18n(STR_UNDO), hint_font, GRect(0, 0, 90, 18),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      hint_w = hs.w + 6;
    }

    int text_w = width - SUB_TEXT_X - hint_w;
    if (text_w < 20) { text_w = 20; }
    GSize ts = graphics_text_layout_get_content_size(
        s->title, font, GRect(0, 0, text_w, 2000),
        GTextOverflowModeWordWrap, GTextAlignmentLeft);
    int rh = ts.h + SUB_ROW_PAD * 2;
    if (rh < SUB_BOX + SUB_ROW_PAD * 2) { rh = SUB_BOX + SUB_ROW_PAD * 2; }
    s_sub_h[i] = rh;

    if (draw) {
      int ry = oy + y;
      int box_y = ry + SUB_ROW_PAD;
      bool focused = (i == s_focus);
      if (focused) {
        // Same highlight as the list rows: accent fill, black text.
        graphics_context_set_fill_color(ctx, theme_accent());
        graphics_fill_rect(ctx, GRect(ox - 3, ry, width + 6, rh), 3, GCornersAll);
      }
      graphics_context_set_text_color(ctx, (s->done && !focused) ? GColorDarkGray : GColorBlack);
      graphics_draw_text(ctx, s->title, font,
                         GRect(ox + SUB_TEXT_X, ry + SUB_ROW_PAD - 3, text_w, ts.h + 4),
                         GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

      graphics_context_set_stroke_color(ctx, (s->done && !focused) ? GColorDarkGray : GColorBlack);
      graphics_draw_round_rect(ctx, GRect(ox, box_y, SUB_BOX, SUB_BOX), 3);
      if (s->done) {
        // Tick inside the box.
        graphics_draw_line(ctx, GPoint(ox + 3, box_y + 7), GPoint(ox + 6, box_y + 10));
        graphics_draw_line(ctx, GPoint(ox + 6, box_y + 10), GPoint(ox + 11, box_y + 3));
      }
      if (pending) {
        graphics_context_set_text_color(ctx, focused ? GColorBlack : theme_accent());
        graphics_draw_text(ctx, i18n(STR_UNDO), hint_font,
                           GRect(ox + width - hint_w, ry + SUB_ROW_PAD - 1, hint_w, 18),
                           GTextOverflowModeFill, GTextAlignmentRight, NULL);
      }
    }
    y += rh;
  }
  return y;
}

static void subs_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  render_subtasks(ctx, 0, 0, b.size.w, true);
}

// Send the pending tick to the server and drop the row (the phone re-fetches the
// detail afterwards, which reconciles anything else that changed).
static void sub_pending_commit(void) {
  if (s_sub_timer) { app_timer_cancel(s_sub_timer); s_sub_timer = NULL; }
  if (!s_sub_pending[0]) { return; }
  config_close_task(s_sub_pending);
  if (!quiet_time_is_active()) { vibes_short_pulse(); }
  data_remove_subtask_by_id(s_sub_pending);
  s_sub_pending[0] = '\0';
  task_detail_reload();
}

// Cancel within the undo window — nothing was sent yet.
static void sub_pending_undo(void) {
  if (s_sub_timer) { app_timer_cancel(s_sub_timer); s_sub_timer = NULL; }
  if (!s_sub_pending[0]) { return; }
  Subtask *s = data_subtask_by_id(s_sub_pending);
  if (s) { s->done = false; }
  s_sub_pending[0] = '\0';
  task_detail_reload();
}

static void sub_timer_cb(void *ctx) {
  s_sub_timer = NULL;   // fired
  sub_pending_commit();
}

// Ticks a subtask off, or undoes it when it is the one already pending.
static void subtask_toggle(const char *id) {
  if (!id || !id[0]) { return; }
  if (s_sub_pending[0] && strcmp(s_sub_pending, id) == 0) { sub_pending_undo(); return; }
  if (s_sub_pending[0]) { sub_pending_commit(); }   // only one pending at a time

  Subtask *s = data_subtask_by_id(id);
  if (!s) { return; }
  s->done = true;
  strncpy(s_sub_pending, id, sizeof(s_sub_pending) - 1);
  s_sub_pending[sizeof(s_sub_pending) - 1] = '\0';
  if (s_sub_timer) { app_timer_cancel(s_sub_timer); }
  s_sub_timer = app_timer_register(SUB_UNDO_WINDOW_MS, sub_timer_cb, NULL);
  task_detail_reload();
}

// --- Select menu (Add subtask / Complete a subtask) --------------------------
//
// Ids are snapshotted when the menu opens: a background re-stream can reorder or
// replace the data array while the menu is up, and an index into it would then
// tick off the wrong task.
static char s_menu_ids[MAX_SUBTASKS][TASK_ID_LEN];

static void sub_complete_performed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  int idx = (int)(uintptr_t)action_menu_item_get_action_data(item);
  if (idx >= 0 && idx < MAX_SUBTASKS) { subtask_toggle(s_menu_ids[idx]); }
}

static void detail_add_sub_result(DictationResult result, const char *transcript) {
  if (result == DICTATION_RESULT_SUCCESS && transcript && transcript[0] && s_task_id[0]) {
    config_add_subtask(s_task_id, transcript);
  }
}

// Deferred: the action menu is still on the window stack while `performed` runs,
// and its teardown would take the dictation window down with it.
static void detail_start_add_sub(void *ctx) { dictation_flow_start(detail_add_sub_result); }

static void detail_add_sub_performed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  app_timer_register(50, detail_start_add_sub, NULL);
}

static void detail_menu_did_close(ActionMenu *menu, const ActionMenuItem *performed, void *context) {
  action_menu_hierarchy_destroy((ActionMenuLevel *)context, NULL, NULL);
}

static void open_detail_menu(void) {
  int n = data_subtask_count();
  int open_count = 0;
  for (int i = 0; i < n && open_count < MAX_SUBTASKS; i++) {
    Subtask *s = data_subtask(i);
    if (s && s->id[0] && !s->done) {
      strncpy(s_menu_ids[open_count], s->id, TASK_ID_LEN - 1);
      s_menu_ids[open_count][TASK_ID_LEN - 1] = '\0';
      open_count++;
    }
  }

  ActionMenuLevel *root = action_menu_level_create(open_count > 0 ? 2 : 1);
  action_menu_level_add_action(root, i18n(STR_ADD_SUBTASK), detail_add_sub_performed, NULL);
  if (open_count > 0) {
    ActionMenuLevel *lvl = action_menu_level_create(open_count);
    int k = 0;
    for (int i = 0; i < n && k < open_count; i++) {
      Subtask *s = data_subtask(i);
      if (s && s->id[0] && !s->done) {
        action_menu_level_add_action(lvl, s->title, sub_complete_performed,
                                     (void *)(uintptr_t)k);
        k++;
      }
    }
    action_menu_level_add_child(root, lvl, i18n(STR_COMPLETE));
  }

  ActionMenuConfig cfg = {
    .root_level = root,
    .colors = { .background = theme_accent(), .foreground = GColorBlack },
    .align = ActionMenuAlignCenter,
    .did_close = detail_menu_did_close,
    .context = root,
  };
  action_menu_open(&cfg);
}

// --- Buttons -----------------------------------------------------------------
//
// Up/Down page through the text while nothing is focused, then walk the subtask
// rows one by one; Select ticks the focused row off, or opens the action menu
// when the buttons are still on the text. Long-press Select always opens the
// menu, so "Add subtask" stays reachable from anywhere in the view.

static int visible_h(void) {
  return layer_get_frame(scroll_layer_get_layer(s_scroll)).size.h;
}

// Most-negative valid content offset (content taller than the window).
static int min_offset(void) {
  int m = visible_h() - scroll_layer_get_content_size(s_scroll).h;
  return m > 0 ? 0 : m;
}

static void scroll_to(int offset_y, bool animated) {
  if (offset_y > 0) { offset_y = 0; }
  if (offset_y < min_offset()) { offset_y = min_offset(); }
  scroll_layer_set_content_offset(s_scroll, GPoint(0, offset_y), animated);
}

// Top edge of a subtask row in content coordinates, or -1.
static int row_top(int i) {
  if (!s_subs || i < 0 || i >= data_subtask_count()) { return -1; }
  int y = layer_get_frame(s_subs).origin.y + SUB_HEADER_H;
  for (int k = 0; k < i; k++) { y += s_sub_h[k]; }
  return y;
}

// Scrolls just enough to bring the focused row fully into view.
static void reveal_row(int i) {
  int top = row_top(i);
  if (top < 0) { return; }
  int shown = -scroll_layer_get_content_offset(s_scroll).y;   // px scrolled down
  int bottom = top + s_sub_h[i];
  if (top - 4 < shown) { scroll_to(-(top - 4), true); }
  else if (bottom + 4 > shown + visible_h()) { scroll_to(-(bottom + 4 - visible_h()), true); }
}

static void focus_changed(void) {
  if (s_subs) { layer_mark_dirty(s_subs); }
}

static void detail_down(ClickRecognizerRef recognizer, void *context) {
  if (!s_scroll) { return; }
  int n = data_subtask_count();
  int page = visible_h() - 24;
  if (n > 0) {
    if (s_focus < 0) {
      // Step into the list once its first row has come into view; until then
      // Down keeps paging through the description.
      int top = row_top(0);
      int shown = -scroll_layer_get_content_offset(s_scroll).y;
      if (top >= 0 && top < shown + visible_h()) {
        s_focus = 0;
        reveal_row(0);
        focus_changed();
        return;
      }
    } else if (s_focus < n - 1) {
      s_focus++;
      reveal_row(s_focus);
      focus_changed();
      return;
    } else {
      return;   // already on the last subtask
    }
  }
  scroll_to(scroll_layer_get_content_offset(s_scroll).y - page, true);
}

static void detail_up(ClickRecognizerRef recognizer, void *context) {
  if (!s_scroll) { return; }
  int page = visible_h() - 24;
  if (s_focus > 0) {
    s_focus--;
    reveal_row(s_focus);
    focus_changed();
    return;
  }
  if (s_focus == 0) {
    s_focus = -1;               // back to reading the text above the list
    focus_changed();
  }
  scroll_to(scroll_layer_get_content_offset(s_scroll).y + page, true);
}

static void detail_select_click(ClickRecognizerRef recognizer, void *context) {
  Subtask *s = (s_focus >= 0) ? data_subtask(s_focus) : NULL;
  if (s && s->id[0]) { subtask_toggle(s->id); return; }
  open_detail_menu();
}

static void detail_select_long(ClickRecognizerRef recognizer, void *context) {
  open_detail_menu();
}

// Up/Down are ours too, so they can move between subtask rows; the scroll layer
// only ever gets the offsets we set.
static void detail_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, detail_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, detail_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, detail_select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, detail_select_long, NULL);
}

// --- Layout ------------------------------------------------------------------

static void clear_layers(void) {
  if (s_title)  { text_layer_destroy(s_title);   s_title  = NULL; }
  if (s_due)    { text_layer_destroy(s_due);     s_due    = NULL; }
  if (s_desc)   { text_layer_destroy(s_desc);    s_desc   = NULL; }
  if (s_badges) { layer_destroy(s_badges);       s_badges = NULL; }
  if (s_subs)   { layer_destroy(s_subs);         s_subs   = NULL; }
  if (s_scroll) { scroll_layer_destroy(s_scroll); s_scroll = NULL; }
}

static void build(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  const int pad = 6;
  const int w = bounds.size.w - pad * 2;

  s_scroll = scroll_layer_create(bounds);
  scroll_layer_set_callbacks(s_scroll, (ScrollLayerCallbacks){
    .click_config_provider = detail_click_config,
  });
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

  // Subtasks (open ones only — a completed subtask drops off the list). The
  // list shrinks under the focus when one is ticked off or the phone re-streams,
  // so keep the focus inside it.
  if (s_focus >= data_subtask_count()) { s_focus = data_subtask_count() - 1; }
  int subs_h = render_subtasks(NULL, 0, 0, w, false);
  if (subs_h > 0) {
    snprintf(s_sub_hdr, sizeof(s_sub_hdr), "%s (%d)", i18n(STR_SUBTASKS), data_subtask_count());
    y += 8;
    s_subs = layer_create(GRect(pad, y, w, subs_h));
    layer_set_update_proc(s_subs, subs_update);
    scroll_layer_add_child(s_scroll, s_subs);
    y += subs_h;
  }

  scroll_layer_set_content_size(s_scroll, GSize(bounds.size.w, y + pad));
  layer_add_child(root, scroll_layer_get_layer(s_scroll));
}

// --- Touch -------------------------------------------------------------------
//
// Raw touch handled by hand; the system bridge crashes the app on firmware
// 4.33.1 (see project_list.c and touch_nav.h). A drag scrolls the text, a
// left-to-right swipe goes back, and a tap on a subtask row ticks it off.

bool task_detail_is_top(void) {
  return s_window && window_stack_get_top_window() == s_window;
}

// The subtask row under `p` (screen coordinates), or -1 when the point misses
// the section. The rows live inside the scroll layer, so the content offset has
// to come off the point before it can be walked against the row heights.
static int subtask_at(GPoint p) {
  if (!s_subs || !s_scroll) { return -1; }
  GRect sf = layer_get_frame(scroll_layer_get_layer(s_scroll));
  GRect f = layer_get_frame(s_subs);
  GPoint off = scroll_layer_get_content_offset(s_scroll);

  int lx = p.x - sf.origin.x - f.origin.x;
  int ly = p.y - sf.origin.y - off.y - f.origin.y;
  if (lx < 0 || lx > f.size.w) { return -1; }
  if (ly < SUB_HEADER_H || ly >= f.size.h) { return -1; }

  int y = SUB_HEADER_H;
  int n = data_subtask_count();
  for (int i = 0; i < n; i++) {
    if (ly < y + s_sub_h[i]) { return i; }
    y += s_sub_h[i];
  }
  return -1;
}

void task_detail_handle_touch(const TouchEvent *event) {
  if (!s_scroll) { return; }
  GPoint point;
  switch (touch_nav_feed(s_scroll, event, &point)) {
    case TOUCH_NAV_TAP: {
      int i = subtask_at(point);
      Subtask *s = (i >= 0) ? data_subtask(i) : NULL;
      if (s && s->id[0]) {
        s_focus = i;             // buttons carry on from the row just tapped
        subtask_toggle(s->id);
      }
      break;
    }
    case TOUCH_NAV_BACK:
      window_stack_pop(true);
      break;
    default:
      break;
  }
}

#if TOUCH_NAV_SYSTEM_BRIDGE
// The content here sits in a ScrollLayer, which the system scrolls by touch but
// whose taps it drops outright — so without this the subtasks cannot be ticked
// off by finger at all.
static void tap_at(GPoint point) {
  int i = subtask_at(point);
  Subtask *s = (i >= 0) ? data_subtask(i) : NULL;
  if (s && s->id[0]) {
    s_focus = i;             // buttons carry on from the row just tapped
    subtask_toggle(s->id);
  }
}
#endif

static void window_load(Window *window) {
  build(window);
#if TOUCH_NAV_SYSTEM_BRIDGE
  touch_tap_attach(window, tap_at);
#else
  window_set_touch_bridge_disabled(window, true);
#endif
}

static void window_unload(Window *window) {
  touch_gestures_release(window);
  touch_nav_reset();
  clear_layers();
  // Tell the phone the detail view is gone, so a later task change doesn't
  // re-fetch a detail nobody is looking at.
  config_request_task_detail("");
  s_task_id[0] = '\0';
  window_destroy(s_window);
  s_window = NULL;
}

static void window_appear(Window *window)    { s_shown = true; }
static void window_disappear(Window *window) {
  s_shown = false;
  touch_nav_reset();
  sub_pending_commit();   // finalize a ticked subtask if the user navigates away
}

void task_detail_push(const Task *t) {
  if (!t) { return; }
  strncpy(s_title_buf, t->title, sizeof(s_title_buf) - 1);
  s_title_buf[sizeof(s_title_buf) - 1] = '\0';
  strncpy(s_task_id, t->id, sizeof(s_task_id) - 1);
  s_task_id[sizeof(s_task_id) - 1] = '\0';
  if (t->due[0]) {
    snprintf(s_due_buf, sizeof(s_due_buf), "%s: %s", i18n(STR_DUE), t->due);
  } else {
    s_due_buf[0] = '\0';
  }

  // Fetch the description + labels + subtasks; the reply rebuilds the window
  // (task_detail_reload).
  s_focus = -1;
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
  // build() creates a fresh ScrollLayer, which starts at the top. This fires on
  // every inbound AppMessage, and the phone streams updates continuously -- so
  // without carrying the offset across, a background refresh yanked the user
  // back to the start of the description they were reading.
  GPoint offset = s_scroll ? scroll_layer_get_content_offset(s_scroll) : GPointZero;
  clear_layers();
  build(s_window);
  if (s_scroll) {
    GSize content = scroll_layer_get_content_size(s_scroll);
    GRect frame = layer_get_frame(scroll_layer_get_layer(s_scroll));
    int16_t min_y = frame.size.h - content.h;      // most-negative valid offset
    if (min_y > 0) { min_y = 0; }
    if (offset.y < min_y) { offset.y = min_y; }    // content may have shrunk
    if (offset.y > 0) { offset.y = 0; }
    scroll_layer_set_content_offset(s_scroll, offset, false);
  }
  layer_mark_dirty(window_get_root_layer(s_window));
}

bool task_detail_is_shown(void) { return s_shown; }
