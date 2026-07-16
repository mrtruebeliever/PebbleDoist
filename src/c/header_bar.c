#include "header_bar.h"
#include "data.h"
#include "theme.h"

static GBitmap    *s_icon   = NULL;
static Layer      *s_active = NULL;    // the header currently on screen (tick/refresh target)
static char        s_title[32] = "";   // context title shown in the bar
static HeaderCount s_count = HEADER_COUNT_NONE;

// The badge value for the active window: -1 = hide, else the count to show.
static int badge_count(void) {
  switch (s_count) {
    case HEADER_COUNT_TASKS:
      return data_task_count();
    case HEADER_COUNT_PROJECTS_TOTAL: {
      int total = 0;
      int n = data_project_count();
      for (int i = 0; i < n; i++) {
        Project *p = data_project(i);
        if (p) total += p->task_count;
      }
      return total;
    }
    default:
      return -1;
  }
}

static void header_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  // Accent fill + a thin black bottom separator.
  graphics_context_set_fill_color(ctx, theme_accent());
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, b.size.h - 1), GPoint(b.size.w, b.size.h - 1));

  graphics_context_set_text_color(ctx, GColorBlack);
  GFont f14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f18 = fonts_get_system_font(FONT_KEY_GOTHIC_18);

  // Icon (left, vertically centered).
  int name_x = 6;
  if (s_icon) {
    GRect ib = gbitmap_get_bounds(s_icon);
    int iy = (b.size.h - ib.size.h) / 2;
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_icon, GRect(3, iy, ib.size.w, ib.size.h));
    name_x = 3 + ib.size.w + 4;
  }

  // Time (right-aligned).
  char time_buf[16];
  clock_copy_time_string(time_buf, sizeof(time_buf));
  const int TIME_W = 56;
  int time_left = b.size.w - 4 - TIME_W;
  graphics_draw_text(ctx, time_buf, f18, GRect(time_left, 4, TIME_W, 20),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  // Open-task count badge (dot + number), just left of the time. Hidden when <= 0.
  int name_right = time_left - 6;
  int total = badge_count();
  if (total > 0) {
    char cnt[8];
    snprintf(cnt, sizeof(cnt), "%d", total);
    GSize cs = graphics_text_layout_get_content_size(
        cnt, f14, GRect(0, 0, 40, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    int block_w = 6 + 2 + cs.w;           // dot + gap + number
    int block_right = time_left - 6;
    int block_left = block_right - block_w;
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, GPoint(block_left + 3, b.size.h / 2), 3);
    graphics_draw_text(ctx, cnt, f14, GRect(block_left + 8, 6, cs.w + 2, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    name_right = block_left - 6;
  }

  // Context title (middle, ellipsized to whatever space is left).
  if (s_title[0] && name_right - name_x > 12) {
    graphics_draw_text(ctx, s_title, f14, GRect(name_x, 6, name_right - name_x, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (s_active) { layer_mark_dirty(s_active); }
}

void header_bar_init(void) {
  s_icon = gbitmap_create_with_resource(RESOURCE_ID_IMG_HEADER_ICON);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

void header_bar_deinit(void) {
  tick_timer_service_unsubscribe();
  if (s_icon) { gbitmap_destroy(s_icon); s_icon = NULL; }
}

Layer *header_bar_create(int width) {
  Layer *layer = layer_create(GRect(0, 0, width, HEADER_BAR_HEIGHT));
  layer_set_update_proc(layer, header_update);
  return layer;
}

void header_bar_set_active(Layer *hb, const char *title, HeaderCount count) {
  s_active = hb;
  s_count = count;
  strncpy(s_title, title ? title : "", sizeof(s_title) - 1);
  s_title[sizeof(s_title) - 1] = '\0';
  if (s_active) { layer_mark_dirty(s_active); }
}

void header_bar_refresh(void) {
  if (s_active) { layer_mark_dirty(s_active); }
}
