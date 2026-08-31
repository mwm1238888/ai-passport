#pragma once
#include "wc_state.h"

#ifdef __cplusplus
extern "C" {
#endif

void wc_ui_init(void);
void wc_ui_dispatch(const wc_event_t *ev);   /* handle a key for the current page */
void wc_ui_refresh(void);                    /* tick/redraw the current page */
void wc_ui_show_reminder(wc_event_id_t id);   /* popup overlay for a due reminder */
void wc_ui_weather_event(wc_event_id_t id);   /* WC_EV_WEATHER_OK / FAIL from the async fetch */

#ifdef __cplusplus
}
#endif
