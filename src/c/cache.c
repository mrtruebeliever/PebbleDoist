#include "cache.h"

// Persist keys — kept clear of config.h's 1..4 settings range. One record per
// key (a Project is ~60 B, a Task ~89 B; both well under PERSIST_DATA_MAX_LENGTH).
#define K_PROJ_COUNT   10
#define K_PROJ_BASE    11                 // 11 .. 11 + MAX_PROJECTS - 1  (max 26)
#define K_TODAY_COUNT  40
#define K_TODAY_BASE   41                 // 41 .. 41 + CACHE_MAX_TODAY - 1

// Cap cached Vandaag rows to keep the total persist footprint small. The list
// still shows all fetched tasks live; only the instant-paint cache is trimmed.
#define CACHE_MAX_TODAY 20

// --- Project overview --------------------------------------------------------

void cache_save_projects(void) {
  int n = data_project_count();
  if (n > MAX_PROJECTS) n = MAX_PROJECTS;
  for (int i = 0; i < n; i++) {
    Project *p = data_project(i);
    if (p) persist_write_data(K_PROJ_BASE + i, p, sizeof(Project));
  }
  persist_write_int(K_PROJ_COUNT, n);
}

bool cache_load_projects(void) {
  if (!persist_exists(K_PROJ_COUNT)) return false;
  int n = persist_read_int(K_PROJ_COUNT);
  if (n <= 0) return false;
  if (n > MAX_PROJECTS) n = MAX_PROJECTS;

  data_clear_projects();
  int loaded = 0;
  for (int i = 0; i < n; i++) {
    Project *p = data_project(i);
    if (p && persist_exists(K_PROJ_BASE + i) &&
        persist_read_data(K_PROJ_BASE + i, p, sizeof(Project)) == (int)sizeof(Project)) {
      loaded++;
    } else {
      break;  // stop at the first gap so we never expose an empty row
    }
  }
  data_set_project_count(loaded);
  return loaded > 0;
}

// --- Vandaag task list -------------------------------------------------------

static bool is_today(const char *list_id) {
  return list_id && strcmp(list_id, TODAY_PROJECT_ID) == 0;
}

void cache_save_tasks(const char *list_id) {
  if (!is_today(list_id)) return;
  int n = data_task_count();
  if (n > CACHE_MAX_TODAY) n = CACHE_MAX_TODAY;
  for (int i = 0; i < n; i++) {
    Task *t = data_task(i);
    if (t) persist_write_data(K_TODAY_BASE + i, t, sizeof(Task));
  }
  persist_write_int(K_TODAY_COUNT, n);   // may be 0: an empty Vandaag is a valid cache
}

bool cache_load_tasks(const char *list_id) {
  if (!is_today(list_id)) return false;
  if (!persist_exists(K_TODAY_COUNT)) return false;
  int n = persist_read_int(K_TODAY_COUNT);
  if (n < 0) return false;
  if (n > CACHE_MAX_TODAY) n = CACHE_MAX_TODAY;

  data_clear_tasks();
  int loaded = 0;
  for (int i = 0; i < n; i++) {
    Task *t = data_task(i);
    if (t && persist_exists(K_TODAY_BASE + i) &&
        persist_read_data(K_TODAY_BASE + i, t, sizeof(Task)) == (int)sizeof(Task)) {
      loaded++;
    } else {
      break;
    }
  }
  data_set_task_count(loaded);
  return true;   // a known (possibly empty) Vandaag cache means "paint now, no spinner"
}
