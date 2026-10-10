#include "sfx.h"
#include "config.h"

// C6 up to G6, the second note left to fade: bright enough for a watch
// speaker, short enough to be done before the list has redrawn.
static const SpeakerNote DONE[] = {
  {84, SpeakerWaveformSine, 60, 90, 0},
  {91, SpeakerWaveformSine, 90, 80, 0},
  {91, SpeakerWaveformSine, 140, 24, 0},
};
#define DONE_VOLUME 60

void sfx_task_done(void) {
  if (quiet_time_is_active()) { return; }
  vibes_short_pulse();
  if (!config_done_sound() || speaker_is_muted()) { return; }
  speaker_play_notes(DONE, sizeof(DONE) / sizeof(DONE[0]), DONE_VOLUME);
}
