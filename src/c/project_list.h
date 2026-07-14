#pragma once
#include <pebble.h>

// The main window: "Vandaag" row + the user's selected Todoist projects.
Window *project_list_window(void);
void    project_list_reload(void);
void    project_list_destroy(void);

// Quick-Launch "Direct inspreken": dictate a task, then pick the target project.
void    project_list_begin_quick_add(void);
