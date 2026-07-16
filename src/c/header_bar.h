#pragma once
#include <pebble.h>

// A branded top bar shared by the list windows (overview / task list / label list):
// filled in the accent color, showing the app icon, name, open-task count and time.
// Replaces the plain system StatusBarLayer. Custom-drawn so the time is kept fresh
// via a single app-wide minute tick (no per-window subscription).

#define HEADER_BAR_HEIGHT 28

// Which open-task count the badge shows for the active window.
typedef enum {
  HEADER_COUNT_NONE,            // no badge (e.g. the label list)
  HEADER_COUNT_PROJECTS_TOTAL,  // open tasks across all projects (the overview)
  HEADER_COUNT_TASKS,           // tasks in the current list (a task list)
} HeaderCount;

// App lifecycle: load the icon + subscribe the minute tick / unsubscribe + unload.
void   header_bar_init(void);
void   header_bar_deinit(void);

// Creates a header layer (0,0,width,HEADER_BAR_HEIGHT) with the shared draw proc.
// The caller adds it to its window root and destroys it on unload.
Layer *header_bar_create(int width);

// Registers which header layer is currently visible, the title it should show
// (app name on the overview, project/label/Today name on a task list, "Labels" on
// the label list), and which open-task count the badge reflects. Call from each
// window's `appear` handler.
void   header_bar_set_active(Layer *hb, const char *title, HeaderCount count);

// Marks the active header dirty (e.g. after project counts change). Safe if none.
void   header_bar_refresh(void);
