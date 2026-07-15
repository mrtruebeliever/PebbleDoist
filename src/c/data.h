#pragma once
#include <pebble.h>

// Fixed capacities — no malloc on the watch.
#define MAX_PROJECTS    16
#define MAX_TASKS       64
#define PROJ_ID_LEN     24   // Todoist project id (v2 ~10 digits; room to spare)
#define PROJ_NAME_LEN   32
#define TASK_ID_LEN     24   // Todoist task id
#define TASK_TITLE_LEN  128  // full task text for the detail view (was 64)
#define TASK_DUE_LEN    32   // short human-readable due label, e.g. "26 jul 08:00"

// Load state of the currently-displayed list (projects or tasks).
#define LOAD_LOADING       0
#define LOAD_OK            1
#define LOAD_ERROR         2  // fetch failed (network / token)
#define LOAD_UNCONFIGURED  3  // no token / no projects selected

// Sentinel project id for the virtual "Vandaag" list (today | overdue).
#define TODAY_PROJECT_ID "today"

// One Todoist project shown on the watch (from the user's selection).
typedef struct {
  char id[PROJ_ID_LEN];
  char name[PROJ_NAME_LEN];
  int  task_count;
} Project;

// One open task of the currently-viewed list.
typedef struct {
  char id[TASK_ID_LEN];
  char title[TASK_TITLE_LEN];
  char due[TASK_DUE_LEN];   // "" when the task has no due date
  bool done;
} Task;

// --- Load state --------------------------------------------------------------
int  data_load_state(void);
void data_set_load_state(int s);

// --- Projects ----------------------------------------------------------------
int      data_project_count(void);
void     data_set_project_count(int n);   // clamps to MAX_PROJECTS
void     data_clear_projects(void);
Project *data_project(int i);             // by index, NULL if out of range

// --- Tasks of the currently-viewed list --------------------------------------
int   data_task_count(void);
void  data_set_task_count(int n);         // clamps to MAX_TASKS
void  data_clear_tasks(void);
Task *data_task(int i);                   // by index, NULL if out of range
