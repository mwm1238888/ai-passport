#include "wc_state.h"
#include "wc_config.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

#define WC_CFG_MAGIC   0x57434D50U   /* 'WCMP' */
#define WC_CFG_VERSION 2
#define WC_NVS_NS      "wcomp"
#define WC_NVS_KEY     "cfg"
#define WC_QUEUE_LEN   16

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    wc_settings_t s;
    uint32_t crc32;
} wc_cfg_blob_t;

static QueueHandle_t s_queue;
static wc_settings_t  s_settings;
static wc_page_t      s_page = WC_PAGE_HOME;
static uint32_t       s_last_activity_ms = 0;

static uint32_t crc32_calc(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
    }
    return ~crc;
}

static void defaults_apply(wc_settings_t *s) {
    s->alarm_h = WC_ALARM_HOURS;      s->alarm_m = WC_ALARM_MINUTES;
    s->offwork_h = WC_OFFWORK_HOURS;  s->offwork_m = WC_OFFWORK_MINUTES;
    s->work_start_h = WC_WORK_START_HOURS; s->work_start_m = WC_WORK_START_MIN;
    s->sedentary_min = WC_SEDENTARY_MIN;
    s->hydration_min = WC_HYDRATION_MIN;
    s->quote_min = WC_QUOTE_MIN;
    s->muted = 0;
    s->theme = 0;                     /* Pixel */
    s->weather_city[0] = 0;
    s->weather_key[0] = 0;
}

static bool load_blob(wc_cfg_blob_t *blob) {
    nvs_handle_t h;
    if (nvs_open(WC_NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t need = sizeof(*blob);
    esp_err_t e = nvs_get_blob(h, WC_NVS_KEY, blob, &need);
    nvs_close(h);
    return e == ESP_OK && need == sizeof(*blob);
}

static void save_blob(const wc_cfg_blob_t *blob) {
    nvs_handle_t h;
    if (nvs_open(WC_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, WC_NVS_KEY, blob, sizeof(*blob));
    nvs_commit(h);
    nvs_close(h);
}

void wc_state_init(void) {
    s_queue = xQueueCreate(WC_QUEUE_LEN, sizeof(wc_event_t));
    defaults_apply(&s_settings);

    wc_cfg_blob_t blob;
    if (load_blob(&blob) && blob.magic == WC_CFG_MAGIC &&
        blob.version == WC_CFG_VERSION && blob.size == sizeof(wc_settings_t)) {
        uint32_t crc = crc32_calc(&blob, offsetof(wc_cfg_blob_t, crc32));
        if (crc == blob.crc32) {
            s_settings = blob.s;
        }
    }
    s_last_activity_ms = (uint32_t)(esp_timer_get_time() / 1000);
}

void wc_state_post(wc_event_t ev) {
    if (s_queue) xQueueSend(s_queue, &ev, 0);
}

bool wc_state_get(wc_event_t *out, uint32_t timeout_ms) {
    return s_queue && xQueueReceive(s_queue, out, pdMS_TO_TICKS(timeout_ms));
}

wc_page_t wc_state_page(void) { return s_page; }
void wc_state_set_page(wc_page_t p) { if (p < WC_PAGE_COUNT) s_page = p; }

const wc_settings_t *wc_state_settings(void) { return &s_settings; }
wc_settings_t       *wc_state_settings_mut(void) { return &s_settings; }

void wc_state_save(void) {
    wc_cfg_blob_t blob;
    blob.magic = WC_CFG_MAGIC;
    blob.version = WC_CFG_VERSION;
    blob.size = sizeof(wc_settings_t);
    blob.s = s_settings;
    blob.crc32 = crc32_calc(&blob, offsetof(wc_cfg_blob_t, crc32));
    save_blob(&blob);
}

void wc_state_notify_activity(void) {
    s_last_activity_ms = (uint32_t)(esp_timer_get_time() / 1000);
}

uint32_t wc_state_last_activity_ms(void) {
    return s_last_activity_ms;
}
