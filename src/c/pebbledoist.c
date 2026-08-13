#include <pebble.h>
#include "config.h"
#include "data.h"
#include "cache.h"
#include "i18n.h"
#include "dictation_flow.h"
#include "project_list.h"
#include "task_list.h"
#include "label_list.h"
#include "task_detail.h"
#include "header_bar.h"

// Fired by config_inbox_received whenever incoming data changes: redraw
// whichever window is currently visible.
static void on_data_changed(void) {
  project_list_reload();
  if (task_list_is_shown()) {
    task_list_reload();
  }
  if (label_list_is_shown()) {
    label_list_reload();
  }
  if (task_detail_is_shown()) {
    task_detail_reload();
  }
  header_bar_refresh();   // open-task count may have changed
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d", (int)reason);
}

static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
  data_set_load_state(LOAD_ERROR);
  on_data_changed();
}

// Applies the configured start view — but only when opened via Quick Launch.
// Deferred slightly so the base window is loaded and the event loop is running
// before we start a dictation session or push a second window.
static void apply_quick_launch(void *data) {
  switch (config_start_view()) {
    case START_VIEW_DICTATE:
      project_list_begin_quick_add();
      break;
    case START_VIEW_PROJECT: {
      const char *id = config_default_project_id();
      if (id[0]) {
        bool today = (strcmp(id, TODAY_PROJECT_ID) == 0);
        task_list_push(id, today ? i18n(STR_TODAY) : config_default_project_name(),
                       today ? TASK_LIST_TODAY : TASK_LIST_PROJECT);
      }
      break;
    }
    case START_VIEW_OVERVIEW:
    default:
      break;  // already showing the overview
  }
}

// Subscribing is what powers the touch sensor on, and it is the one touch path
// proven safe on firmware 4.33.1 (the system navigation bridge is not — see the
// note further down). Events go to the window on top; when that is something the
// system owns (an ActionMenu, the dictation UI), none of these match and the
// event is dropped rather than acted on behind the user's back.
static void touch_dispatch(const TouchEvent *event, void *context) {
  if (task_detail_is_top())        { task_detail_handle_touch(event); }
  else if (label_list_is_top())    { label_list_handle_touch(event); }
  else if (task_list_is_top())     { task_list_handle_touch(event); }
  else if (project_list_is_top())  { project_list_handle_touch(event); }
}

static void init(void) {
  dictation_flow_init();
  header_bar_init();
  config_load();
  // Paint the overview instantly from the cached project list; the phone's
  // "ready" handler re-fetches in the background and overwrites it. Falls back
  // to the loading spinner only on a first run with no cache yet.
  data_set_load_state(cache_load_projects() ? LOAD_OK : LOAD_LOADING);
  data_set_stall_callback(on_data_changed);
  config_set_change_callback(on_data_changed);

  app_message_register_inbox_received(config_inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(2048, 256);

  window_stack_push(project_list_window(), true);

  touch_service_subscribe(touch_dispatch, NULL);
  APP_LOG(APP_LOG_LEVEL_INFO, "touch: subscribed, enabled=%d",
          (int)touch_service_is_enabled());

  // NOTE: do not call app_touch_navigation_enable() here. On firmware 4.33.1 the
  // system touch bridge faults inside firmware on the very first touch, killing
  // the app (identical PC/LR across apps, before any app code runs). Touch in
  // this app is handled per window with gesture recognizers instead — see
  // project_list.c. Re-test the bridge on a later firmware before restoring it.

  // The start-view setting only applies to a Quick-Launch open; a normal
  // launcher start always lands on the project overview.
  if (launch_reason() == APP_LAUNCH_QUICK_LAUNCH) {
    app_timer_register(120, apply_quick_launch, NULL);
  }
}

static void deinit(void) {
  label_list_destroy();
  task_list_destroy();
  project_list_destroy();
  header_bar_deinit();
  dictation_flow_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
