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

// ---------------------------------------------------------------------------
// Taps on top of the system bridge (see touch_nav.h)

// One slot per window that can be loaded at once: the overview, a task list, a
// task's detail, and the label list.
#define TAP_SLOTS 4
// A row's action counts as "just run" for this long, so the second half of one
// tap is swallowed. Short enough that a real second tap always gets through.
#define TAP_DEDUPE_MS 250

typedef struct {
  Window          *window;
  Recognizer      *tap;
  Recognizer      *pan;
  TouchTapHandler  tap_handler;
  TouchNavSettled  settled_handler;
} TouchSlot;

static TouchSlot s_slots[TAP_SLOTS];

static uint32_t s_last_action_ms;

static uint32_t now_ms(void) {
  time_t secs;
  uint16_t ms;
  time_ms(&secs, &ms);
  return ((uint32_t)secs * 1000u) + ms;
}

void touch_tap_note_action(void) {
  s_last_action_ms = now_ms();
}

bool touch_tap_action_just_ran(void) {
  return s_last_action_ms && (now_ms() - s_last_action_ms) < TAP_DEDUPE_MS;
}

// The system's own recognizers claim the gesture first; saying yes here is what
// keeps ours in the running rather than being skipped as a duplicate.
//
// But only for the system's: our own tap and pan must still arbitrate against
// each other. Both thresholds are 10px, so a quick ~10px nudge can satisfy the
// tap and start the pan at once, and answering "yes" to everything let a single
// nudge fire both — reselecting the middle row and opening a row the user never
// meant to tap.
static bool runs_alongside(const Recognizer *recognizer, const Recognizer *other) {
  for (int i = 0; i < TAP_SLOTS; i++) {
    if (other == s_slots[i].tap || other == s_slots[i].pan) { return false; }
  }
  return true;
}

// Whether a pan is driving the gesture the finger is in right now. Both budgets
// are the same 10px, so a nudge right at the boundary can still satisfy the tap
// after the pan has taken the gesture; the drag wins and the tap is dropped.
//
// The end time is kept as well because both recognizers finish within the same
// liftoff, in an order this code does not control: a pan that reports Completed
// first would otherwise clear the flag just before the tap is handed to us.
static bool     s_pan_active;
static uint32_t s_pan_end_ms;
#define PAN_SETTLED_GRACE_MS 50

static bool pan_owns_gesture(void) {
  return s_pan_active || (s_pan_end_ms && (now_ms() - s_pan_end_ms) < PAN_SETTLED_GRACE_MS);
}

// The SDK exposes no way to read a recognizer's user data back, so the window is
// looked up by the recognizer the callback was handed.
static void gesture_event(const Recognizer *recognizer, RecognizerEvent event) {
  for (int i = 0; i < TAP_SLOTS; i++) {
    TouchSlot *slot = &s_slots[i];

    if (slot->tap == recognizer) {
      if (event == RecognizerEvent_Completed && !pan_owns_gesture()) {
        slot->tap_handler(tap_recognizer_get_tap_point(recognizer));
      }
      return;
    }

    if (slot->pan == recognizer) {
      switch (event) {
        case RecognizerEvent_Started:
          s_pan_active = true;
          break;
        case RecognizerEvent_Completed:
          // Liftoff. A plain MenuLayer does not coast on after the finger (the
          // firmware settles the offset here and deliberately leaves the
          // selection alone), so the list is already at its final position. A
          // window without a settled handler keeps its pan purely to arbitrate
          // against its tap.
          if (slot->settled_handler) { slot->settled_handler(); }
          s_pan_active = false;
          s_pan_end_ms = now_ms();
          break;
        case RecognizerEvent_Cancelled:
          s_pan_active = false;
          s_pan_end_ms = now_ms();
          break;
        default:
          break;
      }
      return;
    }
  }
}

// The slot for `window`, claiming a free one if it has none yet, or NULL when
// every slot is taken.
static TouchSlot *slot_for(Window *window) {
  for (int i = 0; i < TAP_SLOTS; i++) {
    if (s_slots[i].window == window) { return &s_slots[i]; }
  }
  for (int i = 0; i < TAP_SLOTS; i++) {
    if (!s_slots[i].window) {
      s_slots[i].window = window;
      return &s_slots[i];
    }
  }
  return NULL;
}

static Recognizer *attach(Window *window, Recognizer *recognizer) {
  if (!recognizer) { return NULL; }
  recognizer_set_simultaneous_with(recognizer, runs_alongside);
  window_attach_recognizer(window, recognizer);
  return recognizer;
}

static void drop(Window *window, Recognizer **recognizer) {
  if (!*recognizer) { return; }
  window_detach_recognizer(window, *recognizer);
  recognizer_destroy(*recognizer);
  *recognizer = NULL;
}

// Every tap window also gets a pan, even one with nothing to settle: a started
// pan is what tells the tap that this gesture is a drag.
//
// NOT recognizer_set_fail_after(tap, pan): that gates on the pan being in the
// Failed state ("if (recognizer_get_state(fail_after) != RecognizerState_Failed)
// return false"), and at touchdown it is merely Possible — so the tap would
// process no touch at all and never fire.
static void add_pan(Window *window, TouchSlot *slot) {
  if (slot->pan) { return; }
  slot->pan = attach(window, pan_recognizer_create(gesture_event, NULL, PanAxis_Vertical));
}

void touch_tap_attach(Window *window, TouchTapHandler handler) {
  if (!window || !handler) { return; }
  TouchSlot *slot = slot_for(window);
  if (!slot) { return; }
  drop(window, &slot->tap);   // a reloaded window must not stack up recognizers
  drop(window, &slot->pan);
  slot->tap_handler = handler;
  slot->tap = attach(window, tap_recognizer_create(gesture_event, NULL));
  add_pan(window, slot);
}

void touch_settle_attach(Window *window, TouchNavSettled handler) {
  if (!window || !handler) { return; }
  TouchSlot *slot = slot_for(window);
  if (!slot) { return; }
  slot->settled_handler = handler;
  add_pan(window, slot);   // normally already there, from touch_tap_attach
}

void touch_gestures_release(Window *window) {
  if (!window) { return; }
  for (int i = 0; i < TAP_SLOTS; i++) {
    if (s_slots[i].window != window) { continue; }
    drop(window, &s_slots[i].tap);
    drop(window, &s_slots[i].pan);
    s_slots[i] = (TouchSlot){0};
  }
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
