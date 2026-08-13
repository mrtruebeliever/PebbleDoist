#pragma once
#include <pebble.h>

// Fixed capacities — no malloc on the watch.
#define MAX_PROJECTS    16
#define MAX_TASKS       64
#define MAX_LABELS      32
#define MAX_SUBTASKS    12   // subtasks shown for the open task's detail view
#define PROJ_ID_LEN     24   // Todoist project id (v2 ~10 digits; room to spare)
#define PROJ_NAME_LEN   32
#define TASK_ID_LEN     24   // Todoist task id
#define TASK_TITLE_LEN  128  // full task text for the detail view (was 64)
#define TASK_DUE_LEN    32   // short human-readable due label, e.g. "26 jul 08:00"
#define LABEL_NAME_LEN  32   // Todoist label name
#define SUB_TITLE_LEN   96   // subtask text (shorter than a task's: MAX_SUBTASKS of them)

// A list id can be a project id, TODAY_PROJECT_ID, or the "label:<name>" sentinel
// used by the label-filtered task view, so it must hold "label:" + a label name.
#define LIST_ID_LEN     48

// On-demand task detail (description + labels), fetched when the detail view opens.
// Not part of Task (would blow the 256 B per-record persist cap) and never cached.
#define DETAIL_DESC_LEN   512
#define DETAIL_LABELS_LEN 160

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

// One open task of the currently-viewed list. Todoist subtasks are tasks with a
// parent, and the phone keeps them out of this list; a parent only carries the
// number of open subtasks it has, shown as a counter on its row.
typedef struct {
  char    id[TASK_ID_LEN];
  char    title[TASK_TITLE_LEN];
  char    due[TASK_DUE_LEN];   // "" when the task has no due date
  bool    done;
  uint8_t sub_count;           // open subtasks (0 = none)
} Task;

// One open subtask of the task whose detail view is open.
typedef struct {
  char id[TASK_ID_LEN];
  char title[SUB_TITLE_LEN];
  bool done;                   // ticked on the watch, pending its undo window
} Subtask;

// One Todoist label shown in the label-filter list.
typedef struct {
  char name[LABEL_NAME_LEN];
} Label;

// --- Load state --------------------------------------------------------------
int  data_load_state(void);
void data_set_load_state(int s);
// Called when a load has sat in LOAD_LOADING too long and has been dropped to
// LOAD_ERROR; the app registers its redraw here.
void data_set_stall_callback(void (*cb)(void));

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
bool  data_remove_task_by_id(const char *id);  // optimistic removal; true if found

// --- Labels (label-filter list) ----------------------------------------------
int    data_label_count(void);
void   data_set_label_count(int n);       // clamps to MAX_LABELS
void   data_clear_labels(void);
Label *data_label(int i);                 // by index, NULL if out of range

// --- On-demand task detail ---------------------------------------------------
// Holds the description + labels of the task whose detail view is open. Filled by
// config_inbox_received from the phone's reply; read by the detail window.
void        data_set_task_detail(const char *id, const char *desc, const char *labels);
void        data_clear_task_detail(void); // marks the detail "not ready" (awaiting reply)
bool        data_detail_ready(void);      // true once a reply has arrived
const char *data_detail_id(void);
const char *data_detail_desc(void);
const char *data_detail_labels(void);     // comma-separated label names

// --- Subtasks of the task whose detail view is open --------------------------
// Streamed by the phone alongside the detail reply; cleared when a new detail is
// requested. Only open subtasks are sent (a completed one drops off the list).
int      data_subtask_count(void);
void     data_set_subtask_count(int n);   // clamps to MAX_SUBTASKS
void     data_clear_subtasks(void);
Subtask *data_subtask(int i);             // by index, NULL if out of range
Subtask *data_subtask_by_id(const char *id);
bool     data_remove_subtask_by_id(const char *id);  // optimistic removal
