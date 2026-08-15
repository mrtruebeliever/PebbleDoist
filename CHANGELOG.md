# Changelog

All notable changes to PebbleDoist are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.6.0] - 2026-08-15

### Changed
- **Back on the system touch bridge**, undoing the 1.4.0 revert. Firmware 4.33.2
  carries the fix for [PebbleOS#1865](https://github.com/coredevices/PebbleOS/issues/1865)
  (PR #1866), so `app_touch_navigation_enable(true)` no longer kills the app.
  Scrolling, swipe-back and — the reason for going back at all — touch inside the
  system **ActionMenu** now come from the firmware instead of from the app.

### Added
- **One tap acts.** The firmware's own `MenuLayer` takes two taps: the first only
  moves the selection, the second opens the row. A tap recognizer of the app's
  own, marked simultaneous so it runs alongside the system's rather than being
  skipped as a duplicate, acts on the first tap.
- **Subtasks are tappable again.** The firmware drops taps on a `ScrollLayer`
  outright ("a ScrollLayer has no tap action"), and a Tier-1 widget excludes the
  touch→button bridge, so the detail view received no taps at all.
- **The middle row highlights after a scroll.** A plain menu's selection is left
  exactly where it was by the firmware — possibly off-screen, with nothing lit and
  the next button press yanking the list back. A pan recognizer of the app's own
  reselects the row in the middle on liftoff.
- **Touch injection for the emulator** (`tools/touch.py`,
  `tools/qemu-touch-wrapper.sh`). The pebble tool has no touch command, but
  qemu-pebble does emulate the PT2 touchscreen, so touch goes in over QEMU's QMP
  socket — the same route the firmware repo's own `./pbl touch` uses. Needs SDK
  4.33.1 or newer; the 4.33 emulator image still faults on the bridge.

### Fixed
- **The 1.3.3 opt-in never actually ran.** `pebbledoist.c` did not include
  `touch_nav.h`, so `TOUCH_NAV_SYSTEM_BRIDGE` was an undefined macro — silently 0
  in `#if` — and the app kept subscribing to the raw touch stream whatever the
  header said. A raw subscriber also disables the system bridge app-wide, which
  is why the ActionMenu stayed dead to touch.
- **One nudge no longer counts twice.** The tap and pan budgets are both 10px, so
  a small quick drag could complete both recognizers — reselecting the middle row
  and opening a row that was never meant to be tapped. A started pan now takes the
  gesture and the tap stands down.

### Removed
- **Swipe right-to-left over a task no longer opens its menu.** The firmware
  ignores left swipes on a `MenuLayer` by design ("activate-on-swipe is
  intentionally disabled"), so the gesture added in 1.5.0 cannot survive on the
  system bridge. A long press of **Select** still opens the menu, from anywhere in
  the list — worth knowing with quick-complete on, where a tap ticks the task off.
- **Lists no longer coast after your finger.** The firmware settles the scroll
  offset at liftoff and has no inertia; the app's own kinetic glide only applied
  to the raw touch path.

## [1.5.0] - 2026-08-13

### Added
- **Subtasks.** A task's subtasks now live in its detail view: a "Subtasks (n)"
  section listing the open ones, each with a checkbox. Tap a row — or pick it
  from the Select menu — to tick it off; like quick-complete, it waits out a
  short undo window (tap again to undo) before it is sent to Todoist, and
  vibrates when it commits, respecting Quiet Time.
- **Subtask counter on a task row.** A task with open subtasks shows a small
  branch mark and their number on the right of its bottom line.
- **Buttons walk the subtask list.** In the detail view Up/Down page through the
  text as before, and once the subtask section comes into view they step from
  row to row (the focused row is highlighted); **Select** ticks the focused one
  off. With nothing focused, Select opens the action menu — and a long press
  always does, so "Add subtask" stays reachable from anywhere in the view.
- **Swipe left on a task** to open its menu (Complete / Details / Add subtask /
  Delete). Previously the menu was button-only: with quick-complete on, a tap
  ticks the task off, which left no way to reach Details by touch.
- **"Add subtask"** in the task menu and in the detail view's Select menu:
  dictate the text and it is created as a child of that task, in the same
  project. Dates in the spoken line are parsed exactly as for a new task.

### Changed
- **Subtasks no longer clutter a project's task list.** Todoist returns a
  project's subtasks alongside its tasks; they used to appear as ordinary rows
  with nothing to tie them to their parent. A project list now shows top-level
  tasks only, and its badge counts those. Today and label lists still show a
  subtask in its own right — a subtask due today is work you have to see.

## [1.4.0] - 2026-08-13

### Added
- **Touch navigation on the Pebble Time 2.** Tap a row to open it, drag the list
  to scroll it (it keeps gliding when you let go, and stops at the ends), and
  swipe left-to-right to go back — from the overview that leaves the app, exactly
  as the BACK button does. The selection lands on the row in the middle of the window after a
  scroll, so the buttons carry on from what you are looking at instead of
  jumping back.

### Changed
- **Reverted the 1.3.3 opt-in into the system touch bridge.** On firmware 4.33.1
  `app_touch_navigation_enable(true)` faults inside firmware on the first touch
  and kills the app, in every app that opts in. Touch is now handled from the
  raw touch stream inside the app (`src/c/touch_nav.c`), which does not go
  through the bridge. Reported as
  [PebbleOS#1865](https://github.com/coredevices/PebbleOS/issues/1865).

## [1.3.3] - 2026-08-13

### Added
- **Vibrates when a task is confirmed complete** (both quick-complete after
  its undo window and the Complete action in the task menu), respecting
  Quiet Time.

### Changed
- Opts into the watch's own touch navigation (Pebble SDK 4.33) on the
  project/task/label lists and the task detail screen.

## [1.3.2] - 2026-07-16

### Changed

- **Settings tidy-up** — the task-title size dropdown is now labelled **Task titles size**
  instead of a full sentence, and the **quick-complete explanation moved below its toggle**.
- **Smaller store download** — a release build script (`tools/release.py`) minifies the
  JavaScript bundle and strips the source map, roughly **halving** the `.pbw` size (≈397 KB →
  ≈174 KB) with no change in behavior.

## [1.3.1] - 2026-07-16

### Changed

- **Task rows redesigned** — the title now uses the **full row width** and only ellipsizes
  when genuinely long; the **due date sits on a small line below the title** (like the
  "dictate" subtitle under "+ New task"). Rows are a **fixed, compact height** per text size,
  so a task with a due date is the same height as one without — and a task with no due date
  **centres its title** vertically. Selecting a long task still scrolls (marquees) the full
  title while its due stays put.

## [1.3.0] - 2026-07-16

### Added

- **Label filtering** — a new **Labels** row on the overview opens the list of your Todoist
  labels; picking one shows every task carrying that label (across lists, prefixed with the
  list name).
- **Task detail** now shows the task's **description** and its **labels** as coloured badges,
  in addition to the title and deadline. Both are fetched on demand when the detail opens.
- **Branded top bar** — the list screens now have an accent-coloured header showing the app
  icon, a context title (app name / list name / "Labels"), the number of open tasks, and the
  time. This replaced the old narrow status bar and frees vertical space.
- **Due dates in task rows** — each task now shows its deadline on the right of the row.
- **Quick-complete** *(optional, off by default)* — turn it on in the phone settings to
  complete a task with a single **Select** press (with a short **Undo** window); long-press
  always opens the menu. When off, Select opens the menu as before.

### Changed

- **Complete is now the first action** in a task's menu (was second), and **Delete asks for
  confirmation** so an accidental press can't remove a task.
- **Completing and deleting are instant** — the task leaves the list immediately instead of
  flashing a "loading" state while the phone syncs.
- Task rows are **a little shorter with slightly larger text**; long titles wrap onto two
  lines when unselected and scroll (marquee) when selected.
- The task name moved from a separate header into the top bar, giving one more row of space.

## [1.2.0] - 2026-07-15

### Added

- **Text size setting** — task lists can be rendered **Small**, **Medium** (default) or
  **Large** via a new `Text size` option in the phone settings. Row height grows with the
  font, and the setting is persisted on the watch so it survives a restart. The overview,
  the add row and the status rows keep their fixed layout.

### Fixed

- Removed stray markup that had slipped into the end of the README.

## [1.1.0] - 2026-07-15

### Added

- **Task detail screen** — a scrollable view with the task's full text and its deadline,
  opened from the new **Details** action. The phone now streams the full title (127 chars)
  plus a compact due label.
- **Marquee** — the selected task row scrolls a long title horizontally, with the checkbox
  column masked so the text never runs under it.
- A thin accent bar under the status bar signals a background refresh.

### Changed

- **Stale-while-revalidate** — the cached list stays on screen while refreshing instead of
  collapsing to a loading row. This also removes the flash when completing or deleting.
- **Contrast** — dark text on the red highlights, headers and action menus, for better
  legibility in low light.

## [1.0.0] - 2026-07-14

Initial release.

### Added

- Browse Todoist tasks per list, with a **Today** view (due today + overdue) that prefixes
  each item with its list name.
- **Add tasks by voice**, with client-side parsing of natural-language due dates.
- **Complete** and **delete** tasks from the watch.
- **Timeline pins** (opt-in) for tasks due in the next few days, each with a reminder that
  buzzes at the deadline.
- **Quick Launch** start screen: overview, dictate right away, or a default list.
- Five languages (English, Nederlands, Français, Deutsch, Español), following the watch's
  system language by default.
- Persisted cache of the overview and Today list for an instant first paint.
- The Todoist API token stays on the phone and is never sent to the watch.
