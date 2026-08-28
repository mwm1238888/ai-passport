#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "bsp_button.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WC_PAGE_HOME = 0,
    WC_PAGE_WEATHER,
    WC_PAGE_QUOTE,
    WC_PAGE_REMIND,
    WC_PAGE_RECORDER,
    WC_PAGE_SETTINGS,
    WC_PAGE_COUNT
} wc_page_t;

typedef enum {
    WC_EV_NONE = 0,
    WC_EV_KEY,          /* btn + btn_ev carry the physical key */
    WC_EV_ALARM,
    WC_EV_SEDENTARY,
    WC_EV_HYDRATION,
    WC_EV_OFFWORK,
    WC_EV_QUOTE_TICK,
    WC_EV_TIME_SYNCED,
    WC_EV_WEATHER_OK,
    WC_EV_WEATHER_FAIL,
    WC_EV_RECORD_DONE,
} wc_event_id_t;

typedef struct {
    wc_event_id_t id;
    bsp_btn_t btn;
    bsp_btn_ev_t btn_ev;
} wc_event_t;

typedef struct {
    uint8_t  alarm_h, alarm_m;
    uint8_t  offwork_h, offwork_m;
    uint8_t  work_start_h, work_start_m;
    uint16_t sedentary_min;
    uint16_t hydration_min;
    uint16_t quote_min;
    uint8_t  muted;
    uint8_t  theme;                    /* 0=Pixel, 1=HUD */
    char     weather_city[24];
    char     weather_key[48];
} wc_settings_t;

void wc_state_init(void);                       /* load settings + create queue */
void wc_state_post(wc_event_t ev);              /* ISR/task-safe post (button cb, scheduler) */
bool wc_state_get(wc_event_t *out, uint32_t timeout_ms);

wc_page_t wc_state_page(void);
void      wc_state_set_page(wc_page_t p);

const wc_settings_t *wc_state_settings(void);
wc_settings_t       *wc_state_settings_mut(void);
void                 wc_state_save(void);       /* persist to NVS */

void     wc_state_notify_activity(void);        /* any key resets sedentary timer */
uint32_t wc_state_last_activity_ms(void);       /* monotonic ms since boot */

#ifdef __cplusplus
}
#endif
