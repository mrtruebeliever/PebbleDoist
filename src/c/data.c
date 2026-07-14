#include "data.h"

static Project s_projects[MAX_PROJECTS];
static int     s_project_count = 0;

static Task    s_tasks[MAX_TASKS];
static int     s_task_count = 0;

static int     s_load_state = LOAD_LOADING;

// --- Load state --------------------------------------------------------------

int  data_load_state(void)      { return s_load_state; }
void data_set_load_state(int s) { s_load_state = s; }

// --- Projects ----------------------------------------------------------------

int data_project_count(void) { return s_project_count; }

void data_set_project_count(int n) {
  if (n < 0) n = 0;
  if (n > MAX_PROJECTS) n = MAX_PROJECTS;
  s_project_count = n;
}

void data_clear_projects(void) {
  s_project_count = 0;
  memset(s_projects, 0, sizeof(s_projects));
}

Project *data_project(int i) {
  if (i < 0 || i >= MAX_PROJECTS) return NULL;
  return &s_projects[i];
}

// --- Tasks -------------------------------------------------------------------

int data_task_count(void) { return s_task_count; }

void data_set_task_count(int n) {
  if (n < 0) n = 0;
  if (n > MAX_TASKS) n = MAX_TASKS;
  s_task_count = n;
}

void data_clear_tasks(void) {
  s_task_count = 0;
  memset(s_tasks, 0, sizeof(s_tasks));
}

Task *data_task(int i) {
  if (i < 0 || i >= MAX_TASKS) return NULL;
  return &s_tasks[i];
}
