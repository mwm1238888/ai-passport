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
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app_main";

static void on_button(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    wc_event_t e = { .id = WC_EV_KEY, .btn = btn, .btn_ev = ev };
    wc_state_post(e);   /* non-blocking: runs in button task */
}

static void controller(void *arg) {
    ESP_LOGI(TAG, "controller task started");
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
            case WC_EV_WEATHER_OK:
            case WC_EV_WEATHER_FAIL:
                wc_ui_weather_event(ev.id);
                break;
            default:
                break;
            }
        }
        wc_ui_refresh();
    }
}

void app_main(void) {
    /* ESP-IDF subsystems the BSP does not bring up itself: must run before
     * any esp_netif / esp_event / NVS / wifi use, or the app panics at boot. */
    esp_netif_init();
    esp_event_loop_create_default();
    esp_err_t nr = nvs_flash_init();
    if (nr == ESP_ERR_NVS_NO_FREE_PAGES || nr == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    bsp_i2c_init();
    ESP_LOGI(TAG, "step: i2c done");
    if (bsp_display_init() != ESP_OK) return;
    ESP_LOGI(TAG, "step: display done");
    if (!bsp_lvgl_init()) return;
    ESP_LOGI(TAG, "step: lvgl done");
    bsp_display_backlight(WC_BACKLIGHT_ACTIVE);

    bsp_battery_init();
    ESP_LOGI(TAG, "step: battery done");
    bsp_audio_init();
    ESP_LOGI(TAG, "step: audio done");
    bsp_button_init(on_button, NULL);
    ESP_LOGI(TAG, "step: button done");

    wc_state_init();
    ESP_LOGI(TAG, "step: state done");
    wc_net_init();
    ESP_LOGI(TAG, "step: net done");
    wc_audio_init();
    ESP_LOGI(TAG, "step: wc_audio done");
    wc_recorder_init();
    ESP_LOGI(TAG, "step: recorder done");
    wc_ui_init();
    ESP_LOGI(TAG, "step: ui done");
    wc_scheduler_init();
    ESP_LOGI(TAG, "step: scheduler done");

    xTaskCreate(controller, "wc_ctrl", 6144, NULL, 5, NULL);
    ESP_LOGI(TAG, "step: controller spawned");
}
