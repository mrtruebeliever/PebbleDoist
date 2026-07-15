#pragma once
#include <pebble.h>

// UI language. Index MUST match the phone side (src/pkjs/i18n.js) and the Clay
// LANGUAGE select values.
typedef enum {
  LANG_EN = 0,
  LANG_NL = 1,
  LANG_FR = 2,
  LANG_DE = 3,
  LANG_ES = 4,
  LANG_COUNT,
} Lang;

// Sentinel stored/sent for "follow the watch's system locale" (see config.c).
#define LANG_AUTO 255

// All translatable UI strings.
typedef enum {
  STR_TODAY,          // "Today"
  STR_TODAY_SUB,      // "today & overdue"
  STR_NEW_TASK,       // "+ New task"
  STR_DICTATE,        // "dictate"
  STR_REFRESH,        // "Refresh"
  STR_COMPLETE,       // "Complete"
  STR_DELETE,         // "Delete"
  STR_LOADING_T,      // "Loading..."
  STR_LOADING_S,      // "Please wait"
  STR_ERROR_T,        // "No connection"
  STR_ERROR_S,        // "Tap to retry"
  STR_UNCONF_T,       // "Not set up"
  STR_UNCONF_S,       // "Set up on the phone"
  STR_NO_PROJECTS,    // "No projects"
  STR_NO_TASKS,       // "No tasks"
  STR_NOTHING_TODAY,  // "Nothing for today"
  STR_N_TASK,         // "%d task"   (printf format, one arg)
  STR_N_TASKS,        // "%d tasks"  (printf format, one arg)
  STR_DETAILS,        // "Details"
  STR_DUE,            // "Deadline"
  STR_COUNT,
} StrId;

// Returns the string in the currently configured language (config_lang()).
const char *i18n(StrId id);
