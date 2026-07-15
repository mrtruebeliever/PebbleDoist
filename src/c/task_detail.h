#pragma once
#include "data.h"

// Pushes a read-only detail window showing a task's full text and, if present,
// its due date. Takes a snapshot of the task, so the caller's copy may change.
void task_detail_push(const Task *t);
