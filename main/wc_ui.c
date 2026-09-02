#include "wc_ui.h"
#include "wc_state.h"
#include "wc_net.h"
#include "wc_recorder.h"
#include "wc_config.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "ui_theme.h"
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
static int  s_weather_status = 0;   /* 0=idle, 1=fetching, 2=ok, 3=fail */
static bool s_reminder_active = false;
static wc_event_id_t s_reminder_id = WC_EV_NONE;

static lv_obj_t *s_scr;      /* current theme screen */
static lv_obj_t *s_body;     /* content label inside the content panel */
static lv_obj_t *s_hint;     /* bottom hint line */
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
    const ui_theme_t *t = ui_theme_get();
    lv_obj_t *old = s_scr;
    s_scr = t->screen_create(page_title(p), &wc_cn_20);

    lv_obj_t *panel = t->panel_create(s_scr, t->panel_x, t->panel_y,
                                      t->panel_w, t->panel_h);
    s_body = t->label(panel, "", body_font(p), UI_ROLE_MAIN);
    lv_obj_set_width(s_body, 186);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_body, LV_ALIGN_TOP_LEFT, 4, 8);

    s_hint = t->label(s_scr, "", &wc_cn_20, UI_ROLE_MUTED);
    lv_obj_set_width(s_hint, t->hint_w);
    lv_label_set_long_mode(s_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(s_hint, t->hint_x, t->hint_y);

    s_popup = NULL;
    lv_screen_load(s_scr);
    if (old) lv_obj_delete(old);
}

/* ---- persistent HUD status stripe (Pixel theme ignores it) ---- */
static void update_status(void) {
    const ui_theme_t *t = ui_theme_get();
    const char *wifi = (wc_net_state() == WC_NET_CONNECTED) ? "WIFI" : "OFF";
    const char *rec = (wc_recorder_state() == WC_REC_RECORDING) ? "REC " : "";
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%s 电%d%%", rec, wifi, bsp_battery_soc());
    t->status_set(s_scr, buf);
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
    lv_label_set_text(s_hint, "OK下页");
}

static void render_weather(void) {
    const wc_weather_result_t *w = wc_weather_result();
    char buf[48];
    if (s_weather_status == 1)
        snprintf(buf, sizeof(buf), "刷新中...");
    else if (w->available)
        snprintf(buf, sizeof(buf), "%d°C\n%s", w->temp, w->text);
    else
        snprintf(buf, sizeof(buf), "OK刷新");
    lv_label_set_text(s_body, buf);
    lv_label_set_text(s_hint, s_weather_status == 3 ? "获取失败,重试" : "需配WiFi+天气Key");
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
        "闹铃 %02d:%02d\n下班 %02d:%02d\n久坐%d 喝水%d 语录%d\n主题:%s",
        s->alarm_h, s->alarm_m, s->offwork_h, s->offwork_m,
        s->sedentary_min, s->hydration_min, s->quote_min,
        ui_theme_name(ui_theme_get_id()));
    lv_label_set_text(s_body, buf);
    lv_label_set_text(s_hint, "OK换主题");
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

static void popup_show(wc_event_id_t id) {
    if (s_popup) return;
    const ui_theme_t *t = ui_theme_get();
    bool alarm = (id == WC_EV_ALARM);

    s_popup = t->panel_create(s_scr, 30, 96, 180, 128);

    lv_obj_t *tt = t->label(s_popup, "提醒", &wc_cn_20,
                            alarm ? UI_ROLE_ALERT : UI_ROLE_MAIN);
    lv_obj_align(tt, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t *m = t->label(s_popup, reminder_msg(id), &wc_cn_20, UI_ROLE_MAIN);
    lv_obj_set_width(m, 148);
    lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);
    lv_obj_align(m, LV_ALIGN_CENTER, 0, 8);

    lv_obj_t *h = t->label(s_popup, "任意键关闭", &wc_cn_20, UI_ROLE_MUTED);
    lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -2);

    t->reminder_open(s_scr, s_popup, alarm);
}

static void close_popup(void) {
    if (s_reminder_active) {
        s_reminder_active = false;
        s_reminder_id = WC_EV_NONE;
        ui_theme_get()->reminder_close(s_scr);
    }
}

/* ---- key handling ---- */
static void on_ok(void) {
    switch (wc_state_page()) {
    case WC_PAGE_WEATHER: {
        const wc_settings_t *s = wc_state_settings();
        s_weather_status = 1;            /* show "刷新中..." */
        wc_weather_request(s->weather_city, s->weather_key);
        break;
    }
    case WC_PAGE_QUOTE:
        s_quote_idx++;
        break;
    case WC_PAGE_RECORDER:
        if (wc_recorder_state() == WC_REC_RECORDING) wc_recorder_stop();
        else if (wc_recorder_state() == WC_REC_IDLE) wc_recorder_start();
        break;
    case WC_PAGE_SETTINGS: {
        uint8_t id = (ui_theme_get_id() + 1) % ui_theme_count();
        ui_theme_set_id(id);
        wc_settings_t *s = wc_state_settings_mut();
        s->theme = id;
        wc_state_save();
        s_built_page = -1;                      /* rebuild on next refresh */
        break;
    }
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
    /* 与官方 on_key 一致:本函数可能直接改 LVGL 对象(close_popup/ui_theme_set_id),
     * 而 esp_lvgl_port 有独立刷新任务,必须持锁才线程安全,否则画面表现为"没反应"。 */
    if (!bsp_lvgl_lock(500)) return;
    if (ev->id != WC_EV_KEY) { bsp_lvgl_unlock(); return; }
    if (s_reminder_active) {   /* any key dismisses the popup */
        close_popup();
        bsp_lvgl_unlock();
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
    bsp_lvgl_unlock();
}

void wc_ui_refresh(void) {
    if (!bsp_lvgl_lock(100)) return;
    if (s_reminder_active) {
        if ((int)wc_state_page() != s_built_page || !s_scr) {
            build_layout(wc_state_page());
            s_built_page = (int)wc_state_page();
        }
        popup_show(s_reminder_id);
    } else {
        if (s_popup) {
            lv_obj_delete(s_popup);
            s_popup = NULL;
        }
        if ((int)wc_state_page() != s_built_page || !s_scr) {
            build_layout(wc_state_page());
            s_built_page = (int)wc_state_page();
        }
        render_page();
    }
    update_status();
    bsp_lvgl_unlock();
}

void wc_ui_show_reminder(wc_event_id_t id) {
    s_reminder_active = true;
    s_reminder_id = id;
}

void wc_ui_weather_event(wc_event_id_t id) {
    s_weather_status = (id == WC_EV_WEATHER_OK) ? 2 : 3;
}

void wc_ui_init(void) {
    if (bsp_lvgl_lock(1000)) {
        uint8_t th = wc_state_settings()->theme;
        if (th < ui_theme_count()) ui_theme_set_id(th);
        build_layout(WC_PAGE_HOME);
        s_built_page = WC_PAGE_HOME;
        render_page();
        update_status();
        bsp_lvgl_unlock();
    }
}