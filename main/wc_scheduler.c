#include "wc_scheduler.h"
#include "wc_state.h"
#include "wc_audio.h"
#include "wc_net.h"
#include "wc_config.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include <stdint.h>

#define SECS_PER_DAY 86400

static void post(wc_event_id_t id) {
    wc_event_t ev = { .id = id };
    wc_state_post(ev);
}

/* Allow at most one notice per wall-clock minute. Reminder intervals share
   boundaries (quote 15min divides hydration 60min -> every hour on the hour),
   and within a minute the scheduler scans many times, so a per-scan flag is
   not enough to stop two popups stacking. The winning notice consumes the
   minute; a lower-priority event that was due yields its slot. */
static bool minute_take(int64_t cur_minute, int64_t *last) {
    if (*last == cur_minute) return false;
    *last = cur_minute;
    return true;
}

/* local time fields from unix seconds (UTC+8). Returns day index for alarm reset. */
static void breakdown(int64_t unix, int *hh, int *mm, int *day) {
    time_t t = (time_t)unix + WC_TZ_OFFSET_SEC;
    struct tm tmv;
    gmtime_r(&t, &tmv);
    *hh = tmv.tm_hour;
    *mm = tmv.tm_min;
    *day = tmv.tm_yday;
}

static bool in_work_hours(const wc_settings_t *s, int hh, int mm) {
    int now = hh * 60 + mm;
    int start = s->work_start_h * 60 + s->work_start_m;
    int end = s->offwork_h * 60 + s->offwork_m;
    return now >= start && now < end;
}

static void scheduler_task(void *arg) {
    int last_day = -1;
    int alarm_done_day = -1;
    int offwork_done_day = -1;
    int64_t last_hydration = -1;
    int64_t last_quote = -1;
    int64_t last_notice_minute = -1;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (!wc_net_time_synced()) continue;
        int64_t now = wc_net_time_unix();
        int hh, mm, day;
        breakdown(now, &hh, &mm, &day);

        /* midnight rollover */
        if (day != last_day) {
            last_day = day;
            alarm_done_day = -1;
            offwork_done_day = -1;
        }

        const wc_settings_t *s = wc_state_settings();
        int64_t cur_minute = (int64_t)day * 1440 + hh * 60 + mm;

        /* F1 alarm */
        if (alarm_done_day != day &&
            hh == s->alarm_h && mm == s->alarm_m) {
            alarm_done_day = day;
            if (!s->muted) wc_audio_play(WC_SOUND_ALARM);
            post(WC_EV_ALARM);
            last_notice_minute = cur_minute;
        }

        /* F6 off-work greeting (silent static reminder — only the alarm is audible) */
        if (offwork_done_day != day &&
            hh == s->offwork_h && mm == s->offwork_m) {
            offwork_done_day = day;
            post(WC_EV_OFFWORK);
            last_notice_minute = cur_minute;
        }

        if (!in_work_hours(s, hh, mm)) continue;

        /* F4 sedentary: silent static reminder; idle measured from last keypress */
        uint32_t cur_ms = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t idle_ms = cur_ms - wc_state_last_activity_ms();
        if (idle_ms >= (uint32_t)s->sedentary_min * 60 * 1000) {
            wc_state_notify_activity();   /* reset idle so it won't immediately refire */
            if (minute_take(cur_minute, &last_notice_minute)) post(WC_EV_SEDENTARY);
        }

        /* F5 hydration (silent static reminder) */
        if (last_hydration < 0) last_hydration = now;
        if (now - last_hydration >= (int64_t)s->hydration_min * 60) {
            last_hydration = now;
            if (minute_take(cur_minute, &last_notice_minute)) post(WC_EV_HYDRATION);
        }

        /* F3 quote rotation */
        if (last_quote < 0) last_quote = now;
        if (now - last_quote >= (int64_t)s->quote_min * 60) {
            last_quote = now;
            if (minute_take(cur_minute, &last_notice_minute)) post(WC_EV_QUOTE_TICK);
        }
    }
}

void wc_scheduler_init(void) {
    xTaskCreate(scheduler_task, "wc_sched", 4096, NULL, 4, NULL);
}
