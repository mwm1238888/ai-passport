#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WC_SOUND_ALARM = 0,    /* loud repeating beep */
    WC_SOUND_CHIME,        /* short double tone */
    WC_SOUND_REMINDER,     /* gentle single beep */
} wc_sound_t;

void wc_audio_init(void);     /* spawn the playback task */
void wc_audio_play(wc_sound_t s);   /* non-blocking, queues a sound */
void wc_audio_stop(void);    /* cancel current sound */

#ifdef __cplusplus
}
#endif
