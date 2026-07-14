#pragma once
#include <pebble.h>
#include "data.h"

// Persist-backed cache for the two most-visited lists: the project overview
// (the screen every launch lands on) and the Vandaag (today) task list. This
// lets those screens paint instantly from last-known data while a fresh fetch
// runs in the background; the fresh stream overwrites both the view and the
// cache when it completes.
//
// Only these two lists are cached — other projects still fetch from scratch —
// to stay well within the app's small persistent-storage budget.

// --- Project overview --------------------------------------------------------
void cache_save_projects(void);   // snapshot the current data_project() rows
bool cache_load_projects(void);   // fill data from cache; true if any were loaded

// --- Vandaag task list -------------------------------------------------------
// Keyed by list id so call sites can pass the id through blindly; only
// TODAY_PROJECT_ID is actually cached (other ids are a no-op / return false).
void cache_save_tasks(const char *list_id);
bool cache_load_tasks(const char *list_id);
