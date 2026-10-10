#pragma once
#include <pebble.h>

// The app's line in the launcher ("app glance"): how many tasks are due today
// or overdue, e.g. "Vandaag: 5". Called when the app closes.
void glance_update(void);
