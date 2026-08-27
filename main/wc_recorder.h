#pragma once
#include <stdbool.h>
#include "bsp_button.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WC_REC_IDLE = 0,
    WC_REC_RECORDING,
    WC_REC_PLAYING,
} wc_rec_state_t;

void wc_recorder_init(void);
bool wc_recorder_start(void);     /* begin a new recording */
void wc_recorder_stop(void);      /* stop recording, posts WC_EV_RECORD_DONE */
wc_rec_state_t wc_recorder_state(void);

int           wc_recorder_count(void);
const char   *wc_recorder_name(int idx);
bool          wc_recorder_play(int idx);   /* play recording idx */
void          wc_recorder_stop_play(void);

#ifdef __cplusplus
}
#endif
