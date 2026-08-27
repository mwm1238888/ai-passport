#pragma once
#include <stdint.h>
#include <stddef.h>

/* ===== Work / reminder defaults (compile-time; runtime-overridable in settings) ===== */
#define WC_ALARM_HOURS         7
#define WC_ALARM_MINUTES       0
#define WC_OFFWORK_HOURS       18
#define WC_OFFWORK_MINUTES     0
#define WC_WORK_START_HOURS    9
#define WC_WORK_START_MIN      0
#define WC_SEDENTARY_MIN       45
#define WC_HYDRATION_MIN       60
#define WC_QUOTE_MIN           15

/* ===== Audio / recording ===== */
#define WC_REC_HZ              16000
#define WC_REC_BITS            16
#define WC_REC_CHANNELS        1
#define WC_REC_CHUNK_MS        200
#define WC_REC_PARTITION      "storage"
#define WC_REC_MOUNT          "/rec"
#define WC_REC_MAX_FILES       16

/* ===== Networking ===== */
#define WC_AP_SSID_PREFIX     "WCOMPANION"
#define WC_NTP_SERVER_1       "ntp.aliyun.com"
#define WC_NTP_SERVER_2       "pool.ntp.org"
#define WC_TZ_OFFSET_SEC      (8 * 3600)
#define WC_WIFI_TIMEOUT_MS    15000
#define WC_RESYNC_MS          (6LL * 60 * 60 * 1000)
#define WC_PROV_IDLE_MS       (5 * 60 * 1000)

/* ===== Weather (QWeather by default) ===== */
#define WC_WEATHER_HOST       "devapi.qweather.com"
#define WC_WEATHER_PATH       "/v7/weather/3d"

/* ===== UI / backlight ===== */
#define WC_BACKLIGHT_IDLE     30
#define WC_BACKLIGHT_ACTIVE   100

/* ===== Inspirational quotes (defined in wc_ui.c) ===== */
extern const char *const WC_QUOTES[];
extern const size_t WC_QUOTES_COUNT;
