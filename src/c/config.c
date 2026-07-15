#include "config.h"
#include "data.h"
#include "cache.h"
#include "i18n.h"

static void (*s_change_cb)(void) = NULL;

static int  s_lang = LANG_EN;
static int  s_start_view = START_VIEW_OVERVIEW;
static char s_default_proj_id[PROJ_ID_LEN]   = "";
static char s_default_proj_name[PROJ_NAME_LEN] = "";

// Which task list the watch last asked for, and the record totals announced by
// the head of the current project/task streams. Used to persist a list's cache
// exactly once, when its final record has arrived (-1 = no stream in flight).
static char s_req_task_list[PROJ_ID_LEN] = "";
static int  s_proj_total = -1;
static int  s_task_total = -1;

// Maps the watch's system locale (e.g. "en_US", "nl_NL") to a UI language.
static int detect_locale_lang(void) {
  const char *loc = i18n_get_system_locale();
  if (loc && loc[0] && loc[1]) {
    if (loc[0] == 'n' && loc[1] == 'l') return LANG_NL;
    if (loc[0] == 'f' && loc[1] == 'r') return LANG_FR;
    if (loc[0] == 'd' && loc[1] == 'e') return LANG_DE;
    if (loc[0] == 'e' && loc[1] == 's') return LANG_ES;
  }
  return LANG_EN;
}

// Turns a stored/received setting into an effective language: LANG_AUTO (or any
// out-of-range value) follows the system locale; 0..LANG_COUNT-1 are explicit.
static int resolve_lang(int stored) {
  if (stored >= 0 && stored < LANG_COUNT) return stored;
  return detect_locale_lang();
}

int         config_lang(void)                { return s_lang; }
int         config_start_view(void)          { return s_start_view; }
const char *config_default_project_id(void)  { return s_default_proj_id; }
const char *config_default_project_name(void){ return s_default_proj_name; }

void config_set_change_callback(void (*cb)(void)) { s_change_cb = cb; }

void config_load(void) {
  // Default (nothing stored yet) is LANG_AUTO -> follow the watch's locale.
  int stored = persist_exists(PERSIST_LANG) ? persist_read_int(PERSIST_LANG) : LANG_AUTO;
  s_lang = resolve_lang(stored);
  if (persist_exists(PERSIST_START_VIEW)) {
    s_start_view = persist_read_int(PERSIST_START_VIEW);
    if (s_start_view < 0 || s_start_view > START_VIEW_PROJECT) {
      s_start_view = START_VIEW_OVERVIEW;
    }
  }
  if (persist_exists(PERSIST_DEFAULT_PROJ_ID)) {
    persist_read_string(PERSIST_DEFAULT_PROJ_ID, s_default_proj_id, sizeof(s_default_proj_id));
  }
  if (persist_exists(PERSIST_DEFAULT_PROJ_NM)) {
    persist_read_string(PERSIST_DEFAULT_PROJ_NM, s_default_proj_name, sizeof(s_default_proj_name));
  }
}

// Copies one project's fields out of a per-project AppMessage.
static void apply_project(DictionaryIterator *iter) {
  Tuple *t_idx = dict_find(iter, MESSAGE_KEY_PROJ_INDEX);
  if (!t_idx) return;
  int idx = t_idx->value->int32;
  Project *p = data_project(idx);
  if (!p) return;

  if (idx >= data_project_count()) {
    data_set_project_count(idx + 1);  // reveal rows progressively
  }

  Tuple *t;
  if ((t = dict_find(iter, MESSAGE_KEY_PROJ_ID))) {
    strncpy(p->id, t->value->cstring, PROJ_ID_LEN - 1);
    p->id[PROJ_ID_LEN - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PROJ_NAME))) {
    strncpy(p->name, t->value->cstring, PROJ_NAME_LEN - 1);
    p->name[PROJ_NAME_LEN - 1] = '\0';
  }
  p->task_count = (t = dict_find(iter, MESSAGE_KEY_PROJ_TASKCOUNT)) ? t->value->int32 : 0;
}

// Copies one task's fields out of a per-task AppMessage.
static void apply_task(DictionaryIterator *iter) {
  Tuple *t_idx = dict_find(iter, MESSAGE_KEY_TASK_INDEX);
  if (!t_idx) return;
  int idx = t_idx->value->int32;
  Task *tk = data_task(idx);
  if (!tk) return;

  if (idx >= data_task_count()) {
    data_set_task_count(idx + 1);  // reveal rows progressively
  }

  Tuple *t;
  if ((t = dict_find(iter, MESSAGE_KEY_TASK_ID))) {
    strncpy(tk->id, t->value->cstring, TASK_ID_LEN - 1);
    tk->id[TASK_ID_LEN - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_TASK_TITLE))) {
    strncpy(tk->title, t->value->cstring, TASK_TITLE_LEN - 1);
    tk->title[TASK_TITLE_LEN - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_TASK_DUE))) {
    strncpy(tk->due, t->value->cstring, TASK_DUE_LEN - 1);
    tk->due[TASK_DUE_LEN - 1] = '\0';
  }
  tk->done = (t = dict_find(iter, MESSAGE_KEY_TASK_DONE)) ? (t->value->int32 != 0) : false;
}

void config_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;

  if ((t = dict_find(iter, MESSAGE_KEY_LANGUAGE))) {
    int v = t->value->int32;                 // may be LANG_AUTO
    persist_write_int(PERSIST_LANG, v);
    s_lang = resolve_lang(v);
  }

  // Persisted quick-launch settings from Clay.
  if ((t = dict_find(iter, MESSAGE_KEY_START_VIEW))) {
    s_start_view = t->value->int32;
    if (s_start_view < 0 || s_start_view > START_VIEW_PROJECT) {
      s_start_view = START_VIEW_OVERVIEW;
    }
    persist_write_int(PERSIST_START_VIEW, s_start_view);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_DEFAULT_PROJECT_ID))) {
    strncpy(s_default_proj_id, t->value->cstring, sizeof(s_default_proj_id) - 1);
    s_default_proj_id[sizeof(s_default_proj_id) - 1] = '\0';
    persist_write_string(PERSIST_DEFAULT_PROJ_ID, s_default_proj_id);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_DEFAULT_PROJECT_NAME))) {
    strncpy(s_default_proj_name, t->value->cstring, sizeof(s_default_proj_name) - 1);
    s_default_proj_name[sizeof(s_default_proj_name) - 1] = '\0';
    persist_write_string(PERSIST_DEFAULT_PROJ_NM, s_default_proj_name);
  }

  if ((t = dict_find(iter, MESSAGE_KEY_LOAD_STATE))) {
    data_set_load_state(t->value->int32);
  }

  // Head messages reset the relevant list; items arrive in following messages.
  // The announced total lets us persist the list's cache once it is complete.
  if ((t = dict_find(iter, MESSAGE_KEY_PROJ_COUNT))) {
    data_clear_projects();
    s_proj_total = t->value->int32;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_TASK_COUNT))) {
    data_clear_tasks();
    s_task_total = t->value->int32;
  }

  // Per-record streams (one message at a time).
  apply_project(iter);
  apply_task(iter);

  // Once every announced record has arrived, snapshot the list to the cache so
  // the next visit paints instantly. Runs once per stream (total reset to -1).
  // An empty list (total 0) completes here at the head, saving an empty cache.
  if (s_proj_total >= 0 && data_project_count() >= s_proj_total) {
    cache_save_projects();
    s_proj_total = -1;
  }
  if (s_task_total >= 0 && data_task_count() >= s_task_total) {
    cache_save_tasks(s_req_task_list);
    s_task_total = -1;
  }

  if (s_change_cb) {
    s_change_cb();
  }
}

// --- Outbox helpers ----------------------------------------------------------

void config_request_projects(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  int one = 1;
  dict_write_int(out, MESSAGE_KEY_REQUEST_PROJECTS, &one, sizeof(int), true);
  app_message_outbox_send();
}

void config_request_tasks(const char *project_id) {
  // Remember which list this stream belongs to so its completion can be cached.
  strncpy(s_req_task_list, project_id ? project_id : "", sizeof(s_req_task_list) - 1);
  s_req_task_list[sizeof(s_req_task_list) - 1] = '\0';

  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  int one = 1;
  dict_write_int(out, MESSAGE_KEY_REQUEST_TASKS, &one, sizeof(int), true);
  dict_write_cstring(out, MESSAGE_KEY_PROJECT_ID, project_id ? project_id : "");
  app_message_outbox_send();
}

void config_add_task(const char *project_id, const char *content) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_cstring(out, MESSAGE_KEY_ADD_TASK, content ? content : "");
  dict_write_cstring(out, MESSAGE_KEY_TASK_CONTENT, content ? content : "");
  dict_write_cstring(out, MESSAGE_KEY_PROJECT_ID, project_id ? project_id : "");
  app_message_outbox_send();
}

void config_close_task(const char *task_id) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_cstring(out, MESSAGE_KEY_CLOSE_TASK, task_id ? task_id : "");
  app_message_outbox_send();
}

void config_delete_task(const char *task_id) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_cstring(out, MESSAGE_KEY_DELETE_TASK, task_id ? task_id : "");
  app_message_outbox_send();
}

void config_request_refresh(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  int one = 1;
  dict_write_int(out, MESSAGE_KEY_REFRESH, &one, sizeof(int), true);
  app_message_outbox_send();
}
