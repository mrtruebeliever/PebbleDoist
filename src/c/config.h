#pragma once
#include <pebble.h>

// Persist storage keys.
#define PERSIST_START_VIEW       1
#define PERSIST_DEFAULT_PROJ_ID  2
#define PERSIST_DEFAULT_PROJ_NM  3
#define PERSIST_LANG             4
#define PERSIST_FONT_SIZE        5
#define PERSIST_QUICK_COMPLETE   6

// Task-list text size (mirrors the Clay FONT_SIZE select).
#define FONT_SIZE_SMALL   0
#define FONT_SIZE_MEDIUM  1  // default
#define FONT_SIZE_LARGE   2

// Quick-Launch start view (mirrors the Clay START_VIEW select).
#define START_VIEW_OVERVIEW  0  // project overview (main menu)
#define START_VIEW_DICTATE   1  // start dictation immediately (new task)
#define START_VIEW_PROJECT   2  // jump straight into the default project's task list

// Loads persisted quick-launch settings.
void config_load(void);

// Current UI language index (Lang from i18n.h).
int         config_lang(void);

// Text size of the task-list rows (FONT_SIZE_* above).
int         config_font_size(void);

// True when a plain Select on a task completes it (quick-complete). Default false.
bool        config_quick_complete(void);

// Current quick-launch settings (valid after config_load / config_inbox_received).
int         config_start_view(void);
const char *config_default_project_id(void);    // "" if unset; may be TODAY_PROJECT_ID
const char *config_default_project_name(void);  // "" if unset

// Registers a callback fired whenever incoming data changes (redraw hook).
void config_set_change_callback(void (*cb)(void));

// AppMessage inbox handler: applies load state, streamed projects/tasks, and the
// persisted quick-launch settings coming from Clay.
void config_inbox_received(DictionaryIterator *iter, void *context);

// Outbox helpers (watch -> phone).
void config_request_tasks(const char *project_id);  // load a list; TODAY_PROJECT_ID for Vandaag
void config_request_labels(void);                   // load the label list
void config_request_task_detail(const char *task_id); // fetch one task's description + labels
void config_add_task(const char *project_id, const char *content);
void config_close_task(const char *task_id);
void config_delete_task(const char *task_id);
void config_request_refresh(void);                  // reload the project list
