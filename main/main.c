#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "wc_config.h"
#include "wc_state.h"
#include "wc_net.h"
#include "wc_audio.h"
#include "wc_recorder.h"
#include "wc_ui.h"
#include "wc_scheduler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void on_button(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    wc_event_t e = { .id = WC_EV_KEY, .btn = btn, .btn_ev = ev };
    wc_state_post(e);   /* non-blocking: runs in button task */
}

static void controller(void *arg) {
    wc_event_t ev;
    for (;;) {
        if (wc_state_get(&ev, 500)) {
            switch (ev.id) {
            case WC_EV_KEY:
                wc_ui_dispatch(&ev);
                wc_state_notify_activity();   /* any key resets sedentary idle */
                break;
            case WC_EV_ALARM:
            case WC_EV_SEDENTARY:
            case WC_EV_HYDRATION:
            case WC_EV_OFFWORK:
                wc_ui_show_reminder(ev.id);
                break;
            default:
                break;
            }
        }
        wc_ui_refresh();
    }
}

void app_main(void) {
    bsp_i2c_init();
    if (bsp_display_init() != ESP_OK) return;
    if (!bsp_lvgl_init()) return;
    bsp_display_backlight(WC_BACKLIGHT_ACTIVE);

    bsp_battery_init();
    bsp_audio_init();
    bsp_button_init(on_button, NULL);

    wc_state_init();
    wc_net_init();
    wc_audio_init();
    wc_recorder_init();
    wc_ui_init();
    wc_scheduler_init();

    xTaskCreate(controller, "wc_ctrl", 6144, NULL, 5, NULL);
}
