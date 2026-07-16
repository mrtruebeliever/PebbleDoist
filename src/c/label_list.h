#pragma once
#include <pebble.h>

// A transient window listing the user's Todoist labels. Selecting one opens a
// task list filtered to that label. Pushed from the project overview's "Labels" row.
void label_list_push(void);

void label_list_reload(void);
bool label_list_is_shown(void);
void label_list_destroy(void);
