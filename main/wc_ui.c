#include "wc_ui.h"
#include "wc_state.h"
#include "wc_net.h"
#include "wc_recorder.h"
#include "wc_config.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "lvgl.h"
#include <stdio.h>
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

static lv_obj_t *title_lbl;
static lv_obj_t *body_lbl;
static lv_obj_t *hint_lbl;

static void build_layout(void) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    title_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(title_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 10);

    body_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(body_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(body_lbl, &lv_font_montserrat_28, 0);
    lv_obj_align(body_lbl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_width(body_lbl, 220);
    lv_label_set_long_mode(body_lbl, LV_LABEL_LONG_WRAP);

    hint_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(hint_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(hint_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(hint_lbl, LV_ALIGN_BOTTOM_MID, 0, -10);
}

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

static void render_home(void) {
    char buf[64];
    if (wc_net_time_synced()) {
        time_t t = (time_t)wc_net_time_unix() + WC_TZ_OFFSET_SEC;
        struct tm tmv; gmtime_r(&t, &tmv);
        snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    } else {
        snprintf(buf, sizeof(buf), "--:--");
    }
    lv_label_set_text(body_lbl, buf);
    snprintf(buf, sizeof(buf), "电:%d%%  下页:OK", bsp_battery_soc());
    lv_label_set_text(hint_lbl, buf);
}

static void render_weather(void) {
    char buf[48];
    if (s_weather_text[0])
        snprintf(buf, sizeof(buf), "%d°C\n%s", s_weather_temp, s_weather_text);
    else
        snprintf(buf, sizeof(buf), "OK刷新");
    lv_label_set_text(body_lbl, buf);
    lv_label_set_text(hint_lbl, "需配WiFi+天气Key");
}

static void render_quote(void) {
    int idx = s_quote_idx % (int)WC_QUOTES_COUNT;
    lv_label_set_text(body_lbl, WC_QUOTES[idx]);
    lv_label_set_text(hint_lbl, "OK换一句");
}

static void render_remind(void) {
    const wc_settings_t *s = wc_state_settings();
    char buf[96];
    snprintf(buf, sizeof(buf),
        "闹铃 %02d:%02d\n久坐 %dmin\n喝水 %dmin\n下班 %02d:%02d",
        s->alarm_h, s->alarm_m, s->sedentary_min, s->hydration_min,
        s->offwork_h, s->offwork_m);
    lv_label_set_text(body_lbl, buf);
    lv_label_set_text(hint_lbl, "时间到会弹窗");
}

static void render_recorder(void) {
    char buf[64];
    wc_rec_state_t st = wc_recorder_state();
    int n = wc_recorder_count();
    const char *stt = (st == WC_REC_RECORDING) ? "REC" :
                      (st == WC_REC_PLAYING) ? "PLAY" : "IDLE";
    snprintf(buf, sizeof(buf), "%s\n文件:%d", stt, n);
    lv_label_set_text(body_lbl, buf);
    lv_label_set_text(hint_lbl, "OK开始/停  长按播放");
}

static void render_settings(void) {
    const wc_settings_t *s = wc_state_settings();
    char buf[96];
    snprintf(buf, sizeof(buf),
        "闹铃 %02d:%02d\n下班 %02d:%02d\n久坐%d 喝水%d 语录%d",
        s->alarm_h, s->alarm_m, s->offwork_h, s->offwork_m,
        s->sedentary_min, s->hydration_min, s->quote_min);
    lv_label_set_text(body_lbl, buf);
    lv_label_set_text(hint_lbl, "见wc_config.h调参");
}

static void render_page(void) {
    lv_label_set_text(title_lbl, page_title(wc_state_page()));
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

/* ---- key handling ---- */
static void on_ok(void) {
    switch (wc_state_page()) {
    case WC_PAGE_WEATHER: {
        const wc_settings_t *s = wc_state_settings();
        wc_weather_t w = {0};
        if (wc_weather_fetch(s->weather_city, s->weather_key, &w)) {
            s_weather_temp = w.temp;
            strncpy(s_weather_text, w.text, sizeof(s_weather_text) - 1);
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
        const char *msg = "提醒";
        switch (s_reminder_id) {
        case WC_EV_ALARM:     msg = "该起床啦!"; break;
        case WC_EV_SEDENTARY: msg = "久坐了,动一动"; break;
        case WC_EV_HYDRATION: msg = "喝口水吧"; break;
        case WC_EV_OFFWORK:   msg = "下班辛苦啦!"; break;
        default: break;
        }
        lv_label_set_text(title_lbl, "提醒");
        lv_label_set_text(body_lbl, msg);
        lv_label_set_text(hint_lbl, "任意键关闭");
    } else {
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
        build_layout();
        render_page();
        bsp_lvgl_unlock();
    }
}
