#pragma once
#include <pebble.h>

// How a task list behaves: a real project (has an "add task" row), the virtual
// "Vandaag" list, or a label-filtered list. Today/Label lists have no add row
// (no single target project to add into).
typedef enum {
  TASK_LIST_PROJECT,
  TASK_LIST_TODAY,
  TASK_LIST_LABEL,
} TaskListMode;

// Pushes the task list for one list. `list_id` is the Todoist project id, or
// TODAY_PROJECT_ID for Vandaag, or the "label:<name>" sentinel for a label list.
// `title` is shown as the header.
void task_list_push(const char *list_id, const char *title, TaskListMode mode);

void task_list_reload(void);
bool task_list_is_shown(void);
void task_list_destroy(void);
