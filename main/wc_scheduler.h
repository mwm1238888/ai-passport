#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Background reminder engine: alarm / sedentary / hydration / off-work / quote.
   Posts wc_event_t to the state queue and fires sounds via wc_audio. */
void wc_scheduler_init(void);

#ifdef __cplusplus
}
#endif
