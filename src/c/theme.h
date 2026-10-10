#pragma once
#include <pebble.h>

// Todoist-ish red accent, used for highlight rows and the status bar.
#define ACCENT_HEX 0xE44332

static inline GColor theme_accent(void) { return GColorFromHEX(ACCENT_HEX); }

// The selected row. Black-and-white has no red: the accent would land on black
// or a dither, so the highlight is black there and the row's text inverts.
static inline GColor theme_highlight_bg(void) {
  return PBL_IF_COLOR_ELSE(theme_accent(), GColorBlack);
}

// Primary text and line art on a list row. Dark on the red highlight reads
// better in low light than white; on black-and-white it has to turn white.
static inline GColor theme_row_fg(bool hl) {
  return PBL_IF_COLOR_ELSE(GColorBlack, hl ? GColorWhite : GColorBlack);
}

// Secondary text on a row (subtitles, due dates): grey until selected.
static inline GColor theme_row_dim(bool hl) {
  return PBL_IF_COLOR_ELSE(hl ? GColorBlack : GColorDarkGray, theme_row_fg(hl));
}

// Accent-coloured text on a row (the Undo hint), which the highlight would
// swallow, so it takes the row's foreground once selected.
static inline GColor theme_row_accent(bool hl) {
  return hl ? theme_row_fg(true) : PBL_IF_COLOR_ELSE(theme_accent(), GColorBlack);
}

// Accent text off the list (the detail view's due date and section header).
static inline GColor theme_accent_text(void) {
  return PBL_IF_COLOR_ELSE(theme_accent(), GColorBlack);
}

// Inset of a row that sits on the screen's middle line -- the focused one, since
// round lists keep the selection centred.
#define THEME_FOCUSED_ROW_INSET PBL_IF_ROUND_ELSE(6, 0)

// The part of a list row its content is drawn in. The lists span the full
// width, so on a round screen the highlight runs out to the bezel; the content
// is inset to the circle's width at the row's current height on screen, so the
// rows slide along the edge as they scroll, like the system launcher.
static inline GRect theme_row_bounds(const Layer *cell) {
  GRect b = layer_get_bounds(cell);
#if defined(PBL_ROUND)
  GRect s = layer_convert_rect_to_screen(cell, b);
  int32_t r = PBL_DISPLAY_WIDTH / 2;
  int32_t dy = s.origin.y + s.size.h / 2 - r;
  if (dy < 0) dy = -dy;
  if (dy > r) dy = r;
  int32_t half = 0;
  while ((half + 1) * (half + 1) + dy * dy <= r * r) half++;   // isqrt, a few dozen steps
  int16_t inset = (int16_t)(r - half + THEME_FOCUSED_ROW_INSET);
  if (inset > r / 2) inset = r / 2;
  b.origin.x += inset;
  b.size.w -= 2 * inset;
#endif
  return b;
}
