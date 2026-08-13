#pragma once
#include <pebble.h>

// A transient window listing the user's Todoist labels. Selecting one opens a
// task list filtered to that label. Pushed from the project overview's "Labels" row.
void label_list_push(void);

void label_list_reload(void);
bool label_list_is_shown(void);

// Raw touch, forwarded from the app-wide subscription while this window is on top.
bool label_list_is_top(void);
void label_list_handle_touch(const TouchEvent *event);
void label_list_destroy(void);
