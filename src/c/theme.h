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

// Horizontal inset of the lists on a round screen, where only the focused row in
// the middle has the full width. Insetting the MenuLayer itself keeps every
// row's own coordinates as they are.
#define THEME_LIST_INSET PBL_IF_ROUND_ELSE(22, 0)
