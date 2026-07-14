#pragma once
#include <pebble.h>

// Pushes the task list for one project (or the virtual "Vandaag" list).
// `project_id` is the Todoist project id, or TODAY_PROJECT_ID for Vandaag.
// `title` is shown as the header. When `is_today` is true there is no "new task"
// row (a today item has no target project to add into).
void task_list_push(const char *project_id, const char *title, bool is_today);

void task_list_reload(void);
bool task_list_is_shown(void);
void task_list_destroy(void);
