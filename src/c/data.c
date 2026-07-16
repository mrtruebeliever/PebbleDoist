#include "data.h"

static Project s_projects[MAX_PROJECTS];
static int     s_project_count = 0;

static Task    s_tasks[MAX_TASKS];
static int     s_task_count = 0;

static Label   s_labels[MAX_LABELS];
static int     s_label_count = 0;

static char s_detail_id[TASK_ID_LEN]         = "";
static char s_detail_desc[DETAIL_DESC_LEN]   = "";
static char s_detail_labels[DETAIL_LABELS_LEN] = "";
static bool s_detail_ready = false;

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

bool data_remove_task_by_id(const char *id) {
  if (!id || !id[0]) return false;
  for (int i = 0; i < s_task_count; i++) {
    if (strcmp(s_tasks[i].id, id) == 0) {
      for (int j = i; j < s_task_count - 1; j++) {
        s_tasks[j] = s_tasks[j + 1];
      }
      s_task_count--;
      memset(&s_tasks[s_task_count], 0, sizeof(Task));
      return true;
    }
  }
  return false;
}

// --- Labels ------------------------------------------------------------------

int data_label_count(void) { return s_label_count; }

void data_set_label_count(int n) {
  if (n < 0) n = 0;
  if (n > MAX_LABELS) n = MAX_LABELS;
  s_label_count = n;
}

void data_clear_labels(void) {
  s_label_count = 0;
  memset(s_labels, 0, sizeof(s_labels));
}

Label *data_label(int i) {
  if (i < 0 || i >= MAX_LABELS) return NULL;
  return &s_labels[i];
}

// --- On-demand task detail ---------------------------------------------------

void data_set_task_detail(const char *id, const char *desc, const char *labels) {
  strncpy(s_detail_id, id ? id : "", sizeof(s_detail_id) - 1);
  s_detail_id[sizeof(s_detail_id) - 1] = '\0';
  strncpy(s_detail_desc, desc ? desc : "", sizeof(s_detail_desc) - 1);
  s_detail_desc[sizeof(s_detail_desc) - 1] = '\0';
  strncpy(s_detail_labels, labels ? labels : "", sizeof(s_detail_labels) - 1);
  s_detail_labels[sizeof(s_detail_labels) - 1] = '\0';
  s_detail_ready = true;
}

void data_clear_task_detail(void) {
  s_detail_ready = false;
  s_detail_id[0] = '\0';
  s_detail_desc[0] = '\0';
  s_detail_labels[0] = '\0';
}

bool        data_detail_ready(void)  { return s_detail_ready; }
const char *data_detail_id(void)     { return s_detail_id; }
const char *data_detail_desc(void)   { return s_detail_desc; }
const char *data_detail_labels(void) { return s_detail_labels; }
