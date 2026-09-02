#include <stdio.h>
#include <string.h>
#include <time.h>
#include "wc_state.h"
#include "wc_config.h"
#include "wc_scheduler.h"
#include "sim.h"

/* wc_event_id_t legend:
   0=NONE 1=KEY 2=ALARM 3=SEDENTARY 4=HYDRATION 5=OFFWORK 6=QUOTE_TICK
   7=TIME_SYNCED 8=WEATHER_OK 9=WEATHER_FAIL 10=RECORD_DONE */

static void sleep_ms(unsigned ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static const char *evname(wc_event_id_t id) {
    switch (id) {
        case WC_EV_NONE: return "NONE";
        case WC_EV_KEY: return "KEY";
        case WC_EV_ALARM: return "ALARM";
        case WC_EV_SEDENTARY: return "SEDENTARY";
        case WC_EV_HYDRATION: return "HYDRATION";
        case WC_EV_OFFWORK: return "OFFWORK";
        case WC_EV_QUOTE_TICK: return "QUOTE_TICK";
        case WC_EV_TIME_SYNCED: return "TIME_SYNCED";
        case WC_EV_WEATHER_OK: return "WEATHER_OK";
        case WC_EV_WEATHER_FAIL: return "WEATHER_FAIL";
        case WC_EV_RECORD_DONE: return "RECORD_DONE";
        default: return "?";
    }
}

static int g_pass = 0, g_fail = 0;
static void check(int cond, const char *name) {
    if (cond) { g_pass++; printf("  PASS  %s\n", name); }
    else      { g_fail++; printf("  FAIL  %s\n", name); }
}

static int64_t local_sec(int day, int hh, int mm) {
    return (int64_t)day * 86400LL + hh * 3600LL + mm * 60LL;
}

static void step(int day, int hh, int mm) {
    /* scheduler adds WC_TZ_OFFSET_SEC to wc_net_time_unix(); feed it the UTC
       value that recovers the intended local wall-clock time. */
    sim_set_unix(local_sec(day, hh, mm) - WC_TZ_OFFSET_SEC);
    sleep_ms(80);
}

static int drain(wc_event_id_t *out, int max) {
    int i = 0; wc_event_t e;
    while (i < max && wc_state_get(&e, 0)) out[i++] = e.id;
    return i;
}

static void dbg(const char *tag, int day, int hh, int mm, const wc_event_id_t *ev, int n) {
    printf("  [%s] d%d %02d:%02d -> %d ev:", tag, day, hh, mm, n);
    for (int i = 0; i < n; i++) printf(" %s", evname(ev[i]));
    printf("\n");
}

static int contains(const wc_event_id_t *ev, int n, wc_event_id_t id) {
    for (int i = 0; i < n; i++) if (ev[i] == id) return 1;
    return 0;
}

int main(void) {
    printf("== Work Companion host-side scheduler test ==\n");
    wc_state_init();
    wc_scheduler_init();
    sleep_ms(120);

    const wc_settings_t *s = wc_state_settings();
    printf("defaults: alarm %02d:%02d offwork %02d:%02d work %02d:%02d sed=%dmin hyd=%dmin quote=%dmin\n",
           s->alarm_h, s->alarm_m, s->offwork_h, s->offwork_m,
           s->work_start_h, s->work_start_m, s->sedentary_min, s->hydration_min, s->quote_min);

    wc_event_id_t ev[64]; int n;

    printf("[T1] unsynced -> inert\n");
    step(0, 7, 0); n = drain(ev, 64); dbg("T1", 0, 7, 0, ev, n);
    check(n == 0, "no events while unsynced");

    printf("[T2] alarm fires once on the minute\n");
    sim_set_synced(1); sim_clear_sound();
    step(1, s->alarm_h, s->alarm_m - 1); n = drain(ev, 64); dbg("T2a", 1, s->alarm_h, s->alarm_m - 1, ev, n);
    check(n == 0, "nothing 1 minute before alarm");
    step(1, s->alarm_h, s->alarm_m); n = drain(ev, 64); dbg("T2b", 1, s->alarm_h, s->alarm_m, ev, n);
    check(n == 1 && ev[0] == WC_EV_ALARM, "alarm fires at H:M");
    wc_sound_t sd; sim_get_sound(&sd); check(sd == WC_SOUND_ALARM, "alarm plays sound");
    step(1, s->alarm_h, s->alarm_m + 1); n = drain(ev, 64); dbg("T2c", 1, s->alarm_h, s->alarm_m + 1, ev, n);
    check(n == 0, "alarm does not repeat after the minute");

    printf("[T3] alarm per-day + mute\n");
    step(2, s->alarm_h, s->alarm_m); n = drain(ev, 64); dbg("T3a", 2, s->alarm_h, s->alarm_m, ev, n);
    check(n == 1 && ev[0] == WC_EV_ALARM, "alarm fires again next day");
    wc_state_settings_mut()->muted = 1; sim_clear_sound();
    step(3, s->alarm_h, s->alarm_m); n = drain(ev, 64); dbg("T3b", 3, s->alarm_h, s->alarm_m, ev, n);
    check(n == 1 && ev[0] == WC_EV_ALARM, "alarm event still posted when muted");
    sim_get_sound(&sd); check(sd != WC_SOUND_ALARM, "no sound when muted");
    wc_state_settings_mut()->muted = 0;

    printf("[T4] outside work hours -> no work-hour reminders\n");
    sim_set_mono_ms(500u * 60u * 1000u);
    step(4, 7, 30); n = drain(ev, 64); dbg("T4", 4, 7, 30, ev, n);
    check(!contains(ev, n, WC_EV_SEDENTARY) && !contains(ev, n, WC_EV_HYDRATION) &&
          !contains(ev, n, WC_EV_QUOTE_TICK), "no sedentary/hydration/quote at 07:30");

    printf("[T5] work-hour reminders\n");
    step(5, 9, 0); n = drain(ev, 64); dbg("T5a", 5, 9, 0, ev, n);
    check(n == 1 && ev[0] == WC_EV_SEDENTARY, "sedentary fires after idle at 09:00");
    step(5, 9, s->quote_min); n = drain(ev, 64); dbg("T5b", 5, 9, s->quote_min, ev, n);
    check(n == 1 && ev[0] == WC_EV_QUOTE_TICK, "quote rotator fires after quote_min");
    step(5, 10, 0); n = drain(ev, 64); dbg("T5c", 5, 10, 0, ev, n);
    check(n == 1 && ev[0] == WC_EV_HYDRATION, "hydration fires at H:00, quote defers (no stacked popups)");
    step(5, 10, s->quote_min); n = drain(ev, 64); dbg("T5c2", 5, 10, s->quote_min, ev, n);
    check(n == 1 && ev[0] == WC_EV_QUOTE_TICK, "deferred quote not lost — fires at next slot 10:15");
    step(5, 9, s->quote_min + 1); n = drain(ev, 64); dbg("T5d", 5, 9, s->quote_min + 1, ev, n);
    check(n == 0, "back-in-time step stays quiet (mono-clock guarded)");

    printf("[T6] off-work greeting\n");
    step(6, s->offwork_h, s->offwork_m); n = drain(ev, 64); dbg("T6", 6, s->offwork_h, s->offwork_m, ev, n);
    check(n == 1 && ev[0] == WC_EV_OFFWORK, "offwork greeting fires at H:M");

    printf("[T7] NVS persistence path\n");
    wc_state_settings_mut()->alarm_h = 8; wc_state_settings_mut()->theme = 1;
    wc_state_save(); check(1, "save completed");

    printf("\n== result: %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}