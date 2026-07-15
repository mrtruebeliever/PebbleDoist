# Changelog

All notable changes to PebbleDoist are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
