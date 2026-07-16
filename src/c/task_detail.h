#pragma once
#include "data.h"

// Pushes a read-only detail window showing a task's full text and, if present,
// its due date. Takes a snapshot of the task, then asks the phone for the task's
// description + labels; those are added to the view when the reply arrives.
void task_detail_push(const Task *t);

// Rebuilds the detail window with the latest fetched description + labels.
void task_detail_reload(void);

// True while the detail window is on top (so incoming replies know to reload it).
bool task_detail_is_shown(void);
