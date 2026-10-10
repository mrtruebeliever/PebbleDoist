#include "glance.h"
#include "data.h"
#include "i18n.h"

static char s_subtitle[48];

static void reload(AppGlanceReloadSession *session, size_t limit, void *context) {
  if (limit < 1) { return; }
  // Tomorrow has its own Vandaag: drop the line at midnight rather than show
  // a count that no longer means anything.
  AppGlanceSlice slice = {
    .layout.icon = APP_GLANCE_SLICE_DEFAULT_ICON,
    .layout.subtitle_template_string = s_subtitle,
    .expiration_time = time_start_of_today() + SECONDS_PER_DAY,
  };
  AppGlanceResult r = app_glance_add_slice(session, slice);
  if (r != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "glance: add_slice %d", (int)r);
  }
}

void glance_update(void) {
  if (data_load_state() == LOAD_UNCONFIGURED) { app_glance_reload(NULL, NULL); return; }
  int n = data_today_count();
  // The phone never answered this session: keep whatever the launcher shows.
  if (n < 0) { return; }
  if (n == 0) { app_glance_reload(NULL, NULL); return; }
  snprintf(s_subtitle, sizeof(s_subtitle), "%s: %d", i18n(STR_TODAY), n);
  app_glance_reload(reload, NULL);
}
