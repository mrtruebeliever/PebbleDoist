#pragma once
#include <pebble.h>

// Touch navigation, done by hand from the raw touch stream.
//
// The system gesture bridge (app_touch_navigation_enable) crashes the app on
// firmware 4.33.1, so every window drives touch itself: this layer turns the
// raw Touchdown/PositionUpdate/Liftoff stream into finger-following kinetic
// scrolling plus the two discrete gestures a window has to act on.
//
// One window is active at a time; the layer keeps the drag and momentum state
// for it. Windows call touch_nav_reset() when they disappear.

typedef enum {
  TOUCH_NAV_NONE = 0,   // nothing to do (drag in progress, or a stray gesture)
  TOUCH_NAV_TAP,        // finger went down and up in the same spot
  TOUCH_NAV_BACK,       // left-to-right swipe: go back one window
  // Vertical flicks, reported only for a window with nothing to scroll (pass
  // NULL as the scroll layer). A scrolling window swallows them as the drag.
  TOUCH_NAV_SWIPE_UP,
  TOUCH_NAV_SWIPE_DOWN,
} TouchNavGesture;

// Row-height callback: the MenuLayer callback of the same shape, passed straight in.
typedef int16_t (*TouchNavRowHeight)(MenuLayer *menu, MenuIndex *index, void *ctx);

// Called once the list has come to rest after a drag or glide, so a window can
// bring its selection back to what the user is now looking at.
typedef void (*TouchNavSettled)(void);
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
