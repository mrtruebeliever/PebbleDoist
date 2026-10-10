#pragma once
#include <pebble.h>

// A task was completed: the short vibration the app always gave, plus -- when
// "Sound on completion" is switched on in the phone settings -- a small
// two-note "ting". Both stay quiet during Quiet Time; the sound also follows
// the watch's speaker mute.
void sfx_task_done(void);
