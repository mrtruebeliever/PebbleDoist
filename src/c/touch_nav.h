#pragma once
#include <pebble.h>

// Which of the two touch paths the app uses.
//
//   1 = the system gesture bridge (app_touch_navigation_enable). Taps become
//       SELECT, a swipe right becomes BACK, and a MenuLayer scrolls the way the
//       system UI does. Needs firmware >= 4.33.2: on 4.33.1 the bridge faulted
//       inside firmware on the first touch of any third-party app, because
//       touch_nav_dispatch() read the kernel's nav gates straight from the
//       unprivileged app task (PebbleOS issue #1865, fixed by PR #1866 — the
//       gates and the recognizer ticks now go through syscalls).
//   0 = the hand-rolled layer below, which is what the app shipped in 1.4.0
//       when the bridge was unusable.
//
// The two are mutually exclusive: with the bridge on, a window must not also
// act on the raw stream or every tap would fire twice.
#define TOUCH_NAV_SYSTEM_BRIDGE 1

// Touch navigation, done by hand from the raw touch stream.
//
// Used when TOUCH_NAV_SYSTEM_BRIDGE is 0: every window drives touch itself,
// turning the raw Touchdown/PositionUpdate/Liftoff stream into finger-following
// kinetic scrolling plus the discrete gestures a window has to act on — including
// swipe-left for a row's action menu, which the system bridge has no notion of.
//
// One window is active at a time; the layer keeps the drag and momentum state
// for it. Windows call touch_nav_reset() when they disappear.

typedef enum {
  TOUCH_NAV_NONE = 0,   // nothing to do (drag in progress, or a stray gesture)
  TOUCH_NAV_TAP,        // finger went down and up in the same spot
  TOUCH_NAV_BACK,       // left-to-right swipe: go back one window
  // Right-to-left swipe: show what can be done with the row under the finger
  // (out_point holds the touchdown point, as it does for a tap).
  TOUCH_NAV_ACTIONS,
  // Vertical flicks, reported only for a window with nothing to scroll (pass
  // NULL as the scroll layer). A scrolling window swallows them as the drag.
  TOUCH_NAV_SWIPE_UP,
  TOUCH_NAV_SWIPE_DOWN,
} TouchNavGesture;

// Called once the list has come to rest after a drag or glide, so a window can
// bring its selection back to what the user is now looking at. Used by both
// touch paths.
typedef void (*TouchNavSettled)(void);

// Taps on top of the system bridge.
//
// With TOUCH_NAV_SYSTEM_BRIDGE on, the firmware drives a MenuLayer or a
// ScrollLayer itself, and its tap handling is not what this app wants:
//
//   MenuLayer   — a tap on an unselected row only moves the selection; it takes
//                 a second tap to open the row.
//   ScrollLayer — taps are dropped outright ("a ScrollLayer has no tap action"),
//                 so the subtasks in the detail view cannot be hit at all.
//
// A tap recognizer of our own, marked simultaneous, runs alongside the system
// one instead of losing to it, so a window can act on the first tap. The system
// keeps everything else: scrolling, swipe-back, and the ActionMenu.
//
// The same goes for scrolling a list: the firmware leaves the selection exactly
// where it was, so it can end up off-screen with nothing highlighted. A pan
// recognizer of our own calls the window's settled handler on liftoff, which is
// where the list brings its selection back to the row now in the middle.
//
// Both recognizers run alongside the system's but not alongside each other, and
// a tap is dropped once the pan has taken the gesture. Their movement budgets
// are the same 10px, so without that a single quick nudge completes both —
// which reads as the watch scrolling and selecting on its own.
//
// Attach in window_load, release in window_unload.
typedef void (*TouchTapHandler)(GPoint point);
void touch_tap_attach(Window *window, TouchTapHandler handler);
void touch_settle_attach(Window *window, TouchNavSettled handler);
void touch_gestures_release(Window *window);

// Same-tap de-duplication for a MenuLayer window. The system runs first, and on
// a row that was already selected it activates it before our handler is called,
// so without this one tap would fire the row's action twice. A window notes
// every activation (from a button or from touch alike) and its tap handler asks
// whether one just happened.
void touch_tap_note_action(void);
bool touch_tap_action_just_ran(void);

// Row-height callback: the MenuLayer callback of the same shape, passed straight in.
typedef int16_t (*TouchNavRowHeight)(MenuLayer *menu, MenuIndex *index, void *ctx);

void touch_nav_set_settled_handler(TouchNavSettled handler);

// Feeds one raw touch event. `scroll` is scrolled to follow the finger and
// keeps gliding after liftoff; pass NULL for a window that must not scroll.
// On a tap, *out_point receives the touchdown point (screen coordinates).
TouchNavGesture touch_nav_feed(ScrollLayer *scroll, const TouchEvent *event, GPoint *out_point);

// Stops any glide and forgets the drag. Call from window_disappear, and before
// destroying the scroll layer being driven.
void touch_nav_reset(void);

// The row of `menu` under `p`, or -1 when the point misses the list. Walks the
// row heights, so it is correct for lists with variable-height rows.
int touch_nav_row_at(MenuLayer *menu, GPoint p, uint16_t num_rows, TouchNavRowHeight height_cb);
