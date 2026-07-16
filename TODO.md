# PebbleDoist — roadmap

Backlog distilled from two code/UX reviews. Items already shipped in **1.3.0** are omitted
(label filtering, task detail description + label badges, branded top bar, due dates in rows,
per-list open-task badge, quick-complete + undo toggle, optimistic complete/delete, menu
reorder + delete confirm, two-line titles, dead-code cleanup).

Effort: **S** small · **M** medium · **L** large.

## Features

- **Priority (p1–p4) display + sort** — Todoist returns `priority` on every task but the phone
  drops it. Show a coloured flag/bar on the row and sort p1 first. **M**
- **Reschedule / snooze on the watch** — a menu with *Today / Tomorrow / Next week* that calls
  the phone; use a reschedule endpoint that preserves recurrence (don't overwrite the due
  string of recurring tasks). **M**
- **Richer Today** — split *overdue* and *today* into sections and colour overdue red (the
  phone already fetches `today | overdue` together). **M**
- **Recently-completed / reopen** — a short list of just-completed tasks that can be reopened
  (Todoist has a reopen endpoint); the natural home for a longer-lived undo. **M**
- **Act from the detail view** — complete / reschedule directly in Details instead of backing
  out to the list. **S–M**
- **Project colours** — Todoist projects carry a colour; tint a leading glyph / left stripe,
  snapped to Pebble's palette. **S–M**
- **On-watch options menu** — extend the overview long-press menu (currently only *Refresh*)
  with *Sort* (priority/due) and *Text size*, so those aren't phone-only. **S**
- **AppGlance complication** — surface today's open count on the watchface via AppGlance,
  updated by the phone on refresh. **S–M**
- **Subtasks / sections** — Todoist returns `parent_id` / `section_id`; start with section
  headers, defer real nesting. **L**
- **Search / filter** — dictation-driven "find task" that filters the current list (phone does
  the matching; no keyboard). **M**
- **Quick-add from Today / label lists** — those contexts have no "+ New task"; add one that
  files to the default project. **S**

## UX / polish

- **Project opens on the first task, not "+ New task"** — the add row is row 0, so opening a
  project highlights dictation and pushes tasks below the fold. Move the add row to the bottom
  (as the overview does) or pre-select the first task. **S**
- **Overview row-type glyphs** — Today / projects / Labels / New task all look identical; add a
  small leading icon per type (calendar / folder / tag / plus) for scannability. **S–M**
- **Marquee polish** — the scrolling selected title clips its first glyph (checkbox column is
  repainted over mid-glyph text) and the 60 ms timer runs the whole time the list is open.
  Add a small left inset, start only after the selection has been still ~1 s, and stop the
  timer when the title actually fits. **M** *(also a battery/CPU win)*
- **Detail loading feedback** — description + labels are fetched on demand and currently pop
  in; show a faint "Loading…" placeholder until `data_detail_ready()`, and show the task's
  priority + project. **S**
- **Contrast** — black on the #E44332 accent is ~3.9:1; fine indoors, worth a glance in bright
  sun. **S**

## Performance & size

- **Stop per-item full reloads** — `on_data_changed` fires after every streamed item, and each
  reload re-measures text per row (≈ O(n²) for a full list); the hidden overview reloads too
  (no `is_shown` guard). Coalesce (head + final only, or throttle) and skip windows that aren't
  shown. **M**
- **Drop the dead `taskCache` + parallelize counts** — `loadProjects` does up to 16 *serial*
  full-task fetches just for counts, feeding a `taskCache` that is never read. Delete the cache
  and fire the count requests in parallel — the slowest step on cold open. **M**
- **Cache each task's two-line flag** — computed by measuring text in `get_cell_height` on
  every reload; compute once when the task is streamed and store a bool/height. **M**
- **Share the three MenuLayer windows** — `project_list` / `task_list` / `label_list` duplicate
  `list_ready`, the refresh-bar helpers, `draw_text_row`, `draw_status_row` and most of
  `window_load`; extract a shared `list_ui` module. **M–L**
- **Trim RAM** — `Task[64]` (~12 KB) dominates the ~32 KB footprint; a lower `MAX_TASKS` (~40)
  saves ~4.5 KB if acceptable. Minor: drop `s_sel_task_id` (use `s_sel_task.id`), cache
  `open_task_total()`, share `getTodayTasks`/`getLabelTasks`. **S** *(capacity is a product call)*
