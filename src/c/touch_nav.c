#include "touch_nav.h"

#define TAP_SLOP        12   // px of drift still counted as a tap
#define BACK_SWIPE_MIN  28   // px of rightward travel that means "back"
#define GLIDE_MS        33   // ~30 fps while coasting
#define GLIDE_KEEP      82   // percent of the speed kept per frame
#define GLIDE_STOP       2   // px/frame below which the glide ends

static ScrollLayer *s_scroll = NULL;
static AppTimer    *s_glide = NULL;
static TouchNavSettled s_settled = NULL;

static GPoint  s_down;          // where the finger landed
static int16_t s_anchor_y;      // y at which dragging actually began
static int16_t s_base_offset;   // content offset at that moment
static int     s_velocity;      // px per event, signed (down is positive)
static bool    s_touching = false;
static bool    s_dragging = false;   // moved past the tap slop

static int int_abs(int v) { return v < 0 ? -v : v; }

// Content may only scroll between "top row flush" (0) and "last row flush".
static int clamp_offset(ScrollLayer *scroll, int offset) {
  GSize content = scroll_layer_get_content_size(scroll);
  GRect frame = layer_get_frame(scroll_layer_get_layer(scroll));
  int min_offset = frame.size.h - content.h;
  if (min_offset > 0) { min_offset = 0; }   // content shorter than the window
  if (offset > 0) { return 0; }
  if (offset < min_offset) { return min_offset; }
  return offset;
}

static void apply_offset(ScrollLayer *scroll, int offset) {
  scroll_layer_set_content_offset(scroll, GPoint(0, clamp_offset(scroll, offset)), false);
}

static void settle(void) {
  if (s_settled) { s_settled(); }
}

static void glide_tick(void *data) {
  s_glide = NULL;
  if (!s_scroll) { return; }
  int wanted = scroll_layer_get_content_offset(s_scroll).y + s_velocity;
  int reached = clamp_offset(s_scroll, wanted);
  apply_offset(s_scroll, wanted);
  s_velocity = s_velocity * GLIDE_KEEP / 100;
  // Stop dead at the ends rather than grinding against them.
  if (reached != wanted || int_abs(s_velocity) <= GLIDE_STOP) {
    s_velocity = 0;
    settle();
    return;
  }
  s_glide = app_timer_register(GLIDE_MS, glide_tick, NULL);
}

static void stop_glide(void) {
  if (s_glide) {
    app_timer_cancel(s_glide);
    s_glide = NULL;
  }
  s_velocity = 0;
}

TouchNavGesture touch_nav_feed(ScrollLayer *scroll, const TouchEvent *event, GPoint *out_point) {
  switch (event->type) {
    case TouchEvent_Touchdown:
      // Touching a coasting list catches it, the way it does everywhere else.
      stop_glide();
      s_scroll = scroll;
      s_down = GPoint(event->x, event->y);
      s_touching = true;
      s_dragging = false;
      return TOUCH_NAV_NONE;

    case TouchEvent_PositionUpdate: {
      if (!s_touching || !s_scroll) { return TOUCH_NAV_NONE; }
      if (!s_dragging) {
        // Wait out the slop, then start scrolling from where the finger is now,
        // so the list does not jump by the slop distance at the first move.
        if (int_abs(event->y - s_down.y) <= TAP_SLOP) { return TOUCH_NAV_NONE; }
        s_dragging = true;
        s_anchor_y = event->y;
        s_base_offset = scroll_layer_get_content_offset(s_scroll).y;
        s_velocity = 0;
        return TOUCH_NAV_NONE;
      }
      int previous = scroll_layer_get_content_offset(s_scroll).y;
      apply_offset(s_scroll, s_base_offset + (event->y - s_anchor_y));
      // Measure the speed from what the list actually did, so a finger dragging
      // against an end contributes no momentum.
      s_velocity = scroll_layer_get_content_offset(s_scroll).y - previous;
      return TOUCH_NAV_NONE;
    }

    case TouchEvent_Liftoff: {
      if (!s_touching) { return TOUCH_NAV_NONE; }
      s_touching = false;
      if (out_point) { *out_point = s_down; }

      int dx = event->x - s_down.x;
      int dy = event->y - s_down.y;
      if (!s_dragging && int_abs(dx) <= TAP_SLOP && int_abs(dy) <= TAP_SLOP) {
        return TOUCH_NAV_TAP;
      }
      if (int_abs(dx) > int_abs(dy) && dx >= BACK_SWIPE_MIN) {
        stop_glide();
        return TOUCH_NAV_BACK;
      }
      if (int_abs(dx) > int_abs(dy) && dx <= -BACK_SWIPE_MIN) {
        stop_glide();
        s_dragging = false;
        return TOUCH_NAV_ACTIONS;
      }
      // Nothing to scroll: report the flick instead, so a window can page or
      // move a selection with it.
      if (!s_scroll && int_abs(dy) > int_abs(dx) && int_abs(dy) >= BACK_SWIPE_MIN) {
        return dy < 0 ? TOUCH_NAV_SWIPE_UP : TOUCH_NAV_SWIPE_DOWN;
      }
      s_dragging = false;
      if (s_scroll && int_abs(s_velocity) > GLIDE_STOP) {
        s_glide = app_timer_register(GLIDE_MS, glide_tick, NULL);
      } else {
        settle();
      }
      return TOUCH_NAV_NONE;
    }

    default:
      return TOUCH_NAV_NONE;
  }
}

void touch_nav_set_settled_handler(TouchNavSettled handler) {
  s_settled = handler;
}

void touch_nav_reset(void) {
  stop_glide();
  s_scroll = NULL;
  s_settled = NULL;
  s_touching = false;
  s_dragging = false;
}

int touch_nav_row_at(MenuLayer *menu, GPoint p, uint16_t num_rows, TouchNavRowHeight height_cb) {
  if (!menu || !height_cb) { return -1; }
  GRect frame = layer_get_frame(menu_layer_get_layer(menu));
  if (!grect_contains_point(&frame, &p)) { return -1; }
  // Offset is negative once scrolled, so subtracting it gives the y within the
  // full row list rather than within the visible window.
  int16_t offset = scroll_layer_get_content_offset(menu_layer_get_scroll_layer(menu)).y;
  int y = p.y - frame.origin.y - offset;
  int top = 0;
  for (uint16_t row = 0; row < num_rows; row++) {
    MenuIndex index = MenuIndex(0, row);
    top += height_cb(menu, &index, NULL);
    if (y < top) { return (int)row; }
  }
  return -1;
}
