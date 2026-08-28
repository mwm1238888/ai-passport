#include "wc_ui.h"
#include "wc_state.h"
#include "wc_net.h"
#include "wc_recorder.h"
#include "wc_config.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "ui_pixel.h"
#include "fonts/wc_cn_20.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ---- inspirational quotes (extern-declared in wc_config.h) ---- */
const char *const WC_QUOTES[] = {
    "专注当下，一步即达。",
    "水滴石穿，非一日之功。",
    "今日事，今日毕。",
    "难者不会，会者不难。",
    "行动是治愈焦虑的良药。",
    "慢一点，但别停下来。",
    "把大事拆小，把小事做完。",
    "休息也是工作的一部分。",
};
const size_t WC_QUOTES_COUNT = sizeof(WC_QUOTES) / sizeof(WC_QUOTES[0]);

static int  s_quote_idx = 0;
static int  s_weather_temp = 0;
static char s_weather_text[32] = "";
static bool s_reminder_active = false;
static wc_event_id_t s_reminder_id = WC_EV_NONE;

static lv_obj_t *s_scr;      /* pixel-theme screen for the current page */
static lv_obj_t *s_body;     /* content label inside the paper panel */
static lv_obj_t *s_hint;     /* bottom hint label next to the mascot */
static lv_obj_t *s_mascot;   /* TV-robot mascot */
static lv_obj_t *s_popup;    /* reminder popup panel, NULL when closed */
static int s_built_page = -1;

static const char *page_title(wc_page_t p) {
    switch (p) {
    case WC_PAGE_HOME:     return "工作伴侣";
    case WC_PAGE_WEATHER:  return "天气";
    case WC_PAGE_QUOTE:    return "鸡汤";
    case WC_PAGE_REMIND:   return "提醒";
    case WC_PAGE_RECORDER: return "录音";
    case WC_PAGE_SETTINGS: return "设置";
    default: return "";
    }
}

static const lv_font_t *body_font(wc_page_t p) {
    return (p == WC_PAGE_HOME) ? &lv_font_montserrat_28 : &wc_cn_20;
}

static void build_layout(wc_page_t p) {
    lv_obj_t *old = s_scr;
    ui_pixel_scr_opts_t opts = {
        .title = page_title(p),
        .title_font = &wc_cn_20,
        .bg = 0,
    };
    s_scr = ui_pixel_screen_create_opts(&opts);

    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 16, 56, 208, 168, UI_PAPER);
    s_body = ui_pixel_label(panel, "", body_font(p), UI_INK);
    lv_obj_set_width(s_body, 184);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_body, LV_ALIGN_TOP_LEFT, 0, 8);

    s_mascot = ui_pixel_mascot_create(s_scr, 16, 230);

    s_hint = ui_pixel_label(s_scr, "", &wc_cn_20, UI_INK);
    lv_obj_set_width(s_hint, 156);
    lv_label_set_long_mode(s_hint, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_hint, LV_ALIGN_TOP_LEFT, 66, 244);

    s_popup = NULL;
    lv_screen_load(s_scr);
    if (old) lv_obj_delete(old);
}

static void render_home(void) {
    char buf[64];
    if (wc_net_time_synced()) {
        time_t t = (time_t)wc_net_time_unix() + WC_TZ_OFFSET_SEC;
        struct tm tmv; gmtime_r(&t, &tmv);
        snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    } else {
        snprintf(buf, sizeof(buf), "--:--");
    }
    lv_label_set_text(s_body, buf);
    snprintf(buf, sizeof(buf), "电:%d%%  下页:OK", bsp_battery_soc());
    lv_label_set_text(s_hint, buf);
}

static void render_weather(void) {
    char buf[48];
    if (s_weather_text[0])
        snprintf(buf, sizeof(buf), "%d°C\n%s", s_weather_temp, s_weather_text);
    else
        snprintf(buf, sizeof(buf), "OK刷新");
    lv_label_set_text(s_body, buf);
    lv_label_set_text(s_hint, "需配WiFi+天气Key");
}

static void render_quote(void) {
    int idx = s_quote_idx % (int)WC_QUOTES_COUNT;
    lv_label_set_text(s_body, WC_QUOTES[idx]);
    lv_label_set_text(s_hint, "OK换一句");
}

static void render_remind(void) {
    const wc_settings_t *s = wc_state_settings();
    char buf[96];
    snprintf(buf, sizeof(buf),
        "闹铃 %02d:%02d\n久坐 %dmin\n喝水 %dmin\n下班 %02d:%02d",
        s->alarm_h, s->alarm_m, s->sedentary_min, s->hydration_min,
        s->offwork_h, s->offwork_m);
    lv_label_set_text(s_body, buf);
    lv_label_set_text(s_hint, "时间到会弹窗");
}

static void render_recorder(void) {
    char buf[64];
    wc_rec_state_t st = wc_recorder_state();
    int n = wc_recorder_count();
    const char *stt = (st == WC_REC_RECORDING) ? "REC" :
                      (st == WC_REC_PLAYING) ? "PLAY" : "IDLE";
    snprintf(buf, sizeof(buf), "%s\n文件:%d", stt, n);
    lv_label_set_text(s_body, buf);
    lv_label_set_text(s_hint, "OK开始/停  长按播放");
}

static void render_settings(void) {
    const wc_settings_t *s = wc_state_settings();
    char buf[96];
    snprintf(buf, sizeof(buf),
        "闹铃 %02d:%02d\n下班 %02d:%02d\n久坐%d 喝水%d 语录%d",
        s->alarm_h, s->alarm_m, s->offwork_h, s->offwork_m,
        s->sedentary_min, s->hydration_min, s->quote_min);
    lv_label_set_text(s_body, buf);
    lv_label_set_text(s_hint, "见wc_config.h调参");
}

static void render_page(void) {
    switch (wc_state_page()) {
    case WC_PAGE_HOME:     render_home(); break;
    case WC_PAGE_WEATHER:  render_weather(); break;
    case WC_PAGE_QUOTE:    render_quote(); break;
    case WC_PAGE_REMIND:   render_remind(); break;
    case WC_PAGE_RECORDER: render_recorder(); break;
    case WC_PAGE_SETTINGS: render_settings(); break;
    default: break;
    }
}

/* ---- reminder popup (silent static reminder, only the alarm has sound) ---- */
static const char *reminder_msg(wc_event_id_t id) {
    switch (id) {
    case WC_EV_ALARM:     return "该起床啦！";
    case WC_EV_SEDENTARY: return "久坐了，动一动";
    case WC_EV_HYDRATION: return "喝口水吧";
    case WC_EV_OFFWORK:   return "下班辛苦啦！";
    default:              return "提醒";
    }
}

static void jump_again_cb(lv_timer_t *t)
{
    ui_pixel_mascot_jump((lv_obj_t *)t->user_data);
}

static void popup_show(wc_event_id_t id) {
    if (s_popup) return;
    uint32_t accent = (id == WC_EV_ALARM) ? UI_RED : UI_INK;
    s_popup = ui_pixel_panel_create(s_scr, 30, 96, 180, 128, UI_PAPER);

    lv_obj_t *t = ui_pixel_label(s_popup, "提醒", &wc_cn_20, accent);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t *m = ui_pixel_label(s_popup, reminder_msg(id), &wc_cn_20, UI_INK);
    lv_obj_set_width(m, 148);
    lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);
    lv_obj_align(m, LV_ALIGN_CENTER, 0, 8);

    lv_obj_t *h = ui_pixel_label(s_popup, "任意键关闭", &wc_cn_20, 0x52525B);
    lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -2);

    ui_pixel_mascot_jump(s_mascot);
    lv_timer_t *tm = lv_timer_create(jump_again_cb, 260, s_mascot);
    lv_timer_set_repeat_count(tm, 1);
}

/* ---- key handling ---- */
static void on_ok(void) {
    switch (wc_state_page()) {
    case WC_PAGE_WEATHER: {
        const wc_settings_t *s = wc_state_settings();
        wc_weather_t w = {0};
        if (wc_weather_fetch(s->weather_city, s->weather_key, &w)) {
            s_weather_temp = w.temp;
            strncpy(s_weather_text, w.text, sizeof(s_weather_text) - 1);
            s_weather_text[sizeof(s_weather_text) - 1] = '\0';
        }
        break;
    }
    case WC_PAGE_QUOTE:
        s_quote_idx++;
        break;
    case WC_PAGE_RECORDER:
        if (wc_recorder_state() == WC_REC_RECORDING) wc_recorder_stop();
        else if (wc_recorder_state() == WC_REC_IDLE) wc_recorder_start();
        break;
    default: break;
    }
}

static void on_ok_long(void) {
    if (wc_state_page() == WC_PAGE_RECORDER && wc_recorder_count() > 0) {
        if (wc_recorder_state() == WC_REC_PLAYING) wc_recorder_stop_play();
        else wc_recorder_play(0);
    }
}

void wc_ui_dispatch(const wc_event_t *ev) {
    if (ev->id != WC_EV_KEY) return;
    if (s_reminder_active) {   /* any key dismisses the popup */
        s_reminder_active = false;
        s_reminder_id = WC_EV_NONE;
        return;
    }
    switch (ev->btn_ev) {
    case BSP_BTN_CLICK:
        if (ev->btn == BSP_BTN_UP) {
            wc_page_t p = wc_state_page();
            wc_state_set_page(p == 0 ? WC_PAGE_COUNT - 1 : p - 1);
        } else if (ev->btn == BSP_BTN_DOWN) {
            wc_page_t p = wc_state_page();
            wc_state_set_page((p + 1) % WC_PAGE_COUNT);
        } else if (ev->btn == BSP_BTN_OK) {
            on_ok();
        }
        break;
    case BSP_BTN_LONG:
        if (ev->btn == BSP_BTN_OK) on_ok_long();
        break;
    default: break;
    }
}

void wc_ui_refresh(void) {
    if (!bsp_lvgl_lock(100)) return;
    if (s_reminder_active) {
        popup_show(s_reminder_id);
    } else {
        if (s_popup) {
            lv_obj_delete(s_popup);
            s_popup = NULL;
        }
        if ((int)wc_state_page() != s_built_page) {
            build_layout(wc_state_page());
            s_built_page = (int)wc_state_page();
            ui_pixel_mascot_jump(s_mascot);
        }
        render_page();
    }
    bsp_lvgl_unlock();
}

void wc_ui_show_reminder(wc_event_id_t id) {
    s_reminder_active = true;
    s_reminder_id = id;
}

void wc_ui_init(void) {
    if (bsp_lvgl_lock(1000)) {
        build_layout(WC_PAGE_HOME);
        s_built_page = WC_PAGE_HOME;
        render_page();
        bsp_lvgl_unlock();
    }
}
