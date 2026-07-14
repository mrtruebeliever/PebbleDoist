#pragma once
#include <pebble.h>

// Todoist-ish red accent, used for highlight rows and the status bar.
#define ACCENT_HEX 0xE44332

static inline GColor theme_accent(void) { return GColorFromHEX(ACCENT_HEX); }
