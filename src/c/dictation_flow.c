#include "dictation_flow.h"

// +1 for the NUL terminator the session writes alongside the capped text.
#define SESSION_BUFFER_SIZE (TRANSCRIPT_MAX_CHARS + 1)

static DictationSession *s_session;
static char s_transcript[SESSION_BUFFER_SIZE];
static DictationResultCallback s_callback;
static bool s_in_progress;

static void prv_status_cb(DictationSession *session, DictationSessionStatus status,
                          char *transcription, void *context) {
  s_in_progress = false;
  DictationResultCallback cb = s_callback;
  s_callback = NULL;
  if (!cb) {
    return;
  }

  switch (status) {
    case DictationSessionStatusSuccess: {
      // The transcription buffer is freed right after this callback returns --
      // copy it into our own storage before doing anything else.
      strncpy(s_transcript, transcription, sizeof(s_transcript) - 1);
      s_transcript[sizeof(s_transcript) - 1] = '\0';
      cb(DICTATION_RESULT_SUCCESS, s_transcript);
      break;
    }

    case DictationSessionStatusFailureNoSpeechDetected:
    case DictationSessionStatusFailureTranscriptionRejected:
    case DictationSessionStatusFailureTranscriptionRejectedWithError:
    case DictationSessionStatusFailureSystemAborted:
      // The user backed out, or nothing usable was heard -- not an error worth
      // surfacing, just go back to listening for the next button press.
      cb(DICTATION_RESULT_CANCELLED, NULL);
      break;

    case DictationSessionStatusFailureConnectivityError:
    case DictationSessionStatusFailureDisabled:
    case DictationSessionStatusFailureInternalError:
    case DictationSessionStatusFailureRecognizerError:
    default:
      cb(DICTATION_RESULT_ERROR, NULL);
      break;
  }
}

void dictation_flow_init(void) {
  s_session = PBL_IF_MICROPHONE_ELSE(
      dictation_session_create(SESSION_BUFFER_SIZE, prv_status_cb, NULL), NULL);
  if (s_session) {
    dictation_session_enable_confirmation(s_session, true);
    dictation_session_enable_error_dialogs(s_session, true);
  }
}

void dictation_flow_deinit(void) {
  if (s_session) {
    dictation_session_destroy(s_session);
    s_session = NULL;
  }
  s_callback = NULL;
  s_in_progress = false;
}

bool dictation_flow_is_available(void) {
  return s_session != NULL;
}

void dictation_flow_start(DictationResultCallback callback) {
  if (!callback || s_in_progress) {
    return;
  }
  if (!s_session) {
    callback(DICTATION_RESULT_ERROR, NULL);
    return;
  }
  s_callback = callback;
  s_in_progress = true;
  dictation_session_start(s_session);
}
